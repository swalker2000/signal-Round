#include "SignalClient.h"

#include <Arduino.h>
#include <cstring>
#include <string>

static void connectTask(void* arg) {
  auto* self = static_cast<SignalClient*>(arg);
  self->connectJob();
  vTaskDelete(nullptr);
}

static bool signalName(const char* name) {
  return name != nullptr && strncmp(name, "Signal", 6) == 0;
}

static bool peerBetter(const Peer& key, const Peer& other) {
  const bool key_signal = signalName(key.name);
  const bool other_signal = signalName(other.name);
  if (key_signal != other_signal) {
    return key_signal;
  }
  if (key.nus != other.nus) {
    return key.nus && !other.nus;
  }
  return key.rssi > other.rssi;
}

void SignalClient::begin() {
  NimBLEDevice::init("");
  NimBLEDevice::setMTU(128);
  NimBLEDevice::setPower(9);
  Serial.println("BLE: клиент готов");
}

void SignalClient::setLink(Link link) {
  portENTER_CRITICAL(&mux_);
  link_ = link;
  portEXIT_CRITICAL(&mux_);
}

bool SignalClient::scanning() const {
  return NimBLEDevice::getScan()->isScanning();
}

bool SignalClient::fresh() const {
  return link_ == Link::Online && last_frame_ms_ != 0 && (millis() - last_frame_ms_) < 1500;
}

bool SignalClient::value(uint8_t id, int16_t& out) const {
  if (id == 0 || id > 10 || !known_[id]) {
    return false;
  }
  out = values_[id];
  return true;
}

void SignalClient::clearValues() {
  memset(known_, 0, sizeof(known_));
  memset(values_, 0, sizeof(values_));
  last_frame_ms_ = 0;
}

bool SignalClient::kickScan() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(this, false);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->setScanResponseTimeout(400);
  scan->setMaxResults(0);
  if (scan->isScanning()) {
    scan->stop();
  }
  const bool ok = scan->start(8000, false, true);
  scan_kick_ms_ = millis();
  Serial.println(ok ? "BLE: поиск запущен" : "BLE: поиск не запустился");
  return ok;
}

void SignalClient::startScan() {
  wanted_ = false;
  stop_requested_ = false;
  if (client_ != nullptr && client_->isConnected()) {
    client_->disconnect();
  }
  portENTER_CRITICAL(&mux_);
  peer_count_ = 0;
  peer_rev_++;
  portEXIT_CRITICAL(&mux_);
  setLink(Link::Scanning);
  kickScan();
}

void SignalClient::connectSaved(const SavedLink& link) {
  strncpy(addr_, link.addr, sizeof(addr_) - 1);
  addr_[sizeof(addr_) - 1] = '\0';
  addr_type_ = link.addr_type;
  stop_requested_ = false;
  wanted_ = true;
  last_attempt_ = 0;
  if (client_ != nullptr && client_->isConnected()) {
    client_->disconnect();
  }
  setLink(Link::Idle);
}

void SignalClient::requestStop() {
  stop_requested_ = true;
  wanted_ = false;
}

void SignalClient::halt() {
  wanted_ = false;
  stop_requested_ = true;
  if (client_ != nullptr && client_->isConnected()) {
    client_->disconnect();
  }
  if (!task_running_) {
    setLink(Link::Idle);
    stop_requested_ = false;
  }
}

void SignalClient::launchConnect() {
  if (task_running_ || !wanted_ || stop_requested_) {
    return;
  }
  last_attempt_ = millis();
  task_running_ = true;
  setLink(Link::Connecting);
  if (xTaskCreate(connectTask, "blec", 10240, this, 1, nullptr) != pdPASS) {
    task_running_ = false;
    setLink(Link::Down);
    Serial.println("BLE: нет памяти на подключение");
  }
}

void SignalClient::connectJob() {
  if (NimBLEDevice::getScan()->isScanning()) {
    NimBLEDevice::getScan()->stop();
  }
  if (stop_requested_ || !wanted_) {
    task_running_ = false;
    setLink(Link::Idle);
    return;
  }
  if (client_ == nullptr) {
    client_ = NimBLEDevice::createClient();
    if (client_ == nullptr) {
      task_running_ = false;
      setLink(Link::Down);
      return;
    }
    client_->setClientCallbacks(this, false);
    client_->setConnectTimeout(8000);
    client_->setConnectionParams(12, 24, 0, 200);
  }
  if (client_->isConnected()) {
    client_->disconnect();
  }

  Serial.printf("BLE: соединяюсь с %s\n", addr_);
  const NimBLEAddress address(std::string(addr_), addr_type_);
  const bool ok = client_->connect(address);
  if (stop_requested_ || !wanted_) {
    if (client_->isConnected()) {
      client_->disconnect();
    }
    task_running_ = false;
    setLink(Link::Idle);
    return;
  }
  if (!ok) {
    Serial.println("BLE: не подключился");
    task_running_ = false;
    setLink(Link::Down);
    return;
  }

  NimBLERemoteService* service = client_->getService(NUS_SERVICE);
  NimBLERemoteCharacteristic* tx = nullptr;
  if (service != nullptr) {
    rx_char_ = service->getCharacteristic(NUS_RX);
    tx = service->getCharacteristic(NUS_TX);
  }
  if (service == nullptr || rx_char_ == nullptr || tx == nullptr) {
    Serial.println("BLE: нет сервиса UART");
    client_->disconnect();
    rx_char_ = nullptr;
    task_running_ = false;
    setLink(Link::Down);
    return;
  }

  clearRx();
  const bool subscribed = tx->subscribe(
      true, [this](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
        this->pushRx(data, len);
      });
  if (!subscribed) {
    Serial.println("BLE: не включились уведомления");
    client_->disconnect();
    rx_char_ = nullptr;
    task_running_ = false;
    setLink(Link::Down);
    return;
  }
  delay(160);
  if (stop_requested_ || !wanted_) {
    client_->disconnect();
    task_running_ = false;
    setLink(Link::Idle);
    return;
  }

  clearRx();
  frame_len_ = 0;
  if (!writeRaw("TELM", 4)) {
    Serial.println("BLE: не включилась телеметрия");
    client_->disconnect();
    task_running_ = false;
    setLink(Link::Down);
    return;
  }
  last_puls_ = millis();
  writeRaw("PULS", 4);
  setLink(Link::Online);
  task_running_ = false;
  Serial.println("BLE: телеметрия");
}

void SignalClient::poll() {
  const Link now = link_;
  if (now != last_polled_) {
    if (now == Link::Connecting) {
      clearValues();
    }
    last_polled_ = now;
  }

  drain();

  if (stop_requested_ && !task_running_) {
    stop_requested_ = false;
    if (!wanted_) {
      setLink(Link::Idle);
    }
  }

  const Link after = link_;
  if (after == Link::Scanning && !stop_requested_ && !task_running_ &&
      !NimBLEDevice::getScan()->isScanning() && millis() - scan_kick_ms_ > 600) {
    kickScan();
  }
  if (after == Link::Online) {
    if (millis() - last_puls_ >= 1000) {
      last_puls_ = millis();
      if (!writeRaw("PULS", 4) && client_ != nullptr) {
        client_->disconnect();
        setLink(Link::Down);
      }
    }
    return;
  }
  if (wanted_ && !stop_requested_ && !task_running_ &&
      (after == Link::Idle || after == Link::Down)) {
    if (last_attempt_ == 0 || millis() - last_attempt_ > 2500) {
      launchConnect();
    }
  }
}

void SignalClient::drain() {
  uint8_t byte = 0;
  while (popRx(byte)) {
    if (frame_len_ > 0 || byte == 0x02) {
      if (frame_len_ == 0) {
        frame_[frame_len_++] = byte;
        continue;
      }
      frame_[frame_len_++] = byte;
      if (frame_len_ < 5) {
        continue;
      }
      frame_len_ = 0;
      if (frame_[4] != 0x03) {
        if (byte == 0x02) {
          frame_[0] = 0x02;
          frame_len_ = 1;
        }
        continue;
      }
      const uint8_t param = frame_[1];
      const int16_t value = (int16_t)((frame_[2] << 8) | frame_[3]);
      if (param >= 1 && param <= 10) {
        values_[param] = value;
        known_[param] = true;
        last_frame_ms_ = millis();
      }
    }
  }
}

bool SignalClient::writeRaw(const char* data, size_t len) {
  NimBLERemoteCharacteristic* rx = rx_char_;
  NimBLEClient* client = client_;
  if (rx == nullptr || client == nullptr || !client->isConnected()) {
    return false;
  }
  return rx->writeValue(reinterpret_cast<const uint8_t*>(data), len, false);
}

void SignalClient::pushRx(const uint8_t* data, size_t len) {
  portENTER_CRITICAL(&mux_);
  for (size_t i = 0; i < len; ++i) {
    const uint16_t next = (uint16_t)((rx_head_ + 1) % RX_CAP);
    if (next == rx_tail_) {
      break;
    }
    rx_[rx_head_] = data[i];
    rx_head_ = next;
  }
  portEXIT_CRITICAL(&mux_);
}

bool SignalClient::popRx(uint8_t& out) {
  bool got = false;
  portENTER_CRITICAL(&mux_);
  if (rx_tail_ != rx_head_) {
    out = rx_[rx_tail_];
    rx_tail_ = (uint16_t)((rx_tail_ + 1) % RX_CAP);
    got = true;
  }
  portEXIT_CRITICAL(&mux_);
  return got;
}

void SignalClient::clearRx() {
  portENTER_CRITICAL(&mux_);
  rx_head_ = 0;
  rx_tail_ = 0;
  portEXIT_CRITICAL(&mux_);
}

uint8_t SignalClient::copyPeers(Peer* out, uint8_t cap) {
  Peer tmp[PEER_MAX];
  uint8_t count = 0;
  portENTER_CRITICAL(&mux_);
  count = peer_count_;
  memcpy(tmp, peers_, count * sizeof(Peer));
  portEXIT_CRITICAL(&mux_);

  for (uint8_t i = 1; i < count; ++i) {
    Peer key = tmp[i];
    int j = (int)i - 1;
    while (j >= 0 && peerBetter(key, tmp[j])) {
      tmp[j + 1] = tmp[j];
      --j;
    }
    tmp[j + 1] = key;
  }
  if (count > cap) {
    count = cap;
  }
  memcpy(out, tmp, count * sizeof(Peer));
  return count;
}

void SignalClient::onDiscovered(const NimBLEAdvertisedDevice* device) {
  notePeer(device);
}

void SignalClient::onResult(const NimBLEAdvertisedDevice* device) {
  notePeer(device);
}

void SignalClient::notePeer(const NimBLEAdvertisedDevice* device) {
  if (device == nullptr || link_ != Link::Scanning) {
    return;
  }
  static const NimBLEUUID kNus(NUS_SERVICE);
  std::string raw_name = device->getName();
  if (raw_name.empty()) {
    raw_name = device->getPayloadByType(0x08);
  }
  const bool nus = device->isAdvertisingService(kNus);
  Peer peer{};
  const std::string raw_addr = device->getAddress().toString();
  strncpy(peer.addr, raw_addr.c_str(), sizeof(peer.addr) - 1);
  if (raw_name.empty()) {
    strncpy(peer.name, peer.addr, sizeof(peer.name) - 1);
  } else {
    strncpy(peer.name, raw_name.c_str(), sizeof(peer.name) - 1);
  }
  peer.type = device->getAddress().getType();
  peer.rssi = device->getRSSI();
  peer.nus = nus;

  bool added = false;
  portENTER_CRITICAL(&mux_);
  int found = -1;
  int weakest = -1;
  int weakest_plain = -1;
  for (uint8_t i = 0; i < peer_count_; ++i) {
    if (strcmp(peers_[i].addr, peer.addr) == 0) {
      found = (int)i;
      break;
    }
    if (weakest < 0 || peers_[i].rssi < peers_[weakest].rssi) {
      weakest = (int)i;
    }
    if (!signalName(peers_[i].name) &&
        (weakest_plain < 0 || peers_[i].rssi < peers_[weakest_plain].rssi)) {
      weakest_plain = (int)i;
    }
  }
  const bool keep = signalName(peer.name);
  if (found >= 0) {
    Peer& slot = peers_[found];
    const bool named = raw_name.empty() == false;
    const bool slot_is_addr = strcmp(slot.name, slot.addr) == 0;
    if (named && (slot_is_addr || strcmp(slot.name, peer.name) != 0)) {
      strncpy(slot.name, peer.name, sizeof(slot.name) - 1);
      slot.name[sizeof(slot.name) - 1] = '\0';
      peer_rev_++;
    }
    if (nus && !slot.nus) {
      slot.nus = true;
      peer_rev_++;
    }
    if (peer.rssi > slot.rssi) {
      slot.rssi = peer.rssi;
    }
  } else if (peer_count_ < PEER_MAX) {
    peers_[peer_count_++] = peer;
    peer_rev_++;
    added = true;
  } else if (keep && weakest_plain >= 0) {
    peers_[weakest_plain] = peer;
    peer_rev_++;
    added = true;
  } else if (!keep && weakest_plain >= 0 && peer.rssi > peers_[weakest_plain].rssi) {
    peers_[weakest_plain] = peer;
    peer_rev_++;
    added = true;
  } else if (keep && weakest >= 0 && !signalName(peers_[weakest].name)) {
    peers_[weakest] = peer;
    peer_rev_++;
    added = true;
  }
  portEXIT_CRITICAL(&mux_);
  if (added) {
    Serial.printf("BLE: видно %s%s\n", peer.nus ? "Signal " : "", peer.name);
  }
}

void SignalClient::onScanEnd(const NimBLEScanResults&, int reason) {
  Serial.printf("BLE: поиск остановился, причина %d, в списке %u\n", reason, peer_count_);
}

void SignalClient::onDisconnect(NimBLEClient*, int reason) {
  rx_char_ = nullptr;
  Serial.printf("BLE: разрыв %d\n", reason);
  if (!wanted_ || stop_requested_) {
    setLink(Link::Idle);
    return;
  }
  setLink(Link::Down);
}

void SignalClient::onConnectFail(NimBLEClient*, int reason) {
  Serial.printf("BLE: ошибка соединения %d\n", reason);
  if (wanted_ && !stop_requested_) {
    setLink(Link::Down);
  }
}

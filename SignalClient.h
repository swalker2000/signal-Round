#pragma once

#include <NimBLEDevice.h>

#include "config.h"

enum class Link : uint8_t {
  Idle,
  Scanning,
  Connecting,
  Online,
  Down,
};

struct Peer {
  char name[24];
  char addr[18];
  uint8_t type;
  int8_t rssi;
  bool nus;
};

class SignalClient : public NimBLEScanCallbacks, public NimBLEClientCallbacks {
 public:
  void begin();
  void startScan();
  void connectSaved(const SavedLink& link);
  void requestStop();
  void halt();
  void poll();

  bool scanning() const;
  bool busy() const { return task_running_; }
  Link link() const { return link_; }
  bool fresh() const;
  bool value(uint8_t id, int16_t& out) const;
  uint8_t copyPeers(Peer* out, uint8_t cap);
  uint16_t peerRev() const { return peer_rev_; }

  void onDiscovered(const NimBLEAdvertisedDevice* device) override;
  void onResult(const NimBLEAdvertisedDevice* device) override;
  void onScanEnd(const NimBLEScanResults& results, int reason) override;
  void onDisconnect(NimBLEClient* client, int reason) override;
  void onConnectFail(NimBLEClient* client, int reason) override;

  void connectJob();

 private:
  static const uint8_t PEER_MAX = 8;
  static const uint16_t RX_CAP = 256;

  void launchConnect();
  void setLink(Link link);
  void pushRx(const uint8_t* data, size_t len);
  bool popRx(uint8_t& out);
  void clearRx();
  void clearValues();
  bool writeRaw(const char* data, size_t len);
  void drain();
  bool kickScan();
  void notePeer(const NimBLEAdvertisedDevice* device);

  NimBLEClient* client_ = nullptr;
  NimBLERemoteCharacteristic* rx_char_ = nullptr;
  Peer peers_[PEER_MAX]{};
  uint8_t peer_count_ = 0;
  volatile uint16_t peer_rev_ = 0;
  uint32_t scan_kick_ms_ = 0;
  volatile bool task_running_ = false;
  volatile bool stop_requested_ = false;
  bool wanted_ = false;
  Link link_ = Link::Idle;
  Link last_polled_ = Link::Idle;
  char addr_[18]{};
  uint8_t addr_type_ = 0;
  uint32_t last_attempt_ = 0;
  uint32_t last_puls_ = 0;
  uint32_t last_frame_ms_ = 0;
  int16_t values_[11]{};
  bool known_[11]{};
  uint8_t rx_[RX_CAP]{};
  uint16_t rx_head_ = 0;
  uint16_t rx_tail_ = 0;
  uint8_t frame_[5]{};
  uint8_t frame_len_ = 0;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

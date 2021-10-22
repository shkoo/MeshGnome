#ifndef ESP_SNIFFER_PROTO_DISPATCH_H
#define ESP_SNIFFER_PROTO_DISPATCH_H

#include "ProtoDispatch.h"

#if defined(ESP8266)

// This is simliar to EspProtoDispatch, but instead of receiving with
// the ESP Now API, it receives with the sniffer API.  This gives us
// access to the RSSI.
class EspSnifferProtoDispatchClass : public ProtoDispatchBase {
 public:
  // Call this once per loop to transmit if needed.
  void espTransmitIfNeeded();

  // Add a hook to get called whenever a packet is received to track the RSSI.
  using rssi_hook_func_t = std::function<void(const uint8_t* src, int8_t rssi)>;
  void setRSSIHook(const rssi_hook_func_t& f);

 private:
  // TODO(nils): Figure out what this number should actually be.
  static constexpr size_t MAX_PKT_LEN = 70;

  static void _esp_sniffer_recv_cb(uint8_t* buf, uint16_t len);
  static void _esp_now_send_cb(u8* dst, u8 status);

  void protoDispatchBegin() override;

  bool _sendInProgress = false;
  uint8_t _lastPeer[ETH_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
  rssi_hook_func_t _rssi_hook;
};

extern EspSnifferProtoDispatchClass EspSnifferProtoDispatch;

#endif
#endif

#ifndef ESP_PROTO_DISPATCH_H
#define ESP_PROTO_DISPATCH_H

#include "ProtoDispatch.h"

#if defined(ESP8266)

class EspProtoDispatchClass : public ProtoDispatchBase {
 public:
  // Call this once per loop to transmit if needed.
  void espTransmitIfNeeded();

 private:
  static constexpr size_t MAX_PKT_LEN = 250;

  static void _esp_now_recv_cb(u8* src, u8* data, u8 len);
  static void _esp_now_send_cb(u8* dst, u8 status);

  void protoDispatchBegin() override;

  bool _sendInProgress = false;
  uint8_t _lastPeer[ETH_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
};

extern EspProtoDispatchClass EspProtoDispatch;

#endif
#endif

#ifndef CUSTOM_PROTO_H
#define CUSTOM_PROTO_H

#include <ProtoDispatch.h>

class CustomProto : public ProtoDispatchTarget {
 public:
  using OnReceiveFunc = std::function<void(const uint8_t* /* source ethernat address */,
                                           const uint8_t* /* packet contents */, size_t /* len */)>;
  using SendIfNeededFunc =
      std::function<int(uint8_t* /* destination ethernet address */, uint8_t* /* packet content */,
                        size_t /* maximum usable length of packet */)>;

  void setPacketReceivedFunc(const OnReceiveFunc& f) { _onPacketReceived = f; }

  void setSendIfNeededFunc(const SendIfNeededFunc& f) { _sendIfNeeded = f; }

 private:
  void onPacketReceived(const uint8_t* srcaddr, const uint8_t* pkt, size_t len) {
    if (_onPacketReceived) {
      _onPacketReceived(srcaddr, pkt, len);
    }
  }

  int sendIfNeeded(uint8_t* dstaddr, uint8_t* pkt, size_t maxlen) {
    if (_sendIfNeeded) {
      _sendIfNeeded(dstaddr, pkt, maxlen);
    }
  }

  OnReceiveFunc _onPacketReceived;
  SendIfNeededFunc _sendIfNeeded;
};

#endif

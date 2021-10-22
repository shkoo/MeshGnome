#ifndef PROTO_DISPATCH_H
#define PROTO_DISPATCH_H

#include <Arduino.h>
#include <WString.h>
#include <assert.h>

#include <utility>

bool etherIsBroadcast(const uint8_t* addr);

struct ProtoDispatchPktHdr {
  uint8_t src[6] = {0, 0, 0, 0, 0, 0};
  int8_t rssi = 0;  // Filled in by some dispatchers (e.g. EspSnifferProtoDispatch)
};

class ProtoDispatchTarget {
 public:
  static constexpr size_t ETH_ADDR_LEN = 6;  // 6 bytes in an ethernet address

  // Returns true if when has occured after ref.  Wraps around, so it
  // won't break after 2^32 milliseconds.
  //
  // Unsigned arithmetic is guaranteed to wrap around and be modulo
  // 2^32; signed arithmetic isn't.
  static bool timeIsAfter(uint32_t when, uint32_t ref) {
    uint32_t diff = ref - when;
    return diff >> 31;
  }

  // Called when an incoming packet is seen on the network
  virtual void onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) = 0;

  // If this protocol wants to send a packet, this should fill in pkt
  // up to a maximum length of maxlen, and then return the length
  // actually filled.
  //
  // If this protocol doesn't need to send a packet right now, it should return -1.
  virtual int sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) = 0;

  virtual ~ProtoDispatchTarget() = default;
};

using DispatchProto = std::pair<uint8_t /* protocol id */, ProtoDispatchTarget*>;

class ProtoDispatchBase {
 public:
  static constexpr size_t ETH_ADDR_LEN = ProtoDispatchTarget::ETH_ADDR_LEN;

  template <typename C>
  void begin(const C& container) {
    assert(_targetsStart == nullptr);
    _targetsStart = std::begin(container);
    _targetsEnd = std::end(container);
    _curSendTarget = _targetsStart;
    this->protoDispatchBegin();
  }

 protected:
  // Subclasses should call this when there's an opportunity to transmit.
  // If a transmission is desired, dst is filled with the destination address, pkt is filled with
  // packet data (up to a maximum of maxlen bytes), and the number of bytes filled is returned.
  //
  // If no transimssion is desired, this returns -1.
  int transmitIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);

  // Subclasses should call this when a packet is received from the network.
  void receivePacket(const ProtoDispatchPktHdr* hdr, const uint8_t* data, size_t len);

  // Subclasses may override this to do additional setup when begin() is called.
  virtual void protoDispatchBegin() {}

 private:
  const DispatchProto* _targetsStart = nullptr;
  const DispatchProto* _targetsEnd = nullptr;
  const DispatchProto* _curSendTarget = nullptr;
};

#endif

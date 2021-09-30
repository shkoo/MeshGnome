#ifndef FAKE_PROTO_DISPATCH_H
#define FAKE_PROTO_DISPATCH_H

#include <deque>
#include <memory>
#include <set>
#include <string>

#include "ProtoDispatch.h"

// Broadcasts all packets to all instances.  Useful for testing.
class FakeProtoDispatch : public ProtoDispatchBase {
 public:
  struct eth_addr {
    uint8_t addr[ETH_ADDR_LEN];
    eth_addr() = default;
    eth_addr(uint64_t val) {
      for (size_t i = 0; i != 6; ++i) {
        addr[i] = val >> ((5 - i) * 8);
      }
    }
    bool operator==(const eth_addr& rhs) const { return memcmp(addr, rhs.addr, ETH_ADDR_LEN) == 0; }
    bool operator!=(const eth_addr& rhs) const { return !(*this == rhs); }
    std::string str() const {
      char buf[100];
      sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4],
              addr[5]);
      return buf;
    }
  };

  FakeProtoDispatch(const eth_addr& localAddress);
  ~FakeProtoDispatch();

  void transmitAndReceive();

  void setSendLossy(double lossyFactor) {
    _sendLossyFactor = lossyFactor;
  }

 private:
  struct pkt {
    eth_addr src;
    eth_addr dst;
    std::string data;
  };

  static std::set<FakeProtoDispatch*> dispatches;

  eth_addr _localAddress;

  std::deque<std::shared_ptr<pkt>> _queue;

  double _sendLossyFactor = 0;
  double _curLossy = 0;
};

#endif

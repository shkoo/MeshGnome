#include "FakeProtoDispatch.h"

std::set<FakeProtoDispatch*> FakeProtoDispatch::dispatches;

FakeProtoDispatch::FakeProtoDispatch(const eth_addr& localAddress) : _localAddress(localAddress) {
  dispatches.insert(this);
}

FakeProtoDispatch::~FakeProtoDispatch() { dispatches.erase(this); }

void FakeProtoDispatch::transmitAndReceive() {
  while (!_queue.empty()) {
    std::shared_ptr<pkt> in = _queue.front();
    _queue.pop_front();

    printf("Fake dispatch %s receiving a packet %p of length %lu from %s\n",
           _localAddress.str().c_str(), in.get(), in->data.size(), in->src.str().c_str());
    receivePacket(in->src.addr, (const uint8_t*)in->data.data(), in->data.size());
  }

  eth_addr dst;
  size_t maxlen = 250;
  uint8_t buf[maxlen];

  int xmitlen = transmitIfNeeded(dst.addr, buf, maxlen);
  if (xmitlen < 0) {
    return;
  }

  bool bcast = etherIsBroadcast(dst.addr);

  std::shared_ptr<pkt> p = std::make_shared<pkt>();
  p->src = _localAddress;
  p->dst = dst;
  p->data = std::string((char*)buf, xmitlen);

  printf("Fake dispatch %s transmitting a packet %p of length %d to %s\n",
         _localAddress.str().c_str(), p.get(), xmitlen, dst.str().c_str());

  for (int i = 0; i != xmitlen; ++i) {
    printf(" %02x", buf[i]);
    if (isprint(buf[i])) {
      printf(" (%c)", buf[i]);
    } else {
      printf("    ");
    }
  }
  putchar('\n');
  _curLossy += _sendLossyFactor;
  if (_curLossy > 1) {
    _curLossy -= 1;
    printf("DROPPING due to simulated packet loss\n");
    return;
  }

  for (FakeProtoDispatch* remote : dispatches) {
    if (remote == this) {
      continue;
    }
    if (!bcast && remote->_localAddress != dst) {
      continue;
    }
    remote->_queue.push_back(p);
    printf("Queued to %s\n", remote->_localAddress.str().c_str());
  }
}

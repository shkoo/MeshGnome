#include "ProtoDispatch.h"

#include <cstdio>

void ProtoDispatchBase::begin() {
  assert(!_targets.empty());
  assert(_curSendTarget == nullptr);
  _curSendTarget = _targets.data();
  protoDispatchBegin();
}

void ProtoDispatchBase::addProtocol(uint8_t protocolId, ProtoDispatchTarget* target) {
  _targets.emplace_back(protocolId, target);
  if (_curSendTarget != nullptr) {
    // Reset to beginning in case reallocation occured.
    _curSendTarget = _targets.data();
  }
}

void ProtoDispatchBase::receivePacket(const ProtoDispatchPktHdr* hdr, const uint8_t* data,
                                      size_t len) {
  if (len < 1) {
    // No protocol ID?
    return;
  }
  uint8_t protoId = data[0];

  for (const auto& proto : _targets) {
    if (proto.first == protoId) {
      proto.second->onPacketReceived(hdr, data + 1, len - 1);
    }
  }
}

int ProtoDispatchBase::transmitIfNeeded(uint8_t* dst, uint8_t* data, size_t maxlen) {
  if (_curSendTarget == nullptr) {
    return -1;
  }
  if (_curSendTarget == (_targets.data() + _targets.size())) {
    _curSendTarget = _targets.data();
    assert(!_targets.empty());
  }

  int res = _curSendTarget->second->sendIfNeeded(dst, data + 1, maxlen - 1);
  if (res > 0) {
    data[0] = _curSendTarget->first;  // protocol id
    ++_curSendTarget;
    return res + 1;
  }
  ++_curSendTarget;
  return -1;
}

bool etherIsBroadcast(const uint8_t* addr) {
  for (size_t i = 0; i != ProtoDispatchBase::ETH_ADDR_LEN; ++i) {
    if (addr[i] != 0xff) {
      return false;
    }
  }
  return true;
}

String etherToString(const uint8_t* addr) {
  char buf[3 * 6 + 1];
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4],
          addr[5]);
  return buf;
}

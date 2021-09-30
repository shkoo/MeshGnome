#include "ProtoDispatch.h"

void ProtoDispatchBase::receivePacket(const uint8_t* src, const uint8_t* data, size_t len) {
  if (len < 1) {
    // No protocol ID?
    return;
  }
  uint8_t protoId = data[0];

  for (const DispatchProto* target = _targetsStart; target != _targetsEnd; ++target) {
    if (target->first == protoId) {
      target->second->onPacketReceived(src, data + 1, len - 1);
    }
  }
}

int ProtoDispatchBase::transmitIfNeeded(uint8_t* dst, uint8_t* data, size_t maxlen) {
  if (_curSendTarget == _targetsEnd) {
    _curSendTarget = _targetsStart;
    assert(_curSendTarget != _targetsEnd);
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

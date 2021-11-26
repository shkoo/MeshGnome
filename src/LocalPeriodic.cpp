#include "LocalPeriodic.h"

#include <limits>

void LocalPeriodicBuf::begin(const MeshSyncTime* timeSource, uint32_t everyMs) {
  _timeSource = timeSource;
  _everyMs = everyMs;
  _synced = false;
  _scheduleNextTimeStep();
}

void LocalPeriodicBuf::run() {
  if (!_everyMs || !_timeSource) {
    return;
  }

  uint32_t now = millis();
  if (!timeIsAfter(now, _nextTimeStep)) {
    return;
  }
  onTimeStep();
  _scheduleNextTimeStep();
}

void LocalPeriodicBuf::_scheduleNextTimeStep() {
  assert(_everyMs > 0);
  uint32_t now = millis();
  uint32_t synced = _timeSource->localToSynced(now);
  // If we don't have time to transmit, wait until the next interval to start.
  uint32_t intervalStart = synced + (_everyMs * 4 / 5);
  intervalStart -= intervalStart % _everyMs;

  _nextBroadcast = _timeSource->syncedToLocal(
      random(intervalStart + _everyMs * 1 / 5, intervalStart + _everyMs * 4 / 5));

  _nextTimeStep = _timeSource->syncedToLocal(intervalStart + _everyMs);
}

void LocalPeriodicBuf::onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt,
                                        size_t len) {
  onReceivedBuf(hdr, pkt, len);
}

int LocalPeriodicBuf::sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) {
  if (!timeIsAfter(millis(), _nextBroadcast)) {
    return -1;
  }
  // Don't broadcast again until next time step.  This probably won't end up being the final time.
  _nextBroadcast += _everyMs;
  int res = onTransmitBuf(pkt, maxlen);
  if (res < 0) {
    return res;
  }

  // Send to broadcast address
  memset(ethaddr, 0xff, 6);

  return res;
}

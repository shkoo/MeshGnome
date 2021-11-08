#include "LocalPeriodic.h"

void LocalPeriodicBuf::begin(const MeshSyncTime* timeSource, uint32_t everyMs) {
  _timeSource = timeSource;
  _everyMs = everyMs;
  _synced = false;
}

void LocalPeriodicBuf::run() {
  if (!_everyMs || !_timeSource) {
    return;
  }
  uint32_t syncedMillis = _timeSource->syncedMillis();
  uint32_t timeSinceLast = syncedMillis - _lastRun;
  //    printf("%u time since last %u: %u\n", synced, _lastRun, timeSinceLast);
  if (timeSinceLast < _everyMs) {
    return;
  }
  if (timeSinceLast > 2 * _everyMs || (_lastRun > 0 && _lastRun < _everyMs)) {
    //      printf("oob; resyncing\n");
    _synced = false;
  }

  if (_synced) {
    _lastRun += _everyMs;
  } else {
    _lastRun = syncedMillis;
    _lastRun -= _lastRun % _everyMs;
    _synced = true;
  }
  // Send sometime in the middle 3/5ths of the step time period, but
  // randomly so we're less likely to step on other local
  // broadcasters.
  _nextBroadcastMillis = millis() + random(_everyMs * 1 / 5, _everyMs * 4 / 5);
  onTimeStep();
}

void LocalPeriodicBuf::onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt,
                                        size_t len) {
  onReceivedBuf(hdr, pkt, len);
}

int LocalPeriodicBuf::sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) {
  uint32_t timeAfterNext = millis() - _nextBroadcastMillis;
  if (timeAfterNext > _everyMs) {
    return -1;
  }
  // Don't broadcast again until next time step.
  _nextBroadcastMillis += std::numeric_limits<uint32_t>::max() / 2;
  int res = onTransmitBuf(pkt, maxlen);
  if (res < 0) {
    return res;
  }

  // Send to broadcast address
  memset(ethaddr, 0xff, 6);

  return res;
}

#include "MeshSyncTime.h"

#include <limits>

MeshSyncTime::MeshSyncTime() {
  _syncStart = millis() - random(0, TRANSMIT_INTERVAL_MS);
  _adjustmentEnd = millis();
  _nextTransmit = millis() + random(0, TRANSMIT_INTERVAL_MS);
}

void MeshSyncTime::onPacketReceived(const ProtoDispatchPktHdr*  hdr, const uint8_t* pkt,
                                    size_t len) {
  if (len != sizeof(MeshSyncTimeData)) {
    return;
  }

  MeshSyncTimeData remoteData;
  memcpy(&remoteData, pkt, sizeof(MeshSyncTimeData));

  uint32_t now = millis();
  if (_receiveHook) {
    _receiveHook(hdr, remoteData.syncedMillis - localToSynced(now), remoteData.syncedDuration);
  }

  if (remoteData.syncedDuration < syncedDuration(now)) {
    // The time synchronization we already have has been active longer
    // than this origin's synchronization, so don't update.
    return;
  }

  applySync(remoteData.syncedMillis, now);
  _syncStart = now - (remoteData.syncedDuration * 3/4);
}

void MeshSyncTime::applySync(uint32_t newSyncedMillis, uint32_t now) {
  // If we're in the middle of an adjustment, apply the adjustment so far before
  // resetting the adjustment.
  _originOffset = localToSynced(now) - now;

  uint32_t newOriginOffset = newSyncedMillis - now;

  uint32_t forwardAdjust = newOriginOffset - _originOffset;
  uint32_t backwardAdjust = _originOffset - newOriginOffset;
  if (forwardAdjust < MAX_ADJUST_MS) {
    // Run double time for the adjustment time.
    _adjustForward = true;
    _adjustmentEnd = now + forwardAdjust;
    if (_adjustHook) {
      _adjustHook(forwardAdjust);
    }
  } else if (backwardAdjust < MAX_ADJUST_MS) {
    // Run half time for double the adjustment time.
    _adjustForward = false;
    _adjustmentEnd = now + backwardAdjust * 2;
    if (_adjustHook) {
      _adjustHook(-backwardAdjust);
    }
  } else {
    // Don't adjust at all; just jump.  But reset our synced time.
    _adjustmentEnd = now;
    if (_jumpHook) {
      _jumpHook(forwardAdjust);
    }
  }

  _originOffset = newOriginOffset;
}

int MeshSyncTime::sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) {
  uint32_t now = millis();
  if (!timeIsAfter(now, _nextTransmit)) {
    return -1;
  }
  _nextTransmit = now + random(TRANSMIT_INTERVAL_MS, 2 * TRANSMIT_INTERVAL_MS);

  assert(maxlen >= sizeof(MeshSyncTimeData));

  MeshSyncTimeData data;
  data.syncedDuration = syncedDuration();
  data.syncedMillis = localToSynced(now);

  memset(ethaddr, 0xFF, 6);
  memcpy(pkt, &data, sizeof(data));
  return sizeof(data);
}

uint32_t MeshSyncTime::localToSynced(uint32_t localMillis) const {
  uint32_t adjusted = localMillis + _originOffset;
  if (timeIsAfter(_adjustmentEnd, localMillis)) {
    uint32_t timeUntilAdjustmentDone = _adjustmentEnd - localMillis;

    // "adjusted" is what the time should be at _adjustmentEnd.
    // If we're before then, we need to compensate.
    if (_adjustForward) {
      // We're adjusting forward, so increase our time as we get closer to the end of adjustment.
      adjusted -= timeUntilAdjustmentDone;
    } else {
      // We're adjusting backward, so decrease our time as we get closer to the end of adjustment.
      adjusted += timeUntilAdjustmentDone / 2;
    }
  }
  return adjusted;
}

uint32_t MeshSyncTime::syncedToLocal(uint32_t syncedMillis) const {
  uint32_t adjusted = syncedMillis - _originOffset;
  if (timeIsAfter(_adjustmentEnd, adjusted)) {
    // Time in synced seconds.
    uint32_t timeUntilAdjustmentDone = _adjustmentEnd - adjusted;

    if (_adjustForward) {
      // Synced clock is running twice as fast as local.
      adjusted += timeUntilAdjustmentDone / 2;
    } else {
      // Synced clock is running half as fast as local.
      adjusted -= timeUntilAdjustmentDone;
    }
  }
  return adjusted;
}

uint32_t MeshSyncTime::syncedDuration(uint32_t now) const { return now - _syncStart; }

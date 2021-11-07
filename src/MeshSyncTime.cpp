#include "MeshSyncTime.h"

#include <limits>

MeshSyncTime::MeshSyncTime() {
  _originOffset = random(1, std::numeric_limits<long>::max());

  // Random, but long enough ago that if we get a real sync time it'll take precedence.
  _originTime = millis();
  _originTime -= (std::numeric_limits<uint32_t>::max() / 2);
  _originTime += (uint32_t)random(1, std::numeric_limits<uint32_t>::max() / 16);

  _nextTransmit = millis() + random(TRANSMIT_INTERVAL_MS, 2 * TRANSMIT_INTERVAL_MS);
}

void MeshSyncTime::onPacketReceived(const ProtoDispatchPktHdr* /* hdr */, const uint8_t* pkt,
                                    size_t len) {
  if (len != sizeof(MeshSyncTimeData)) {
    return;
  }

  MeshSyncTimeData _remoteData;
  memcpy(&_remoteData, pkt, sizeof(MeshSyncTimeData));
  if (_remoteData.distanceMillis >= distanceMillis()) {
    // Not better.
    return;
  }

  // Update!
  _originTime = millis() - _remoteData.distanceMillis;
  _originOffset = _remoteData.syncedMillis - millis();

  // Immediately rebroadcast instead of waiting.
  _nextTransmit = millis() + random(1, 50);
}

int MeshSyncTime::sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) {
  uint32_t now = millis();
  if (!timeIsAfter(now, _nextTransmit)) {
    return -1;
  }
  _nextTransmit = now + random(TRANSMIT_INTERVAL_MS, 2 * TRANSMIT_INTERVAL_MS);

  assert(maxlen >= sizeof(MeshSyncTimeData));

  MeshSyncTimeData data;
  data.distanceMillis = distanceMillis();
  data.syncedMillis = syncedMillis();

  memset(ethaddr, 0xFF, 6);
  memcpy(pkt, &data, sizeof(data));
  return sizeof(data);
}

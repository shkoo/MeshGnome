#ifndef MESH_SYNC_TIME_H
#define MESH_SYNC_TIME_H

#include "MeshSync.h"

struct MeshSyncTimeData {
  // Millisecond distance from origin through all the hops
  uint32_t distanceMillis;

  // Current synchronized timestamp
  uint32_t syncedMillis;
};

class MeshSyncTime : public ProtoDispatchTarget {
 public:
  MeshSyncTime();
  void onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) override;
  int sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) override;

  uint32_t syncedMillis() const { return millis() + _originOffset; }
  uint32_t distanceMillis() const { return millis() - _originTime; }

 private:
  static constexpr uint32_t TRANSMIT_INTERVAL_MS = 1000 * 10;

  // Time in local millis that the origin generated a time.
  // distanceMillis is millis() - _originTime
  uint32_t _originTime;

  // Offset of origin with relation to local time.
  // syncedMillis() = millis() + _originOffset;
  uint32_t _originOffset = 0;

  // Next time to transmit our synchronizing packet
  uint32_t _nextTransmit = 0;
};

// Runs a periodic function occasionally
class SyncedPeriodic {
 public:
  void begin(uint32_t everyMs, uint32_t offset = 0) {
    _everyMs = everyMs;
    _offset = offset;
  }

  void run(uint32_t syncedMillis, const std::function<void(void)>& f) {
    if (!_everyMs) {
      return;
    }
    syncedMillis -= _offset;
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
      //      printf("Advancing to %u\n", _lastRun);
    } else {
      _lastRun = syncedMillis;
      //      printf("synced %u", _lastRun);
      _lastRun -= _lastRun % _everyMs;
      _synced = true;
      //      printf(" to %u\n", _lastRun);
    }

    f();
  }

  void end() {
    _everyMs = 0;
    _synced = false;
  }

 private:
  uint32_t _everyMs = 0;
  uint32_t _offset = 0;
  
  // Synced last runtime.
  uint32_t _lastRun = 0;
  bool _synced = false;
};

#endif

#ifndef MESH_SYNC_TIME_H
#define MESH_SYNC_TIME_H

#include <functional>

#include "MeshSync.h"

struct MeshSyncTimeData {
  // Current synchronized timestamp
  uint32_t syncedMillis;

  // Length of time (in millis) that we've been synced.
  uint32_t syncedDuration;
};

class MeshSyncTime : public ProtoDispatchTarget {
 public:
  MeshSyncTime();
  void onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) override;
  int sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) override;

  void applySync(uint32_t synced, uint32_t now = millis());
  uint32_t syncedDuration(uint32_t now = millis()) const;

  uint32_t localToSynced(uint32_t localMillis) const;
  uint32_t syncedToLocal(uint32_t syncedMillis) const;

  using adjust_hook_func_t = std::function<void(int32_t adjustment)>;
  void setAdjustHook(const adjust_hook_func_t& f) { _adjustHook = f; }

  using jump_hook_func_t = std::function<void(int32_t adjustment)>;
  void setJumpHook(const jump_hook_func_t& f) { _jumpHook = f; }

  using receive_hook_func_t =
      std::function<void(const ProtoDispatchPktHdr* hdr, int32_t offset, uint32_t syncedDuration)>;
  void setReceiveHook(const receive_hook_func_t& f) { _receiveHook = f; }

 private:
  static constexpr uint32_t TRANSMIT_INTERVAL_MS = 1000 * 10;
  // Maximum time different to adjust instead of jump

  static constexpr uint32_t MAX_ADJUST_MS = 1000;

  // Local millis of when synchronized time started.
  uint32_t _syncStart = 0;

  // Offset of origin with relation to local time.
  // syncedMillis() = millis() + _originOffset;
  //
  // If we're in the process of adjustment, this is the target offset
  // we're adjusting towards.
  uint32_t _originOffset = 0;

  // True if we're trying to run our clock faster to catch up with the synced clock.

  bool _adjustForward = false;
  // Time in local millis when we're ending our adjustment.
  uint32_t _adjustmentEnd = 0;

  // Next time to transmit our synchronizing packet
  uint32_t _nextTransmit = 0;

  adjust_hook_func_t _adjustHook;
  jump_hook_func_t _jumpHook;
  receive_hook_func_t _receiveHook;
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

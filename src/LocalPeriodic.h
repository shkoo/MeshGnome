#ifndef LOCAL_PERIODIC_H
#define LOCAL_PERIODIC_H

#include "MeshSyncTime.h"
#include "ProtoDispatch.h"

class LocalPeriodicBuf : public ProtoDispatchTarget {
 public:
  void begin(const MeshSyncTime* timeSource, uint32_t everyMs);
  void run();
  ~LocalPeriodicBuf() override = default;

  virtual void onTimeStep() {}
  virtual void onReceivedBuf(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) = 0;
  virtual int onTransmitBuf(uint8_t* pkt, size_t maxlen) = 0;

 protected:
  void onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) override;
  int sendIfNeeded(uint8_t* ethaddr, uint8_t* pkt, size_t maxlen) override;

 private:
  const MeshSyncTime* _timeSource = nullptr;
  bool _synced = false;

  uint32_t _everyMs = 0;

  // Next time to broadcast in local time
  uint32_t _nextBroadcastMillis = 0;

  // Last time step in synced time.
  uint32_t _lastRun = 0;
};

template <typename T>
class LocalPeriodicStruct : public LocalPeriodicBuf {
 public:
  ~LocalPeriodicStruct() override = default;

  using timestep_func = std::function<void(void)>;
  void setTimestepHook(const timestep_func& f) { _timestepHook = f; }

  using received_func = std::function<void(const ProtoDispatchPktHdr* hdr, const T& val)>;
  void setReceivedHook(const received_func& f) { _receivedHook = f; }

  using fill_for_transmit_func = std::function<bool(T* val)>;
  void setFillForTransmitHook(const fill_for_transmit_func& f) { _fillForTransmitHook = f; }

 protected:
  void onTimeStep() override {
    if (_timestepHook) _timestepHook();
  }
  void onReceivedBuf(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) override {
    if (!_receivedHook) {
      return;
    }
    if (len == sizeof(T)) {
      T val;
      // Make sure alignment is proper.
      memcpy(&val, pkt, sizeof(T));
      _receivedHook(hdr, val);
    }
  }
  int onTransmitBuf(uint8_t* pkt, size_t maxlen) override {
    if (!_fillForTransmitHook) {
      return -1;
    }

    assert(sizeof(T) <= maxlen);
    // Allocate as a struct for proper alignment.
    T val;
    if (_fillForTransmitHook(&val)) {
      memcpy(pkt, &val, sizeof(T));
      return sizeof(T);
    }

    return -1;
  }

 private:
  timestep_func _timestepHook;
  received_func _receivedHook;
  fill_for_transmit_func _fillForTransmitHook;
};

#endif

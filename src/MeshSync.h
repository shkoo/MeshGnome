#ifndef MESH_SYNC_H
#define MESH_SYNC_H

#include <functional>

#include "ProtoDispatch.h"

class MeshSync : public ProtoDispatchTarget {
 public:
  // Sets the number of milliseconds between retries.  The actual
  // interval will be a random interval between 1 and 2 times this
  // number to avoid synchronization issues.
  void retryMs(uint32_t ms) { _retryMs = ms; }

  // Sets the number of milliseconds between advertisements.  The
  // actual interval will be a random interval between 1 and 2 times
  // this number to avoid synchronization issues.
  void advertiseMs(uint32_t ms) { _advertiseMs = ms; }

  // Sets the number of milliseconds to initially wait for an upgrade
  // upon startup before setting _upToDate.
  void initialUpgradeMs(uint32_t ms) { _initialUpgradeMs = ms; }

  // Sets maximum number of retries before giving up.
  void maxRetries(uint32_t retries) { _maxRetries = retries; }

  int localVersion() const { return _localVersion.version; }
  size_t localSize() const { return _localVersion.len; }

  // Returns true if the local version is up to date with what is available on the network.
  //
  // If any of the following conditions happen, the local version is not up to date:
  //
  // * An advertisement for a newer version has been seen.
  // * An update is currently in progress
  // * Less than _initialUpgradeMs has passed since startup and we haven't
  //   seen anything else on the network that's not newer than we have.
  //
  // This is useful for e.g. sketch upgrades, where if you're out of
  // date you want to upgrade immediately in case the old version has
  // some kind of crashing bug that interrupts updates.
  bool upToDate() const;

  // For receiving updates:
  // Returns false if update should be aborted.
  virtual bool startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                           size_t metadataLen) = 0;

  // Returns false if update should be aborted.
  virtual bool receiveUpdateChunk(const uint8_t* /* chunk */, size_t /* chunklen */) { abort(); };

  virtual void onUpdateAbort() {}
  virtual void onUpdateComplete() {}

  // For sending updates.  Provides the given chunk.  Does not need to
  // worry about bounds checking.
  virtual bool provideUpdateChunk(size_t /* offset */, uint8_t* /* chunk */,
                                  size_t /* size of chunk */) {
    abort();
  };

  // Provides additional metadata to be included with the advertise message.
  virtual int provideUpdateMetadata(uint8_t* /* metadata */, size_t /* maxlen */) { return 0; }

  using progress_hook_func_t = std::function<void(size_t /* offset */, size_t /* length */)>;
  void setReceiveProgressHook(const progress_hook_func_t& f);
  void setTransmitProgressHook(const progress_hook_func_t& f);

  using updateStopHook_func_t = std::function<void(String /* reason */)>;
  void setUpdateStopHook(const updateStopHook_func_t& f);

 protected:
  MeshSync(int localVersion = -1, size_t localSize = 0);

  void updateVersion(int newLocalVersion, size_t newLocalSize);

  size_t getNewOffset() const { return _updateCurOffset; }
  size_t getNewSize() const { return _updateVersion.len; }

 private:
  void onPacketReceived(const ProtoDispatchPktHdr* srcaddr, const uint8_t* pkt,
                        size_t len) override;
  void _onAdvertise(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _onRequest(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _onProvide(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _checkUpdateComplete();

  int sendIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) override;
  int _sendRequestIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);
  int _sendProvideIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);
  int _sendAdvertiseIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);

  void _updateProgress();
  void _updateUpToDate();
  void _updateStop(String msg);

  void _resetRetryTime();

  enum class Op : uint8_t { ADVERTISE, REQUEST, PROVIDE };

  struct AdvertiseData {
    int version;
    size_t len;
    // Metadata (for instance, checksum) follows this packet.
  };

  struct RequestData {
    int version;
    size_t offset;
  };

  struct ProvideData {
    int version;
    size_t offset;
    // Data follows afterwards.
  };

  progress_hook_func_t _receiveProgressHook;
  progress_hook_func_t _transmitProgressHook;
  updateStopHook_func_t _updateStopHook;

  uint32_t _retryMs = 300;
  uint32_t _advertiseMs = 15000;
  uint32_t _initialUpgradeMs = 2000;
  uint32_t _maxRetries = 100;

  AdvertiseData _localVersion;

  uint32_t _startTime = 0;
  uint32_t _nextAdvertiseTime = 0;

  // For receiving updates
  bool _updateInProgress = false;
  bool _seenNewerVersion = false;
  bool _seenThisOrOlderVersion = false;

  // True if someone else is advertising or upgrading; in this case we
  // won't compete for network.
  bool _seenOther = false;

  AdvertiseData _updateVersion;
  uint8_t _updateEth[ETH_ADDR_LEN];
  size_t _updateCurOffset = 0;
  size_t _nextRetryTime = 0;
  uint8_t _retryCount = 0;

  // For sending updates
  bool _dataRequested = false;  // True if a client wants some of our data.
  size_t _maxRequestedOffset = 0;
  uint32_t _nextProvideTime = 0;
};

#endif

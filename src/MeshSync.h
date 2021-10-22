#ifndef MESH_SYNC_H
#define MESH_SYNC_H

#include "ProtoDispatch.h"

class MeshSync : public ProtoDispatchTarget {
 public:
#if defined(EPOXY_DUINO)
  static constexpr uint32_t RETRY_INTERVAL_MS = 100;
#else
  static constexpr uint32_t RETRY_INTERVAL_MS = 1000;
#endif
  static constexpr uint32_t ADVERTISE_INTERVAL_MS = 15 * 1000;
  static constexpr uint8_t MAX_RETRIES = 5;

  int localVersion() const { return _localVersion.version; }
  size_t localSize() const { return _localVersion.len; }

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
  virtual bool provideUpdateChunk(size_t /* offset */, uint8_t* /* chunk */, size_t /* size of chunk */) {
    abort();
  };

  // Provides additional metadata to be included with the advertise message.
  virtual int provideUpdateMetadata(uint8_t* /* metadata */, size_t /* maxlen */) { return 0; }

 protected:
  MeshSync(int localVersion = -1, size_t localSize = 0);

  void updateVersion(int newLocalVersion, size_t newLocalSize);

  size_t getNewOffset() const { return _updateCurOffset; }
  size_t getNewSize() const { return _updateVersion.len; }

 private:
  void onPacketReceived(const ProtoDispatchPktHdr* srcaddr, const uint8_t* pkt, size_t len) override;
  void _onAdvertise(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _onRequest(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _onProvide(const uint8_t* srcaddr, const uint8_t* pkt, size_t len);
  void _checkUpdateComplete();

  int sendIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) override;
  int _sendRequestIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);
  int _sendProvideIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);
  int _sendAdvertiseIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen);

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

  AdvertiseData _localVersion;

  uint32_t _nextAdvertiseTime = 0;

  // For receiving updates
  bool _updateInProgress = false;
  AdvertiseData _updateVersion;
  uint8_t _updateEth[ETH_ADDR_LEN];
  size_t _updateCurOffset = 0;
  size_t _nextRetryTime = 0;

  uint8_t _retryCount = 0;

  // For sending updates
  bool _dataRequested = false;  // True if a client wants some of our data.
  size_t _maxRequestedOffset = 0;
};

#endif

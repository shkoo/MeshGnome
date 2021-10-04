#ifndef MESH_SYNC_METADATA_H
#define MESH_SYNC_METADATA_H

#include "MeshSync.h"

// In-memory sync.  The synchronized data must be small enough to be
// kept in memory.  The synchronized metadata must be small enought to
// be sent in an individual packet, including the MeshGnome protocol
// overhead.
class MeshSyncMem : public MeshSync {
 public:
  ~MeshSyncMem();
  // Update the synchronized data and metadata locally.  If the
  // version number is higher than neighboring nodes, this updated
  // data will be propagated.
  void update(int version, const uint8_t* metadata, size_t metadataLen,
              const uint8_t* data = nullptr, size_t dataLen = 0);
  void update(int version, String metadata, String data = String());

  // Returns data and metadata buffers as Arduino Strings.  These
  // should not be used for binary data, as the Arduino String does
  // not deal well with null characters.
  String localData() const;
  String localMetadata() const;

  // Returns raw buffers.  These may be null if the sizes are 0.
  const uint8_t* localDataBuffer() const { return _data; }
  size_t localDataBufferLen() const { return _dataLen; }

  const uint8_t* localMetadataBuffer() const { return _metadata; }
  size_t localMetadataBufferLen() const { return _metadataLen; }

 private:
  bool startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                   size_t metadataLen) override;
  bool receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) override;
  void onUpdateAbort() override;
  void onUpdateComplete() override;
  int provideUpdateMetadata(uint8_t* metadata, size_t maxlen) override;
  bool provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) override;

  static void _copyBuf(const uint8_t* src, size_t srclen, uint8_t** dst, size_t* dstlen);
  static String _bufToString(const uint8_t* buf, size_t buflen);
  void _freeNew();

  uint8_t* _data = nullptr;
  size_t _dataLen = 0;

  uint8_t* _metadata = nullptr;
  size_t _metadataLen = 0;

  uint8_t* _newMetadata = nullptr;
  size_t _newMetadataLen = 0;

  uint8_t* _newData = nullptr;
  size_t _newDataLen = 0;
  size_t _newDataOffset = 0;

  int _newVersion;
};

#endif

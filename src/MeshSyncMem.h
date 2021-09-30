#ifndef MESH_SYNC_METADATA_H
#define MESH_SYNC_METADATA_H

#include "MeshSync.h"

// In-memory sync.  The synchronized data must be small enough to be
// kept in memory.  The synchronized metadata must be small enought to
// be sent in an individual packet, including the MeshGnome protocol
// overhead.
class MeshSyncMem : public MeshSync {
 public:
  // Update the synchronized data and metadata locally.  If the
  // version number is higher than neighboring nodes, this updated
  // data will be propagated.
  void update(int version, String metadata, String data = String()) {
    _metadata = std::move(metadata);
    _data = std::move(data);
    updateVersion(version, _data.length());
  }

  String localData() const { return _data; };
  String localMetadata() const { return _metadata; };

 private:
  bool startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                   size_t metadataLen) override;
  bool receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) override;
  void onUpdateAbort() override;
  void onUpdateComplete() override;
  int provideUpdateMetadata(uint8_t* metadata, size_t maxlen) override;
  bool provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) override;

  String _metadata;
  String _data;

  String _newMetadata;
  String _newData;
  int _newVersion;
};

#endif

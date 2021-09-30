#ifndef MESH_SYNC_METADATA_H
#define MESH_SYNC_METADATA_H

#include "MeshSync.h"

// In-memory sync.  Must be small enough to be kept in memory.
class MeshSyncMem : public MeshSync {
 public:
  void update(int version, String metadata, String data = String()) {
    _metadata = std::move(metadata);
    _data = std::move(data);
    updateVersion(version, _data.length());
  }

  String localData() { return _data; };
  String localMetadata() { return _metadata; };

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

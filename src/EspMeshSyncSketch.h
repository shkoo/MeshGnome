#ifndef ESP_MESH_SYNC_SKETCH_H
#define ESP_MESH_SYNC_SKETCH_H

#if defined(ESP8266)

#include "MeshSync.h"

// Keeps the code on the sketch up to date.
class MeshSyncSketch : public MeshSync {
 public:
  MeshSyncSketch(int currentVersion);

  bool startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                   size_t metadataLen) override;
  bool receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) override;
  void onUpdateAbort() override;
  void onUpdateComplete() override;

  bool provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) override;
  int provideUpdateMetadata(uint8_t* metadata, size_t maxlen) override;

 private:
  String _localSketchMD5;
};

#endif
#endif

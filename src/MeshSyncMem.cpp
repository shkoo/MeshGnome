#include "MeshSyncMem.h"

bool MeshSyncMem::startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                              size_t metadataLen) {
  _newMetadata = String();
  _newData = String();
  while (metadataLen) {
    _newMetadata += char(*metadata);
    ++metadata;
    --metadataLen;
  }
  _newData.reserve(updateLen);
  _newVersion = newVersion;

  return true;
}

bool MeshSyncMem::receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) {
  while (chunklen) {
    _newData += char(*chunk);
    ++chunk;
    --chunklen;
  }
  return true;
}

void MeshSyncMem::onUpdateAbort() {
  _newMetadata = String();
  _newData = String();
}

void MeshSyncMem::onUpdateComplete() {
  _metadata = std::move(_newMetadata);
  _data = std::move(_newData);
  updateVersion(_newVersion, _data.length());

  _newMetadata = String();
  _newData = String();
}

int MeshSyncMem::provideUpdateMetadata(uint8_t* metadata, size_t maxlen) {
  assert(_metadata.length() <= maxlen);
  memcpy(metadata, _metadata.begin(), _metadata.length());
  return _metadata.length();
}

bool MeshSyncMem::provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) {
  memcpy(chunk, _data.begin() + offset, size);
  return true;
}

#include "MeshSyncMem.h"

bool MeshSyncMem::startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                              size_t metadataLen) {
  assert(!_newData);
  assert(!_newMetadata);
  _copyBuf(metadata, metadataLen, &_newMetadata, &_newMetadataLen);
  _newData = (uint8_t*)malloc(updateLen);
  _newDataLen = updateLen;
  _newDataOffset = 0;
  _newVersion = newVersion;
  return true;
}

bool MeshSyncMem::receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) {
  memcpy(_newData + _newDataOffset, chunk, chunklen);
  _newDataOffset += chunklen;
  return true;
}

void MeshSyncMem::onUpdateAbort() { _freeNew(); }

void MeshSyncMem::_freeNew() {
  if (_newMetadata) {
    free(_newMetadata);
    _newMetadata = nullptr;
  }
  if (_newData) {
    free(_newData);
    _newData = nullptr;
  }
}

void MeshSyncMem::onUpdateComplete() {
  assert(_newDataOffset == _newDataLen);
  std::swap(_data, _newData);
  std::swap(_dataLen, _newDataLen);
  std::swap(_metadata, _newMetadata);
  std::swap(_metadataLen, _newMetadataLen);
  updateVersion(_newVersion, _dataLen);

  _freeNew();
}

int MeshSyncMem::provideUpdateMetadata(uint8_t* metadata, size_t maxlen) {
  assert(_metadataLen <= maxlen);
  memcpy(metadata, _metadata, _metadataLen);
  return _metadataLen;
}

bool MeshSyncMem::provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) {
  memcpy(chunk, _data + offset, size);
  return true;
}

void MeshSyncMem::update(int version, String metadata, String data) {
  update(version, (const uint8_t*)metadata.begin(), metadata.length(), (const uint8_t*)data.begin(),
         data.length());
}

void MeshSyncMem::update(int version, const uint8_t* metadata, size_t metadataLen,
                         const uint8_t* data, size_t dataLen) {
  _copyBuf(metadata, metadataLen, &_metadata, &_metadataLen);
  _copyBuf(data, dataLen, &_data, &_dataLen);
  updateVersion(version, _dataLen);
}

void MeshSyncMem::_copyBuf(const uint8_t* src, size_t srclen, uint8_t** dst, size_t* dstlen) {
  if (*dst) {
    free(*dst);
  }
  *dst = (uint8_t*)malloc(srclen);
  memcpy(*dst, src, srclen);
  *dstlen = srclen;
}

MeshSyncMem::~MeshSyncMem() {
  if (_metadata) free(_metadata);
  if (_data) free(_data);

  _freeNew();
}

String MeshSyncMem::localData() const { return _bufToString(_data, _dataLen); }

String MeshSyncMem::localMetadata() const { return _bufToString(_metadata, _metadataLen); }

String MeshSyncMem::_bufToString(const uint8_t* buf, size_t buflen) {
  String result;
  result.reserve(buflen);
  while (buflen) {
    result += char(*buf);
    ++buf;
    --buflen;
  }
  return result;
}

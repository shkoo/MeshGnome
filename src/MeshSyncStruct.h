#ifndef MESH_SYNC_STRUCT_H
#define MESH_SYNC_STRUCT_H

#include "MeshSync.h"

// In-memory synhronization of a structure, which must be small enough to fit in a single packet.
template <typename T>
class MeshSyncStruct : public MeshSync {
 public:
  template <typename... Arg>
  MeshSyncStruct(Arg&&... args) : _val(std::forward<Arg>(args)...) {}
  ~MeshSyncStruct() override = default;

  const T& operator*() const { return _val; }
  T& operator*() { return _val; }
  const T* operator->() const { return &_val; }
  T* operator->() { return &_val; }

  void push() { updateVersion(localVersion() + 1, 0); }

 protected:
  bool startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                   size_t metadataLen) override {
    if (metadataLen != sizeof(T) || updateLen != 0) {
      return false;
    }
    memcpy(&_val, metadata, sizeof(T));
    updateVersion(newVersion, 0 /* no body */);
    return true;
  }
  bool receiveUpdateChunk(const uint8_t* /* chunk */, size_t /* chunklen */) override {
    return false;
  }
  void onUpdateAbort() override {}
  void onUpdateComplete() override {}
  int provideUpdateMetadata(uint8_t* metadata, size_t maxlen) {
    assert(maxlen >= sizeof(T));
    memcpy(metadata, &_val, sizeof(T));
    return sizeof(T);
  }
  bool provideUpdateChunk(size_t /* offset */, uint8_t* /* chunk */, size_t /* size */) override {
    // Should not be called, since we have 0 body size.
    abort();
  }

 private:
  T _val;
};

#endif

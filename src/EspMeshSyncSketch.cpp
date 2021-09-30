#if defined(ESP8266)

#include "EspMeshSyncSketch.h"

MeshSyncSketch::MeshSyncSketch(int version) : MeshSync(version, ESP.getSketchSize()) {
  _localSketchMD5 = ESP.getSketchMD5();
}

bool MeshSyncSketch::startUpdate(size_t updateLen, int newVersion, const uint8_t* metadata,
                                 size_t metadataLen) {
  Update.begin(updateLen);
  Update.runAsync(true);
  String expectedMD5;
  while (metadataLen) {
    expectedMD5.concat(char(*metadata));
    ++metadata;
    --metadataLen;
  }
  Update.setMD5(expectedMD5.c_str());
  Serial.printf("Starting firmware update to version %d from %d\n", newVersion, localVersion());
  return true;
}

bool MeshSyncSketch::receiveUpdateChunk(const uint8_t* chunk, size_t chunklen) {
  static uint8_t printCounter = 0;
  if (printCounter & 0xF) {
    Serial.print(".");
  } else {
    Serial.printf("%lu/%lu\n", getNewOffset(), getNewSize());
  }
  ++printCounter;

  // Ugh, why does Update need to write to this?
  uint8_t* writableChunk = const_cast<uint8_t*>(chunk);

  int writelen = Update.write(writableChunk, chunklen);
  if (writelen != chunklen) {
    Serial.printf("MeshSyncSketch only able to save %d of %d requested\n", writelen, chunklen);
    return false;
  }

  return true;
}

void MeshSyncSketch::onUpdateAbort() {
  Serial.printf("Aborting firmware update\n");
  Update.end();
}

void MeshSyncSketch::onUpdateComplete() {
  if (!Update.end()) {
    Serial.print("MeshSyncSketch: Update failed: ");
    Update.printError(Serial);
  } else {
    Serial.println("Firmware update complete! Restarting.");
    ESP.reset();
  }
}

bool MeshSyncSketch::provideUpdateChunk(size_t offset, uint8_t* chunk, size_t size) {
  return ESP.flashRead(offset, chunk, size);
}

int MeshSyncSketch::provideUpdateMetadata(uint8_t* metadata, size_t maxlen) {
  if (_localSketchMD5.length() > maxlen) {
    Serial.println("MeshSyncSketch: md5 too long to fit in packet\n");
    return -1;
  }

  memcpy(metadata, _localSketchMD5.begin(), _localSketchMD5.length());
  return _localSketchMD5.length();
}

#endif

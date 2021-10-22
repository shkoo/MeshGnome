#include "MeshSync.h"

#include <stdio.h>

MeshSync::MeshSync(int localVersion, size_t localSize) {
  _localVersion.version = localVersion;
  _localVersion.len = localSize;
}

void MeshSync::onPacketReceived(const ProtoDispatchPktHdr* hdr, const uint8_t* pkt, size_t len) {
  if (len < 1) {
    return;
  }
  Op op = (Op)pkt[0];
  switch (op) {
    case Op::ADVERTISE:
      _onAdvertise(hdr->src, pkt + 1, len - 1);
      break;
    case Op::REQUEST:
      _onRequest(hdr->src, pkt + 1, len - 1);
      break;
    case Op::PROVIDE:
      _onProvide(hdr->src, pkt + 1, len - 1);
      break;
    default:
      Serial.printf("Unknown mesh sync packet type %d recceived with length %d\n", int(op), len);
      break;
  }
}

void MeshSync::_onAdvertise(const uint8_t* srcaddr, const uint8_t* pkt, size_t len) {
  if (_updateInProgress) {
    // IDEA(nils): If we're in the middle of updating and we lose
    // contact, should we start downloading from a different source if
    // available?
    return;
  }

  if (len < sizeof(AdvertiseData)) {
    return;
  }

  memcpy(&_updateVersion, pkt, sizeof(AdvertiseData));

  if (_updateVersion.version <= _localVersion.version) {
    return;
  }

  if (startUpdate(_updateVersion.len, _updateVersion.version, pkt + sizeof(AdvertiseData),
                  len - sizeof(AdvertiseData))) {
    _updateInProgress = true;
    memcpy(_updateEth, srcaddr, ETH_ADDR_LEN);
    _updateCurOffset = 0;

    _nextRetryTime = millis();
    _checkUpdateComplete();

    // Abort any update sending, if we don't have the newest version.
    _dataRequested = false;
  }
}

void MeshSync::_onRequest(const uint8_t* /* srcaddr */, const uint8_t* pkt, size_t len) {
  if (_updateInProgress) {
    // Don't serve anything while we're updating ourselves.
    return;
  }

  if (len < sizeof(RequestData)) {
    return;
  }

  RequestData req;
  memcpy(&req, pkt, sizeof(RequestData));
  if (req.version != _localVersion.version) {
    return;
  }

  if (_dataRequested) {
    if (req.offset > _maxRequestedOffset) {
      _maxRequestedOffset = req.offset;
    }
  } else {
    _dataRequested = true;
    _maxRequestedOffset = req.offset;
  }
}

void MeshSync::_onProvide(const uint8_t* /* srcaddr */, const uint8_t* pkt, size_t len) {
  if (!_updateInProgress) {
    return;
  }

  if (len <= sizeof(ProvideData)) {
    return;
  }

  ProvideData prov;
  memcpy(&prov, pkt, sizeof(ProvideData));
  if (prov.version != _updateVersion.version) {
    return;
  }
  if (prov.offset != _updateCurOffset) {
    return;
  }

  size_t chunkLen = len - sizeof(ProvideData);

  bool res = receiveUpdateChunk(pkt + sizeof(ProvideData), chunkLen);
  if (!res) {
    Serial.println("Receiving chunk failed");
    onUpdateAbort();
    _updateInProgress = false;
    return;
  }
#if VERBOSE
   Serial.printf(" %u+%d/u", _updateCurOffset, chunkLen, _updateVersion.len);
#endif
  _updateCurOffset += chunkLen;
  _retryCount = 0;
  _nextRetryTime = millis();
  _checkUpdateComplete();
}

void MeshSync::_checkUpdateComplete() {
  assert(_updateCurOffset <= _updateVersion.len);
  assert(_updateInProgress);
  if (_updateCurOffset == _updateVersion.len) {
    Serial.println("\nUpdate complete!");
    onUpdateComplete();
    _updateInProgress = false;
  }
}

int MeshSync::sendIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (_updateInProgress) {
    return _sendRequestIfNeeded(dst, pkt, maxlen);
  }

  int res = _sendProvideIfNeeded(dst, pkt, maxlen);
  if (res > 0) {
    return res;
  }

  res = _sendAdvertiseIfNeeded(dst, pkt, maxlen);
  if (res > 0) {
    return res;
  }

  return -1;
}

int MeshSync::_sendRequestIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (timeIsAfter(millis(), _nextRetryTime)) {
    ++_retryCount;
    if (_retryCount > MAX_RETRIES) {
      onUpdateAbort();
      _updateInProgress = false;
      return -1;
    }
    _nextRetryTime = millis() + RETRY_INTERVAL_MS;

    assert(maxlen >= 1 + sizeof(RequestData));

    memcpy(dst, _updateEth, ETH_ADDR_LEN);

    RequestData req;
    req.version = _updateVersion.version;
    req.offset = _updateCurOffset;
    pkt[0] = int(Op::REQUEST);
    memcpy(pkt + 1, &req, sizeof(RequestData));

    return 1 + sizeof(RequestData);
  }

  return -1;
}

int MeshSync::_sendProvideIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (!_dataRequested) {
    return -1;
  }

  _dataRequested = false;

  if (_updateInProgress) {
    return -1;
  }

  assert(maxlen >= 1 + sizeof(ProvideData));
  pkt[0] = int(Op::PROVIDE);

  memset(dst, 0xff, 6);  // broadcast update to everyone

  ProvideData provide;
  provide.version = _localVersion.version;
  provide.offset = _maxRequestedOffset;
  memcpy(pkt + 1, &provide, sizeof(ProvideData));

  size_t chunkSize = maxlen - 1 - sizeof(ProvideData);
  if (provide.offset + chunkSize > _localVersion.len) {
    assert(provide.offset < _localVersion.len);
    chunkSize = _localVersion.len - provide.offset;
  }

  _dataRequested = false;
  _maxRequestedOffset = 0;

  bool res = provideUpdateChunk(provide.offset, pkt + 1 + sizeof(ProvideData), chunkSize);
  if (!res) {
    Serial.printf("Unable to gather update chunk at %d\n", _maxRequestedOffset);
    return -1;
  }

  return 1 + sizeof(ProvideData) + chunkSize;
}

int MeshSync::_sendAdvertiseIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (timeIsAfter(millis(), _nextAdvertiseTime)) {
    _nextAdvertiseTime = millis() + ADVERTISE_INTERVAL_MS;
    pkt[0] = int(Op::ADVERTISE);
    memset(dst, 0xff, 6);  // broadcast to everyone!
    assert(maxlen >= 1 + sizeof(AdvertiseData));
    memcpy(pkt + 1, &_localVersion, sizeof(AdvertiseData));

    size_t maxMetadata = maxlen - 1 - sizeof(AdvertiseData);
    int metalen = provideUpdateMetadata(pkt + 1 + sizeof(AdvertiseData), maxMetadata);
    if (metalen < 0) {
      return -1;
    }
    return 1 + sizeof(AdvertiseData) + metalen;
  }

  return -1;
}

void MeshSync::updateVersion(int newLocalVersion, size_t newLocalSize) {
  if (_updateInProgress) {
    onUpdateAbort();
    _updateInProgress = false;
  }

  // Don't serve old data
  _dataRequested = false;

  _localVersion.version = newLocalVersion;
  _localVersion.len = newLocalSize;

  // Advertise right away that we have a new version.o
  _nextAdvertiseTime = millis();

  Serial.printf("Updated to version %u, size=%u\n", newLocalVersion, newLocalSize);
}

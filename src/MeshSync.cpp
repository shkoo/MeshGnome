#include "MeshSync.h"

#include <stdio.h>

// If true, update stragglers first.
static constexpr bool k_lower_first = true;

MeshSync::MeshSync(int localVersion, size_t localSize) {
  _localVersion.version = localVersion;
  _localVersion.len = localSize;
  _nextProvideTime = millis() + random(0, _initialUpgradeMs / 2);
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

void MeshSync::_updateStop(String msg) {
  onUpdateAbort();
  if (_updateStopHook) {
    _updateStopHook(msg);
  } else {
    Serial.println(msg);
  }
  _updateInProgress = false;
}

void MeshSync::_updateProgress() {
  _updateInProgress = true;
  if (_receiveProgressHook) {
    _receiveProgressHook(_updateCurOffset, _updateVersion.len);
  }
}

bool MeshSync::upToDate() const {
  if (_updateInProgress) {
    return false;
  }

  if (_seenNewerVersion) {
    return false;
  }

  if (_seenThisOrOlderVersion) {
    // Something else on the network hasn't been upgraded, so maybe we won't need to upgrade.
    return true;
  }

  if (!_startTime || (millis() - _startTime) < _initialUpgradeMs) {
    // Wait _initialUpgradeMs to try and make sure we have a recent version.
    return false;
  }
  return true;
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
    _seenThisOrOlderVersion = true;
    if (_updateVersion.version < _localVersion.version &&
        (_nextAdvertiseTime - millis()) > (_initialUpgradeMs / 2)) {
      // Something just appeared with an old version; make sure they're aware right away that
      // there's a new one.
      _nextAdvertiseTime = millis() + random(0, _initialUpgradeMs / 2);
    }
    return;
  }

  _seenNewerVersion = true;

  if (startUpdate(_updateVersion.len, _updateVersion.version, pkt + sizeof(AdvertiseData),
                  len - sizeof(AdvertiseData))) {
    _updateProgress();
    memcpy(_updateEth, srcaddr, ETH_ADDR_LEN);
    _updateCurOffset = 0;

    _nextRetryTime = millis();
    _checkUpdateComplete();

    // Abort any update sending, if we don't have the newest version.
    _dataRequested = false;
  }
}

void MeshSync::_onRequest(const uint8_t* /* srcaddr */, const uint8_t* pkt, size_t len) {
  if (len < sizeof(RequestData)) {
    return;
  }

  RequestData req;
  memcpy(&req, pkt, sizeof(RequestData));

  if (_updateInProgress) {
    if (req.version != _updateVersion.version) {
      return;
    }
    // Don't serve anything while we're updating ourselves.  However,
    // if someone else is requesting things, let them go first if they're farther along.
    if (k_lower_first ? (req.offset <= _updateCurOffset) : (req.offset >= _updateCurOffset)) {
      _resetRetryTime();
      _seenOther = true;
    }
    return;
  }

  if (req.version != _localVersion.version) {
    return;
  }

  if (_dataRequested) {
    if (k_lower_first ? (req.offset < _maxRequestedOffset) : (req.offset > _maxRequestedOffset)) {
      _maxRequestedOffset = req.offset;
    }
  } else {
    _dataRequested = true;
    _maxRequestedOffset = req.offset;
  }
}

void MeshSync::_onProvide(const uint8_t* /* srcaddr */, const uint8_t* pkt, size_t len) {
  if (!_updateInProgress) {
    // Someone else is providing; let them do it.
    _nextProvideTime = millis() + random(_retryMs * 2, _retryMs * 4);
    _dataRequested = false;
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
    if (k_lower_first ? (prov.offset < _updateCurOffset) : (prov.offset > _updateCurOffset)) {
      _resetRetryTime();
      _seenOther = true;
    }
    return;
  }

  size_t chunkLen = len - sizeof(ProvideData);

  bool res = receiveUpdateChunk(pkt + sizeof(ProvideData), chunkLen);
  if (!res) {
    _updateStop("Receiving chunk failed");
    return;
  }
#if VERBOSE
  Serial.printf(" %u+%d/u", _updateCurOffset, chunkLen, _updateVersion.len);
#endif
  _updateCurOffset += chunkLen;
  _retryCount = 0;
  _updateProgress();

  if (_seenOther) {
    _resetRetryTime();
  } else {
    _nextRetryTime = millis();
  }
  _checkUpdateComplete();
}

void MeshSync::_checkUpdateComplete() {
  assert(_updateCurOffset <= _updateVersion.len);
  assert(_updateInProgress);
  if (_updateCurOffset == _updateVersion.len) {
    Serial.println("\nUpdate complete!");
    if (_updateStopHook) {
      _updateStopHook("Update complete");
    }
    onUpdateComplete();
    _updateInProgress = false;
    _seenNewerVersion = false;
    _seenThisOrOlderVersion = true;
  }
}

int MeshSync::sendIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (!_startTime) {
    _startTime = millis();
  }
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

void MeshSync::_resetRetryTime() { _nextRetryTime = millis() + random(_retryMs, _retryMs * 2); }

int MeshSync::_sendRequestIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (timeIsAfter(millis(), _nextRetryTime)) {
    ++_retryCount;
    if (_retryCount > _maxRetries) {
      _updateStop("Retries exceeded");
      return -1;
    }
    _resetRetryTime();
    _seenOther = false;

    assert(maxlen >= 1 + sizeof(RequestData));

    // memcpy(dst, _updateEth, ETH_ADDR_LEN);
    memset(dst, 0xff, ETH_ADDR_LEN);

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

  if (_updateInProgress) {
    _dataRequested = false;
    return -1;
  }

  if (!timeIsAfter(millis(), _nextProvideTime)) {
    return -1;
  }
  _nextProvideTime = millis();

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

  bool res = provideUpdateChunk(provide.offset, pkt + 1 + sizeof(ProvideData), chunkSize);
  if (!res) {
    Serial.printf("Unable to gather update chunk at %d\n", _maxRequestedOffset);
    return -1;
  }

  if (_transmitProgressHook) {
    _transmitProgressHook(provide.offset, _localVersion.len);
  }

  return 1 + sizeof(ProvideData) + chunkSize;
}

int MeshSync::_sendAdvertiseIfNeeded(uint8_t* dst, uint8_t* pkt, size_t maxlen) {
  if (timeIsAfter(millis(), _nextAdvertiseTime)) {
    _nextAdvertiseTime = millis() + random(_advertiseMs, 2 * _advertiseMs);
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
    _updateStop("Different newer version encountered");
  }

  // Don't serve old data
  _dataRequested = false;

  _localVersion.version = newLocalVersion;
  _localVersion.len = newLocalSize;

  // Advertise right away that we have a new version.o
  _nextAdvertiseTime = millis();

  Serial.printf("Updated to version %u, size=%u\n", newLocalVersion, newLocalSize);
}

void MeshSync::setReceiveProgressHook(const progress_hook_func_t& f) { _receiveProgressHook = f; }

void MeshSync::setTransmitProgressHook(const progress_hook_func_t& f) { _transmitProgressHook = f; }

void MeshSync::setUpdateStopHook(const updateStopHook_func_t& f) { _updateStopHook = f; }

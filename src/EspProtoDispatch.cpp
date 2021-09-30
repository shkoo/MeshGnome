
#include "EspProtoDispatch.h"

#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <espnow.h>
#define WIFI_CHAN 1

EspProtoDispatchClass EspProtoDispatch;

void EspProtoDispatchClass::_esp_now_recv_cb(u8* src, u8* data, u8 len) {
  EspProtoDispatch.receivePacket(src, data, len);
}

void EspProtoDispatchClass::_esp_now_send_cb(u8* dst, u8 status) {
  assert(EspProtoDispatch._sendInProgress);
  EspProtoDispatch._sendInProgress = false;
}

void EspProtoDispatchClass::protoDispatchBegin() {
  wifi_set_channel(1);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  uint8_t broadcastAddress[6] = {255, 255, 255, 255, 255, 255};
  int res = esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, WIFI_CHAN, NULL, 0);
  if (res != 0) {
    Serial.printf("esp_now_add_peer failed: %d\n", res);
  }

  res = esp_now_register_recv_cb(_esp_now_recv_cb);
  if (res < 0) {
    Serial.printf("Registering receive callback failed: %d\n", res);
    abort();
  }
  res = esp_now_register_send_cb(_esp_now_send_cb);
  if (res < 0) {
    Serial.printf("Registering send callback failed: %d\n", res);
    abort();
  }
}

void EspProtoDispatchClass::espTransmitIfNeeded() {
  if (_sendInProgress) {
    return;
  }

  uint8_t dst[ETH_ADDR_LEN];
  uint8_t xmitBuf[MAX_PKT_LEN];
  int pktLen = transmitIfNeeded(dst, xmitBuf, MAX_PKT_LEN);
  if (pktLen < 0) {
    return;
  }

  _sendInProgress = true;
  if (memcmp(dst, _lastPeer, ETH_ADDR_LEN) && !etherIsBroadcast(dst)) {
    memcpy(_lastPeer, dst, ETH_ADDR_LEN);
    int res = esp_now_add_peer(dst, ESP_NOW_ROLE_COMBO, WIFI_CHAN, NULL, 0);
    if (res != 0) {
      Serial.printf("esp_now_add_peer failed: %d\n", res);
    }
  }
  int res = esp_now_send(dst, xmitBuf, pktLen);
  if (res == 0) {
    _sendInProgress = true;
  } else {
    Serial.printf("esp_now_send failed: %d\n", res);
  }
}

#endif

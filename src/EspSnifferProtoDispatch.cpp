
#include "EspSnifferProtoDispatch.h"

#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <espnow.h>

#include "ieee80211_structs.h"
#define WIFI_CHAN 1

EspSnifferProtoDispatchClass EspSnifferProtoDispatch;

struct sniffer_buf2 {
  wifi_pkt_rx_ctrl_t rx_ctrl;
  u8 buf[112];  // may be 240, please refer to the real source code
  u16 cnt;
  u16 len;  // length of packet
};

void EspSnifferProtoDispatchClass::_esp_sniffer_recv_cb(uint8_t *buf, uint16_t len) {
  if (len != sizeof(sniffer_buf2)) {
    return;
  }

  const sniffer_buf2 *ppkt = reinterpret_cast<const sniffer_buf2 *>(buf);

  // Second layer: define pointer to where the actual 802.11 packet is within the structure
  const wifi_ieee80211_packet_t *ipkt =
      reinterpret_cast<const wifi_ieee80211_packet_t *>(ppkt->buf);

  // Third layer: define pointers to the 802.11 packet header, payload, frame control
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
  const uint8_t *data = ipkt->payload;
  const wifi_header_frame_control_t *frame_ctrl = (wifi_header_frame_control_t *)&hdr->frame_ctrl;

  static const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  static const uint8_t EspressifOUI[] = {0x18, 0xfe, 0x34};  // 24, 254, 52
  static const uint8_t EspressifType = 4;
  static const uint8_t MAC_SIZE = 6;

  // Only continue processing if this is an action frame containing the Espressif OUI.
  if ((wifi_mgmt_subtypes_t)frame_ctrl->subtype != ACTION ||  // action subtype match
      memcmp(data + 2, EspressifOUI, 3) != 0 ||               // OUI
      *(data + 2 + 3) != EspressifType                        //||                     // Type
      /*      (memcmp(hdr->addr1, broadcastAddress, MAC_SIZE) != 0) */) {
    return;
  }

  const wifi_pkt_mgmt_t *mpkt = (wifi_pkt_mgmt_t *)&ipkt->payload;
  u8 *src = (u8 *)&hdr->addr1;
  u8 *espdata = (u8 *)(data + 7);
  u8 plen = *(data + 1) - 5;  // Length: The length is the total length of Organization Identifier,
                              // Type, Version and Body.

  if (memcmp(src, EspSnifferProtoDispatch._localAddr, 6) == 0) {
    // Don't process packets we transmitted ourself.
    return;
  }

  static ProtoDispatchPktHdr protohdr;
  memcpy(&protohdr.src, src, 6);
  protohdr.rssi = ppkt->rx_ctrl.rssi;
  EspSnifferProtoDispatch.receivePacket(&protohdr, espdata, plen);
  if (EspSnifferProtoDispatch._rssi_hook) {
    EspSnifferProtoDispatch._rssi_hook(protohdr.src, protohdr.rssi);
  }
}

void EspSnifferProtoDispatchClass::_esp_now_send_cb(u8 *dst, u8 status) {
  assert(EspSnifferProtoDispatch._sendInProgress);
  EspSnifferProtoDispatch._sendInProgress = false;
}

void EspSnifferProtoDispatchClass::protoDispatchBegin() {
  wifi_set_channel(1);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  if (!wifi_get_macaddr(0, _localAddr)) {
    Serial.println("Error getting local address");
  }

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

  wifi_set_promiscuous_rx_cb(_esp_sniffer_recv_cb);
  res = esp_now_register_send_cb(_esp_now_send_cb);
  if (res < 0) {
    Serial.printf("Registering send callback failed: %d\n", res);
    abort();
  }
  wifi_promiscuous_enable(1);
}

void EspSnifferProtoDispatchClass::espTransmitIfNeeded() {
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

void EspSnifferProtoDispatchClass::setRSSIHook(const rssi_hook_func_t &f) { _rssi_hook = f; }

#endif

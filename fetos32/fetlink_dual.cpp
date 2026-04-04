#include "fetlink_dual.h"
#include "fetlink.h"
#include "device_net.h"
#include "system.h"
#include <WiFi.h>
#include <esp_now.h>
#include <NimBLEDevice.h>
#include <LittleFS.h>
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"
#include <esp_wifi.h>

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

bool s_connected = false;
static NimBLEServer *s_server = nullptr;
static NimBLECharacteristic *s_tx_char = nullptr;

static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool fetlink_transport_write(const uint8_t *data, uint8_t len)
{
  bool success = false;
  esp_err_t result = esp_now_send(broadcast_mac, data, len);
  if (result == ESP_OK)
    success = true;

  if (s_connected && s_tx_char)
  {
    s_tx_char->setValue((uint8_t *)data, len);
    s_tx_char->notify();
    success = true;
  }
  return success;
}

void on_esp_now_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)
{
  for (int i = 0; i < data_len; i++)
  {
    fetlink_feed(data[i]);
  }
}

class FetLinkServerCallbacks : public NimBLEServerCallbacks
{

  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
  {
    s_connected = true;
    Serial.println("[FetLink BLE] FetHub Conectado");
    system_request("audio:beep", nullptr);
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
  {
    s_connected = false;
    Serial.println("[FetLink BLE] FetHub Desconectado");
    NimBLEDevice::startAdvertising();
  }
};

class FetLinkRxCallbacks : public NimBLECharacteristicCallbacks
{

  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
  {
    std::string value = characteristic->getValue();
    if (value.length() > 0)
    {
      for (size_t i = 0; i < value.length(); i++)
      {
        fetlink_feed(value[i]);
      }
    }
  }
};

static void on_fetlink_receive(uint8_t src, uint8_t type, const uint8_t *data, uint16_t len)
{
  if (type == FLINK_PUB)
  {
    uint8_t topic_len = data[0];
    char topic[33];
    memcpy(topic, &data[1], topic_len);
    topic[topic_len] = '\0';

    uint8_t data_type = data[1 + topic_len];
    const uint8_t *payload_data = &data[2 + topic_len];
    uint16_t plen = len - 2 - topic_len;

    int32_t value = 0;
    if (data_type == 0x02 && plen >= 2)
    {
      value = (int32_t)(int16_t)((payload_data[0] << 8) | payload_data[1]);
    }

    Serial.printf("[FetLink DUAL] Recebeu PUB -> topic: '%s' value: %d\n", topic, value);
    net_device_publish(topic, value);
  }
}

void fetlink_ble_init()
{
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  uint8_t node_id = mac[5];

  fetlink_init(node_id, on_fetlink_receive);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK)
    return;
  esp_now_register_recv_cb(on_esp_now_recv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcast_mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  NimBLEDevice::init("FetOS32");
  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(new FetLinkServerCallbacks());

  NimBLEService *service = s_server->createService(NUS_SERVICE_UUID);

  s_tx_char = service->createCharacteristic(
      NUS_CHARACTERISTIC_TX,
      NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic *rx_char = service->createCharacteristic(
      NUS_CHARACTERISTIC_RX,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx_char->setCallbacks(new FetLinkRxCallbacks());

  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(NUS_SERVICE_UUID);

  NimBLEDevice::startAdvertising();

  Serial.printf("[FetLink DUAL] No ar — node_id: 0x%02X\n", node_id);
}
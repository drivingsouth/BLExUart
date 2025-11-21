#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <BLEServer.h>
#include <BLE2902.h>

#include <Adafruit_NeoPixel.h>
#include <esp_mac.h>

#define BUFFER 64

/*
Serial  : Connector "COM" on ESP32-S3 board, CDC on ESP32-C3
Serial1 : U1TxD(GPIO16),U1RxD(GPIO15) on ESP32-S3 board
          U0TxD(GPIO21),U0RxD(GPIO20) on ESP32-C3 Super mini board
*/

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue0[BUFFER] = "";
uint8_t txValue1[BUFFER] = "";

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#if CONFIG_IDF_TARGET_ESP32S3
#define BLE_DEVICE_NAME "BLE<->UART S3"
#define ENABLE_PIXEL_LED true
#elif CONFIG_IDF_TARGET_ESP32C3
#define BLE_DEVICE_NAME "BLE<->UART C3"
#define ENABLE_PIXEL_LED false
#else
#define BLE_DEVICE_NAME "BLE<->UART"
#define ENABLE_PIXEL_LED false
#endif

// Button
#define BUTTON 0  //タクトスイッチ Boot
#define BUTTON_OFF 0
#define BUTTON_ON 1
uint8_t key_code;
int old_button_status = 1;
uint32_t button_detect_count;

// NeoPixel
#define DIN_PIN 48    // NeoPixel　の出力ピン番号
#define LED_COUNT 1   // LEDの連結数
#define BRIGHTNESS 8  // 輝度
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);

typedef enum {
  PIX_RED,
  PIX_GREEN,
  PIX_YELLOW,
  PIX_BLUE,
  PIX_PURPLE,
  PIX_LIGHT_BLUE,
  PIX_WHITE,
  PIX_MIN = PIX_RED,
  PIX_MAX = PIX_WHITE
} PIX_COLOR_t;
uint8_t pixel_color;
uint8_t old_pixel_color;

// Connect LED
#define PIN_CONNECT_LED 8

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);  // BLE to COM
        Serial1.print(rxValue[i]);  // BLE to Uart1
      }
    }
  }
};

void setup_button() {
  pinMode(BUTTON, INPUT_PULLUP);  //プルダウンで入力
}

// #elif CONFIG_IDF_TARGET_ESP32C3
// #define RX1 (gpio_num_t)18	= USBD N

// #elif CONFIG_IDF_TARGET_ESP32C3
// #define TX1 (gpio_num_t)19	= USBD P

// #elif CONFIG_IDF_TARGET_ESP32C3
// #define SOC_RX0 (gpio_num_t)20	= U0RxD

// #elif CONFIG_IDF_TARGET_ESP32C3
// #define SOC_TX0 (gpio_num_t)21	= U0TxD

void setup_uart() {
#if CONFIG_IDF_TARGET_ESP32S3
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, RX1, TX1);
#elif CONFIG_IDF_TARGET_ESP32C3
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, SOC_RX0, SOC_TX0);
#endif
}
void setup_BLE() {
  uint8_t mac_value[7] = {0};
  String device_name = BLE_DEVICE_NAME;

  esp_err_t err = esp_read_mac(mac_value,ESP_MAC_BT);
  if(err == ESP_OK){
    char mac_str[6] = {0};
    sprintf(mac_str,"_%02X%02X", mac_value[4],mac_value[5]);
    device_name += (String)mac_str;
  }

  // Create the BLE Device
  BLEDevice::init(device_name);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
}
void setup_neopixel() {
#if ENABLE_PIXEL_LED
  pixels.begin();  //NeoPixel
  pixels.clear();
  set_pix_color(pixel_color);
#endif
}

void connect_led_on() {
#if CONFIG_IDF_TARGET_ESP32C3
  digitalWrite(PIN_CONNECT_LED, LOW);
#endif
}
void connect_led_off() {
#if CONFIG_IDF_TARGET_ESP32C3
  digitalWrite(PIN_CONNECT_LED, HIGH);
#endif
}
void setup_connect_led() {
#if CONFIG_IDF_TARGET_ESP32C3
  pinMode(PIN_CONNECT_LED, OUTPUT_OPEN_DRAIN);
  connect_led_off();
#endif
}

void setup() {
  // put your setup code here, to run once:
  setup_button();
  setup_uart();
  setup_BLE();
  setup_neopixel();
  setup_connect_led();
}

void observe_disconnected() {
  if (!deviceConnected && oldDeviceConnected) {
    connect_led_off();
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    oldDeviceConnected = deviceConnected;
  }
}
void observe_connected() {
  if (deviceConnected && !oldDeviceConnected) {
    connect_led_on();
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
void observe_uart_received() {
  if (deviceConnected == true) {
    if (Serial.available() != 0) {
      // USB to BLE
      size_t bufSize = Serial.read(txValue0, Serial.available());
      pTxCharacteristic->setValue(txValue0, bufSize);
      pTxCharacteristic->notify();
      delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
    }

    if (Serial1.available() != 0) {
      // Uart1 to BLE
      size_t bufSize1 = Serial1.read(txValue1, Serial1.available());
      pTxCharacteristic->setValue(txValue1, bufSize1);
      pTxCharacteristic->notify();
      delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
    }
  }
}

void observe_button() {
  int button_status = digitalRead(BUTTON);

  if (button_status == 0) {
    if (button_status != old_button_status) {
      button_detect_count = 64;
    }
  } else {
    key_code = BUTTON_OFF;
    button_detect_count = 0;
  }
  old_button_status = button_status;

  if (button_detect_count > 0) {
    --button_detect_count;
    if (0 == button_detect_count) {
      key_code = BUTTON_ON;
    }
  } else {
    key_code = BUTTON_OFF;
  }
}

void set_pix_color(uint8_t color) {
#if ENABLE_PIXEL_LED
  switch (color) {
    case PIX_RED:
      pixels.setPixelColor(0, pixels.Color(BRIGHTNESS, 0, 0));
      pixels.show();
      break;
    case PIX_GREEN:
      pixels.setPixelColor(0, pixels.Color(0, BRIGHTNESS, 0));
      pixels.show();
      break;
    case PIX_YELLOW:
      pixels.setPixelColor(0, pixels.Color(BRIGHTNESS, BRIGHTNESS, 0));
      pixels.show();
      break;
    case PIX_BLUE:
      pixels.setPixelColor(0, pixels.Color(0, 0, BRIGHTNESS));
      pixels.show();
      break;
    case PIX_PURPLE:
      pixels.setPixelColor(0, pixels.Color(BRIGHTNESS, 0, BRIGHTNESS));
      pixels.show();
      break;
    case PIX_LIGHT_BLUE:
      pixels.setPixelColor(0, pixels.Color(0, BRIGHTNESS, BRIGHTNESS));
      pixels.show();
      break;
    case PIX_WHITE:
    default:
      pixels.setPixelColor(0, pixels.Color(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS));
      pixels.show();
      break;
  }
#endif
}
void update_baudrate(uint8_t color) {
  switch (color) {
    case PIX_RED:
      Serial1.updateBaudRate(9600);
      break;
    case PIX_GREEN:
      Serial1.updateBaudRate(19200);
      break;
    case PIX_YELLOW:
      Serial1.updateBaudRate(38400);
      break;
    case PIX_BLUE:
      Serial1.updateBaudRate(57600);
      break;
    case PIX_PURPLE:
      Serial1.updateBaudRate(115200);
      break;
    case PIX_LIGHT_BLUE:
      Serial1.updateBaudRate(230400);
      break;
    case PIX_WHITE:
    default:
      Serial1.updateBaudRate(9600);
      break;
  }
}
void observe_pix() {
  if (key_code == BUTTON_ON) {
    pixel_color = pixel_color + 1;
    if (pixel_color > PIX_MAX) {
      pixel_color = PIX_MIN;
    }
  }

  if (pixel_color != old_pixel_color) {
    old_pixel_color = pixel_color;
    set_pix_color(pixel_color);
    update_baudrate(pixel_color);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  observe_button();
  observe_pix();

  observe_uart_received();

  // disconnecting
  observe_disconnected();
  // connecting
  observe_connected();
}

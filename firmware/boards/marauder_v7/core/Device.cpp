#include "core/Device.h"
#include "core/StorageSD.h"
#include "core/StorageLFS.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static StorageSD      storageSD;
static StorageLFS     storageLFS;
static ExtSpiClass    sdSpi(HSPI);  // dedicated SD SPI bus (SCK=14, MISO=12, MOSI=13)

void Device::applyNavMode() {}
void Device::boardHook() {}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  sdSpi.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, -1);
  storageLFS.begin();
  storageSD.begin(SDCARD_CS, sdSpi);

  auto* dev = new Device(display, power, &navigation, nullptr,
                         &storageSD, &storageLFS, &sdSpi, nullptr);
  dev->ExI2C = &Wire;
  return dev;
}
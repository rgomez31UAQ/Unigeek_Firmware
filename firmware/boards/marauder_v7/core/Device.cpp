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
static ExtSpiClass    sdSpi(HSPI);  // shares display bus (SCK=18, MISO=19, MOSI=23)

void Device::applyNavMode() {}
void Device::boardHook() {}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  sdSpi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);
  storageLFS.begin();
  storageSD.begin(SD_CS, sdSpi);

  auto* dev = new Device(display, power, &navigation, nullptr,
                         &storageSD, &storageLFS, &sdSpi, nullptr);
  dev->ExI2C = &Wire;
  return dev;
}
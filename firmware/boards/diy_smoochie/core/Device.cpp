#include "core/Device.h"
#include "core/StorageSD.h"
#include "core/StorageLFS.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"
#include "core/SpeakerI2S.h"

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static StorageSD      storageSD;
static StorageLFS     storageLFS;
static ExtSpiClass    sdSpi(FSPI);

void Device::applyNavMode() {}
void Device::boardHook() {}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  auto* dev = new Device(display, power, &navigation, nullptr,
                         &storageSD, &storageLFS, &sdSpi, nullptr);
  dev->ExI2C = &Wire;  // free — no internal I2C on this board
  return dev;
}
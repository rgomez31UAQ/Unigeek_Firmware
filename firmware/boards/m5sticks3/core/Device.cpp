//
// M5StickS3 — ESP32-S3, 8MB flash + OPI PSRAM, M5PM1 power IC, LittleFS only.
//

#include "core/Device.h"
#include "core/StorageLFS.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"          // pulls in ../lib/M5PM1.h
#include "Speaker.h"
#include <Wire.h>

M5PM1 pm1;

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static StorageLFS     storageLFS;
static SpeakerStickS3 speaker;

void Device::applyNavMode() {}

void Device::boardHook() {}

Device* Device::createInstance() {
  // M5PM1 owns Wire1 — initialises it internally
  pm1.begin();

  storageLFS.begin();

  auto* dev = new Device(display, power, &navigation, nullptr,
                         nullptr, &storageLFS, nullptr, &speaker);

  dev->ExI2C = &Wire;   // Grove I2C (SDA=9, SCL=10)
  dev->InI2C = &Wire1;  // M5PM1 power IC (SDA=47, SCL=48)
  return dev;
}

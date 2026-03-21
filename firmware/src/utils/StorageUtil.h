#pragma once

#include "core/Device.h"

class StorageUtil {
public:
  static constexpr uint64_t MIN_FREE_BYTES = 20 * 1024; // 20KB

  // Returns true if storage has at least MIN_FREE_BYTES free
  static bool hasSpace() {
    if (!Uni.Storage || !Uni.Storage->isAvailable()) return false;
    return Uni.Storage->freeBytes() >= MIN_FREE_BYTES;
  }
};

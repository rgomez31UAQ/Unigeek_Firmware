//
// WigleUtil.h — Shared Wigle API utilities (token, stats, upload).
//

#pragma once

#include <Arduino.h>
#include "core/IStorage.h"
#include "ui/views/ScrollListView.h"

class WigleUtil
{
public:
  static constexpr const char* TOKEN_PATH    = "/unigeek/wigle_token";
  static constexpr const char* WARDRIVE_PATH = "/unigeek/gps/wardriver";
  static constexpr uint8_t MAX_FILES = 20;
  static constexpr uint8_t MAX_STAT_ROWS = 12;

  // Token helpers
  static String readToken(IStorage* storage);
  static bool   saveToken(IStorage* storage, const String& token);
  static String tokenSublabel(IStorage* storage);

  // Fetch user stats into rows, returns row count (0 on failure — error shown via ShowStatusAction)
  static uint8_t fetchStats(ScrollListView::Row* rows, uint8_t maxRows);

  // List wardrive CSV files (sorted, populates names/labels/uploaded flags)
  static uint8_t listFiles(IStorage* storage, String* names, String* labels,
                            bool* uploaded, uint8_t max);

  // Upload a single file, returns true on success (shows progress/status via actions)
  static bool uploadFile(IStorage* storage, const String& fileName);

  // JSON value extractor
  static String jsonVal(const String& json, const char* key);

private:
  static String _httpGet(const String& token, const String& path);
};


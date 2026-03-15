#pragma once

#include "core/Device.h"
#include "core/ConfigManager.h"
#include <TFT_eSPI.h>

class ShowProgressAction
{
public:
  // Show a progress overlay with a message and 0-100 percent fill bar.
  // Always returns immediately (non-blocking). Call repeatedly to update.
  static void show(const char* message, uint8_t percent) {
    ShowProgressAction action(message, percent);
    action._run();
  }

private:
  static constexpr int PAD      = 6;
  static constexpr int BAR_H    = 10;
  static constexpr int MIN_W    = 120;
  static constexpr int LINE_H   = 10;
  static constexpr int MAX_LINES = 3;

  const char* _message;
  uint8_t     _percent;
  TFT_eSprite _spr;

  ShowProgressAction(const char* message, uint8_t percent)
    : _message(message), _percent(percent), _spr(&Uni.Lcd)
  {}

  void _run() {
    auto& lcd = Uni.Lcd;
    lcd.setTextSize(1);

    // Split message into lines, max 3
    String lines[MAX_LINES];
    int lineCount = 0;
    int maxTextW = lcd.width() - 8 - PAD * 4;

    String msg = _message;
    int pos = 0;
    while (pos < (int)msg.length() && lineCount < MAX_LINES) {
      int nl = msg.indexOf('\n', pos);
      if (nl == -1) nl = msg.length();
      String line = msg.substring(pos, nl);

      // Truncate line to fit width
      while (line.length() > 0 && lcd.textWidth(line) > maxTextW) {
        line = line.substring(0, line.length() - 1);
      }

      // If this is the last allowed line and there's more text, add ellipsis
      if (lineCount == MAX_LINES - 1 && nl < (int)msg.length()) {
        if (line.length() > 3) line = line.substring(0, line.length() - 3) + "...";
      }

      lines[lineCount++] = line;
      pos = nl + 1;
    }

    if (lineCount == 0) { lines[0] = ""; lineCount = 1; }

    // Calculate widest line for box width
    int widest = 0;
    for (int i = 0; i < lineCount; i++) {
      int tw = lcd.textWidth(lines[i]);
      if (tw > widest) widest = tw;
    }

    int w = max(widest + PAD * 4, MIN_W);
    if (w > lcd.width() - 8) w = lcd.width() - 8;
    int textH = lineCount * LINE_H;
    int h = PAD + textH + PAD + BAR_H + PAD;
    int x = (lcd.width()  - w) / 2;
    int y = (lcd.height() - h) / 2;

    uint16_t barColor = Config.getThemeColor();

    _spr.createSprite(w, h);
    _spr.fillSprite(TFT_BLACK);
    _spr.drawRoundRect(0, 0, w, h, 4, TFT_WHITE);

    // Message text centered
    _spr.setTextColor(TFT_WHITE, TFT_BLACK);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextSize(1);
    for (int i = 0; i < lineCount; i++) {
      int ly = PAD + i * LINE_H + LINE_H / 2;
      _spr.drawString(lines[i], w / 2, ly);
    }

    // Progress bar outline + fill
    int barX = PAD * 2;
    int barY = PAD + textH + PAD;
    int barW = w - PAD * 4;
    _spr.drawRect(barX, barY, barW, BAR_H, barColor);
    int fillW = (int)(barW * _percent / 100.0f);
    if (fillW > 0)
      _spr.fillRect(barX + 1, barY + 1, fillW, BAR_H - 2, barColor);

    _spr.pushSprite(x, y);
    _spr.deleteSprite();
  }
};

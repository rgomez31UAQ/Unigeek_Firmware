#pragma once

#include "core/Device.h"
#include "core/ConfigManager.h"
#include "ui/components/StatusBar.h"
#include "ui/components/Header.h"

class ProgressView
{
public:
  // Show a full-body progress view with a message and 0-100 percent fill bar.
  // Always returns immediately (non-blocking). Call repeatedly to update.
  static void show(const char* message, uint8_t percent) {
    ProgressView action(message, percent);
    action._run();
  }

private:
  static constexpr int BAR_H    = 10;
  static constexpr int BAR_PAD  = 8;
  static constexpr int LINE_H   = 12;
  static constexpr int MAX_LINES = 2;

  const char* _message;
  uint8_t     _percent;

  ProgressView(const char* message, uint8_t percent)
    : _message(message), _percent(percent)
  {}

  void _run() {
    auto& lcd = Uni.Lcd;
    uint16_t bx = StatusBar::WIDTH;
    uint16_t by = Header::HEIGHT;
    uint16_t bw = lcd.width() - StatusBar::WIDTH - 4;
    uint16_t bh = lcd.height() - by - 4;

    lcd.fillRect(bx, by, bw, bh, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);

    // Word-wrap message into max 2 lines
    String lines[MAX_LINES];
    int lineCount = 0;
    int maxTextW = bw - BAR_PAD * 2;

    String msg = _message;
    msg.replace("\n", " ");

    if (lcd.textWidth(msg) <= maxTextW) {
      lines[0] = msg;
      lineCount = 1;
    } else {
      int lastSpace = -1;
      for (int i = 0; i < (int)msg.length(); i++) {
        if (msg[i] == ' ') lastSpace = i;
        if (lcd.textWidth(msg.substring(0, i + 1)) > maxTextW) {
          if (lastSpace > 0) {
            lines[0] = msg.substring(0, lastSpace);
            msg = msg.substring(lastSpace + 1);
          } else {
            lines[0] = msg.substring(0, i);
            msg = msg.substring(i);
          }
          lineCount = 1;
          break;
        }
      }
      if (lineCount == 0) {
        lines[0] = msg;
        lineCount = 1;
        msg = "";
      }

      if (msg.length() > 0) {
        if (lcd.textWidth(msg) <= maxTextW) {
          lines[1] = msg;
        } else {
          for (int i = msg.length() - 1; i >= 0; i--) {
            String truncated = msg.substring(0, i) + "...";
            if (lcd.textWidth(truncated) <= maxTextW) {
              lines[1] = truncated;
              break;
            }
          }
        }
        lineCount = 2;
      }
    }

    // Center text block vertically above bar
    int barY     = bh - BAR_PAD - BAR_H;
    int textStartY = (barY - lineCount * LINE_H) / 2;

    for (int i = 0; i < lineCount; i++) {
      int ly = by + textStartY + i * LINE_H + LINE_H / 2;
      lcd.drawString(lines[i], bx + bw / 2, ly);
    }

    // Progress bar — draw directly on LCD
    int      barX    = bx + BAR_PAD;
    int      barW    = bw - BAR_PAD * 2;
    uint16_t barColor = Config.getThemeColor();
    int      absBarY  = by + barY;
    lcd.drawRect(barX, absBarY, barW, BAR_H, barColor);
    int fillW = (int)(barW * _percent / 100.0f);
    if (fillW > 0)
      lcd.fillRect(barX + 1, absBarY + 1, fillW, BAR_H - 2, barColor);
  }
};

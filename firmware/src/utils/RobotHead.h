#pragma once
#include <Arduino.h>

// ── RobotHead ────────────────────────────────────────────────────────────────
// Pixel-art robot mascot — a boxy head with an antenna, grille mouth and square
// eyes. Like CatHead it is a single-colour silhouette, so the body colour is
// supplied at draw time (Mascot.h feeds it the theme colour). Eyes are drawn on
// top of the base art so they can blink (tall when open, a squint when closed).

static constexpr int ROBOT_W = 11;
static constexpr int ROBOT_H = 12;

// '.' transparent · 'K' black outline / grille · 'B' body (theme-coloured).
static const char* const ROBOT_ART[ROBOT_H] = {
  ".....K.....",   // antenna tip
  "....KKK....",   // antenna bulb
  "..KKKKKKK..",   // head top
  "..KBBBBBK..",
  "..KBBBBBK..",   // eyes row (drawn on top)
  "..KBBBBBK..",
  "..KBKBKBK..",   // grille mouth
  "..KBBBBBK..",
  "..KKKKKKK..",   // head bottom
  "...K...K...",   // legs
  "...K...K...",
  "..KK...KK..",   // feet
};

static constexpr uint16_t ROBOT_K = 0x0000;   // black outline / grille / eyes

template<typename T>
void robotDrawEyes(T& dc, int ox, int oy, int ps, bool blink, uint16_t body) {
  auto cell = [&](int cx, int cy, uint16_t c) { dc.fillRect(ox + cx * ps, oy + cy * ps, ps, ps, c); };
  // clear the eye cells back to the body colour first (so blink can toggle)
  for (int y = 4; y <= 5; y++) { cell(3, y, body); cell(7, y, body); }
  if (blink) {                          // squint — only the lower cell
    cell(3, 5, ROBOT_K); cell(7, 5, ROBOT_K);
  } else {                              // wide open — both cells
    cell(3, 4, ROBOT_K); cell(3, 5, ROBOT_K);
    cell(7, 4, ROBOT_K); cell(7, 5, ROBOT_K);
  }
}

template<typename T>
void robotDrawHead(T& dc, int ox, int oy, int ps, bool blink, uint16_t body) {
  auto cell = [&](int cx, int cy, uint16_t c) { dc.fillRect(ox + cx * ps, oy + cy * ps, ps, ps, c); };
  for (int y = 0; y < ROBOT_H; y++) {
    const char* row = ROBOT_ART[y];
    for (int x = 0; x < ROBOT_W; x++) {
      switch (row[x]) {
        case 'K': cell(x, y, ROBOT_K); break;
        case 'B': cell(x, y, body);    break;
        default: break;   // '.' transparent
      }
    }
  }
  robotDrawEyes(dc, ox, oy, ps, blink, body);
}

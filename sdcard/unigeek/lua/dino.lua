-- dino.lua — Dino Jump.
-- Mostly direct overdraw (cheap, fast). The dino sits in a small sprite-backed
-- "lane" that spans its full vertical motion range, so when it jumps the new
-- pose appears in one push instead of an erase-then-draw flash. Cactus motion
-- stays as plain overdraw — moving every frame already masks any erase blink.

local lcd = require("uni.lcd")
local sd  = require("uni.sd")
local nav = require("uni.nav")

local W, H = lcd.w(), lcd.h()
local GY         = H - 18
local DX, DW, DH = 22, 13, 17
local OW         = 11
local GRAVITY    = 0.75
local JUMP_FORCE = -9.5

local C_WHITE  = lcd.color(255, 255, 255)
local C_GREEN  = lcd.color( 60, 210,  80)
local C_DARK   = lcd.color(  0, 100,  30)
local C_ORANGE = lcd.color(255, 140,   0)
local C_RED    = lcd.color(255,  60,  60)
local C_YELLOW = lcd.color(255, 220,   0)
local C_GREY   = lcd.color( 70,  70,  70)
local C_BLACK  = lcd.color(  0,   0,   0)

-- Dino sprite "lane" — fixed x band wide enough for body+head+legs, tall
-- enough to span ground rest pose to peak-of-jump. ~19 × ~50 ≈ 1.9 KB.
local LANE_X = DX - 2
local LANE_W = DW + 8
local LANE_Y = GY - DH - 28
local LANE_H = (GY + 5) - LANE_Y
local GROUND_Y_IN_LANE = (GY + 1) - LANE_Y

local dino_sp = lcd.sprite(LANE_W, LANE_H)
if not dino_sp then
  uni.debug("dino: sprite OOM, falling back to overdraw")
end

local hiScore = 0
local raw = sd.read("/unigeek/games/lua_dino.txt")
if raw and #raw > 0 then hiScore = math.floor(tonumber(raw) or 0) end

math.randomseed(math.floor(uni.millis()))

local gstate = "idle"
local score, dinoY, jumpV, onGnd
local obsX, obsH, speed, tick, leg, newHi
local prevDY, prevOX, prevOH

local function _reset()
  score  = 0
  dinoY  = GY - DH
  jumpV  = 0
  onGnd  = true
  obsX   = W + 40
  obsH   = 18 + math.random(0, 18)
  speed  = 2.5
  tick   = 0
  newHi  = false
  leg    = 0
  prevDY = math.floor(GY - DH)
  prevOX = math.floor(W + 40)
  prevOH = math.floor(obsH)
end

-- Compose the dino into a generic surface using the same shape both for the
-- on-screen overdraw path and the sprite-buffered path. `drawRect` matches
-- lcd.rect's signature; (ox, oy) is where the dino's body top-left lands.
local function _composeDino(drawRect, ox, oy, running)
  drawRect(ox,          oy,     DW, DH, C_GREEN)
  drawRect(ox + DW - 4, oy - 4,  7,  6, C_GREEN)
  drawRect(ox + DW + 1, oy - 3,  2,  2, C_BLACK)
  drawRect(ox + DW - 4, oy + 2,  4,  2, C_DARK)
  if not running or math.floor(leg / 4) % 2 == 0 then
    drawRect(ox + 2, oy + DH,     4, 4, C_GREEN)
    drawRect(ox + 8, oy + DH + 1, 4, 3, C_GREEN)
  else
    drawRect(ox + 2, oy + DH + 1, 4, 3, C_GREEN)
    drawRect(ox + 8, oy + DH,     4, 4, C_GREEN)
  end
end

local function _drawDinoLcd(dy, running)
  _composeDino(lcd.rect, DX, math.floor(dy), running)
end

local function _drawDinoSprite(dy, running)
  dino_sp:fill(C_BLACK)
  dino_sp:rect(0, GROUND_Y_IN_LANE, LANE_W, 2, C_GREY)
  _composeDino(
    function(x, y, w, h, c) dino_sp:rect(x, y, w, h, c) end,
    DX - LANE_X, math.floor(dy) - LANE_Y, running)
  dino_sp:push(LANE_X, LANE_Y)
end

local function _drawCactus(ox, oh)
  local ix = math.floor(ox)
  local iy = math.floor(oh)
  lcd.rect(ix,       GY - iy, OW, iy, C_ORANGE)
  if iy > 14 then
    lcd.rect(ix - 4, GY - iy + 5, 4, 5, C_ORANGE)
    lcd.rect(ix - 4, GY - iy + 5, 5, 3, C_ORANGE)
  end
  if iy > 10 then
    lcd.rect(ix + OW,     GY - iy + 8, 4, 4, C_ORANGE)
    lcd.rect(ix + OW - 1, GY - iy + 8, 5, 3, C_ORANGE)
  end
end

local function _collision()
  local dy  = math.floor(dinoY)
  local ox  = math.floor(obsX)
  local oh  = math.floor(obsH)
  local TOL = 3
  return (DX + DW - TOL > ox + TOL) and
         (DX + TOL < ox + OW - TOL) and
         (dy + DH - TOL > GY - oh + TOL)
end

-- ── Main loop ─────────────────────────────────────────────────────────

local prevState = ""

while true do
  local btn     = nav.btn()
  if btn == "back" then break end
  local entered = (gstate ~= prevState)
  prevState = gstate

  -- ── IDLE ──────────────────────────────────────────────────────────
  if gstate == "idle" then
    if entered then
      lcd.clear()
      lcd.rect(0, GY + 1, W, 2, C_GREY)
      lcd.textSize(2)
      lcd.textColor(C_WHITE)
      lcd.print(math.floor(W/2) - 52, math.floor(H/2) - 28, "DINO JUMP")
      lcd.textSize(1)
      lcd.textColor(C_GREY)
      lcd.print(math.floor(W/2) - 30, math.floor(H/2) - 4, "OK / UP to start")
      if hiScore > 0 then
        lcd.textColor(C_YELLOW)
        lcd.print(math.floor(W/2) - 28, math.floor(H/2) + 10, "Best: " .. hiScore)
      end
      _drawDinoLcd(GY - DH, false)
    end

    if btn == "ok" or btn == "up" then
      _reset()
      lcd.clear()
      lcd.rect(0, GY + 1, W, 2, C_GREY)
      gstate = "play"
    end

  -- ── GAME OVER ─────────────────────────────────────────────────────
  elseif gstate == "over" then
    if entered then
      lcd.clear()
      lcd.rect(0, GY + 1, W, 2, C_GREY)
      lcd.textSize(2)
      lcd.textColor(C_RED)
      lcd.print(math.floor(W/2) - 52, math.floor(H/2) - 26, "GAME OVER")
      lcd.textSize(1)
      lcd.textColor(C_WHITE)
      lcd.print(math.floor(W/2) - 26, math.floor(H/2) - 4, "Score: " .. score)
      if newHi then
        lcd.textColor(C_YELLOW)
        lcd.print(math.floor(W/2) - 28, math.floor(H/2) + 8, "NEW BEST!")
      else
        lcd.textColor(C_GREY)
        lcd.print(math.floor(W/2) - 28, math.floor(H/2) + 8, "Best:  " .. hiScore)
      end
      lcd.textColor(C_GREY)
      lcd.print(math.floor(W/2) - 32, math.floor(H/2) + 22, "OK: retry")
      _drawDinoLcd(GY - DH, false)
    end

    if btn == "ok" or btn == "up" then
      _reset()
      lcd.clear()
      lcd.rect(0, GY + 1, W, 2, C_GREY)
      gstate = "play"
    end

  -- ── PLAY ──────────────────────────────────────────────────────────
  else
    tick = tick + 1
    leg  = leg + 1

    local pox = prevOX
    local poh = prevOH

    if (btn == "ok" or btn == "up") and onGnd then
      jumpV = JUMP_FORCE
      onGnd = false
      uni.beep(1200, 20)
    end

    dinoY = dinoY + jumpV
    jumpV = jumpV + GRAVITY
    if dinoY >= GY - DH then
      dinoY = GY - DH
      jumpV = 0
      onGnd = true
    end

    obsX = obsX - speed
    if obsX < -OW - 10 then
      obsX  = W + math.random(30, 70)
      obsH  = 16 + math.random(0, 22)
      speed = speed + 0.04
    end

    if tick % 8 == 0 then score = score + 1 end
    if score > 0 and score % 50 == 0 and tick % 8 == 0 then
      uni.beep(660, 30)
    end

    -- Cactus erase (the lane sprite handles dino erase implicitly).
    if pox < W + 5 then
      lcd.rect(pox - 5, GY - poh - 2, OW + 11, poh + 5, C_BLACK)
    end
    -- Repaint ground line outside the dino lane only — the lane redraws its
    -- own slice of the ground when it pushes, so blasting the full strip here
    -- would just race the lane push every frame.
    lcd.rect(0, GY + 1, LANE_X, 2, C_GREY)
    lcd.rect(LANE_X + LANE_W, GY + 1, W - (LANE_X + LANE_W), 2, C_GREY)

    _drawCactus(obsX, obsH)
    if dino_sp then
      _drawDinoSprite(dinoY, onGnd)
    else
      lcd.rect(DX - 1, prevDY - 5, DW + 6, DH + 11, C_BLACK)
      _drawDinoLcd(dinoY, onGnd)
    end

    -- HUD: bg fills behind glyphs — no erase rect needed.
    lcd.textSize(1)
    lcd.textColor(C_WHITE, C_BLACK)
    lcd.print(0, 0, string.format("Score:%-5d", score))
    if hiScore > 0 then
      lcd.textColor(C_GREY, C_BLACK)
      lcd.print(W - 54, 0, string.format("Best:%-4d", hiScore))
    end
    lcd.textColor(C_WHITE)

    prevDY = math.floor(dinoY)
    prevOX = math.floor(obsX)
    prevOH = math.floor(obsH)

    if _collision() then
      uni.beep(150, 120)
      if score > hiScore then
        hiScore = score
        newHi   = true
        sd.write("/unigeek/games/lua_dino.txt", tostring(hiScore))
        uni.debug("new high score: " .. hiScore)
      end
      gstate = "over"
    end
  end

  uni.delay(16)
end

if dino_sp then dino_sp:free() end

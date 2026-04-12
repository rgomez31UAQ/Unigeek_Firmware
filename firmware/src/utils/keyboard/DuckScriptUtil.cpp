#include "DuckScriptUtil.h"

uint8_t DuckScriptUtil::_charToHID(const char* str)
{
  uint8_t value = 0;
  size_t  len   = strlen(str);
  if (len == 1) {
    value = (uint8_t)str[0];
  } else if (len >= 2 && len <= 3 && (str[0] == 'F' || str[0] == 'f') && isdigit(str[1])) {
    int fn = atoi(&str[1]);
    if (fn >= 1 && fn <= 12) value = (uint8_t)(0xC1 + fn);
  } else if (strcmp(str, "ENTER") == 0) {
    value = KEY_RETURN;
  } else if (strcmp(str, "ESC") == 0) {
    value = KEY_ESC;
  } else if (strcmp(str, "SPACE") == 0) {
    value = (uint8_t)' ';
  } else if (strcmp(str, "TAB") == 0) {
    value = KEY_TAB;
  } else if (strcmp(str, "BACKSPACE") == 0) {
    value = KEY_BACKSPACE;
  } else if (strcmp(str, "DELETE") == 0 || strcmp(str, "DEL") == 0) {
    value = KEY_DELETE;
  } else if (strcmp(str, "UP") == 0 || strcmp(str, "UPARROW") == 0) {
    value = KEY_UP_ARROW;
  } else if (strcmp(str, "DOWN") == 0 || strcmp(str, "DOWNARROW") == 0) {
    value = KEY_DOWN_ARROW;
  } else if (strcmp(str, "LEFT") == 0 || strcmp(str, "LEFTARROW") == 0) {
    value = KEY_LEFT_ARROW;
  } else if (strcmp(str, "RIGHT") == 0 || strcmp(str, "RIGHTARROW") == 0) {
    value = KEY_RIGHT_ARROW;
  } else if (strcmp(str, "HOME") == 0) {
    value = KEY_HOME;
  } else if (strcmp(str, "END") == 0) {
    value = KEY_END;
  } else if (strcmp(str, "PAGEUP") == 0) {
    value = KEY_PAGE_UP;
  } else if (strcmp(str, "PAGEDOWN") == 0) {
    value = KEY_PAGE_DOWN;
  }
  return value;
}

void DuckScriptUtil::_holdPress(uint8_t modifier, uint8_t key)
{
  KeyReport r = {};
  _keyboard->reportModifier(&r, modifier);
  _keyboard->reportModifier(&r, key);
  _keyboard->sendReport(&r);
  delay(50);
  _keyboard->releaseAll();
}

void DuckScriptUtil::_holdPress2(uint8_t mod1, uint8_t mod2, uint8_t key)
{
  KeyReport r = {};
  _keyboard->reportModifier(&r, mod1);
  _keyboard->reportModifier(&r, mod2);
  _keyboard->reportModifier(&r, key);
  _keyboard->sendReport(&r);
  delay(50);
  _keyboard->releaseAll();
}

bool DuckScriptUtil::runCommand(const String& line)
{
  if (line.startsWith("STRING ")) {
    String s = line.substring(7);
    _keyboard->write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
  } else if (line.startsWith("STRINGLN ")) {
    String s = line.substring(9);
    _keyboard->write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
    _keyboard->write(KEY_RETURN);
  } else if (line.startsWith("DELAY ")) {
    delay(line.substring(6).toInt());
  } else if (line.startsWith("GUI ")) {
    _holdPress(KEY_LEFT_GUI, _charToHID(line.substring(4).c_str()));
  } else if (line.startsWith("CTRL ")) {
    _holdPress(KEY_LEFT_CTRL, _charToHID(line.substring(5).c_str()));
  } else if (line.startsWith("ALT ")) {
    _holdPress(KEY_LEFT_ALT, _charToHID(line.substring(4).c_str()));
  } else if (line.startsWith("SHIFT ")) {
    _holdPress(KEY_LEFT_SHIFT, _charToHID(line.substring(6).c_str()));
  } else if (line.startsWith("ENTER")) {
    _keyboard->write(KEY_RETURN);
  } else if (line.startsWith("CTRL-SHIFT ")) {
    _holdPress2(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, _charToHID(line.substring(11).c_str()));
  } else if (line.startsWith("WINDOWS ")) {
    _holdPress(KEY_LEFT_GUI, _charToHID(line.substring(8).c_str()));
  } else if (line == "SPACE") {
    _keyboard->write((uint8_t)' ');
  } else if (line == "TAB") {
    _keyboard->write(KEY_TAB);
  } else if (line == "LEFTARROW") {
    _keyboard->write(KEY_LEFT_ARROW);
  } else if (line == "RIGHTARROW") {
    _keyboard->write(KEY_RIGHT_ARROW);
  } else if (line == "UPARROW") {
    _keyboard->write(KEY_UP_ARROW);
  } else if (line == "DOWNARROW") {
    _keyboard->write(KEY_DOWN_ARROW);
  } else if (line == "BACKSPACE") {
    _keyboard->write(KEY_BACKSPACE);
  } else if (line == "DELETE") {
    _keyboard->write(KEY_DELETE);
  } else if (line == "HOME") {
    _keyboard->write(KEY_HOME);
  } else if (line == "END") {
    _keyboard->write(KEY_END);
  } else if (line == "PAGEUP") {
    _keyboard->write(KEY_PAGE_UP);
  } else if (line == "PAGEDOWN") {
    _keyboard->write(KEY_PAGE_DOWN);
  } else if (line == "ESC") {
    _keyboard->write(KEY_ESC);
  } else if (line.startsWith("REM ") || line.startsWith("REM\t") || line == "REM") {
    // comment — do nothing
  } else {
    // standalone F1-F12
    uint8_t k = _charToHID(line.c_str());
    if (k) { _keyboard->write(k); } else { return false; }
  }
  return true;
}
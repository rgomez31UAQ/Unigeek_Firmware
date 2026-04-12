#pragma once

#include "ui/templates/BaseScreen.h"

class GameNumberGuessScreen : public BaseScreen
{
public:
  const char* title()       override { return "Number Guess"; }
  bool inhibitPowerSave()   override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()    override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE,
    STATE_HIGH_SCORES
  } _state = STATE_MENU;

  static constexpr uint8_t kDiffCount = 4;
  static constexpr uint8_t kMenuItems = 4;
  static constexpr uint8_t kMaxScores = 5;

  struct Score {
    int      guesses;
    uint32_t ms;
  };

  // Menu
  int8_t  _menuIdx    = 0;
  uint8_t _diff       = 0;
  uint8_t _hsViewDiff = 0;

  // Game state
  int      _target     = 0;
  int      _lastGuess  = -1;
  int      _guesses    = 0;
  int      _trialsLeft = 0;
  uint32_t _startMs    = 0;
  int8_t   _cmp        = 0;    // -1=lower, 0=none, 1=higher
  bool     _hasGuessed = false;

  // Digit input
  char    _inputBuf[6] = {};   // max 4 digits + null
  uint8_t _inputCursor = 0;
  int8_t  _cycleDigit  = 0;   // non-keyboard: 0-9=digit, 10=backspace '<'
  bool    _holdFired   = false;

  // Scores for current difficulty
  Score   _scores[kMaxScores] = {};
  uint8_t _scoreCount = 0;
  int8_t  _newRank    = -1;

  uint8_t _maxDigits()  const;

  void _initGame();
  void _submitGuess();
  void _confirmDigit();      // non-keyboard: lock in cycled digit
  void _backDigit();
  void _loadScores(uint8_t diff);
  void _saveScores(uint8_t diff);
  void _insertScore(int g, uint32_t ms);

  void _renderMenu();
  void _renderPlay();
  void _renderResult();
  void _renderHighScores();
};
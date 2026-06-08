//
// Created by L Shaf on 2026-06-08.
//
// Wikipedia browser (Network menu). Menu:
//   Search        — free-text query -> paged results -> fetch + cache -> view
//   Random        — open a random article
//   On This Day   — historical events for today (RTC date), open linked article
//   Search Cached — free-text filter over cached articles (current language)
//   All Cached    — every cached article (current language)
//   Favorites     — pinned articles (all languages)
//   Language      — persisted Wikipedia language (Latin-script editions)
//
// Long-press a list row for a context menu: Open / Favorite / Share QR.
// Articles are saved per language under /unigeek/wikipedia/<lang>/<title>.txt
// and opened with the shared FileViewerScreen (word-wraps .txt). Content is
// plain-text (TextExtracts API), ASCII-folded + transliterated for the TFT.

#pragma once

#include "ui/templates/ListScreen.h"
#include <vector>

class WikipediaScreen : public ListScreen
{
public:
  explicit WikipediaScreen(bool readOnly = false) : _readOnly(readOnly) {}

  const char* title() override { return _titleBuf; }

  void onInit() override;
  void onUpdate() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;

private:
  enum State {
    STATE_MENU,
    STATE_RESULTS,
    STATE_FILELIST,     // cached or favorites (see _flMode)
    STATE_ONTHISDAY,
    STATE_ITEM_MENU,    // per-item context menu
  } _state = STATE_MENU;

  enum FlMode { FL_CACHED, FL_FAV } _flMode = FL_CACHED;

  enum CtxAction : uint8_t { CTX_OPEN, CTX_FAV, CTX_SHARE, CTX_CANCEL };

  static constexpr const char* WIKI_DIR     = "/unigeek/wikipedia";
  static constexpr const char* FAV_FILE     = "/unigeek/wikipedia/favorites.txt";
  static constexpr uint8_t      kPageSize         = 50;
  static constexpr uint8_t      kFileScanCap      = 150;
  static constexpr uint32_t     kMaxArticleBytes  = 100000;  // cap full-text fetch at 100 KB

  char _titleBuf[48] = "Wikipedia";

  ListItem _menuItems[7] = {
    {"Search"}, {"Random Article"}, {"On This Day"},
    {"Search Cached"}, {"All Cached"}, {"Favorites"}, {"Language"},
  };
  String   _langSub;   // backs _menuItems[6].sublabel

  // Composed display list = optional[Prev] + data + optional[Next].
  ListItem _listItems[kPageSize + 2];
  int8_t   _rowPrev      = -1;
  int8_t   _rowNext      = -1;
  uint8_t  _rowDataStart = 0;

  // Search results (current page).
  String   _titles[kPageSize];        // raw UTF-8 title (for fetch URL)
  String   _resultLabels[kPageSize];  // ASCII-folded display label
  ListItem _resultItems[kPageSize];
  uint8_t  _resultCount   = 0;
  String   _searchQuery;
  int      _srOffset      = 0;
  bool     _resultHasMore = false;

  // File list (cached / favorites), paged.
  std::vector<String>  _flRel;     // path relative to WIKI_DIR ("en/Title.txt")
  std::vector<String>  _flLabel;   // display label (title)
  std::vector<uint8_t> _flFav;     // favorited flag per row
  ListItem             _pageItems[kPageSize];
  uint16_t             _flPage = 0;
  String               _cacheFilter;

  // On This Day
  String   _otdLabels[kPageSize];  // "1969 - text"
  String   _otdTitles[kPageSize];  // article title to open
  ListItem _otdItems[kPageSize];
  uint8_t  _otdCount = 0;

  // Favorites set (relative paths), loaded from FAV_FILE.
  std::vector<String> _favRel;

  // Context menu
  ListItem  _ctxItems[4];
  CtxAction _ctxActions[4];
  uint8_t   _ctxCount = 0;
  State     _ctxFrom  = STATE_RESULTS;
  String    _ctxLabel;        // display title (also context-menu title)
  String    _ctxFavLabel;     // "Favorite" / "Unfavorite"
  String    _ctxTitleForUrl;  // raw title for the share URL / fetch
  String    _ctxLang;
  String    _ctxPath;         // full path (file list items)
  String    _ctxRel;          // relative path (favorite toggle)
  uint8_t   _ctxResultIdx = 0;
  bool      _ctxIsFav     = false;

  uint32_t _navReadyAt = 0;   // drain ghost-presses after blocking HTTP/input
  bool     _holdFired  = false;
  bool     _readOnly   = false;

  // States / actions
  void _showMenu();
  void _chooseLanguage();
  void _doSearch();
  void _runSearch(int offset);
  void _openResultByIdx(uint8_t dataIndex);
  void _showRandom();
  void _showOnThisDay();
  void _openOtd(uint8_t index);
  void _searchCached();
  void _showAllCached();
  void _showFavorites();
  void _loadFileList();
  void _showFilePage(int page);
  void _openFileAbs(uint16_t absIndex);
  void _viewArticle(const String& path, const String& lang);
  void _recordRead(const String& lang);

  void _openItemMenu(uint8_t displayIndex);
  void _returnFromContext();
  void _shareQr(const String& titleForUrl, const String& lang, const String& label);

  // Favorites
  void _favLoad();
  void _favSave();
  bool _favHas(const String& rel);
  bool _favToggle(const String& rel);   // returns new favorited state

  // Paged-list helper
  void _composePage(ListItem* data, uint8_t dataCount, bool hasPrev, bool hasNext);

  // Networking / parsing
  bool   _fetchAndCache(const String& rawTitle, const String& label,
                        const String& lang, String& outPath);
  bool   _httpGet(const String& url, String& out);
  String _extractField(const String& body);
  static int _strEnd(const String& s, int start);   // index of closing JSON quote

  String        _langCode();
  static const char* _langName(const String& code);

  static bool   _containsCI(const String& hay, const String& needle);
  static String _urlEncode(const String& s);
  static String _jsonUnescape(const String& s);
  static String _asciiFold(const String& s);
  static String _sanitize(const String& s);
  static void   _appendUtf8(String& out, uint32_t cp);
};

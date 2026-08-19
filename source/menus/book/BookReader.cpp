#include "BookReader.hpp"
#include "PageLayout.hpp"
#include "LandscapePageLayout.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <libconfig.h>
#include <cstring>
#include <cstdio>

// Reading fonts offered for reflowable books. The CSS names map to MuPDF's
// built-in font substitutes, so no extra font files need to be shipped.
static const char *FONT_FAMILIES[] = { "serif", "sans-serif", "monospace" };
static const char *FONT_NAMES[]    = { "Serif", "Sans-serif", "Monospace" };
static const int   FONT_COUNT      = 3;

extern "C"  {
    #include "SDL_helper.h"
    #include "status_bar.h"
    #include "config.h"
    #include "textures.h"
    #include "common.h"
}

fz_context *ctx = NULL;
int windowX, windowY;
config_t *config = NULL;
char* configFile = "/switch/eBookReader/saved_pages.cfg";

static int load_last_page(const char *book_name)  {
    if (!config) {
        config = (config_t *)malloc(sizeof(config_t));
        config_init(config);
        config_read_file(config, configFile);
    }
    
    config_setting_t *setting = config_setting_get_member(config_root_setting(config), book_name);
    
    if (setting) {
        return config_setting_get_int(setting);
    }

    return 0;
}

static void save_last_page(const char *book_name, int current_page) {
    config_setting_t *setting = config_setting_get_member(config_root_setting(config), book_name);
    
    if (!setting) {
        setting = config_setting_add(config_root_setting(config), book_name, CONFIG_TYPE_INT);
    }
    
    if (setting) {
        config_setting_set_int(setting, current_page);
        config_write_file(config, configFile);
    }
}

BookReader::BookReader(const char *path, int* result) {
    if (ctx == NULL) {
        ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
        fz_register_document_handlers(ctx);
    }

    SDL_GetWindowSize(WINDOW, &windowX, &windowY);
    
    book_name = std::string(path).substr(std::string(path).find_last_of("/\\") + 1);
    
    std::string invalid_chars = " :/?#[]@!$&'()*+,;=.";
    for (char& c: invalid_chars) {
        book_name.erase(std::remove(book_name.begin(), book_name.end(), c), book_name.end());
    }
    
    fz_try(ctx)	{
        std::cout << "fz_open_document" << std::endl;
        doc = fz_open_document(ctx, path);

        if (!doc)
        {
            std::cout << "Error opening file!" << std::endl;
            *result = -1;
            return;
        }

        // EPUB and other reflowable formats can have their reading font changed;
        // fixed-layout formats (PDF, CBZ, XPS) cannot. Lay the document out once
        // with our defaults before the first page layout is built.
        _reflowable = fz_is_document_reflowable(ctx, doc);
        if (_reflowable) {
            apply_reflow_settings(false);
        }

        int current_page = load_last_page(book_name.c_str());
        //int current_page = 0;

        load_bookmarks();

        std::cout << "current_page = " << current_page << std::endl;

        switch_current_page_layout(_currentPageLayout, current_page);

        if (current_page > 0) {
            show_status_bar();
        }
    }
    fz_catch(ctx){
        std::cout << "fz_catch reached, closing gracefully" << std::endl;
        *result = -2;
        return;
    }
}

BookReader::~BookReader() {
    fz_drop_document(ctx, doc);
    
    delete layout;
}

void BookReader::previous_page(int n) {
    layout->previous_page(n);
    show_status_bar();
    save_last_page(book_name.c_str(), layout->current_page());
}

void BookReader::next_page(int n) {
    layout->next_page(n);
    show_status_bar();
    save_last_page(book_name.c_str(), layout->current_page());
}

void BookReader::zoom_in() {
    layout->zoom_in();
    show_status_bar();
}

void BookReader::zoom_out() {
    layout->zoom_out();
    show_status_bar();
}

void BookReader::move_page_up() {
    layout->move_up();
}

void BookReader::move_page_down() {
    layout->move_down();
}

void BookReader::move_page_left() {
    layout->move_left();
}

void BookReader::move_page_right() {
    layout->move_right();
}

void BookReader::pan_by(float dx, float dy) {
    if (layout) layout->pan(dx, dy);
}

bool BookReader::is_zoomed() {
    return layout ? layout->is_zoomed() : false;
}

void BookReader::apply_reflow_settings(bool rebuild) {
    if (!_reflowable) return;

    // Portrait: a comfortable reading column at native (unrotated) size.
    // Landscape: a book-page-shaped area, since it now genuinely rotates
    // 90° (same mechanism as PDF) rather than just widening in place.
    bool landscape = (_currentPageLayout == BookPageLayoutLandscape);
    float page_w = landscape ? 580.0f : 700.0f;
    float page_h = landscape ? 880.0f : 720.0f;

    // Portrait: taller top/bottom margins (status bar clearance, comfortable
    // reading), narrower side margins (use more of the screen width).
    // Landscape keeps its own separately-tuned padding -- it's rotated 90°,
    // so this "top" padding is what protects against the bar there too, and
    // widening it the same way as portrait would eat into the page height.
    const char *padding = landscape ? "64px 44px 36px 44px" : "72px 24px 56px 24px";

    char css[640];
    snprintf(css, sizeof(css),
             // Custom bundled fonts. MuPDF's HTML/EPUB engine falls back to
             // reading `src` as a real filesystem path whenever it isn't
             // found inside the book's own zip, so these romfs paths load
             // exactly like the UI font already does. No matching serif
             // TTF was supplied, so "serif" keeps MuPDF's built-in Charis
             // SIL substitute.
             "@font-face{font-family:\"sans-serif\";src:url(\"romfs:/resources/font/Sans-serif.ttf\")}"
             "@font-face{font-family:\"monospace\";src:url(\"romfs:/resources/font/Mono.ttf\")}"
             "@page{margin:0}"
             "html{margin:0;padding:0}"
             "body{margin:0 !important;padding:%s !important;"
             "line-height:1.4 !important}"
             "*{font-family:%s !important}",
             padding, FONT_FAMILIES[_font_index]);

    // Disable the book's own stylesheet so our chosen font actually takes
    // effect (many EPUBs pin their own font-family, which would otherwise win).
    fz_set_use_document_css(ctx, 0);
    fz_set_user_css(ctx, css);

    fz_try(ctx) {
        // Re-flow the whole book at the chosen size. Width/height define the
        // reading area in points; em is the base font size.
        fz_layout_document(ctx, doc, page_w, page_h, _em);
    }
    fz_catch(ctx) {
        std::cout << "apply_reflow_settings: relayout failed" << std::endl;
        return;
    }

    if (rebuild) {
        // Re-flowing changes the page count and page indices, so rebuild the
        // current layout, keeping roughly the same position.
        switch_current_page_layout(_currentPageLayout, layout ? layout->current_page() : 0);
        show_status_bar();
    }
}

void BookReader::cycle_font() {
    if (!_reflowable) {
        set_toast("Fonts apply to EPUB / text books only");
        return;
    }

    _font_index = (_font_index + 1) % FONT_COUNT;
    apply_reflow_settings();

    char msg[64];
    snprintf(msg, sizeof(msg), "Font: %s", FONT_NAMES[_font_index]);
    set_toast(msg);
}

void BookReader::increase_font_size() {
    if (!_reflowable) {
        set_toast("Font size applies to EPUB / text books only");
        return;
    }

    if (_em < 31.0f) _em += 1.0f;
    apply_reflow_settings();

    char msg[64];
    snprintf(msg, sizeof(msg), "Font size: %.0f", _em);
    set_toast(msg);
}

void BookReader::decrease_font_size() {
    if (!_reflowable) {
        set_toast("Font size applies to EPUB / text books only");
        return;
    }

    if (_em > 21.0f) _em -= 1.0f;
    apply_reflow_settings();

    char msg[64];
    snprintf(msg, sizeof(msg), "Font size: %.0f", _em);
    set_toast(msg);
}

void BookReader::set_toast(const char *msg) {
    strncpy(_toast, msg, sizeof(_toast) - 1);
    _toast[sizeof(_toast) - 1] = '\0';
    _toast_counter = 120; // ~2 seconds at 60fps
}

// Bookmarks are stored per book as a comma-separated list of page numbers in
// the same libconfig file used for the last-read page.
void BookReader::load_bookmarks() {
    _bookmarks.clear();
    if (!config) return;

    std::string key = book_name + "_bm";
    config_setting_t *setting = config_setting_get_member(config_root_setting(config), key.c_str());
    if (!setting) return;

    const char *csv = config_setting_get_string(setting);
    if (!csv) return;

    int page = 0;
    bool have = false;
    for (const char *p = csv; ; p++) {
        if (*p >= '0' && *p <= '9') { page = page * 10 + (*p - '0'); have = true; }
        else { if (have) _bookmarks.insert(page); page = 0; have = false; if (*p == '\0') break; }
    }
}

void BookReader::save_bookmarks() {
    if (!config) return;

    std::string csv;
    char num[16];
    for (int p : _bookmarks) {
        snprintf(num, sizeof(num), "%d", p);
        if (!csv.empty()) csv += ",";
        csv += num;
    }

    std::string key = book_name + "_bm";
    config_setting_t *setting = config_setting_get_member(config_root_setting(config), key.c_str());
    if (!setting) {
        setting = config_setting_add(config_root_setting(config), key.c_str(), CONFIG_TYPE_STRING);
    }
    if (setting) {
        config_setting_set_string(setting, csv.c_str());
        config_write_file(config, configFile);
    }
}

bool BookReader::is_current_bookmarked() {
    if (!layout) return false;
    return _bookmarks.count(layout->current_page()) > 0;
}

void BookReader::toggle_bookmark() {
    if (!layout) return;

    int page = layout->current_page();
    char msg[64];
    if (_bookmarks.count(page)) {
        _bookmarks.erase(page);
        snprintf(msg, sizeof(msg), "Removed bookmark (page %d)", page + 1);
    } else {
        _bookmarks.insert(page);
        snprintf(msg, sizeof(msg), "Bookmarked page %d", page + 1);
    }
    save_bookmarks();
    set_toast(msg);
}

void BookReader::jump_to_bookmark() {
    if (!layout) return;
    if (_bookmarks.empty()) { set_toast("No bookmarks yet"); return; }

    int cur = layout->current_page();
    auto it = _bookmarks.upper_bound(cur);        // next bookmark after current
    int target = (it != _bookmarks.end()) ? *it : *_bookmarks.begin(); // else wrap

    if (target > cur)      next_page(target - cur);
    else if (target < cur) previous_page(cur - target);

    char msg[48];
    snprintf(msg, sizeof(msg), "Jumped to page %d", target + 1);
    set_toast(msg);
}

// Flattens the document's outline (table of contents) tree into a simple
// ordered list with a depth for indentation, skipping any entries that lack
// a real page target (e.g. external links).
// Resolves an outline entry to a real flat page number, threading the
// document through the recursion so each node can be resolved via its URI.
//
// Some MuPDF document handlers (this bundled version's EPUB support among
// them) never actually populate fz_outline::page -- it's left permanently
// at -1, even though the outline's titles and URIs are parsed correctly.
// Using fz_resolve_link + fz_page_number_from_location recovers the real
// page independent of that field, so EPUB chapters resolve exactly like
// PDF ones (which set page directly and don't need this fallback).
static void flatten_outline(fz_context *ctx, fz_document *doc, fz_outline *node, int depth,
                             std::vector<BookReader::ChapterEntry> &out) {
    while (node) {
        if (node->title) {
            int page = node->page;
            if (page < 0 && node->uri) {
                fz_try(ctx) {
                    float x, y;
                    fz_location loc = fz_resolve_link(ctx, doc, node->uri, &x, &y);
                    page = fz_page_number_from_location(ctx, doc, loc);
                }
                fz_catch(ctx) {
                    page = -1;
                }
            }
            if (page >= 0) {
                BookReader::ChapterEntry e;
                e.title = node->title;
                e.page = page;
                e.depth = depth;
                out.push_back(e);
            }
        }
        if (node->down) flatten_outline(ctx, doc, node->down, depth + 1, out);
        node = node->next;
    }
}

void BookReader::load_chapters() {
    _chapters.clear();
    _chaptersLoaded = true;
    if (!doc) return;

    fz_outline *outline = NULL;
    fz_try(ctx) {
        outline = fz_load_outline(ctx, doc);
    }
    fz_catch(ctx) {
        std::cout << "load_chapters: failed to load outline" << std::endl;
        outline = NULL;
    }

    if (outline) {
        flatten_outline(ctx, doc, outline, 0, _chapters);
        fz_drop_outline(ctx, outline);
    }
}

void BookReader::toggle_jump_menu() {
    if (_jumpMenuOpen) {
        _jumpMenuOpen = false;
        return;
    }

    if (!_chaptersLoaded) {
        load_chapters();
    }

    _jumpTargetPage = layout ? layout->current_page() : 0;
    _jumpSelRow = 0;
    _jumpFirstVisibleChapter = 0;
    _jumpMenuOpen = true;
}

void BookReader::jump_menu_input(u64 kDown, u64 kHeld) {
    int totalPages = layout ? layout->total_pages() : 1;
    if (totalPages < 1) totalPages = 1;
    int totalRows = 1 + (int) _chapters.size(); // row 0 = page number, rest = chapters

    if (kDown & HidNpadButton_B) {
        _jumpMenuOpen = false;
        return;
    }

    if (kDown & HidNpadButton_X) {
        jump_menu_type_page();
        return;
    }

    if (kDown & HidNpadButton_Up) {
        _jumpSelRow = (_jumpSelRow == 0) ? totalRows - 1 : _jumpSelRow - 1;
    } else if (kDown & HidNpadButton_Down) {
        _jumpSelRow = (_jumpSelRow == totalRows - 1) ? 0 : _jumpSelRow + 1;
    }

    if (_jumpSelRow == 0) {
        if (kDown & HidNpadButton_Left)       _jumpTargetPage = std::max(0, _jumpTargetPage - 1);
        else if (kDown & HidNpadButton_Right) _jumpTargetPage = std::min(totalPages - 1, _jumpTargetPage + 1);
        else if (kDown & HidNpadButton_L)     _jumpTargetPage = std::max(0, _jumpTargetPage - 10);
        else if (kDown & HidNpadButton_R)     _jumpTargetPage = std::min(totalPages - 1, _jumpTargetPage + 10);
    }

    // Keep the chapter list scrolled so the current selection stays visible.
    const int visibleChapterRows = 6;
    if (_jumpSelRow >= 1) {
        int chapIdx = _jumpSelRow - 1;
        if (chapIdx < _jumpFirstVisibleChapter) {
            _jumpFirstVisibleChapter = chapIdx;
        } else if (chapIdx >= _jumpFirstVisibleChapter + visibleChapterRows) {
            _jumpFirstVisibleChapter = chapIdx - visibleChapterRows + 1;
        }
    }

    if (kDown & HidNpadButton_A) {
        int cur = layout ? layout->current_page() : 0;
        int target = (_jumpSelRow == 0) ? _jumpTargetPage : _chapters[_jumpSelRow - 1].page;
        target = std::max(0, std::min(totalPages - 1, target));

        if (target > cur)      next_page(target - cur);
        else if (target < cur) previous_page(cur - target);

        char msg[48];
        snprintf(msg, sizeof(msg), "Jumped to page %d", target + 1);
        set_toast(msg);

        _jumpMenuOpen = false;
    }
}

static std::string fit_text_to_width(TTF_Font *font, const std::string &text, int maxW) {
    int w = 0;
    TTF_SizeText(font, text.c_str(), &w, NULL);
    if (w <= maxW) return text;

    std::string out = text;
    while (!out.empty()) {
        out.pop_back();
        std::string test = out + "...";
        TTF_SizeText(font, test.c_str(), &w, NULL);
        if (w <= maxW) return test;
    }
    return "...";
}

void BookReader::jump_menu_type_page() {
    int totalPages = layout ? layout->total_pages() : 1;
    if (totalPages < 1) totalPages = 1;

    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return;

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);   // numeric keypad only

    char header[48];
    snprintf(header, sizeof(header), "Page (1-%d)", totalPages);
    swkbdConfigSetHeaderText(&kbd, header);

    char initial[16];
    snprintf(initial, sizeof(initial), "%d", _jumpTargetPage + 1);
    swkbdConfigSetInitialText(&kbd, initial);

    char result[16] = {0};
    Result rc = swkbdShow(&kbd, result, sizeof(result));
    swkbdClose(&kbd);

    if (R_SUCCEEDED(rc) && result[0] != '\0') {
        int typed = atoi(result);
        if (typed >= 1) {
            _jumpTargetPage = std::min(totalPages, typed) - 1;
            _jumpSelRow = 0; // reflect the typed value on the page-number row
        }
    }
}

void BookReader::draw_jump_menu() {
    SDL_Color textColor = configDarkMode ? WHITE : BLACK;
    SDL_Color subColor  = configDarkMode ? TEXT_MIN_COLOUR_DARK : TEXT_MIN_COLOUR_LIGHT;
    SDL_Color selColor  = configDarkMode ? SDL_MakeColour(66, 66, 66, 255) : SDL_MakeColour(230, 230, 230, 255);

    int boxW = 620, boxH = 480;
    int boxX = (windowX - boxW) / 2;
    int boxY = (windowY - boxH) / 2;

    SDL_DrawRect(RENDERER, 0, 0, windowX, windowY, SDL_MakeColour(0, 0, 0, 140));
    SDL_DrawRect(RENDERER, boxX, boxY, boxW, boxH, configDarkMode ? HINT_COLOUR_DARK : HINT_COLOUR_LIGHT);
    SDL_DrawText(RENDERER, ROBOTO_30, boxX + 24, boxY + 18, textColor, "Jump to page / chapter");

    // Page-number row.
    int rowY = boxY + 70;
    int rowH = 44;
    if (_jumpSelRow == 0) {
        SDL_DrawRect(RENDERER, boxX + 12, rowY - 4, boxW - 24, rowH, selColor);
    }

    int totalPages = layout ? layout->total_pages() : 1;
    if (totalPages < 1) totalPages = 1;
    char pagebuf[48];
    snprintf(pagebuf, sizeof(pagebuf), "Page %d / %d", _jumpTargetPage + 1, totalPages);
    SDL_DrawText(RENDERER, ROBOTO_25, boxX + 24, rowY + 4, textColor, pagebuf);
    SDL_DrawText(RENDERER, ROBOTO_20, boxX + boxW - 300, rowY + 6, subColor, "Left/Right +-1  L/R +-10");

    SDL_DrawRect(RENDERER, boxX + 12, rowY + rowH + 6, boxW - 24, 2, subColor);

    // Chapter list.
    int listY = rowY + rowH + 20;
    int listRowH = 40;
    const int visibleChapterRows = 6;

    if (_chapters.empty()) {
        SDL_DrawText(RENDERER, ROBOTO_20, boxX + 24, listY, subColor, "No table of contents in this book.");
    } else {
        int shown = std::min((int) _chapters.size() - _jumpFirstVisibleChapter, visibleChapterRows);
        for (int i = 0; i < shown; i++) {
            int idx = _jumpFirstVisibleChapter + i;
            const ChapterEntry &c = _chapters[idx];
            int y = listY + i * listRowH;

            if (_jumpSelRow == idx + 1) {
                SDL_DrawRect(RENDERER, boxX + 12, y - 4, boxW - 24, listRowH, selColor);
            }

            std::string indented = std::string(std::min(c.depth, 4) * 2, ' ') + c.title;
            std::string fitted = fit_text_to_width(ROBOTO_20, indented, boxW - 140 - std::min(c.depth, 4) * 16);
            SDL_DrawText(RENDERER, ROBOTO_20, boxX + 24 + std::min(c.depth, 4) * 16, y + 6, textColor, fitted.c_str());

            char pnum[16];
            snprintf(pnum, sizeof(pnum), "p.%d", c.page + 1);
            SDL_DrawText(RENDERER, ROBOTO_20, boxX + boxW - 90, y + 6, subColor, pnum);
        }
    }

    SDL_DrawText(RENDERER, ROBOTO_20, boxX + 24, boxY + boxH - 34, subColor, "A: Jump   B: Cancel   Up/Down: Select   X: Type page #");
}

void BookReader::reset_page() {
    layout->reset();
    show_status_bar();
}

void BookReader::switch_page_layout() {
    BookPageLayout target = (_currentPageLayout == BookPageLayoutPortrait)
                            ? BookPageLayoutLandscape : BookPageLayoutPortrait;

    if (_reflowable) {
        // Re-flow to the dimensions appropriate for the new orientation
        // before rebuilding the layout, since page count/bounds depend on
        // the current reflow parameters. switch_current_page_layout (called
        // just below) does the actual rebuild, so don't rebuild twice here.
        _currentPageLayout = target;
        apply_reflow_settings(false);
    }

    switch_current_page_layout(target, layout ? layout->current_page() : 0);
}

// Draws the help box (background + title + every prompt/touch line) with
// its top-left at (originX, originY). Used both to draw it directly to the
// screen in portrait, and to draw it into an offscreen texture in landscape
// (which is then rotated as a single unit -- see draw() below), so the
// content itself only needs to be written once.
static void draw_help_content(SDL_Renderer *target, int originX, int originY, int helpWidth, int helpHeight) {
    SDL_Color textColor = configDarkMode ? WHITE : BLACK;

    SDL_DrawRect(target, originX, originY, helpWidth, helpHeight, configDarkMode ? HINT_COLOUR_DARK : HINT_COLOUR_LIGHT);

    int textX = originX + 20;
    int textY = originY + 87;
    SDL_DrawText(target, ROBOTO_30, textX, originY + 10, textColor, "Help Menu:");

    SDL_DrawButtonPrompt(target, button_b,               ROBOTO_25, textColor, "Stop reading / Close help menu.", textX, textY,          35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_minus,           ROBOTO_25, textColor, "Switch to dark/light theme.",     textX, textY + 38,     35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, right_stick_up_down,    ROBOTO_25, textColor, "Zoom in/out.",                    textX, textY + 38 * 2, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, left_stick_up_down,     ROBOTO_25, textColor, "Page up/down.",                   textX, textY + 38 * 3, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_y,               ROBOTO_25, textColor, "Rotate / widen page.",            textX, textY + 38 * 4, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_x,               ROBOTO_25, textColor, "Show/hide status bar.",           textX, textY + 38 * 5, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_dpad_left_right, ROBOTO_25, textColor, "Next/previous page.",             textX, textY + 38 * 6, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_dpad_up_down,    ROBOTO_25, textColor, "Bookmark: Up add/remove, Down jump.", textX, textY + 38 * 7, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_a,               ROBOTO_25, textColor, "Change reading font (EPUB).",     textX, textY + 38 * 8, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_a,               ROBOTO_25, textColor, "From this help menu: jump to page/chapter.", textX, textY + 38 * 9, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_lt,              ROBOTO_25, textColor, "Decrease font size (EPUB).",      textX, textY + 38 * 10, 35, 35, 5, 0);
    SDL_DrawButtonPrompt(target, button_rt,              ROBOTO_25, textColor, "Increase font size (EPUB).",      textX, textY + 38 * 11, 35, 35, 5, 0);
    SDL_DrawText(target, ROBOTO_25, textX, textY + 38 * 12, textColor, "Touch: swipe to turn pages, drag to pan when zoomed.");
    SDL_DrawText(target, ROBOTO_25, textX, textY + 38 * 13, textColor, "Touch: two fingers on screen to exit to the menu.");
}

void BookReader::draw(bool drawHelp) {
    if (configDarkMode == true) {
        SDL_ClearScreen(RENDERER, BLACK);
    } else {
        SDL_ClearScreen(RENDERER, WHITE);
    }

    SDL_RenderClear(RENDERER);
    
    layout->draw_page();

    if (_jumpMenuOpen) {
        draw_jump_menu();
    }

    if (drawHelp) { // Help menu
        int helpWidth = 760;
        int helpHeight = 87 + 38 * 13 + 40; // title gap + prompts + touch lines

        if (!configDarkMode) { // Display a dimmed background if on light mode
            SDL_DrawRect(RENDERER, 0, 0, 1280, 720, SDL_MakeColour(50, 50, 50, 150));
        }

        if (_currentPageLayout == BookPageLayoutPortrait) {
            draw_help_content(RENDERER, (windowX - helpWidth) / 2, (windowY - helpHeight) / 2, helpWidth, helpHeight);
        } else {
            // Landscape is rotated 90°, so the whole help box is composed
            // once into an offscreen texture at its natural (portrait)
            // orientation, then rotated as a single unit onto the screen --
            // far simpler and more reliable than rotating each of its many
            // individual text/icon elements by hand.
            SDL_Texture *helpTex = SDL_CreateTexture(RENDERER, SDL_PIXELFORMAT_RGBA8888,
                                                       SDL_TEXTUREACCESS_TARGET, helpWidth, helpHeight);
            if (helpTex) {
                SDL_SetTextureBlendMode(helpTex, SDL_BLENDMODE_BLEND);
                SDL_SetRenderTarget(RENDERER, helpTex);
                SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 0);
                SDL_RenderClear(RENDERER);
                draw_help_content(RENDERER, 0, 0, helpWidth, helpHeight);
                SDL_SetRenderTarget(RENDERER, NULL);

                // SDL_RenderCopyEx rotates around the centre of the rect it's
                // given, not its top-left, so the source rect's x/y have to
                // be offset by half the width/height swap the rotation
                // introduces to land the rotated result centred on screen.
                int finalX = (windowX - helpHeight) / 2;
                int finalY = (windowY - helpWidth) / 2;
                SDL_Rect dst;
                dst.x = finalX + (helpHeight - helpWidth) / 2;
                dst.y = finalY + (helpWidth - helpHeight) / 2;
                dst.w = helpWidth;
                dst.h = helpHeight;
                SDL_RenderCopyEx(RENDERER, helpTex, NULL, &dst, 90.0, NULL, SDL_FLIP_NONE);
                SDL_DestroyTexture(helpTex);
            }
        }
    }

    // A small red ribbon marks a bookmarked page, hanging off the "top" of
    // the reading area in whichever orientation is active. In portrait that's
    // the physical top; in landscape the page is rotated 90°, so the ribbon
    // is drawn rotated too, hanging off the physical right edge instead.
    if (is_current_bookmarked()) {
        SDL_Color ribbon = SDL_MakeColour(214, 69, 69, 255);
        SDL_Color notch_bg = configDarkMode ? BLACK : WHITE;

        if (_currentPageLayout == BookPageLayoutPortrait) {
            int rw = 30, rh = 44, rx = windowX - 72, ry = 0;
            SDL_DrawRect(RENDERER, rx, ry, rw, rh, ribbon);
            // Notch at the bottom (the tab's free end), carved with the page background.
            SDL_DrawCircle(RENDERER, rx + rw / 2, ry + rh, rw / 2 - 1, notch_bg);
        } else {
            int rw = 44, rh = 30, rx = windowX - rw, ry = 28;
            SDL_DrawRect(RENDERER, rx, ry, rw, rh, ribbon);
            // Notch at the left (the tab's free end after rotation).
            SDL_DrawCircle(RENDERER, rx, ry + rh / 2, rh / 2 - 1, notch_bg);
        }
    }

    // The status bar is either always shown or always hidden, per the user's
    // choice (toggled with X or a centre tap). It no longer auto-appears on
    // page turns.
    if (permStatusBar)  {
        char *title = layout->info();
        
        int title_width = 0, title_height = 0;
        TTF_SizeText(ROBOTO_15, title, &title_width, &title_height);
        
        SDL_Color color = configDarkMode ? STATUS_BAR_DARK : STATUS_BAR_LIGHT;
        
        if (_currentPageLayout == BookPageLayoutPortrait) {
            SDL_DrawRect(RENDERER, 0, 0, 1280, 45, SDL_MakeColour(color.r, color.g, color.b , 180));
            SDL_DrawText(RENDERER, ROBOTO_25, (1280 - title_width) / 2, (40 - title_height) / 2, WHITE, title);
            
            StatusBar_DisplayTime(false);
        } else if (_currentPageLayout == BookPageLayoutLandscape) {
            SDL_DrawRect(RENDERER, 1280 - 45, 0, 45, 720, SDL_MakeColour(color.r, color.g, color.b , 180));
            int x = (1280 - title_width) - ((40 - title_height) / 2);
            int y = (720 - title_height) / 2;
            SDL_DrawRotatedText(RENDERER, ROBOTO_25, (double) 90, x, y, WHITE, title);

            StatusBar_DisplayTime(true);
        }
    }
    
    // Transient message (e.g. font changes) shown near the bottom of the screen.
    if (_toast_counter > 0) {
        _toast_counter--;

        int tw = 0, th = 0;
        TTF_SizeText(ROBOTO_30, _toast, &tw, &th);

        int padX = 24, padY = 12;
        SDL_Color bg = configDarkMode ? SDL_MakeColour(70, 70, 70, 220)
                                      : SDL_MakeColour(210, 210, 210, 230);
        SDL_Color fg = configDarkMode ? WHITE : BLACK;

        if (_currentPageLayout == BookPageLayoutPortrait) {
            int boxW = tw + padX * 2;
            int boxH = th + padY * 2;
            int boxX = (windowX - boxW) / 2;
            int boxY = windowY - boxH - 60;

            SDL_DrawRect(RENDERER, boxX, boxY, boxW, boxH, bg);
            SDL_DrawText(RENDERER, ROBOTO_30, boxX + padX, boxY + padY, fg, _toast);
        } else {
            // Landscape is rotated 90°, so the toast is drawn rotated too --
            // otherwise it reads sideways to someone holding the console the
            // way this mode expects. "Bottom, horizontally centred" in
            // portrait becomes "left edge, vertically centred" here, the
            // same mapping already used for the bookmark ribbon.
            int boxW = th + padY * 2;   // swapped: rotated text is narrow...
            int boxH = tw + padX * 2;   // ...and tall on screen
            int boxX = 60;
            int boxY = (windowY - boxH) / 2;

            SDL_DrawRect(RENDERER, boxX, boxY, boxW, boxH, bg);

            // SDL_RenderCopyEx (used inside SDL_DrawRotatedText) rotates
            // around the CENTRE of the rect built from the x/y it's given,
            // not its top-left corner. To land the rotated text's top-left
            // at a chosen (finalX, finalY), the input x/y has to be offset
            // by half the width/height swap the rotation introduces.
            int finalX = boxX + padY;
            int finalY = boxY + padX;
            int x = finalX + (th - tw) / 2;
            int y = finalY + (tw - th) / 2;
            SDL_DrawRotatedText(RENDERER, ROBOTO_30, (double) 90, x, y, fg, _toast);
        }
    }

    SDL_RenderPresent(RENDERER);
}

void BookReader::show_status_bar() {
    status_bar_visible_counter = 200;
}

void BookReader::switch_current_page_layout(BookPageLayout bookPageLayout, int current_page) {
    if (layout) {
        current_page = layout->current_page();
        delete layout;
        layout = NULL;
    }
    
    _currentPageLayout = bookPageLayout;
    
    switch (bookPageLayout) {
        case BookPageLayoutPortrait:
            layout = new PageLayout(doc, current_page);
            break;
        case BookPageLayoutLandscape:
            // Genuine 90° rotation for every format, matching the physical
            // "hold the console like a book" landscape mode. Reflowable
            // books are laid out at book-shaped dimensions beforehand (see
            // apply_reflow_settings) so this rotates and fits the same way
            // a portrait PDF page would.
            layout = new LandscapePageLayout(doc, current_page);
            break;
    }
}

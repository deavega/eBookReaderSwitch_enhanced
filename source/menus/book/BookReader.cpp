#include "BookReader.hpp"
#include "PageLayout.hpp"
#include "LandscapePageLayout.hpp"
#include <switch.h>
#include <algorithm>
#include <iostream>
#include <libconfig.h>
#include <cstring>
#include <cstdio>
#include <vector>

static const char *FONT_NAMES[] = { "Serif", "Sans-serif", "Monospace" };
static const int   FONT_COUNT   = 3;

static const char *FONT_CANDIDATES[3][8] = {
    {
        "/switch/eBookReader/fonts/Serif.ttf",
        "/switch/eBookReader/fonts/serif.ttf",
        "/switch/eBookReader/Serif.ttf",
        "romfs:/resources/font/Serif.ttf",
        "romfs:/resources/font/serif.ttf",
        "romfs:/resources/fonts/Serif.ttf",
        "romfs:/font/Serif.ttf",
        "romfs:/Serif.ttf"
    },
    {
        "/switch/eBookReader/fonts/Sans.ttf",
        "/switch/eBookReader/fonts/sans.ttf",
        "/switch/eBookReader/Sans.ttf",
        "romfs:/resources/font/Sans.ttf",
        "romfs:/resources/font/sans.ttf",
        "romfs:/resources/fonts/Sans.ttf",
        "romfs:/font/Sans.ttf",
        "romfs:/Sans.ttf"
    },
    {
        "/switch/eBookReader/fonts/Mono.ttf",
        "/switch/eBookReader/fonts/mono.ttf",
        "/switch/eBookReader/Mono.ttf",
        "romfs:/resources/font/Mono.ttf",
        "romfs:/resources/font/mono.ttf",
        "romfs:/resources/fonts/Mono.ttf",
        "romfs:/font/Mono.ttf",
        "romfs:/Mono.ttf"
    }
};

static std::vector<unsigned char> g_font_data[3];
static int g_active_font_index = 0;
static bool g_romfs_ready = false;
static std::string g_current_doc_path = "";

extern "C" {
    #include "SDL_helper.h"
    #include "status_bar.h"
    #include "config.h"
    #include "textures.h"
    #include "common.h"
}

fz_context *ctx = NULL;
int windowX, windowY;
config_t *config = NULL;
char* configFile = (char*)"/switch/eBookReader/saved_pages.cfg";

static fz_font *cb_load_system_font(fz_context *c, const char *name, int bold, int italic, int needs_exact) {
    if (g_active_font_index >= 0 && g_active_font_index < FONT_COUNT) {
        if (!g_font_data[g_active_font_index].empty()) {
            return fz_new_font_from_memory(c, name, g_font_data[g_active_font_index].data(), g_font_data[g_active_font_index].size(), 0, 0);
        }
    }
    return NULL;
}

static fz_font *cb_load_system_fallback_font(fz_context *c, int script, int language, int serif, int bold, int italic) {
    if (g_active_font_index >= 0 && g_active_font_index < FONT_COUNT) {
        if (!g_font_data[g_active_font_index].empty()) {
            return fz_new_font_from_memory(c, "DefaultCustomFont", g_font_data[g_active_font_index].data(), g_font_data[g_active_font_index].size(), 0, 0);
        }
    }
    return NULL;
}

static void ensure_fonts_loaded() {
    if (!g_romfs_ready) {
        if (R_SUCCEEDED(romfsInit())) {
            g_romfs_ready = true;
        }
    }

    for (int i = 0; i < FONT_COUNT; i++) {
        if (!g_font_data[i].empty()) continue;
        for (int p = 0; p < 8; p++) {
            const char *path = FONT_CANDIDATES[i][p];
            FILE *f = fopen(path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz > 0) {
                    g_font_data[i].resize(sz);
                    fread(g_font_data[i].data(), 1, sz, f);
                    fclose(f);
                    break;
                }
                fclose(f);
            }
        }
    }
}

static int load_last_page(const char *book_name) {
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
    g_current_doc_path = std::string(path);
    ensure_fonts_loaded();
    g_active_font_index = _font_index;

    if (ctx == NULL) {
        ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
        fz_register_document_handlers(ctx);
    }

    fz_install_load_system_font_funcs(ctx, cb_load_system_font, NULL, cb_load_system_fallback_font);

    SDL_GetWindowSize(WINDOW, &windowX, &windowY);
    
    book_name = std::string(path).substr(std::string(path).find_last_of("/\\") + 1);
    std::string invalid_chars = " :/?#[]@!$&'()*+,;=.";
    for (char& c: invalid_chars) {
        book_name.erase(std::remove(book_name.begin(), book_name.end(), c), book_name.end());
    }
    
    fz_try(ctx) {
        doc = fz_open_document(ctx, path);
        if (!doc) {
            *result = -1;
            return;
        }

        _reflowable = fz_is_document_reflowable(ctx, doc);
        if (_reflowable) {
            apply_reflow_settings(false);
        }

        int current_page = load_last_page(book_name.c_str());
        load_bookmarks();
        switch_current_page_layout(_currentPageLayout, current_page);

        if (current_page > 0) {
            show_status_bar();
        }
    }
    fz_catch(ctx) {
        *result = -2;
        return;
    }
}

BookReader::~BookReader() {
    if (doc && ctx) {
        fz_drop_document(ctx, doc);
    }
    delete layout;
}

void BookReader::previous_page(int n) {
    if (!layout) return;
    layout->previous_page(n);
    show_status_bar();
    save_last_page(book_name.c_str(), layout->current_page());
}

void BookReader::next_page(int n) {
    if (!layout) return;
    layout->next_page(n);
    show_status_bar();
    save_last_page(book_name.c_str(), layout->current_page());
}

void BookReader::zoom_in() {
    if (!layout) return;
    layout->zoom_in();
    show_status_bar();
}

void BookReader::zoom_out() {
    if (!layout) return;
    layout->zoom_out();
    show_status_bar();
}

void BookReader::move_page_up() {
    if (layout) layout->move_up();
}

void BookReader::move_page_down() {
    if (layout) layout->move_down();
}

void BookReader::move_page_left() {
    if (layout) layout->move_left();
}

void BookReader::move_page_right() {
    if (layout) layout->move_right();
}

void BookReader::pan_by(float dx, float dy) {
    if (layout) layout->pan(dx, dy);
}

bool BookReader::is_zoomed() {
    return layout ? layout->is_zoomed() : false;
}

void BookReader::apply_reflow_settings(bool rebuild) {
    if (!_reflowable || !doc || !ctx) return;

    g_active_font_index = _font_index;

    // Dimensi kanvas reflow dengan padding atas dan bawah:
    // Landscape (rotasi 90°): 700 width, 1120 height -> Padding atas/bawah ~80px, samping ~10px
    // Portrait: 680 width, 590 height -> Padding atas/bawah ~65px, samping ~20px
    float page_w = (_currentPageLayout == BookPageLayoutLandscape) ? 700.0f : 680.0f;
    float page_h = (_currentPageLayout == BookPageLayoutLandscape) ? 1120.0f : 590.0f;

    char css[512];
    snprintf(css, sizeof(css),
             "@page { margin: 0 !important; }\n"
             "html, body { margin: 0 !important; padding: 0 !important; width: 100%% !important; }\n"
             "* { font-family: '%s', sans-serif !important; }\n"
             "p, div, span, blockquote, article { margin: 0.35em 0 !important; padding: 0 !important; line-height: 1.4 !important; }\n",
             FONT_NAMES[_font_index]);

    fz_set_use_document_css(ctx, 0);
    fz_set_user_css(ctx, css);

    fz_try(ctx) {
        fz_layout_document(ctx, doc, page_w, page_h, _em);
    }
    fz_catch(ctx) {
        return;
    }

    if (rebuild) {
        int target_page = layout ? layout->current_page() : 0;
        switch_current_page_layout(_currentPageLayout, target_page);
        show_status_bar();
    }
}

void BookReader::cycle_font() {
    if (!_reflowable) {
        set_toast("Fonts apply to EPUB / text books only");
        return;
    }

    ensure_fonts_loaded();

    int cur_page = layout ? layout->current_page() : 0;

    _font_index = (_font_index + 1) % FONT_COUNT;
    g_active_font_index = _font_index;

    // Reset konteks MuPDF untuk membersihkan cache font
    if (layout) {
        delete layout;
        layout = NULL;
    }
    if (doc && ctx) {
        fz_drop_document(ctx, doc);
        doc = NULL;
    }
    if (ctx) {
        fz_drop_context(ctx);
        ctx = NULL;
    }

    ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    fz_install_load_system_font_funcs(ctx, cb_load_system_font, NULL, cb_load_system_fallback_font);

    fz_try(ctx) {
        doc = fz_open_document(ctx, g_current_doc_path.c_str());
        if (doc) {
            apply_reflow_settings(false);
            switch_current_page_layout(_currentPageLayout, cur_page);
        }
    }
    fz_catch(ctx) {
        std::cout << "Error reloading document on font cycle" << std::endl;
    }

    char msg[64];
    if (!g_font_data[_font_index].empty()) {
        snprintf(msg, sizeof(msg), "Font: %s", FONT_NAMES[_font_index]);
    } else {
        snprintf(msg, sizeof(msg), "Font: %s (No .ttf in fonts folder)", FONT_NAMES[_font_index]);
    }
    set_toast(msg);
}

void BookReader::increase_font_size() {
    if (!_reflowable) {
        set_toast("Font size applies to EPUB / text books only");
        return;
    }

    if (_em < 42.0f) _em += 1.5f;
    apply_reflow_settings(true);

    char msg[64];
    snprintf(msg, sizeof(msg), "Font size: %.1f", _em);
    set_toast(msg);
}

void BookReader::decrease_font_size() {
    if (!_reflowable) {
        set_toast("Font size applies to EPUB / text books only");
        return;
    }

    if (_em > 16.0f) _em -= 1.5f;
    apply_reflow_settings(true);

    char msg[64];
    snprintf(msg, sizeof(msg), "Font size: %.1f", _em);
    set_toast(msg);
}

void BookReader::set_toast(const char *msg) {
    strncpy(_toast, msg, sizeof(_toast) - 1);
    _toast[sizeof(_toast) - 1] = '\0';
    _toast_counter = 120;
}

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
    auto it = _bookmarks.upper_bound(cur);
    int target = (it != _bookmarks.end()) ? *it : *_bookmarks.begin();

    if (target > cur)      next_page(target - cur);
    else if (target < cur) previous_page(cur - target);

    char msg[48];
    snprintf(msg, sizeof(msg), "Jumped to page %d", target + 1);
    set_toast(msg);
}

void BookReader::reset_page() {
    if (layout) layout->reset();
    show_status_bar();
}

void BookReader::switch_page_layout() {
    BookPageLayout target = (_currentPageLayout == BookPageLayoutPortrait)
                            ? BookPageLayoutLandscape : BookPageLayoutPortrait;

    _currentPageLayout = target;

    if (_reflowable) {
        apply_reflow_settings(true);
    } else {
        int current_page = layout ? layout->current_page() : 0;
        switch_current_page_layout(target, current_page);
    }
    set_toast(target == BookPageLayoutLandscape ? "Landscape Mode (90°)" : "Portrait Mode");
}

void BookReader::draw(bool drawHelp) {
    if (configDarkMode) {
        SDL_ClearScreen(RENDERER, BLACK);
    } else {
        SDL_ClearScreen(RENDERER, WHITE);
    }

    SDL_RenderClear(RENDERER);
    
    if (layout) {
        layout->draw_page();
    }
    
    if (drawHelp) {
        int helpWidth = 760;
        int helpHeight = 87 + 38 * 12 + 40;

        if (!configDarkMode) {
            SDL_DrawRect(RENDERER, 0, 0, 1280, 720, SDL_MakeColour(50, 50, 50, 150));
        }

        SDL_DrawRect(RENDERER, (windowX - helpWidth) / 2, (windowY - helpHeight) / 2, helpWidth, helpHeight, configDarkMode ? HINT_COLOUR_DARK : HINT_COLOUR_LIGHT);

        int textX = (windowX - helpWidth) / 2 + 20;
        int textY = (windowY - helpHeight) / 2 + 87;
        SDL_Color textColor = configDarkMode ? WHITE : BLACK;
        SDL_DrawText(RENDERER, ROBOTO_30, textX, (windowY - helpHeight) / 2 + 10, textColor, "Help Menu:");

        SDL_DrawButtonPrompt(RENDERER, button_b,               ROBOTO_25, textColor, "Stop reading / Close help menu.", textX, textY,          35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_minus,           ROBOTO_25, textColor, "Switch to dark/light theme.",     textX, textY + 38,     35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, right_stick_up_down,    ROBOTO_25, textColor, "Zoom in/out.",                    textX, textY + 38 * 2, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, left_stick_up_down,     ROBOTO_25, textColor, "Page up/down.",                   textX, textY + 38 * 3, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_y,               ROBOTO_25, textColor, "Rotate 90° (Landscape).",         textX, textY + 38 * 4, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_x,               ROBOTO_25, textColor, "Show/hide status bar.",           textX, textY + 38 * 5, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_dpad_left_right, ROBOTO_25, textColor, "Next/previous page.",             textX, textY + 38 * 6, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_dpad_up_down,    ROBOTO_25, textColor, "Bookmark: Up add/remove, Down jump.", textX, textY + 38 * 7, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_a,               ROBOTO_25, textColor, "Change reading font (EPUB).",     textX, textY + 38 * 8, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_lt,              ROBOTO_25, textColor, "Decrease font size (EPUB).",      textX, textY + 38 * 9, 35, 35, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_rt,              ROBOTO_25, textColor, "Increase font size (EPUB).",      textX, textY + 38 * 10, 35, 35, 5, 0);
        SDL_DrawText(RENDERER, ROBOTO_25, textX, textY + 38 * 11, textColor, "Touch: Swipe to flip pages, Drag to pan.");
        SDL_DrawText(RENDERER, ROBOTO_25, textX, textY + 38 * 12, textColor, "Touch: Tap Corners or 2-Fingers to Exit.");
    }

    if (is_current_bookmarked()) {
        int rw = 30, rh = 44, rx = windowX - 72, ry = 0;
        SDL_DrawRect(RENDERER, rx, ry, rw, rh, SDL_MakeColour(214, 69, 69, 255));
        SDL_DrawCircle(RENDERER, rx + rw / 2, ry + rh, rw / 2 - 1, configDarkMode ? BLACK : WHITE);
    }

    if (permStatusBar && layout) {
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
    
    if (_toast_counter > 0) {
        _toast_counter--;

        int tw = 0, th = 0;
        TTF_SizeText(ROBOTO_30, _toast, &tw, &th);

        int padX = 24, padY = 12;
        int boxW = tw + padX * 2;
        int boxH = th + padY * 2;
        int boxX = (windowX - boxW) / 2;
        int boxY = windowY - boxH - 60;

        SDL_Color bg = configDarkMode ? SDL_MakeColour(70, 70, 70, 220)
                                      : SDL_MakeColour(210, 210, 210, 230);
        SDL_Color fg = configDarkMode ? WHITE : BLACK;

        SDL_DrawRect(RENDERER, boxX, boxY, boxW, boxH, bg);
        SDL_DrawText(RENDERER, ROBOTO_30, boxX + padX, boxY + padY, fg, _toast);
    }

    SDL_RenderPresent(RENDERER);
}

void BookReader::show_status_bar() {
    status_bar_visible_counter = 200;
}

void BookReader::switch_current_page_layout(BookPageLayout bookPageLayout, int current_page) {
    if (layout) {
        delete layout;
        layout = NULL;
    }
    
    _currentPageLayout = bookPageLayout;
    
    if (bookPageLayout == BookPageLayoutPortrait) {
        layout = new PageLayout(doc, current_page);
    } else {
        layout = new LandscapePageLayout(doc, current_page);
    }
}
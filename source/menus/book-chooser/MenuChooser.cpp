extern "C" {
    #include "MenuChooser.h"
    #include "menu_book_reader.h"
    #include "SDL_helper.h"
    #include "common.h"
    #include "textures.h"
    #include "config.h"
}

#include <switch.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <utility>
#include <cstdio>
#include <cstdint>
#include <cctype>

#include "CoverArt.hpp"
#include "../book/TouchGesture.hpp"

#include <SDL2/SDL_image.h>

using namespace std;
namespace fs = filesystem;

extern TTF_Font *ROBOTO_35, *ROBOTO_30, *ROBOTO_27, *ROBOTO_25, *ROBOTO_20, *ROBOTO_15;

// ---------------------------------------------------------------------------
// A book entry in the library list.
// ---------------------------------------------------------------------------
struct BookEntry {
    string filename;   // e.g. "My Book.epub"
    string title;      // filename without extension
    string ext;        // lowercase extension incl. dot, e.g. ".epub"
    string sizeStr;    // human-readable size, e.g. "2.3 MB"
    bool   warned;     // experimental format (epub/cbz/xps)

    // Real cover art, loaded lazily the first time the row is drawn. NULL
    // (with coverAttempted=true) means no real cover was available and the
    // generated placeholder card should be used instead -- attempted only
    // once per menu session so a bad file doesn't retry every frame.
    SDL_Texture *cover = nullptr;
    bool coverAttempted = false;
};

static string to_lower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char) tolower(c); });
    return s;
}

static string human_size(uintmax_t b) {
    char buf[32];
    double x = (double) b;
    if (x < 1024.0)                snprintf(buf, sizeof(buf), "%llu B",  (unsigned long long) b);
    else if (x < 1024.0 * 1024.0)  snprintf(buf, sizeof(buf), "%.0f KB", x / 1024.0);
    else                           snprintf(buf, sizeof(buf), "%.1f MB", x / (1024.0 * 1024.0));
    return string(buf);
}

// A stable pleasant colour derived from the title, used for the generated cover.
static SDL_Color cover_colour(const string &s) {
    static const SDL_Color palette[] = {
        {198, 40,  40,  255}, { 21, 101, 192, 255}, { 46, 125, 50,  255}, {106, 27,  154, 255},
        {230, 81,  0,   255}, {  0, 131, 143, 255}, { 40, 53,  147, 255}, {173, 20,  87,  255}
    };
    unsigned h = 2166136261u;
    for (char c : s) { h = (h ^ (unsigned char) c) * 16777619u; }
    return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}

static SDL_Color shade(SDL_Color c, float f) {
    return SDL_MakeColour((Uint8)(c.r * f), (Uint8)(c.g * f), (Uint8)(c.b * f), c.a);
}

// Truncate text with an ellipsis so it fits within maxW pixels.
static string fit_text(TTF_Font *font, const string &text, int maxW) {
    int w = 0;
    TTF_SizeText(font, text.c_str(), &w, NULL);
    if (w <= maxW) return text;

    string out = text;
    while (!out.empty()) {
        out.pop_back();
        string test = out + "...";
        TTF_SizeText(font, test.c_str(), &w, NULL);
        if (w <= maxW) return test;
    }
    return "...";
}

static void draw_centered(TTF_Font *font, const string &text, int cx, int cy, SDL_Color colour) {
    int w = 0, h = 0;
    TTF_SizeText(font, text.c_str(), &w, &h);
    SDL_DrawText(RENDERER, font, cx - w / 2, cy - h / 2, colour, text.c_str());
}

// Draws the book's real cover art, cropped to fill the box (like a typical
// library grid) so thumbnails stay visually uniform regardless of the
// source image's own aspect ratio.
static void draw_real_cover(int x, int y, int w, int h, SDL_Texture *tex) {
    int texW = 0, texH = 0;
    SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);
    if (texW <= 0 || texH <= 0) return;

    float scale = std::max((float) w / texW, (float) h / texH);
    int srcW = (int) (w / scale);
    int srcH = (int) (h / scale);
    SDL_Rect src = { (texW - srcW) / 2, (texH - srcH) / 2, srcW, srcH };
    SDL_Rect dst = { x, y, w, h };
    SDL_RenderCopy(RENDERER, tex, &src, &dst);
}

// Draw a generated "cover": coloured card, darker spine, big initial, format badge.
static void draw_cover(int x, int y, int w, int h, const BookEntry &b) {
    SDL_Color base = cover_colour(b.title);

    SDL_DrawRect(RENDERER, x, y, w, h, base);              // cover body
    SDL_DrawRect(RENDERER, x, y, 7, h, shade(base, 0.70f)); // spine

    // Big initial (first alphabetic char of the title, else the format letter).
    char initial = 0;
    for (char c : b.title) { if (isalpha((unsigned char) c)) { initial = (char) toupper((unsigned char) c); break; } }
    if (!initial) initial = (char) toupper((unsigned char) (b.ext.size() > 1 ? b.ext[1] : '?'));
    char is[2] = { initial, 0 };
    draw_centered(ROBOTO_35, is, x + w / 2, y + h / 2 - 6, WHITE);

    // Format badge along the bottom of the cover.
    string label = "";
    for (size_t i = 1; i < b.ext.size(); i++) label += (char) toupper((unsigned char) b.ext[i]);
    SDL_Color badge = b.warned ? SDL_MakeColour(255, 179, 0, 255) : shade(base, 0.55f);
    int badgeH = 26;
    SDL_DrawRect(RENDERER, x, y + h - badgeH, w, badgeH, badge);
    draw_centered(ROBOTO_15, label, x + w / 2, y + h - badgeH / 2, b.warned ? BLACK : WHITE);
}

void Menu_StartChoosing() {
    string path = "/switch/eBookReader/books";
    list<string> warnedExtentions = {".epub", ".cbz", ".xps"};
    list<string> allowedExtentions = {".pdf", ".epub", ".cbz", ".xps"};

    // ---- Build the library list once, safely (no C++ exceptions). ------------
    vector<BookEntry> books;
    {
        std::error_code ec;
        fs::directory_iterator it(path, ec), end;
        if (ec) {
            std::cout << "Menu_StartChoosing: cannot read books dir: " << ec.message() << std::endl;
        }
        for (; !ec && it != end; it.increment(ec)) {
            const fs::directory_entry &entry = *it;

            std::error_code fec;
            if (!entry.is_regular_file(fec) || fec) continue;

            string filename = entry.path().filename().string();
            size_t dot = filename.find_last_of('.');
            if (dot == string::npos) continue;                 // no extension -> skip

            string ext = to_lower(filename.substr(dot));
            if (find(allowedExtentions.begin(), allowedExtentions.end(), ext) == allowedExtentions.end())
                continue;                                       // unsupported -> skip

            BookEntry b;
            b.filename = filename;
            b.title    = filename.substr(0, dot);
            b.ext      = ext;
            b.warned   = find(warnedExtentions.begin(), warnedExtentions.end(), ext) != warnedExtentions.end();
            uintmax_t sz = entry.file_size(fec);
            if (fec) sz = 0;
            b.sizeStr  = human_size(sz);
            books.push_back(std::move(b));
        }
    }

    sort(books.begin(), books.end(), [](const BookEntry &a, const BookEntry &b){
        return to_lower(a.title) < to_lower(b.title);
    });

    int choosenIndex = 0;
    int firstVisible = 0;
    bool readingBook = false;

    int windowX = 0, windowY = 0;
    SDL_GetWindowSize(WINDOW, &windowX, &windowY);

    // Layout metrics.
    const int headerH   = 72;
    const int footerH   = 52;
    const int listTop   = headerH + 20;
    const int rowH      = 132;
    const int cardX     = 32;
    const int cardW     = windowX - 64 - 12;    // leave room for a scrollbar
    const int coverW    = 78;
    const int coverH    = 104;
    const int visible   = (windowY - listTop - footerH) / rowH;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    hidInitializeTouchScreen();
    TouchGesture gesture;

    while (appletMainLoop()) {
        if (readingBook) break;

        SDL_Color textColor = configDarkMode ? WHITE : BLACK;
        SDL_Color subColor  = configDarkMode ? TEXT_MIN_COLOUR_DARK : TEXT_MIN_COLOUR_LIGHT;
        SDL_Color backColor = configDarkMode ? BACK_BLACK : BACK_WHITE;
        SDL_Color cardColor = configDarkMode ? SDL_MakeColour(45, 45, 45, 255)  : SDL_MakeColour(224, 224, 224, 255);
        SDL_Color selColor  = configDarkMode ? SDL_MakeColour(66, 66, 66, 255)  : SDL_MakeColour(243, 243, 243, 255);
        SDL_Color headColor = configDarkMode ? STATUS_BAR_DARK : STATUS_BAR_LIGHT;

        SDL_ClearScreen(RENDERER, backColor);
        SDL_RenderClear(RENDERER);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        HidTouchScreenState state = {0};
        hidGetTouchScreenStates(&state, 1);
        gesture.update(state);

        bool openRequested = false;

        TouchGestureResult g;
        if (!books.empty() && gesture.consume_gesture(g)) {
            if (g.type == TouchGestureTap) {
                // A tap on a visible book row selects and opens it directly --
                // touch users don't have a separate "confirm" step available.
                int shown = min(visible, (int) books.size() - firstVisible);
                if (g.x >= cardX && g.x <= cardX + cardW && g.y >= listTop) {
                    int row = (g.y - listTop) / rowH;
                    if (row >= 0 && row < shown) {
                        choosenIndex = firstVisible + row;
                        openRequested = true;
                    }
                }
            } else if (visible > 0) {
                // Swipe to scroll roughly a page at a time.
                int step = max(1, visible - 1);
                int maxFirst = max(0, (int) books.size() - visible);
                if (g.type == TouchGestureSwipeUp)
                    firstVisible = min(maxFirst, firstVisible + step);
                else if (g.type == TouchGestureSwipeDown)
                    firstVisible = max(0, firstVisible - step);
            }
        }

        // ---- Input --------------------------------------------------------
        if (kDown & HidNpadButton_Plus) break;
        if (kDown & HidNpadButton_B) break;

        if (!books.empty() && (kDown & HidNpadButton_A)) {
            openRequested = true;
        }

        if (openRequested) {
            BookEntry &b = books[choosenIndex];
            string book = path + "/" + b.filename;
            std::cout << "Opening book: " << book << std::endl;
            Menu_OpenBook((char*) book.c_str());
            readingBook = true;
            break;
        }

        if (!books.empty() &&
            (kDown & HidNpadButton_Up || kDown & HidNpadButton_StickRUp || kDown & HidNpadButton_StickLUp)) {
            choosenIndex = (choosenIndex == 0) ? (int) books.size() - 1 : choosenIndex - 1;
        }
        if (!books.empty() &&
            (kDown & HidNpadButton_Down || kDown & HidNpadButton_StickRDown || kDown & HidNpadButton_StickLDown)) {
            choosenIndex = (choosenIndex == (int) books.size() - 1) ? 0 : choosenIndex + 1;
        }

        if (kDown & HidNpadButton_Minus) configDarkMode = !configDarkMode;

        // Keep the selected row within the visible window.
        if (visible > 0) {
            if (choosenIndex < firstVisible) firstVisible = choosenIndex;
            else if (choosenIndex >= firstVisible + visible) firstVisible = choosenIndex - visible + 1;
        }

        // ---- Header -------------------------------------------------------
        SDL_DrawRect(RENDERER, 0, 0, windowX, headerH, headColor);
        SDL_DrawText(RENDERER, ROBOTO_35, 40, (headerH - 42) / 2, WHITE, "eBook Reader");
        {
            char count[48];
            snprintf(count, sizeof(count), "%d book%s", (int) books.size(), books.size() == 1 ? "" : "s");
            int cw = 0; TTF_SizeText(ROBOTO_25, count, &cw, NULL);
            SDL_DrawText(RENDERER, ROBOTO_25, windowX - cw - 40, (headerH - 30) / 2, WHITE, count);
        }

        // ---- Book list (or empty state) -----------------------------------
        if (books.empty()) {
            draw_centered(ROBOTO_30, "No books found", windowX / 2, windowY / 2 - 40, textColor);
            draw_centered(ROBOTO_20, "Put .pdf, .epub, .cbz or .xps files in:", windowX / 2, windowY / 2 + 4, subColor);
            draw_centered(ROBOTO_20, "/switch/eBookReader/books", windowX / 2, windowY / 2 + 34, subColor);
        } else {
            int shown = min(visible, (int) books.size() - firstVisible);
            for (int i = 0; i < shown; i++) {
                int idx  = firstVisible + i;
                BookEntry &b = books[idx];
                int rowY = listTop + i * rowH;
                bool selected = (idx == choosenIndex);

                SDL_DrawRect(RENDERER, cardX, rowY + 6, cardW, rowH - 12, selected ? selColor : cardColor);
                if (selected)
                    SDL_DrawRect(RENDERER, cardX, rowY + 6, 6, rowH - 12, cover_colour(b.title));

                int coverX = cardX + 22;
                int coverY = rowY + (rowH - coverH) / 2;

                if (!b.coverAttempted) {
                    b.coverAttempted = true;
                    b.cover = CoverArt_Get(RENDERER, path + "/" + b.filename, b.filename);
                }
                if (b.cover) {
                    draw_real_cover(coverX, coverY, coverW, coverH, b.cover);
                } else {
                    draw_cover(coverX, coverY, coverW, coverH, b);
                }

                int tx = coverX + coverW + 28;
                int maxTextW = cardX + cardW - tx - 30;
                SDL_DrawText(RENDERER, ROBOTO_35, tx, rowY + 34, textColor,
                             fit_text(ROBOTO_35, b.title, maxTextW).c_str());

                char sub[96];
                string fmt;
                for (size_t k = 1; k < b.ext.size(); k++) fmt += (char) toupper((unsigned char) b.ext[k]);
                snprintf(sub, sizeof(sub), "%s   %s%s", fmt.c_str(), b.sizeStr.c_str(),
                         b.warned ? "   experimental" : "");
                SDL_DrawText(RENDERER, ROBOTO_20, tx, rowY + 82, subColor, sub);
            }

            // Scrollbar.
            if ((int) books.size() > visible && visible > 0) {
                int trackX = windowX - 10, trackY = listTop, trackH = visible * rowH;
                SDL_DrawRect(RENDERER, trackX, trackY, 4, trackH, cardColor);
                int thumbH = max(30, trackH * visible / (int) books.size());
                int thumbY = trackY + (trackH - thumbH) * firstVisible / max(1, (int) books.size() - visible);
                SDL_DrawRect(RENDERER, trackX, thumbY, 4, thumbH, headColor);
            }
        }

        // ---- Footer hints -------------------------------------------------
        int fy = windowY - footerH + 8;
        SDL_DrawButtonPrompt(RENDERER, button_a,     ROBOTO_20, textColor, "Open",   40,  fy, 32, 32, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_b,     ROBOTO_20, textColor, "Exit",   160, fy, 32, 32, 5, 0);
        SDL_DrawButtonPrompt(RENDERER, button_minus, ROBOTO_20, textColor, "Theme",  290, fy, 32, 32, 5, 0);

        SDL_RenderPresent(RENDERER);
    }

    // Free any real cover textures loaded this session -- Menu_StartChoosing
    // runs again every time the reader closes, so these would otherwise pile
    // up in video memory over a long session.
    for (BookEntry &b : books) {
        if (b.cover) SDL_DestroyTexture(b.cover);
    }
}

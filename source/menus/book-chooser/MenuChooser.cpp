#include <switch.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <mupdf/pdf.h>

extern "C" {
    #include "MenuChooser.h"
    #include "menu_book_reader.h"
    #include "common.h"
    #include "config.h"
    #include "SDL_helper.h"
    #include "status_bar.h"
}

extern fz_context *ctx;

static std::vector<std::string> fileList;
static int selectedIndex = 0;
static const char* BOOKS_DIR = "/switch/eBookReader/books";

static SDL_Texture *coverTexture = NULL;
static int lastCoverIndex = -1;
static int coverTexW = 0, coverTexH = 0;

// Deterministic pastel color palette for book placeholder cards
static const SDL_Color CARD_PALETTE[] = {
    { 66, 133, 244, 255 },  // Blue
    { 234, 67, 53, 255 },   // Red
    { 251, 188, 5, 255 },   // Yellow
    { 52, 168, 83, 255 },   // Green
    { 142, 68, 173, 255 },  // Purple
    { 230, 126, 34, 255 },  // Orange
    { 26, 188, 156, 255 },  // Teal
    { 44, 62, 80, 255 }     // Dark Slate
};
static const int PALETTE_COUNT = 8;

static bool hasSupportedExtension(const std::string& filename) {
    size_t dotPos = filename.find_last_of(".");
    if (dotPos == std::string::npos) return false;

    std::string ext = filename.substr(dotPos);
    for (char &c : ext) c = (char)std::tolower((unsigned char)c);

    return (ext == ".epub" || ext == ".pdf" || ext == ".cbz" || 
            ext == ".xps"  || ext == ".fb2" || ext == ".txt");
}

static void FreeCover() {
    if (coverTexture) {
        SDL_DestroyTexture(coverTexture);
        coverTexture = NULL;
    }
    lastCoverIndex = -1;
}

static void LoadCoverForIndex(int idx) {
    if (idx == lastCoverIndex) return;
    FreeCover();
    lastCoverIndex = idx;

    if (idx < 0 || idx >= (int)fileList.size()) return;

    if (ctx == NULL) {
        ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
        fz_register_document_handlers(ctx);
    }

    std::string fullPath = std::string(BOOKS_DIR) + "/" + fileList[idx];

    fz_document *doc = NULL;
    fz_try(ctx) {
        doc = fz_open_document(ctx, fullPath.c_str());
        if (!doc) return;

        if (fz_count_pages(ctx, doc) <= 0) {
            fz_drop_document(ctx, doc);
            return;
        }

        fz_page *page = fz_load_page(ctx, doc, 0);
        if (!page) {
            fz_drop_document(ctx, doc);
            return;
        }

        fz_rect bounds = fz_bound_page(ctx, page);
        float bw = bounds.x1 - bounds.x0;
        float bh = bounds.y1 - bounds.y0;
        if (bw <= 0 || bh <= 0) {
            fz_drop_page(ctx, page);
            fz_drop_document(ctx, doc);
            return;
        }

        float scale = fmin(380.0f / bw, 500.0f / bh);
        fz_matrix ctm = fz_scale(scale, scale);

        fz_pixmap *pix = fz_new_pixmap_from_page_contents(ctx, page, ctm, fz_device_rgb(ctx), 0);
        if (pix) {
            SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
                pix->samples, pix->w, pix->h, pix->n * 8, pix->w * pix->n,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0
            );
            if (surf) {
                coverTexture = SDL_CreateTextureFromSurface(RENDERER, surf);
                coverTexW = pix->w;
                coverTexH = pix->h;
                SDL_FreeSurface(surf);
            }
            fz_drop_pixmap(ctx, pix);
        }

        fz_drop_page(ctx, page);
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        if (doc) fz_drop_document(ctx, doc);
        FreeCover();
    }
}

static void ScanBooks() {
    fileList.clear();
    DIR *d = opendir(BOOKS_DIR);
    if (!d) return;

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        std::string name = dir->d_name;
        if (name == "." || name == ".." || name[0] == '.') continue;
        if (hasSupportedExtension(name)) {
            fileList.push_back(name);
        }
    }
    closedir(d);
    std::sort(fileList.begin(), fileList.end());
}

static std::string TruncateText(TTF_Font *font, const std::string &text, int maxWidth) {
    int w = 0, h = 0;
    TTF_SizeText(font, text.c_str(), &w, &h);
    if (w <= maxWidth) return text;

    std::string truncated = text;
    while (!truncated.empty()) {
        truncated.pop_back();
        std::string candidate = truncated + "...";
        TTF_SizeText(font, candidate.c_str(), &w, &h);
        if (w <= maxWidth) {
            return candidate;
        }
    }
    return "...";
}

// Extracts 1 or 2 capital initials from a title
static std::string GetInitials(const std::string &name) {
    std::string inits = "";
    bool next = true;
    for (char c : name) {
        if (c == '.' || c == '_' || c == '-') break;
        if (std::isalpha((unsigned char)c)) {
            if (next) {
                inits += (char)std::toupper((unsigned char)c);
                next = false;
                if (inits.length() >= 2) break;
            }
        } else if (c == ' ') {
            next = true;
        }
    }
    if (inits.empty()) inits = "B";
    return inits;
}

// Generates a hash for consistent card color
static unsigned int HashString(const std::string &s) {
    unsigned int h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + (unsigned char)c;
    }
    return h;
}

extern "C" void Menu_StartChoosing(void) {
    ScanBooks();
    selectedIndex = 0;
    FreeCover();

    if (!fileList.empty()) {
        LoadCoverForIndex(selectedIndex);
    }

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus || kDown & HidNpadButton_B) {
            break;
        }

        if (!fileList.empty()) {
            int prevIndex = selectedIndex;

            if (kDown & HidNpadButton_Down || kDown & HidNpadButton_StickLDown) {
                selectedIndex = (selectedIndex + 1) % (int)fileList.size();
            }
            if (kDown & HidNpadButton_Up || kDown & HidNpadButton_StickLUp) {
                selectedIndex = (selectedIndex - 1 + (int)fileList.size()) % (int)fileList.size();
            }
            if (kDown & HidNpadButton_Right || kDown & HidNpadButton_R) {
                selectedIndex = std::min((int)fileList.size() - 1, selectedIndex + 5);
            }
            if (kDown & HidNpadButton_Left || kDown & HidNpadButton_L) {
                selectedIndex = std::max(0, selectedIndex - 5);
            }

            if (selectedIndex != prevIndex) {
                LoadCoverForIndex(selectedIndex);
            }

            if (kDown & HidNpadButton_A) {
                std::string fullPath = std::string(BOOKS_DIR) + "/" + fileList[selectedIndex];
                FreeCover();
                Menu_OpenBook((char*)fullPath.c_str());
                ScanBooks();
                if (selectedIndex >= (int)fileList.size()) selectedIndex = 0;
                LoadCoverForIndex(selectedIndex);
            }
        }

        SDL_ClearScreen(RENDERER, configDarkMode ? BLACK : WHITE);
        SDL_RenderClear(RENDERER);

        SDL_Color textColor = configDarkMode ? WHITE : BLACK;
        SDL_Color subColor  = configDarkMode ? SDL_MakeColour(170, 170, 170, 255) : SDL_MakeColour(100, 100, 100, 255);
        SDL_Color bgCard    = configDarkMode ? SDL_MakeColour(35, 35, 35, 255) : SDL_MakeColour(240, 240, 240, 255);
        SDL_Color selCard   = SDL_MakeColour(0, 150, 220, 255);

        // Header
        SDL_DrawRect(RENDERER, 0, 0, 1280, 75, configDarkMode ? SDL_MakeColour(25, 25, 25, 255) : SDL_MakeColour(230, 230, 230, 255));
        SDL_DrawText(RENDERER, ROBOTO_35, 40, 18, textColor, "eBookReader");
        StatusBar_DisplayTime(false);

        if (fileList.empty()) {
            SDL_DrawText(RENDERER, ROBOTO_25, 50, 130, textColor, "No books found in /switch/eBookReader/books/");
            SDL_DrawText(RENDERER, ROBOTO_20, 50, 170, textColor, "Place your .epub, .pdf, or .cbz files into that folder on your SD card.");
        } else {
            // Book List Panel (Left Side: width 740px)
            int startY = 90;
            int itemHeight = 52;
            int maxItems = 10;
            int pageOffset = (selectedIndex / maxItems) * maxItems;

            for (int i = 0; i < maxItems && (pageOffset + i) < (int)fileList.size(); i++) {
                int idx = pageOffset + i;
                int currentY = startY + i * itemHeight;

                std::string displayTitle = TruncateText(ROBOTO_25, fileList[idx], 690);

                if (idx == selectedIndex) {
                    SDL_DrawRect(RENDERER, 40, currentY, 740, 46, selCard);
                    SDL_DrawText(RENDERER, ROBOTO_25, 55, currentY + 8, WHITE, displayTitle.c_str());
                } else {
                    SDL_DrawRect(RENDERER, 40, currentY, 740, 46, bgCard);
                    SDL_DrawText(RENDERER, ROBOTO_25, 55, currentY + 8, textColor, displayTitle.c_str());
                }
            }

            // Cover Art Preview Area (Right Side: 410px width)
            int coverBoxX = 820, coverBoxY = 90;
            int coverBoxW = 410, coverBoxH = 515;

            if (coverTexture) {
                SDL_DrawRect(RENDERER, coverBoxX, coverBoxY, coverBoxW, coverBoxH,
                             configDarkMode ? SDL_MakeColour(28, 28, 28, 255) : SDL_MakeColour(245, 245, 245, 255));

                SDL_Rect r;
                r.w = coverTexW;
                r.h = coverTexH;
                r.x = coverBoxX + (coverBoxW - coverTexW) / 2;
                r.y = coverBoxY + (coverBoxH - coverTexH) / 2;
                SDL_RenderCopy(RENDERER, coverTexture, NULL, &r);
            } else {
                // Initial colored card placeholder
                unsigned int hashVal = HashString(fileList[selectedIndex]);
                SDL_Color cardColor = CARD_PALETTE[hashVal % PALETTE_COUNT];

                int cardW = 340, cardH = 470;
                int cardX = coverBoxX + (coverBoxW - cardW) / 2;
                int cardY = coverBoxY + (coverBoxH - cardH) / 2;

                // Card background with rounded shadow border
                SDL_DrawRect(RENDERER, cardX, cardY, cardW, cardH, cardColor);

                // Initial Letter
                std::string initials = GetInitials(fileList[selectedIndex]);
                int initW = 0, initH = 0;
                TTF_SizeText(ROBOTO_35, initials.c_str(), &initW, &initH);
                SDL_DrawText(RENDERER, ROBOTO_35, cardX + (cardW - initW) / 2, cardY + 160, WHITE, initials.c_str());

                // Short Title Preview on Card
                std::string cardTitle = TruncateText(ROBOTO_20, fileList[selectedIndex], cardW - 40);
                int titleW = 0, titleH = 0;
                TTF_SizeText(ROBOTO_20, cardTitle.c_str(), &titleW, &titleH);
                SDL_DrawText(RENDERER, ROBOTO_20, cardX + (cardW - titleW) / 2, cardY + 280, WHITE, cardTitle.c_str());
            }

            // Bottom Full Title Hint & Navigation Footer
            std::string fullTitleHint = TruncateText(ROBOTO_20, fileList[selectedIndex], 1180);
            SDL_DrawText(RENDERER, ROBOTO_20, 40, 620, textColor, fullTitleHint.c_str());

            char countStr[64];
            snprintf(countStr, sizeof(countStr), "Book %d of %d  |  Press (A) to Read  |  (B/+) to Exit", 
                     selectedIndex + 1, (int)fileList.size());
            SDL_DrawText(RENDERER, ROBOTO_15, 40, 655, subColor, countStr);
        }

        SDL_RenderPresent(RENDERER);
    }

    FreeCover();
}
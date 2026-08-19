#include "CoverArt.hpp"

#include <mupdf/pdf.h>
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <cstdio>
#include <algorithm>
#include <iostream>

// Shared with BookReader.cpp/PageLayout.cpp -- same MuPDF context, reused
// rather than creating a second one. Safe because the menu and the reader
// are never active on screen at the same time (single-threaded, one or the
// other), so there's no concurrent use of ctx.
extern fz_context *ctx;

// Thumbnails are rendered once and cached here so opening the menu again
// (or scrolling back to a book) is instant afterwards.
static const char *THUMB_DIR = "/switch/eBookReader/thumbnails";

// The box thumbnails are rendered to fit within (kept modest: this is a
// library-grid thumbnail, not a reading page, and a bigger render only
// costs more time/SD space for no visible benefit at card size).
static const float THUMB_MAX_W = 240.0f;
static const float THUMB_MAX_H = 340.0f;

static bool file_exists(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static std::string cache_path_for(const std::string &filename) {
    // Book filenames are already valid filesystem names (they exist as
    // real files), so reusing one directly as the cache key is safe.
    return std::string(THUMB_DIR) + "/" + filename + ".png";
}

// Renders page 0 of the book to a PNG at `out_path`. Returns true on success.
// Every failure path is caught and reported as false -- this must never let
// an exception escape, since it runs while just browsing the menu, before
// the user has chosen to open anything.
static bool generate_thumbnail(const std::string &book_path, const std::string &out_path) {
    if (!ctx) {
        ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
        if (ctx) fz_register_document_handlers(ctx);
    }
    if (!ctx) return false;

    bool ok = false;
    fz_document *doc = NULL;
    fz_page *page = NULL;
    fz_pixmap *pix = NULL;

    fz_var(doc);
    fz_var(page);
    fz_var(pix);

    fz_try(ctx) {
        doc = fz_open_document(ctx, book_path.c_str());
        if (fz_needs_password(ctx, doc)) {
            fz_authenticate_password(ctx, doc, "");
        }

        if (fz_is_document_reflowable(ctx, doc)) {
            // A page count/bound requires layout to have run at least once
            // (unlaid-out reflowable docs have an undefined page height,
            // which is exactly the kind of state that crashes downstream).
            // Dimensions here only need to be "book shaped" for a decent
            // thumbnail crop -- they don't need to match the reader's own.
            fz_layout_document(ctx, doc, 300.0f, 420.0f, 11.0f);
        }

        if (fz_count_pages(ctx, doc) < 1) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "no pages");
        }

        page = fz_load_page(ctx, doc, 0);
        fz_rect bounds = fz_bound_page(ctx, page);
        if (bounds.x1 <= 0 || bounds.y1 <= 0) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "empty page bounds");
        }

        float zoom = std::min(THUMB_MAX_W / bounds.x1, THUMB_MAX_H / bounds.y1);
        pix = fz_new_pixmap_from_page_contents(ctx, page, fz_scale(zoom, zoom), fz_device_rgb(ctx), 0);

        // Covers stay in their natural colours regardless of dark/light
        // theme -- inverting a photographic cover would look wrong.
        fz_save_pixmap_as_png(ctx, pix, out_path.c_str());
        ok = true;
    }
    fz_always(ctx) {
        if (pix) fz_drop_pixmap(ctx, pix);
        if (page) fz_drop_page(ctx, page);
        if (doc) fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        std::cout << "CoverArt: could not generate thumbnail for " << book_path << std::endl;
        ok = false;
    }

    return ok;
}

SDL_Texture* CoverArt_Get(SDL_Renderer *renderer, const std::string &book_path, const std::string &filename) {
    mkdir("/switch/eBookReader", 0777);
    mkdir(THUMB_DIR, 0777);

    std::string cache = cache_path_for(filename);

    if (!file_exists(cache)) {
        if (!generate_thumbnail(book_path, cache)) {
            return NULL;
        }
    }

    SDL_Texture *tex = IMG_LoadTexture(renderer, cache.c_str());
    if (!tex) {
        std::cout << "CoverArt: failed to load cached thumbnail " << cache << std::endl;
    }
    return tex;
}

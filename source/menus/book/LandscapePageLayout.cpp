#include "LandscapePageLayout.hpp"
#include <algorithm>
#include <cmath>

extern "C" {
    #include "common.h"
    #include "config.h"
    #include "SDL_helper.h"
}

LandscapePageLayout::LandscapePageLayout(fz_document *doc, int current_page)
    : PageLayout(doc, current_page)
{
    render_page_to_texture(_current_page, false);
}

void LandscapePageLayout::render_page_to_texture(int num, bool reset_zoom) {
    FreeTextureIfNeeded(&page_texture);

    _current_page = std::min(std::max(0, num), pages_count - 1);

    fz_page *page = fz_load_page(ctx, doc, _current_page);
    fz_rect bounds = fz_bound_page(ctx, page);

    page_bounds = bounds;
    page_center = fz_make_point(viewport.w / 2, viewport.h / 2);

    min_zoom = 1.0f;
    max_zoom = 2.5f;
    zoom = min_zoom;

    // Render 1:1 tajam langsung ke resolusi native
    fz_matrix ctm = fz_identity;
    fz_pixmap *pix = fz_new_pixmap_from_page_contents(ctx, page, ctm, fz_device_rgb(ctx), 0);
    if (configDarkMode) {
        fz_invert_pixmap(ctx, pix);
    }

    SDL_Surface *image = SDL_CreateRGBSurfaceFrom(pix->samples, pix->w, pix->h, pix->n * 8, pix->w * pix->n,
                                                  0x000000FF, 0x0000FF00, 0x00FF0000, 0);
    page_texture = SDL_CreateTextureFromSurface(RENDERER, image);

    SDL_FreeSurface(image);
    fz_drop_pixmap(ctx, pix);
    fz_drop_page(ctx, page);
}

void LandscapePageLayout::draw_page() {
    if (!page_texture) return;

    float w = page_bounds.x1 - page_bounds.x0;
    float h = page_bounds.y1 - page_bounds.y0;

    SDL_Rect rect;
    rect.w = (int)w;
    rect.h = (int)h;
    rect.x = (int)(page_center.x - w / 2);
    rect.y = (int)(page_center.y - h / 2);

    // Rotasi 90 derajat searah jarum jam tepat di tengah layar
    SDL_RenderCopyEx(RENDERER, page_texture, NULL, &rect, 90.0, NULL, SDL_FLIP_NONE);
}

void LandscapePageLayout::reset() {
    page_center = fz_make_point(viewport.w / 2, viewport.h / 2);
    set_zoom(min_zoom);
}

void LandscapePageLayout::move_page(float x, float y) {
    page_center.x += x;
    page_center.y += y;
}

void LandscapePageLayout::move_up() {
    move_page(0, -6);
}

void LandscapePageLayout::move_down() {
    move_page(0, 6);
}

void LandscapePageLayout::move_left() {
    move_page(-6, 0);
}

void LandscapePageLayout::move_right() {
    move_page(6, 0);
}
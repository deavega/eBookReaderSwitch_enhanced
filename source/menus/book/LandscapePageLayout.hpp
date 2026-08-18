#ifndef EBOOK_READER_LANDSCAPE_PAGE_LAYOUT_HPP
#define EBOOK_READER_LANDSCAPE_PAGE_LAYOUT_HPP

#include "PageLayout.hpp"

class LandscapePageLayout : public PageLayout
{
    public:
        LandscapePageLayout(fz_document *doc, int current_page);
        virtual ~LandscapePageLayout() {}

        void draw_page() override;
        void move_up() override;
        void move_down() override;
        void move_left() override;
        void move_right() override;
        void reset() override;

    protected:
        void render_page_to_texture(int num, bool reset_zoom) override;
        void move_page(float x, float y) override;
};

#endif
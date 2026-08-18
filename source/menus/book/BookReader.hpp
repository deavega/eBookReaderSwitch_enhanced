#ifndef EBOOK_READER_BOOK_READER_HPP
#define EBOOK_READER_BOOK_READER_HPP

#include <mupdf/pdf.h>
#include <string>
#include <set>
#include "PageLayout.hpp"
#include <switch.h>
struct SDL_Texture;

typedef enum {
    BookPageLayoutPortrait,
    BookPageLayoutLandscape
} BookPageLayout;

class BookReader {
    public:
        BookReader(const char *path, int *result);
        ~BookReader();

        bool permStatusBar = false;

        void previous_page(int n);
        void next_page(int n);
        void zoom_in();
        void zoom_out();
        void move_page_up();
        void move_page_down();
        void move_page_left();
        void move_page_right();

        // Touch helpers.
        void pan_by(float dx, float dy);   // drag the page by a pixel delta
        bool is_zoomed();                  // is the page zoomed past fit?

        // Reading-font controls (reflowable books only, e.g. EPUB).
        void cycle_font();
        void increase_font_size();
        void decrease_font_size();
        bool reflowable() { return _reflowable; }

        // Bookmarks.
        void toggle_bookmark();       // add/remove a bookmark on the current page
        void jump_to_bookmark();      // jump to the next bookmarked page

        void reset_page();
        void switch_page_layout();
        void draw(bool drawHelp);
    
        BookPageLayout currentPageLayout() {
            return _currentPageLayout;
        }
    
    private:
        void show_status_bar();
        void switch_current_page_layout(BookPageLayout bookPageLayout, int current_page);

        // Applies the current font family + size to a reflowable document and
        // (optionally) rebuilds the page layout afterwards.
        void apply_reflow_settings(bool rebuild = true);
        void set_toast(const char *msg);

        // Bookmark persistence + query.
        void load_bookmarks();
        void save_bookmarks();
        bool is_current_bookmarked();

        fz_document *doc = NULL;
        int status_bar_visible_counter = 0;

        std::set<int> _bookmarks;    // bookmarked page numbers for this book

        bool  _reflowable = false;   // true for EPUB / plain-text style books
        int   _font_index = 0;       // index into the font-family table
        float _em = 26.0f;           // reflow font size in points (default)

        char _toast[64] = {0};       // transient on-screen message
        int  _toast_counter = 0;     // frames remaining to show the toast
    
        BookPageLayout _currentPageLayout = BookPageLayoutPortrait;
        PageLayout *layout = NULL;
    
        std::string book_name;
};

#endif

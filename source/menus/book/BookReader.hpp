#ifndef EBOOK_READER_BOOK_READER_HPP
#define EBOOK_READER_BOOK_READER_HPP

#include <mupdf/pdf.h>
#include <string>
#include <set>
#include <vector>
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

        // Jump-to-page/chapter overlay. Opened with A while the help menu is
        // showing (helpMenu is turned off by the caller at that point).
        void toggle_jump_menu();               // open (loading chapters on first use) or close
        bool jump_menu_open() { return _jumpMenuOpen; }
        void jump_menu_input(u64 kDown, u64 kHeld);
        void jump_menu_type_page();            // opens the system keyboard for direct numeric entry
        void draw_jump_menu();                 // called from draw() when open

        void reset_page();
        void switch_page_layout();
        void draw(bool drawHelp);

        // A single flattened table-of-contents entry (see load_chapters()).
        struct ChapterEntry { std::string title; int page; int depth; };
    
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

        // Table of contents, flattened for a simple linear list. Loaded once,
        // lazily, the first time the jump menu is opened.
        void load_chapters();
        std::vector<ChapterEntry> _chapters;
        bool _chaptersLoaded = false;

        fz_document *doc = NULL;
        int status_bar_visible_counter = 0;

        std::set<int> _bookmarks;    // bookmarked page numbers for this book

        bool  _reflowable = false;   // true for EPUB / plain-text style books
        int   _font_index = 0;       // index into the font-family table
        float _em = 26.0f;           // reflow font size in points (default)

        char _toast[64] = {0};       // transient on-screen message
        int  _toast_counter = 0;     // frames remaining to show the toast

        // Jump-to-page/chapter overlay state.
        bool _jumpMenuOpen = false;
        int  _jumpTargetPage = 0;    // 0-indexed page being adjusted
        int  _jumpSelRow = 0;        // 0 = page-number row, else 1+chapter index
        int  _jumpFirstVisibleChapter = 0;
    
        BookPageLayout _currentPageLayout = BookPageLayoutPortrait;
        PageLayout *layout = NULL;
    
        std::string book_name;
};

#endif

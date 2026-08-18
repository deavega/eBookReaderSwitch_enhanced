extern "C" {
    #include "menu_book_reader.h"
    #include "MenuChooser.h"
    #include "common.h"
    #include "config.h"
    #include "SDL_helper.h"
}

#include <iostream>
#include "BookReader.hpp"
#include "TouchGesture.hpp"

void Menu_OpenBook(char *path) {
    BookReader *reader = NULL;
    int result = 0;

    reader = new BookReader(path, &result);
    
    if (result < 0) {
        std::cout << "Menu_OpenBook: document not loaded" << std::endl;
        delete reader;
        return;
    }
    
    hidInitializeTouchScreen();

    TouchGesture gesture;
    bool helpMenu = false;
    
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (result >= 0 && appletMainLoop()) {
        reader->draw(helpMenu);
        
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);
        u64 kHeld = padGetButtons(&pad);	
        u64 kUp   = padGetButtonsUp(&pad);

        HidTouchScreenState state = {0};
        hidGetTouchScreenStates(&state, 1);
        gesture.update(state);

        // Touch exit when joy-cons are disconnected: Two-finger tap
        if (state.count >= 2) {
            break;
        }

        // While a finger is held on a zoomed-in page, drag to pan it around.
        float dragX = 0, dragY = 0;
        if (!helpMenu && gesture.consume_drag(dragX, dragY) && reader->is_zoomed()) {
            reader->pan_by(dragX, dragY);
        }

        // On finger-up, act on the recognised tap or swipe.
        TouchGestureResult g;
        if (!helpMenu && gesture.consume_gesture(g)) {
            bool portrait = reader->currentPageLayout() == BookPageLayoutPortrait;

            // Touch exit when joy-cons are disconnected: Top-Right Corner Tap (120x120 area)
            if (g.type == TouchGestureTap && g.x > 1160 && g.y < 120) {
                break;
            }

            if (g.type == TouchGestureTap) {
                // Edge taps turn the page; a centre tap toggles the status bar.
                if (portrait) {
                    if (g.x < 1280 / 3)          reader->previous_page(1);
                    else if (g.x > 1280 * 2 / 3) reader->next_page(1);
                    else                         reader->permStatusBar = !reader->permStatusBar;
                } else {
                    if (g.y < 720 / 3)           reader->previous_page(1);
                    else if (g.y > 720 * 2 / 3)  reader->next_page(1);
                    else                         reader->permStatusBar = !reader->permStatusBar;
                }
            } else if (!reader->is_zoomed()) {
                if (g.type == TouchGestureSwipeLeft || g.type == TouchGestureSwipeUp)
                    reader->next_page(1);
                else if (g.type == TouchGestureSwipeRight || g.type == TouchGestureSwipeDown)
                    reader->previous_page(1);
            }
        }

        if (!helpMenu && (kDown & HidNpadButton_Left)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->previous_page(1);
            } else {
                reader->zoom_out();
            }
        } else if (!helpMenu && (kDown & HidNpadButton_Right)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->next_page(1);
            } else {
                reader->zoom_in();
            }
        }

        if (!helpMenu && (kDown & HidNpadButton_R)) {
            reader->next_page(10);
        } else if (!helpMenu && (kDown & HidNpadButton_L)) {
            reader->previous_page(10);
        }

        if (!helpMenu && (kHeld & HidNpadButton_StickRUp)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->zoom_in();
            } else {
                reader->previous_page(1);
            }
        } else if (!helpMenu && (kHeld & HidNpadButton_StickRDown)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->zoom_out();
            } else {
                reader->next_page(1);
            }
        }

        // Bookmarks
        if (!helpMenu && (kDown & HidNpadButton_Up)) {
            reader->toggle_bookmark();
        } else if (!helpMenu && (kDown & HidNpadButton_Down)) {
            reader->jump_to_bookmark();
        }

        if (!helpMenu && (kHeld & HidNpadButton_StickLUp)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->move_page_up();
            } else {
                reader->move_page_right();
            }
        } else if (!helpMenu && (kHeld & HidNpadButton_StickLDown)) {
            if (reader->currentPageLayout() == BookPageLayoutPortrait) {
                reader->move_page_down();
            } else {
                reader->move_page_left();
            }
        } else if (!helpMenu && (kHeld & HidNpadButton_StickLRight)) {
            if (reader->currentPageLayout() == BookPageLayoutLandscape) {
                reader->move_page_down();
            }
        } else if (!helpMenu && (kHeld & HidNpadButton_StickLLeft)) {
            if (reader->currentPageLayout() == BookPageLayoutLandscape) {
                reader->move_page_up();
            }
        }

        if (!helpMenu && (kDown & HidNpadButton_LeftSR))
            reader->next_page(10);
        else if (!helpMenu && (kDown & HidNpadButton_LeftSL))
            reader->previous_page(10);

        if (kUp & HidNpadButton_B) {
            if (helpMenu) {
                helpMenu = !helpMenu;
            } else {
                break;
            }
        }

        if (!helpMenu && (kDown & HidNpadButton_X)) {
            reader->permStatusBar = !reader->permStatusBar;
        }
            
        if ((!helpMenu && (kDown & HidNpadButton_StickL)) || (kDown & HidNpadButton_StickR)) {
            reader->reset_page();
        }
        
        if (!helpMenu && (kDown & HidNpadButton_Y)) {
            reader->switch_page_layout();
        }

        if (!helpMenu && (kDown & HidNpadButton_A)) {
            reader->cycle_font();
        }

        if (!helpMenu && (kDown & HidNpadButton_ZR)) {
            reader->increase_font_size();
        } else if (!helpMenu && (kDown & HidNpadButton_ZL)) {
            reader->decrease_font_size();
        }

        if (!helpMenu && (kUp & HidNpadButton_Minus)) {
            configDarkMode = !configDarkMode;
            reader->previous_page(0);
        }

        if (kDown & HidNpadButton_Plus) {
            helpMenu = !helpMenu;
        }
    }

    std::cout << "Exiting reader" << std::endl;
    delete reader;
}
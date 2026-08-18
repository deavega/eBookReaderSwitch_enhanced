# eBookReaderSwitch — Enhancements

Two features were added: gesture-based touch controls and reading-font switching.
The bundled `mupdf/` library is unchanged and is omitted from this archive — keep
your existing `mupdf/` checkout and build with `make mupdf && make` as before.

## New / changed files
- source/menus/book/TouchGesture.hpp   (new) — touch gesture recognizer
- source/menus/book/TouchGesture.cpp   (new)
- source/menus/book/menu_book_reader.cpp — gesture-driven input; A/ZL/ZR font keys
- source/menus/book/PageLayout.hpp/.cpp  — pan(dx,dy) + is_zoomed()
- source/menus/book/BookReader.hpp/.cpp  — pan/zoom passthrough, font reflow, toast
- README.md — documented controls

The Makefile globs `*.cpp` under source/menus/book, so TouchGesture.cpp is picked
up automatically — no Makefile change needed.

## 1. Better touch scrolling / page flipping
The old code triggered an action on every frame a finger touched a fixed screen
region, so a resting finger flipped pages continuously and there was no swipe or
drag support.

`TouchGesture` now tracks the primary finger and classifies each touch on release:
- movement under ~28 px  -> a TAP
- dominant-axis travel over ~90 px -> a directional SWIPE
- per-frame deltas are exposed for continuous drag-panning.

In the reader loop:
- Swipe (fit-to-screen) -> flip page. Left/Up = next, Right/Down = previous.
- Tap screen edge -> previous/next page; tap centre -> toggle status bar.
- Drag while zoomed in -> pan the page (swipes are treated as pans, not flips).
Taps and swipes are single-fire, fixing the runaway page-flipping.

## 2. Change fonts (reflowable books)
For EPUB/text (fz_is_document_reflowable), the reader now sets a MuPDF user
stylesheet (fz_set_user_css) and re-flows the document (fz_layout_document):
- A         -> cycle Serif / Sans-serif / Monospace (MuPDF built-in families)
- ZR / ZL   -> increase / decrease the base font size (em)
After each change the page layout is rebuilt and an on-screen toast confirms the
new font/size. Fixed-layout PDF/CBZ/XPS report that fonts don't apply and are
left untouched.

## Build (unchanged)
    pacman -S libnx switch-portlibs
    make mupdf
    make

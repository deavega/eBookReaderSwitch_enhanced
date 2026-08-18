# eBookReaderSwitch

### Features:
* Saves last page number
* Reads PDF, EPUB, CBZ, and XPS files
* Dark and light mode
* Landscape reading view
* Portrait reading view
* Gesture-based touch controls
	* **Swipe** left/right (or up/down) to flip pages.
	* **Tap** the left/right edge to go to the previous/next page; tap the centre to toggle the status bar.
	* **Drag** to pan around a page once you have zoomed in.
	* Taps and swipes are properly debounced, so a resting finger no longer flips pages.
* Adjustable reading font for reflowable books (EPUB / text)
	* Cycle between Serif, Sans-serif and Monospace with **A**.
	* Shrink/grow the text size with **ZL** / **ZR**.
	* Fixed-layout formats (PDF, CBZ, XPS) keep their original typography.
* Books read from `/switch/eBookReader/books`

### Controls:
| Input | Action |
| --- | --- |
| Swipe / D-pad Left-Right | Previous / next page |
| Tap screen edge | Previous / next page |
| Tap screen centre | Toggle status bar |
| Drag (when zoomed) | Pan the page |
| Right Stick Up/Down | Zoom in / out |
| Left Stick | Pan the page |
| L / R | Skip back / forward 10 pages |
| A | Change reading font (EPUB) |
| ZL / ZR | Decrease / increase font size (EPUB) |
| Y | Rotate page (portrait/landscape) |
| X | Keep status bar on |
| Minus | Toggle dark / light theme |
| Plus | Help menu |
| B | Stop reading / close help |

### TODO:
* Do some extra testing on file compatibility.
* 2 pages side by side in landscape.
* Hardware lock to prevent accidental touches (maybe Vol- ?) (?).
* Save orientation, dark mode, and font settings.
* Kinetic/momentum scrolling after a drag.

### Screen Shots:

Dark Mode Help Menu:
<br></br>
<img src="screenshots/darkModeHelp.jpg" width="322" height="178.4">
<br></br>

Dark Mode Landscape Reading (With the Switch horizonal):
<br></br>
<img src="screenshots/darkModeLandscape.jpg" width="512" height="288">
<br></br>

Dark Mode Portrait Reading (With the Switch vertical):
<br></br>
<img src="screenshots/darkModePortrait.jpg" width="285.6" height="408.8">
<br></br>

Dark Mode Book Selection:
<br></br>
<img src="screenshots/darkModeSelection.jpg" width="512" height="288">
<br></br>

Light Mode Landscape Reading:
<br></br>
<img src="screenshots/lightModeLandscape.jpg" width="512" height="288">

### Credit:
* moronigranja - For allowing more file support
* NX-Shell Team - A good amount of the code is from an old version of their application.

### Building
* Release built with [libnx release v4.1.3](https://github.com/switchbrew/libnx).
* Uses `freetype` and other libs which comes with `switch-portlibs` via `devkitPro pacman`:
```
pacman -S libnx switch-portlibs
```
then run:
```
make mupdf
make
```
to build.

If you don't have twili debugger installed, delete the `-ltwili` flag on the Makefile to compile:
```
LIBS: -ltwili
```

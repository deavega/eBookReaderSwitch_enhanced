# eBookReader for Nintendo Switch

This is a forked and enhanced version from [Sean](https://github.com/SeanOMik/eBookReaderSwitch). An e-book and document reader homebrew application for the Nintendo Switch powered by **MuPDF** and **SDL2**. Still buggy (i.e. font changing is not working and maybe become part of future development; not all epub files are compatible). 
>Added features are **bookmarking, larger default font size, book cover art thumbnail**.

Use your own NSP Forwarder to add the NRO's shorcut on the HOME menu (Look at **Release** for the NRO). 

Credit belongs to the original developer @SeanOMik.

---

## Features

* **Multi-Format Support:** EPUB, PDF, CBZ, XPS, FB2, and TXT.
* **Dual Orientation Modes:**
* **Portrait Mode:** Standard horizontal view with generous reading margins.
* **Landscape (Rotated 90°) Mode:** Hold the console vertically like a real book, featuring crisp 1:1 rendering and comfortable top/bottom padding.
* **Custom Font Engine:** Support for user-provided TrueType fonts (`.ttf`) with dynamic runtime switching across Serif, Sans-Serif, and Monospace families.
* **Interactive Cover Cards:** Dynamic colored cards with title initials generated for files without embedded cover art.
* **Touchscreen & Joy-Con Controls:** Full touch navigation (swipe, drag-to-pan, tap-to-turn, corner/two-finger exit) alongside physical button mappings.
* **Reading Utilities:** Auto-saving last read page per book, bookmark management, dark/light theme toggle, and font size adjustment.

![Sample Image](https://res.cloudinary.com/dayer1hez/image/upload/w_0.6,c_scale/v1787029907/2026081812053700-AD808EFD2C53DCD6F8380113B64A0DD4_otcpp0.jpg)

![Sample Image](https://res.cloudinary.com/dayer1hez/image/upload/w_0.6,c_scale/v1787029907/2026081812055400-AD808EFD2C53DCD6F8380113B64A0DD4_e4ikk0.jpg)

![Sample Image](https://res.cloudinary.com/dayer1hez/image/upload/w_0.6,c_scale/v1787029907/2026081812173900-96348BC3A80D3510E6442162AAEBE4ED_dhffno.jpg)

![Sample Image](https://res.cloudinary.com/dayer1hez/image/upload/w_0.6,c_scale/v1787029907/2026081812170100-57B4628D2267231D57E0FC1078C0596D_y0olyw.jpg)

---

## Directory Structure

Place your books and font files onto your Switch SD card in the following directories:

* `sdmc:/switch/eBookReader/eBookReader.nro`
* `sdmc:/switch/eBookReader/saved_pages.cfg` *(Auto-generated)*
* `sdmc:/switch/eBookReader/books/` *(Place `.epub`, `.pdf`, `.cbz` files here)*
* `sdmc:/switch/eBookReader/fonts/Serif.ttf`
* `sdmc:/switch/eBookReader/fonts/Sans.ttf`
* `sdmc:/switch/eBookReader/fonts/Mono.ttf`

---

## Controls

### Physical Controls

| Button / Input | Action |
| --- | --- |
| **D-Pad Left / Right** | Previous / Next page (Portrait) |
| **D-Pad Up / Down** | Bookmark: Up to add/remove, Down to jump |
| **L / R** | Jump 10 pages backward / forward |
| **ZL / ZR** | Decrease / Increase font size (EPUB/reflow) |
| **A** | Cycle font family (`Serif` → `Sans-serif` → `Monospace`) |
| **B** | Exit to main menu / Close help overlay |
| **Y** | Toggle layout orientation (Portrait / 90° Rotated Landscape) |
| **X** | Toggle reading status bar |
| **Minus (-)** | Toggle Dark / Light theme |
| **Plus (+)** | Show / Hide Help Menu |
| **Left Stick** | Pan page (when zoomed) |
| **Right Stick Up / Down** | Zoom in / Zoom out |
| **Left / Right Stick Click** | Reset page zoom & position |

### Touch Controls

| Gesture | Action |
| --- | --- |
| **Left / Right Side Tap** | Previous / Next page |
| **Center Tap** | Toggle reading status bar |
| **Horizontal Swipe** | Turn page forward / backward |
| **Drag (when zoomed)** | Pan page viewport |
| **Top-Right Corner Tap** | Exit reader to book menu (Joy-Con detached mode) |
| **Two-Finger Tap** | Exit reader to book menu (Joy-Con detached mode) |

---

## Recommended Fonts

You can download standard, open-source `.ttf` fonts from Google Fonts and rename them to match the expected filenames:

* **Serif (`Serif.ttf`):** Literata, Merriweather, or Georgia
* **Sans-serif (`Sans.ttf`):** Roboto, Inter, or Open Sans
* **Monospace (`Mono.ttf`):** Roboto Mono or JetBrains Mono

---

## Building from Source

### Prerequisites

* devkitPro with `devkitA64` toolchain
* `libnx`, `switch-sdl2`, `switch-sdl2_ttf`, `switch-sdl2_image`, `switch-libconfig`, and `switch-mupdf` libraries

Install dependencies via pacman:

```bash
dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-sdl2_image switch-libconfig switch-mupdf

```

### Compilation

Clone the repository and build the `.nro` binary:

```bash
git clone https://github.com/your-username/eBookReader-Switch.git
cd eBookReader-Switch
make clean
make NODEBUG=1

```

Copy the generated `eBookReader.nro` to `/switch/eBookReader/` on your SD card. Use NSP Forwarder to ease access to eBookReader.

---
## Support Me

<p>Buying me a coffee to support this project and future enhancements...</p>
<a href="https://ko-fi.com/vegatroz" target="_blank">
  <img src="https://storage.ko-fi.com/cdn/kofi3.png?v=3" height="48" alt="Buy Me a Coffee at ko-fi.com" />
</a>

---

## License

This project is open-source software licensed under the MIT License. Document rendering is powered by the MuPDF library (AGPL/Commercial license).



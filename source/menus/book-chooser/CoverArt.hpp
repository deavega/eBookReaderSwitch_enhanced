#ifndef EBOOK_READER_COVER_ART_HPP
#define EBOOK_READER_COVER_ART_HPP

#include <SDL2/SDL.h>
#include <string>

// Returns a texture with the book's real first-page/cover image, loading it
// from an on-disk cache when available and generating+caching it via MuPDF
// otherwise. Returns NULL if a real cover can't be produced for any reason
// (unsupported/corrupt file, out of memory, etc.) -- callers should fall
// back to a generated placeholder cover in that case, never treat NULL as
// an error to surface to the user.
//
// `book_path` is the full path to the book file; `filename` is just its
// name (used to build a stable cache filename).
SDL_Texture* CoverArt_Get(SDL_Renderer *renderer, const std::string &book_path, const std::string &filename);

#endif

#ifndef ECO_BOOK_H
#define ECO_BOOK_H

#include <cstddef>

// ECO opening book (contents of eco.pgn) embedded at build time so the
// engine works as a single self-contained executable.
// Regenerate eco_book.cpp with scripts/embed_book.py after editing eco.pgn.
extern const unsigned char ECO_BOOK_DATA[];
extern const std::size_t ECO_BOOK_SIZE;

#endif // ECO_BOOK_H

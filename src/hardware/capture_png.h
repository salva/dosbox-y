/* Checked screenshot encoder shared by capture owners.
 * GPL-2.0-or-later, same license as hardware.cpp.
 * No emulator state, filenames, popup, or pause in this primitive.
 */
#ifndef DOSBOX_CAPTURE_PNG_H
#define DOSBOX_CAPTURE_PNG_H
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
namespace CapturePNG {
struct IO {
    size_t (*write)(const void *, size_t, size_t, FILE *) = fwrite;
    int (*flush)(FILE *) = fflush;
    int (*close)(FILE *) = fclose;
};
struct Sink {
    FILE *file;
    IO io;
    size_t bytes, limit;
};
inline void output(png_structp png, png_bytep data, png_size_t size) {
    Sink *sink = static_cast<Sink *>(png_get_io_ptr(png));
    if (size > sink->limit - sink->bytes)
        png_error(png, "encoded PNG quota exceeded");
    if (sink->io.write(data, 1, size, sink->file) != size)
        png_error(png, "short PNG write");
    sink->bytes += size;
}
inline void flush(png_structp png) {
    Sink *sink = static_cast<Sink *>(png_get_io_ptr(png));
    if (sink->io.flush(sink->file) != 0) png_error(png, "PNG flush failed");
}
// Owns FILE on every return. Sink is caller-owned so longjmp does not invalidate
// a changed automatic local. No C++ object with a destructor crosses setjmp.
inline bool encode(Sink *sink, uint32_t w, uint32_t h, uint32_t bpp,
                   size_t pitch, bool dblw, bool dblh, const uint8_t *pixels,
                   const uint8_t *pal, bool mac32, const char *software) {
    const uint32_t outw = w * (dblw ? 2u : 1u);
    const uint32_t outh = h * (dblh ? 2u : 1u);
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return false;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); return false; }
    uint8_t *row = static_cast<uint8_t *>(malloc(outw * 3u));
    if (!row) { png_destroy_write_struct(&png, &info); return false; }
    if (setjmp(png_jmpbuf(png))) {
        free(row);
        png_destroy_write_struct(&png, &info);
        return false;
    }
    png_set_write_fn(png, sink, output, flush);
    png_set_compression_level(png, 9);
    png_set_IHDR(png, info, outw, outh, 8,
                 bpp == 8 ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    if (bpp == 8) {
        png_color palette[256];
        for (unsigned i = 0; i < 256; ++i) {
            palette[i].red = pal[i * 4];
            palette[i].green = pal[i * 4 + 1];
            palette[i].blue = pal[i * 4 + 2];
        }
        png_set_PLTE(png, info, palette, 256);
    } else png_set_bgr(png);
#ifdef PNG_pHYs_SUPPORTED
    if (outw >= 8 && outh >= 8) {
        uint32_t x = 4 * outh, y = 3 * outw, a = x, b = y;
        while (b) { uint32_t r = a % b; a = b; b = r; }
        png_set_pHYs(png, info, x / a, y / a, PNG_RESOLUTION_UNKNOWN);
    }
#endif
#ifdef PNG_TEXT_SUPPORTED
    png_text text = {};
    text.compression = PNG_TEXT_COMPRESSION_NONE;
    text.key = const_cast<char *>("Software");
    text.text = const_cast<char *>(software);
    png_set_text(png, info, &text, 1);
#else
    (void)software;
#endif
    png_write_info(png, info);
    for (uint32_t y = 0; y < outh; ++y) {
        const uint8_t *src = pixels + (y / (dblh ? 2u : 1u)) * pitch;
        for (uint32_t x = 0; x < outw; ++x) {
            const uint32_t sx = x / (dblw ? 2u : 1u);
            if (bpp == 8) row[x] = src[sx];
            else if (bpp == 15 || bpp == 16) {
                uint16_t pixel;
                memcpy(&pixel, src + sx * 2, sizeof(pixel));
                // Preserve legacy channel expansion/truncation, not a new scale.
                row[x * 3] = static_cast<uint8_t>(((pixel & 0x001f) * 0x21) >> 2);
                row[x * 3 + 1] = static_cast<uint8_t>(bpp == 15 ?
                    ((pixel & 0x03e0) * 0x21) >> 7 : ((pixel & 0x07e0) * 0x41) >> 9);
                row[x * 3 + 2] = static_cast<uint8_t>(bpp == 15 ?
                    ((pixel & 0x7c00) * 0x21) >> 12 : ((pixel & 0xf800) * 0x21) >> 13);
            } else {
                row[x * 3] = src[sx * 4 + (mac32 ? 3 : 0)];
                row[x * 3 + 1] = src[sx * 4 + (mac32 ? 2 : 1)];
                row[x * 3 + 2] = src[sx * 4 + (mac32 ? 1 : 2)];
            }
        }
        png_write_row(png, row);
    }
    png_write_end(png, info);
    free(row);
    png_destroy_write_struct(&png, &info);
    return true;
}
inline bool write(FILE *file, uint64_t w, uint64_t h, uint64_t bpp,
                  uint64_t pitch, bool dblw, bool dblh, const uint8_t *pixels,
                  const uint8_t *pal, bool mac32, const char *software,
                  size_t limit = 16u * 1024u * 1024u, IO io = IO(),
                  size_t *bytes = NULL) {
    if (bytes) *bytes = 0;
    if (!file) return false;
    Sink sink = {file, io, 0, limit};
    const uint64_t unit = bpp == 8 ? 1 : bpp == 32 ? 4 : 2;
    const bool valid = pixels && (bpp != 8 || pal) && software &&
        (bpp == 8 || bpp == 15 || bpp == 16 || bpp == 32) &&
        w >= 1 && w <= 4096 && h >= 1 && h <= 4096 &&
        w * (dblw ? 2 : 1) <= 4096 && h * (dblh ? 2 : 1) <= 4096 &&
        pitch >= w * unit && pitch <= (64u * 1024u * 1024u) / h &&
        limit >= 1 && limit <= 16u * 1024u * 1024u;
    bool ok = valid && encode(&sink, static_cast<uint32_t>(w),
        static_cast<uint32_t>(h), static_cast<uint32_t>(bpp),
        static_cast<size_t>(pitch), dblw, dblh, pixels, pal, mac32, software);
    // Call close exactly once, also after encoder/flush failure. No short-circuit.
    if (ok && io.flush(file) != 0) ok = false;
    if (io.close(file) != 0) ok = false;
    if (ok && bytes) *bytes = sink.bytes;
    return ok;
}
}
#endif

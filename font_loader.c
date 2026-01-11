#include "font_loader.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/em_js.h>
#endif

#include <raylib.h>

typedef struct {
    float x, y, width, height, ascent, advance, bleedLeft;
} GlyphRect;

/* clang-format off */
EM_JS(void, generate_system_font_atlas, (const char* fontName, int fontSize, const char* chars, const char* fontWeight, const char* fontStyle, unsigned char* pixelDest, GlyphRect* rectsDest, int atlasSize), {
    const weight = UTF8ToString(fontWeight);
    const style = UTF8ToString(fontStyle);
    const name = UTF8ToString(fontName);
    const text = UTF8ToString(chars);
    const dpr = window.devicePixelRatio || 1;

    const canvas = document.createElement('canvas');
    canvas.width = atlasSize * dpr;
    canvas.height = atlasSize * dpr;
    const ctx = canvas.getContext('2d', {willReadFrequently: true});

    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, atlasSize, atlasSize);

    ctx.font = `${style} ${weight} ${fontSize}px ${name}`;
    ctx.textBaseline = "alphabetic";
    ctx.fillStyle = "white";

    let x = 0; let y = 0; const padding = 4;

    for (let i = 0; i < text.length; i++) {
        const metrics = ctx.measureText(text[i]);

        const ascent = Math.ceil(metrics.actualBoundingBoxAscent) || fontSize;
        const descent = Math.ceil(metrics.actualBoundingBoxDescent) || 0;
        const bleedLeft = Math.ceil(Math.abs(metrics.actualBoundingBoxLeft)) || 0;

        const w = Math.ceil(metrics.width + bleedLeft + (metrics.actualBoundingBoxRight - metrics.width)) + 2;
        const h = ascent + descent + 2;

        if (x + w > atlasSize) {
            x = 0;
            y += Math.ceil(fontSize * 1.2) + padding;
        }
        
        if (y + h > atlasSize) break;

        ctx.fillText(text[i], x + bleedLeft, y + ascent);

        const offset = i * 28;
        HEAPF32[(rectsDest + offset) >> 2] = x * dpr;
        HEAPF32[(rectsDest + offset + 4) >> 2] = y * dpr;
        HEAPF32[(rectsDest + offset + 8) >> 2] = w * dpr;
        HEAPF32[(rectsDest + offset + 12) >> 2] = h * dpr;
        HEAPF32[(rectsDest + offset + 16) >> 2] = ascent * dpr;
        HEAPF32[(rectsDest + offset + 20) >> 2] = metrics.width * dpr;
        HEAPF32[(rectsDest + offset + 24) >> 2] = bleedLeft * dpr;

        x += w + padding;
    }

    const imgData = ctx.getImageData(0, 0, atlasSize * dpr, atlasSize * dpr).data;
    HEAPU8.set(imgData, pixelDest);
});
/* clang-format on */

Font LoadFontAtlasFromJS(const char* fontName, int fontSize, const char* charset, const char* fontWeight, const char* fontStyle) {
    int glyphCount = 0;
    int* codepoints = LoadCodepoints(charset, &glyphCount);

    float dpr = emscripten_get_device_pixel_ratio();
    int atlasSize = 1024;

    int physicalWidth = (int) (atlasSize * dpr);
    int physicalHeight = (int) (atlasSize * dpr);

    unsigned char* pixels = (unsigned char*) malloc(physicalWidth * physicalHeight * 4);
    GlyphRect* rects = (GlyphRect*) malloc(glyphCount * sizeof(GlyphRect));

    generate_system_font_atlas(fontName, fontSize, charset, fontWeight, fontStyle, pixels, rects, atlasSize);

    Image img = {
            .data = pixels,
            .width = physicalWidth,
            .height = physicalHeight,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };

    Font font = {0};
    font.baseSize = fontSize;
    font.glyphCount = glyphCount;
    font.texture = LoadTextureFromImage(img);

    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);

    font.recs = (Rectangle*) malloc(glyphCount * sizeof(Rectangle));
    font.glyphs = (GlyphInfo*) malloc(glyphCount * sizeof(GlyphInfo));

    for (int i = 0; i < glyphCount; i++) {
        font.recs[i] = (Rectangle){rects[i].x, rects[i].y, rects[i].width, rects[i].height};

        font.glyphs[i].value = codepoints[i];
        font.glyphs[i].offsetX = (int) (-rects[i].bleedLeft / dpr);
        font.glyphs[i].offsetY = (int) (fontSize - rects[i].ascent / dpr);

        font.glyphs[i].advanceX = (int) (rects[i].advance / dpr);
    }

    UnloadImage(img);
    free(rects);
    UnloadCodepoints(codepoints);
    return font;
}
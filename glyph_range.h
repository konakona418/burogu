#include <stdio.h>
#include <stdlib.h>

#define MARKDOWN_BASE_PATH "markdown/"
#define GLYPH_RANGE_FILE MARKDOWN_BASE_PATH "glyph_range.txt"

static inline char* ReadGlyphRange() {
    FILE* file = fopen(GLYPH_RANGE_FILE, "rb");
    if (!file) {
        printf("Failed to open glyph range file: %s\n", GLYPH_RANGE_FILE);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*) malloc(fileSize + 1);
    fread(buffer, 1, fileSize, file);
    buffer[fileSize] = '\0';

    fclose(file);

    return buffer;
}
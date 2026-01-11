#include <stdio.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/em_js.h>
#endif

#include <clay.h>

#include <cmark.h>

#include "font_loader.h"
#include "glyph_range.h"
#include "renderer.c"
#include "util.h"

#define SCROLL_SPEED 5.0f

typedef struct {
    int h1;
    int h2;
    int h3;
    int body;
    int small;
} FontSizes;

const FontSizes fontSizes = {
        .h1 = 48.0f,
        .h2 = 36.0f,
        .h3 = 28.0f,
        .body = 18.0f,
        .small = 14.0f,
};

#define FONT_NAME_NORMAL "-apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Noto Sans SC', sans-serif"
#define FONT_NAME_MONOSPACE "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace"

#define FONT_NORMAL_WEIGHT "300"
#define FONT_BOLD_WEIGHT "500"

#define FONT_STYLE_NORMAL "normal"
#define FONT_STYLE_ITALIC "italic"

Font embeddedFonts[16];
#define ZHCN_FONT_NORMAL 0
#define ZHCN_FONT_BIG 1

#define ZHCN_FONT_NORMAL_BOLD 2
#define ZHCN_FONT_NORMAL_ITALIC 3
#define ZHCN_FONT_NORMAL_BOLD_ITALIC 4

#define ZHCN_FONT_BIG_BOLD 5
#define ZHCN_FONT_BIG_ITALIC 6
#define ZHCN_FONT_BIG_BOLD_ITALIC 7

#define CODE_FONT_MONOSPACE 8

double GetDevicePixelRatio() {
#ifdef EMSCRIPTEN
    return emscripten_get_device_pixel_ratio();
#else
    return GetDevicePixelRatio();
#endif
}

#define TEXT_ARENA_SIZE 1024 * 1024
char textArenaMemory[TEXT_ARENA_SIZE];
int textArenaOffset = 0;

Clay_String AllocateStringInArena(const char* str) {
    if (!str) {
        return (Clay_String){0};
    }

    if (textArenaOffset + strlen(str) + 1 > TEXT_ARENA_SIZE) {
        printf("Text arena out of memory!\n");
        return (Clay_String){0};
    }

    char* dest = &textArenaMemory[textArenaOffset];
    strcpy(dest, str);
    textArenaOffset += strlen(str) + 1;

    return (Clay_String){.chars = dest, .length = (int) strlen(str)};
}

void ResetTextArena() {
    textArenaOffset = 0;
}

char* ReadMarkdown(const char* path) {
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), MARKDOWN_BASE_PATH "%s", path);

    FILE* file = fopen(fullPath, "rb");
    if (!file) {
        printf("Failed to open markdown file: %s\n", fullPath);
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

typedef struct {
    Bool bold;
    Bool italic;
    Bool underline;
    Bool strikethrough;

    Bool monospace;
} TextState;

typedef enum {
    CMD_BLOCK_OPEN,
    CMD_TEXT,
    CMD_BLOCK_CLOSE,
} CommandType;

typedef enum {
    BT_NONE,
    BT_HEADING,
    BT_QUOTE,
    BT_CODE,
    BT_PARAGRAPH,

    BT_LIST_CONTAINER,
    BT_LIST_ITEM,

    BT_HR,
} BlockType;

typedef struct {
    CommandType type;
    BlockType blockType;
    Clay_String content;
    Clay_TextElementConfig textConfig;
    TextState textState;
} RenderCommand;

typedef struct {
    Clay_TextElementConfig config;
    TextState state;
} StyleFrame;

Bool IsBlockPopStyleStackRequired(cmark_node_type type) {
    return type == CMARK_NODE_HEADING || type == CMARK_NODE_BLOCK_QUOTE ||
           type == CMARK_NODE_STRONG || type == CMARK_NODE_EMPH ||
           type == CMARK_NODE_LIST;
}


RenderCommand* ParseMarkdownToCommands(const char* markdown, int* outCommandCount) {
    cmark_node* root = cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
    if (!root) return NULL;

    RenderCommand* commands;
    DYNARRAY_INIT(commands, 16);

    StyleFrame* styleStack;
    STACK_INIT(styleStack, 8);

    int* orderedListCounterStack;
    STACK_INIT(orderedListCounterStack, 4);

    Clay_TextElementConfig currentConfig = {
            .fontId = ZHCN_FONT_NORMAL,
            .fontSize = fontSizes.body,
            .textColor = {0, 0, 0, 255},
            .letterSpacing = 1.0f,
    };
    TextState currentState = {
            .bold = FALSE,
            .italic = FALSE,
            .underline = FALSE,
            .strikethrough = FALSE,
            .monospace = FALSE,
    };

    cmark_iter* iter = cmark_iter_new(root);
    cmark_event_type ev;

    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node* node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev == CMARK_EVENT_ENTER) {
            if (IsBlockPopStyleStackRequired(type)) {
                STACK_PUSH(styleStack, ((StyleFrame){currentConfig, currentState}));
            }

            if (type == CMARK_NODE_HEADING) {
                int level = cmark_node_get_heading_level(node);
                currentConfig.fontId = ZHCN_FONT_BIG;
                currentConfig.fontSize = (level == 1)
                                                 ? fontSizes.h1
                                         : (level == 2)
                                                 ? fontSizes.h2
                                                 : fontSizes.h3;
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_HEADING}));
            } else if (type == CMARK_NODE_BLOCK_QUOTE) {
                currentConfig.textColor = (Clay_Color){120, 120, 120, 255};
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_QUOTE}));
            } else if (type == CMARK_NODE_PARAGRAPH) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_PARAGRAPH}));
            } else if (type == CMARK_NODE_CODE_BLOCK) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_CODE}));
                RenderCommand codeTxt = {
                        .type = CMD_TEXT,
                        .content = AllocateStringInArena(cmark_node_get_literal(node)),
                        .textConfig = currentConfig,
                        .textState = currentState,
                };
                codeTxt.textState.monospace = TRUE;
                codeTxt.textConfig.textColor = (Clay_Color){200, 100, 50, 255};

                DYNARRAY_PUSHBACK(commands, codeTxt);
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_CODE}));
            } else if (type == CMARK_NODE_STRONG) {
                currentState.bold = true;
            } else if (type == CMARK_NODE_EMPH) {
                currentState.italic = true;
            } else if (type == CMARK_NODE_TEXT) {
                RenderCommand tCmd = {
                        .type = CMD_TEXT,
                        .content = AllocateStringInArena(cmark_node_get_literal(node)),
                        .textConfig = currentConfig,
                        .textState = currentState,
                };
                DYNARRAY_PUSHBACK(commands, tCmd);
            } else if (type == CMARK_NODE_CODE) {
                RenderCommand inlineCode = {
                        .type = CMD_TEXT,
                        .content = AllocateStringInArena(cmark_node_get_literal(node)),
                        .textConfig = currentConfig,
                        .textState = currentState,
                };
                inlineCode.textState.monospace = TRUE;
                inlineCode.textConfig.textColor = (Clay_Color){50, 50, 50, 255};

                DYNARRAY_PUSHBACK(commands, inlineCode);
            } else if (type == CMARK_NODE_SOFTBREAK) {
                RenderCommand spaceCmd = {
                        .type = CMD_TEXT,
                        .content = AllocateStringInArena(" "),
                        .textConfig = currentConfig,
                        .textState = currentState,
                };
                DYNARRAY_PUSHBACK(commands, spaceCmd);
            } else if (type == CMARK_NODE_LIST) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_LIST_CONTAINER}));
                if (cmark_node_get_list_type(node) == CMARK_ORDERED_LIST) {
                    STACK_PUSH(orderedListCounterStack, 1);
                }
            } else if (type == CMARK_NODE_ITEM) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_LIST_ITEM}));

                cmark_node* listNode = cmark_node_parent(node);
                char prefix[16];
                if (cmark_node_get_list_type(listNode) == CMARK_BULLET_LIST) {
                    strcpy(prefix, " -  ");
                } else {
                    int* idx = STACK_TOP(orderedListCounterStack);
                    if (idx) {
                        snprintf(prefix, sizeof(prefix), " %d. ", *idx);
                        *idx += 1;
                    } else {
                        printf("Error: ordered list item without counter stack!\n");
                    }
                }

                RenderCommand bulletCmd = {
                        .type = CMD_TEXT,
                        .content = AllocateStringInArena(prefix),
                        .textConfig = currentConfig,
                        .textState = currentState,
                };
                bulletCmd.textConfig.textColor = (Clay_Color){50, 50, 50, 255};
                DYNARRAY_PUSHBACK(commands, bulletCmd);
            } else if (type == CMARK_NODE_THEMATIC_BREAK) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_OPEN, .blockType = BT_HR}));
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_HR}));
            }
            // todo: handle more node types
        } else if (ev == CMARK_EVENT_EXIT) {
            if (type == CMARK_NODE_HEADING) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_HEADING}));
            } else if (type == CMARK_NODE_BLOCK_QUOTE) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_QUOTE}));
            } else if (type == CMARK_NODE_PARAGRAPH) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_PARAGRAPH}));
            } else if (type == CMARK_NODE_LIST) {
                if (cmark_node_get_list_type(node) == CMARK_ORDERED_LIST) {
                    int popped;
                    STACK_POP(orderedListCounterStack, &popped);
                }
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_LIST_CONTAINER}));
            } else if (type == CMARK_NODE_ITEM) {
                DYNARRAY_PUSHBACK(commands, ((RenderCommand){.type = CMD_BLOCK_CLOSE, .blockType = BT_LIST_ITEM}));
            }

            if (IsBlockPopStyleStackRequired(type)) {
                StyleFrame popped;
                STACK_POP(styleStack, &popped);
                currentConfig = popped.config;
                currentState = popped.state;
            }
        }
    }

    STACK_FREE(styleStack);
    cmark_iter_free(iter);
    cmark_node_free(root);
    *outCommandCount = DYNARRAY_SIZE(commands);
    return commands;
}

int RemapFontId(int normalFontId, Bool bold, Bool italic, Bool mono) {
    if (mono) {
        // override all
        return CODE_FONT_MONOSPACE;
    }

    if (normalFontId == ZHCN_FONT_NORMAL) {
        if (bold && italic) {
            return ZHCN_FONT_NORMAL_BOLD_ITALIC;
        } else if (bold) {
            return ZHCN_FONT_NORMAL_BOLD;
        } else if (italic) {
            return ZHCN_FONT_NORMAL_ITALIC;
        } else {
            return ZHCN_FONT_NORMAL;
        }
    } else if (normalFontId == ZHCN_FONT_BIG) {
        if (bold && italic) {
            return ZHCN_FONT_BIG_BOLD_ITALIC;
        } else if (bold) {
            return ZHCN_FONT_BIG_BOLD;
        } else if (italic) {
            return ZHCN_FONT_BIG_ITALIC;
        } else {
            return ZHCN_FONT_BIG;
        }
    }

    return normalFontId;
}

void MarkdownRenderer(RenderCommand* commands, int commandCount) {
    CLAY({
            .id = CLAY_ID("MainContent"),
            .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {
                            CLAY_SIZING_GROW(),
                            CLAY_SIZING_FIT(),
                    },
                    .padding = CLAY_PADDING_ALL(32),
                    .childGap = 8,
            },
            .clip = {
                    .vertical = TRUE,
                    .childOffset = Clay_GetScrollOffset(),
            },
    }) {
        for (int i = 0; i < commandCount; i++) {
            RenderCommand* cmd = &commands[i];

            if (cmd->type == CMD_BLOCK_OPEN) {
                Clay_ElementDeclaration decl = {
                        .id = CLAY_IDI("Block", i),
                        .layout = {
                                .sizing = {
                                        CLAY_SIZING_GROW(),
                                        CLAY_SIZING_FIT(),
                                },
                        },
                };

                if (cmd->blockType == BT_QUOTE) {
                    decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
                    decl.layout.padding = (Clay_Padding){32, 10, 10, 10};
                    decl.backgroundColor = (Clay_Color){240, 240, 240, 100};
                    decl.border = (Clay_BorderElementConfig){
                            .width = {
                                    .left = 4,
                            },
                            .color = (Clay_Color){200, 200, 200, 255},
                    };
                } else if (cmd->blockType == BT_CODE) {
                    decl.layout.padding = CLAY_PADDING_ALL(16);
                    decl.backgroundColor = (Clay_Color){30, 32, 35, 255};
                    decl.cornerRadius = CLAY_CORNER_RADIUS(8);
                    decl.border = (Clay_BorderElementConfig){.width = CLAY_BORDER_ALL(1), .color = {60, 60, 60, 255}};
                } else if (cmd->blockType == BT_PARAGRAPH) {
                    decl.layout.sizing.width = CLAY_SIZING_GROW();
                } else if (cmd->blockType == BT_LIST_CONTAINER) {
                    decl.layout.padding = (Clay_Padding){24, 0, 0, 0};
                    decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
                    decl.layout.childGap = 4;
                } else if (cmd->blockType == BT_LIST_ITEM) {
                    decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
                    decl.layout.sizing.width = CLAY_SIZING_GROW();
                } else if (cmd->blockType == BT_HR) {
                    Clay_ElementDeclaration decl = {
                            .id = CLAY_IDI("HR", i),
                            .layout = {
                                    .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(2)},
                                    .padding = {0, 0, 16, 16},
                            },
                            .backgroundColor = {100, 100, 100, 100}};
                    Clay__OpenElement();
                    Clay__ConfigureOpenElement(decl);
                    Clay__CloseElement();
                }

                Clay__OpenElement();
                Clay__ConfigureOpenElement(decl);
            } else if (cmd->type == CMD_TEXT) {
                CLAY_TEXT(cmd->content,
                          CLAY_TEXT_CONFIG({
                                  .fontId = RemapFontId(
                                          cmd->textConfig.fontId,
                                          cmd->textState.bold,
                                          cmd->textState.italic,
                                          cmd->textState.monospace),
                                  .fontSize = cmd->textConfig.fontSize,
                                  .textColor = cmd->textConfig.textColor,
                                  .lineHeight = cmd->textConfig.fontSize * 1.5f,
                                  .wrapMode = CLAY_TEXT_WRAP_WORDS,
                          }));
            } else if (cmd->type == CMD_BLOCK_CLOSE) {
                Clay__CloseElement();
            }
        }
    }
}

RenderCommand* globalRenderCommandCache;
int globalRenderCommandCount;
int needsParse = 1;
const char* needsParseFileName;

void RequireMarkdownReparse(const char* fileName) {
    needsParse = 1;
    needsParseFileName = fileName;
}

// ! extremely hacky way to workaround clay/raylib text measurement issues
// ! it just works, though does not look fine
char* InjectSpaces(const char* input) {
    size_t len = strlen(input);
    char* out = malloc(len * 2 + 1);
    size_t out_idx = 0;
    for (size_t i = 0; i < len;) {
        int char_len = 0;
        unsigned char c = (unsigned char) input[i];
        if (c < 0x80) char_len = 1;
        else if (c < 0xE0)
            char_len = 2;
        else if (c < 0xF0)
            char_len = 3;
        else
            char_len = 4;

        for (int j = 0; j < char_len; j++) out[out_idx++] = input[i++];

        if (char_len >= 3) {
            out[out_idx++] = ' ';
        }
    }
    out[out_idx] = '\0';
    return out;
}

void ReparseIfRequested() {
    if (needsParse) {
        printf("Reparsing markdown requested\n");

        ResetTextArena();

        if (needsParseFileName == NULL) {
            needsParseFileName = "_main.md";
        }

        char* markdown = ReadMarkdown(needsParseFileName);
        if (!markdown) {
            printf("Failed to read markdown file for reparsing\n");
            return;
        }

        char* spacedMarkdown = InjectSpaces(markdown);
        free(markdown);

        if (globalRenderCommandCache) {
            free(globalRenderCommandCache);
        }
        globalRenderCommandCache = ParseMarkdownToCommands(spacedMarkdown, &needsParse);
        globalRenderCommandCount = needsParse;

        free(spacedMarkdown);
        needsParse = 0;
    }
}

typedef struct {
    Clay_String name;
    Clay_String path;
    Bool active;
} ArchiveEntry;

#include "archives.h"
ArchiveEntry archives[] = {
        {.name = CLAY_STRING("Main Page"), .path = CLAY_STRING("_main.md"), .active = TRUE},
        ARCHIVE_ENTRIES,
};
int archiveCount = sizeof(archives) / sizeof(ArchiveEntry);

void HandleArchiveListItemClick(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
    if (pointerInfo.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        return;
    }

    ArchiveEntry* entry = (ArchiveEntry*) userData;
    for (int i = 0; i < archiveCount; i++) {
        archives[i].active = FALSE;
    }

    entry->active = TRUE;
    RequireMarkdownReparse(entry->path.chars);
}

void SideBar() {
    CLAY({
            .id = CLAY_ID("SideBar"),
            .layout = {
                    .sizing = {
                            CLAY_SIZING_FIXED(250),
                            CLAY_SIZING_GROW(),
                    },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = CLAY_PADDING_ALL(16),
                    .childGap = 8,
            },
            .backgroundColor = {245, 245, 245, 255},
    }) {
        CLAY_TEXT(CLAY_STRING("Archives"),
                  CLAY_TEXT_CONFIG({
                          .fontId = ZHCN_FONT_BIG,
                          .fontSize = 24,
                          .textColor = {36, 41, 46, 255},
                  }));

        for (int i = 0; i < archiveCount; i++) {
            ArchiveEntry* entry = &archives[i];
            CLAY({
                    .id = CLAY_IDI("SidebarItem", i),
                    .layout = {.padding = {12, 12, 8, 12}, .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT()}},
                    .backgroundColor = archives[i].active
                                               ? (Clay_Color){220, 234, 255, 255}
                                               : (Clay_Hovered()
                                                          ? (Clay_Color){240, 240, 240, 255}
                                                          : (Clay_Color){0, 0, 0, 0}),
                    .cornerRadius = CLAY_CORNER_RADIUS(6),
            }) {
                Clay_OnHover(HandleArchiveListItemClick, (intptr_t) entry);
                CLAY_TEXT(entry->name,
                          CLAY_TEXT_CONFIG({
                                  .fontId = ZHCN_FONT_NORMAL,
                                  .fontSize = 18,
                                  .textColor = {36, 41, 46, 255},
                          }));
            }
        }
    }
}

void MainContainer() {
    CLAY({
            .id = CLAY_ID("MainContainer"),
            .layout = {
                    .sizing = {
                            CLAY_SIZING_GROW(),
                            CLAY_SIZING_GROW(),
                    },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = {255, 255, 255, 255},
    }) {
        SideBar();
        MarkdownRenderer(globalRenderCommandCache, globalRenderCommandCount);
    }
}

void MainLoop() {
    Clay_SetLayoutDimensions((Clay_Dimensions){GetScreenWidth(), GetScreenHeight()});

    double ratio = GetDevicePixelRatio();

    Vector2 mousePos = GetMousePosition();
    Vector2 wheelMove = GetMouseWheelMoveV();
    Clay_SetPointerState((Clay_Vector2){mousePos.x, mousePos.y}, IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    Clay_UpdateScrollContainers(
            FALSE,
            (Clay_Vector2){
                    wheelMove.x * SCROLL_SPEED,
                    wheelMove.y * SCROLL_SPEED,
            },
            GetFrameTime());

    Clay_BeginLayout();

    ReparseIfRequested();

    MainContainer();

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(renderCommands, embeddedFonts);
    EndDrawing();
}

void HandleError(Clay_ErrorData errorData) {
    printf("Error: %s\n", errorData.errorText.chars);
}

void LoadEmbeddedResources() {
    char* glyphRange = ReadGlyphRange();

    embeddedFonts[ZHCN_FONT_NORMAL] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 18,
            glyphRange, FONT_NORMAL_WEIGHT, FONT_STYLE_NORMAL);
    embeddedFonts[ZHCN_FONT_NORMAL_BOLD] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 18,
            glyphRange, FONT_BOLD_WEIGHT, FONT_STYLE_NORMAL);
    embeddedFonts[ZHCN_FONT_NORMAL_ITALIC] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 18,
            glyphRange, FONT_NORMAL_WEIGHT, FONT_STYLE_ITALIC);
    embeddedFonts[ZHCN_FONT_NORMAL_BOLD_ITALIC] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 18,
            glyphRange, FONT_BOLD_WEIGHT, FONT_STYLE_ITALIC);

    embeddedFonts[ZHCN_FONT_BIG] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 48,
            glyphRange, FONT_NORMAL_WEIGHT, FONT_STYLE_NORMAL);
    embeddedFonts[ZHCN_FONT_BIG_BOLD] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 48,
            glyphRange, FONT_BOLD_WEIGHT, FONT_STYLE_NORMAL);
    embeddedFonts[ZHCN_FONT_BIG_ITALIC] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 48,
            glyphRange, FONT_NORMAL_WEIGHT, FONT_STYLE_ITALIC);
    embeddedFonts[ZHCN_FONT_BIG_BOLD_ITALIC] = LoadFontAtlasFromJS(
            FONT_NAME_NORMAL, 48,
            glyphRange, FONT_BOLD_WEIGHT, FONT_STYLE_ITALIC);

    embeddedFonts[CODE_FONT_MONOSPACE] = LoadFontAtlasFromJS(
            FONT_NAME_MONOSPACE, 18,
            glyphRange, FONT_NORMAL_WEIGHT, FONT_STYLE_NORMAL);

    free(glyphRange);
}

void UnloadEmbeddedResources() {
    for (int i = 0; i < 16; i++) {
        if (embeddedFonts[i].texture.id != 0) {
            UnloadFont(embeddedFonts[i]);
        }
    }
}

int main() {
    uint64_t clayMemorySize = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, malloc(clayMemorySize));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    Clay_Initialize(arena, (Clay_Dimensions){800, 600}, (Clay_ErrorHandler){HandleError});
    Clay_Raylib_Initialize(800, 600, "test", 0);
    LoadEmbeddedResources();

    Clay_SetMeasureTextFunction(Raylib_MeasureText, embeddedFonts);

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(MainLoop, 0, 1);
#endif

    UnloadEmbeddedResources();
    Clay_Raylib_Close();

    return 0;
}

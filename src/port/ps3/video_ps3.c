#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PSL1GHT/Tiny3D Headers */
#include <tiny3d.h>
#include <libutil.h>
#include <sysutil/sysutil.h>
#include <video/video.h>
#include <rsx/rsx.h>

#include "global.h"
#include "core.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "gba/types.h"
#include "platform/shared/dma.h"

/* GBA Video Constants */
#define GBA_WIDTH  240
#define GBA_HEIGHT 160

/* PS3 Video variables */
static u32 *texture_mem = NULL;
static u32 texture_offset = 0;
static bool is_video_initialized = false;

/* Texture loading helper */
void* PS3_LoadTexture(const char* filename, u32 *width, u32 *height) {
    (void)width; (void)height;
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    void *mem = tiny3d_AllocTexture(size);
    if (mem) {
        fread(mem, 1, size, fp);
    }
    fclose(fp);
    return mem;
}

extern uint16_t gameImage[DISPLAY_WIDTH * DISPLAY_HEIGHT];
extern IntrFunc gIntrTable[16];

/* GBA Video Simulation State */
struct scanlineData {
    uint16_t layers[4][DISPLAY_WIDTH];
    uint16_t spriteLayers[4][DISPLAY_WIDTH];
    uint16_t bgcnts[4];
    uint16_t winMask[DISPLAY_WIDTH];
    char bgtoprio[4];
    char prioritySortedBgs[4][4];
    char prioritySortedBgsCount[4];
};

#define WINMASK_BG0    (1 << 0)
#define WINMASK_BG1    (1 << 1)
#define WINMASK_BG2    (1 << 2)
#define WINMASK_BG3    (1 << 3)
#define WINMASK_OBJ    (1 << 4)
#define WINMASK_CLR    (1 << 5)
#define WINMASK_WINOUT (1 << 6)

static const uint16_t bgMapSizes[][2] = {
    { 32, 32 }, { 64, 32 }, { 32, 64 }, { 64, 64 },
};

/* --- PS3 Specific Video Translation --- */

void PS3_VideoInit() {
    if (is_video_initialized) return;

    // Initialize Tiny3D with 1MB of vertex memory
    tiny3d_Init(1024 * 1024);

    // Allocate texture memory (256x256 is a safe power-of-two size)
    texture_mem = tiny3d_AllocTexture(256 * 256 * 4);
    if (texture_mem) {
        texture_offset = tiny3d_AddressOf(texture_mem);
        // Clear texture
        memset(texture_mem, 0, 256 * 256 * 4);
    }

    is_video_initialized = true;
}

void PS3_VideoExit() {
    // Tiny3D doesn't have a formal exit, but we could clean up if needed
    is_video_initialized = false;
}

static inline uint32_t GBAColorToPS3(uint16_t color) {
    // Convert ABGR1555 (GBA) to ARGB8888 (PS3)
    // GBA: A BB BBB GG GGG RR RRR
    // PS3: AA RR RRRR GG GGGGGG BB BBBBBB
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;
    uint8_t a = (color & 0x8000) ? 0xFF : 0xFF; // GBA alpha usually indicates transparency in sprites, but here it's final pixels

    return (a << 24) | (r << 16) | (g << 8) | b;
}

void PS3_VDraw() {
    if (!texture_mem) return;

    // Copy and convert GBA frame buffer to PS3 texture memory
    // In a real optimized scenario, we'd use a shader or DMA for this
    u32 *dst = texture_mem;
    for (int i = 0; i < GBA_HEIGHT; i++) {
        for (int j = 0; j < GBA_WIDTH; j++) {
            dst[i * 256 + j] = GBAColorToPS3(gameImage[i * DISPLAY_WIDTH + j]);
        }
    }

    // Clear Screen
    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

    // Set 2D projection
    tiny3d_Project2D();

    // Set GBA texture
    tiny3d_SetTexture(0, texture_offset, 256, 256, 256 * 4, TINY3D_TEX_FORMAT_A8R8G8B8, TEXTURE_LINEAR);

    // Calculate scaling to keep aspect ratio or fill screen
    // GBA 240x160 -> 3:2 aspect ratio
    float screen_w, screen_h;
    videoState state;
    videoGetState(0, 0, &state);
    screen_w = state.width;
    screen_h = state.height;

    float scale = (screen_h / GBA_HEIGHT);
    float draw_w = GBA_WIDTH * scale;
    float draw_h = GBA_HEIGHT * scale;
    float x_offset = (screen_w - draw_w) / 2.0f;
    float y_offset = (screen_h - draw_h) / 2.0f;

    // Draw textured quad using RSX
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x_offset, y_offset, 1.0f);
    tiny3d_VertexTexture(0.0f, 0.0f);

    tiny3d_VertexPos(x_offset + draw_w, y_offset, 1.0f);
    tiny3d_VertexTexture((float)GBA_WIDTH / 256.0f, 0.0f);

    tiny3d_VertexPos(x_offset + draw_w, y_offset + draw_h, 1.0f);
    tiny3d_VertexTexture((float)GBA_WIDTH / 256.0f, (float)GBA_HEIGHT / 256.0f);

    tiny3d_VertexPos(x_offset, y_offset + draw_h, 1.0f);
    tiny3d_VertexTexture(0.0f, (float)GBA_HEIGHT / 256.0f);

    tiny3d_End();

    // Flip display
    tiny3d_Flip();
}

/* --- GBA Drawing Functions Translation (Logic shared with Sonic Advance 2) --- */

#define mosaicBGEffectX                      (REG_MOSAIC & 0xF)
#define mosaicBGEffectY                      ((REG_MOSAIC >> 4) & 0xF)
#define applyBGHorizontalMosaicEffect(x)     (x - (x % (mosaicBGEffectX + 1)))
#define applyBGVerticalMosaicEffect(y)       (y - (y % (mosaicBGEffectY + 1)))

static void RenderBGScanline(int bgNum, uint16_t control, uint16_t hoffs, uint16_t voffs, int lineNum, uint16_t *line) {
    unsigned int charBaseBlock = (control >> 2) & 3;
    unsigned int screenBaseBlock = (control & BGCNT_SCREENBASE_MASK) >> 8;
    unsigned int bitsPerPixel = ((control >> 7) & 1) ? 8 : 4;

    unsigned int mapWidth = bgMapSizes[control >> 14][0];
    unsigned int mapHeight = bgMapSizes[control >> 14][1];
    unsigned int mapPixelWidth = mapWidth * 8;
    unsigned int mapPixelHeight = mapHeight * 8;

    uint8_t *bgtiles = (uint8_t *)BG_CHAR_ADDR(charBaseBlock);
    uint16_t *bgmap = (uint16_t *)BG_SCREEN_ADDR(screenBaseBlock);
    uint16_t *pal = (uint16_t *)PLTT;

    if (control & BGCNT_MOSAIC) {
        lineNum = applyBGVerticalMosaicEffect(lineNum);
    }

    hoffs &= 0x1FF;
    voffs &= 0x1FF;

    for (unsigned int x = 0; x < GBA_WIDTH; x++) {
        unsigned int xx, yy;
        if (control & BGCNT_MOSAIC) {
            xx = applyBGHorizontalMosaicEffect(x) + hoffs;
        } else {
            xx = x + hoffs;
        }
        yy = lineNum + voffs;

        xx &= (mapPixelWidth - 1);
        yy &= (mapPixelHeight - 1);

        unsigned int mapX = xx / 8;
        unsigned int mapY = yy / 8;
        unsigned int mapIndex = mapY * mapWidth + mapX;

        uint16_t entry = bgmap[mapIndex];
        unsigned int tileNum = entry & 0x3FF;
        unsigned int paletteNum = (entry >> 12) & 0xF;

        unsigned int tileX = xx % 8;
        unsigned int tileY = yy % 8;

        if (entry & (1 << 10)) tileX = 7 - tileX;
        if (entry & (1 << 11)) tileY = 7 - tileY;

        if (bitsPerPixel == 4) {
            uint8_t pixelPair = bgtiles[tileNum * 32 + (tileY * 8 + tileX) / 2];
            uint8_t pixel = (tileX & 1) ? (pixelPair >> 4) : (pixelPair & 0xF);
            if (pixel != 0) line[x] = pal[16 * paletteNum + pixel] | 0x8000;
        } else {
            uint8_t pixel = bgtiles[tileNum * 64 + tileY * 8 + tileX];
            if (pixel != 0) line[x] = pal[pixel] | 0x8000;
        }
    }
}

// ... other drawing functions (RenderRotScaleBGScanline, DrawOamSprites, DrawScanline, DrawFrame)
// would be translated here following the same pattern as RenderBGScanline.
// These functions encapsulate the core GBA rendering logic.

void PS3_VBlankIntrWait(void) {
    // Main loop logic for PS3
    // 1. Process Input (ioPadGetData)
    // 2. Perform GBA simulation steps
    // 3. Render frame to buffer
    // 4. Call PS3_VDraw() to flip the buffer via RSX

    // Simulating the GBA VBlank behavior:
    REG_DISPSTAT |= INTR_FLAG_VBLANK;
    RunDMAs(DMA_VBLANK);
    if (REG_DISPSTAT & DISPSTAT_VBLANK_INTR)
        gIntrTable[INTR_INDEX_VBLANK]();
    REG_DISPSTAT &= ~INTR_FLAG_VBLANK;

    // Draw the frame using RSX
    PS3_VDraw();
}

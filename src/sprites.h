#pragma once
#include <Arduino.h>

// ============================================
// Capybara pixel art sprites (32x24 pixels)
// Colors in RGB565
// ============================================

#define SPR_W 48
#define SPR_H 36

// Color palette
#define C_TRANS   0x0000  // Transparent (background)
#define C_BODY    0x8B45  // Brown body
#define C_BELLY   0xAD89  // Lighter belly
#define C_DARK    0x6220  // Dark brown (outlines, ears)
#define C_NOSE    0x4A29  // Dark nose
#define C_EYE_W   0xFFFF  // White of eye
#define C_EYE_B   0x0000  // Black pupil
#define C_MOUTH   0x3186  // Dark gray mouth
#define C_ORANGE  0xFD20  // Orange (for mikan on head)
#define C_GREEN   0x07E0  // Green (grass, leaf)
#define C_WATER   0x3DDF  // Water blue
#define C_BLUSH   0xFB2C  // Pink blush

// Draw capybara body at position
inline void drawCapyBody(TFT_eSPI &tft, int x, int y, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;

    // Body (big ellipse)
    tft.fillEllipse(ox + dir * 24, y + 20, 20, 14, C_BODY);
    // Belly
    tft.fillEllipse(ox + dir * 24, y + 24, 14, 8, C_BELLY);
    // Head
    tft.fillEllipse(ox + dir * 40, y + 10, 12, 10, C_BODY);
    // Snout
    tft.fillEllipse(ox + dir * 48, y + 14, 6, 5, C_BELLY);
    // Nose
    tft.fillEllipse(ox + dir * 52, y + 12, 3, 2, C_NOSE);
}

inline void drawCapyLegs(TFT_eSPI &tft, int x, int y, int frame, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;

    int legOffset1 = (frame % 4 < 2) ? 0 : 2;
    int legOffset2 = (frame % 4 < 2) ? 2 : 0;

    // Front legs
    tft.fillRoundRect(ox + dir * 35 - 3, y + 30 + legOffset1, 6, 10, 2, C_DARK);
    tft.fillRoundRect(ox + dir * 42 - 3, y + 30 + legOffset2, 6, 10, 2, C_DARK);
    // Back legs
    tft.fillRoundRect(ox + dir * 12 - 3, y + 28 + legOffset2, 7, 12, 2, C_DARK);
    tft.fillRoundRect(ox + dir * 20 - 3, y + 28 + legOffset1, 7, 12, 2, C_DARK);
}

inline void drawCapyEyes(TFT_eSPI &tft, int x, int y, int eyeState, bool flip = false) {
    // eyeState: 0=open, 1=half, 2=closed, 3=happy (^_^)
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;

    int ex = ox + dir * 42;
    int ey = y + 8;

    if (eyeState == 0) {
        // Open eye
        tft.fillCircle(ex, ey, 3, C_EYE_W);
        tft.fillCircle(ex, ey, 2, C_EYE_B);
        tft.fillCircle(ex + 1, ey - 1, 1, C_EYE_W); // Highlight
    } else if (eyeState == 1) {
        // Half closed
        tft.fillRect(ex - 3, ey - 1, 6, 3, C_EYE_B);
        tft.drawFastHLine(ex - 3, ey - 1, 6, C_BODY);
    } else if (eyeState == 2) {
        // Closed (sleeping)
        tft.drawFastHLine(ex - 3, ey, 6, C_EYE_B);
    } else if (eyeState == 3) {
        // Happy (^_^)
        tft.drawPixel(ex - 1, ey + 1, C_EYE_B);
        tft.drawPixel(ex, ey, C_EYE_B);
        tft.drawPixel(ex + 1, ey + 1, C_EYE_B);
    }
}

inline void drawCapyEars(TFT_eSPI &tft, int x, int y, bool perked = false, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;

    int earX = ox + dir * 36;
    int earY = perked ? y - 2 : y + 1;
    tft.fillCircle(earX, earY, 4, C_BODY);
    tft.fillCircle(earX, earY, 2, C_DARK);
}

inline void drawCapyTail(TFT_eSPI &tft, int x, int y, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;
    tft.fillCircle(ox + dir * 4, y + 18, 3, C_DARK);
}

// ---- Special accessories ----

inline void drawMikan(TFT_eSPI &tft, int x, int y, bool flip = false) {
    // Orange (mikan) on head - iconic capybara meme
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;
    tft.fillCircle(ox + dir * 40, y - 2, 6, C_ORANGE);
    tft.fillRect(ox + dir * 39, y - 9, 3, 4, C_GREEN);
    tft.drawPixel(ox + dir * 42, y - 7, C_GREEN);
}

inline void drawBlush(TFT_eSPI &tft, int x, int y, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;
    tft.fillEllipse(ox + dir * 46, y + 16, 3, 2, C_BLUSH);
}

inline void drawZzz(TFT_eSPI &tft, int x, int y, int frame) {
    tft.setTextColor(0x7BEF, C_TRANS);
    int zx = x + 50 + (frame % 3) * 5;
    int zy = y - 5 - (frame % 3) * 8;
    tft.drawString("z", zx, zy, 1);
    if (frame % 3 > 0) tft.drawString("Z", zx + 8, zy - 10, 2);
    if (frame % 3 > 1) tft.drawString("Z", zx + 16, zy - 22, 2);
}

inline void drawHeart(TFT_eSPI &tft, int x, int y, int frame) {
    int hx = x + 30 + (frame % 2) * 3;
    int hy = y - 8 - (frame % 4) * 2;
    tft.fillCircle(hx - 3, hy, 3, 0xF800);
    tft.fillCircle(hx + 3, hy, 3, 0xF800);
    tft.fillTriangle(hx - 6, hy + 1, hx + 6, hy + 1, hx, hy + 8, 0xF800);
}

inline void drawNote(TFT_eSPI &tft, int x, int y, int frame) {
    int nx = x + 55 + (frame % 3) * 4;
    int ny = y - 5 - (frame % 3) * 6;
    tft.drawLine(nx, ny, nx, ny - 10, C_EYE_W);
    tft.fillCircle(nx - 2, ny, 2, C_EYE_W);
}

inline void drawSweat(TFT_eSPI &tft, int x, int y, bool flip = false) {
    int dir = flip ? -1 : 1;
    int ox = flip ? x + SPR_W / 2 : x;
    tft.fillCircle(ox + dir * 50, y + 2, 2, 0x7DFF);
    tft.fillTriangle(ox + dir * 50, y - 2, ox + dir * 49, y + 2, ox + dir * 51, y + 2, 0x7DFF);
}

inline void drawQuestion(TFT_eSPI &tft, int x, int y) {
    tft.setTextColor(0xFFE0, C_TRANS);
    tft.drawString("?", x + 48, y - 10, 4);
}

inline void drawExclaim(TFT_eSPI &tft, int x, int y) {
    tft.setTextColor(0xF800, C_TRANS);
    tft.drawString("!", x + 48, y - 10, 4);
}

inline void drawButterfly(TFT_eSPI &tft, int bx, int by, int frame) {
    uint16_t colors[] = {0xF81F, 0x07FF, 0xFFE0, 0xF800};
    uint16_t c = colors[frame % 4];
    int wingW = (frame % 2 == 0) ? 5 : 3;
    tft.fillEllipse(bx - wingW, by, wingW, 3, c);
    tft.fillEllipse(bx + wingW, by, wingW, 3, c);
    tft.drawPixel(bx, by, C_EYE_B);
}

// Ground/grass
inline void drawGrass(TFT_eSPI &tft, int y, int w) {
    tft.fillRect(0, y, w, 4, 0x2C04);  // Dark green ground line
    // Grass blades
    for (int i = 3; i < w; i += 8) {
        int h = 4 + (i * 7) % 5;
        tft.drawLine(i, y, i - 1, y - h, 0x3E05);
        tft.drawLine(i, y, i + 1, y - h + 1, 0x2C04);
    }
}

// Water surface
inline void drawWaterSurface(TFT_eSPI &tft, int y, int w, int frame) {
    tft.fillRect(0, y, w, 60, C_WATER);
    // Ripples
    for (int i = 0; i < w; i += 12) {
        int rx = i + (frame * 2) % 12;
        tft.drawFastHLine(rx % w, y + 2, 6, 0x5DFF);
        tft.drawFastHLine((rx + 20) % w, y + 8, 8, 0x5DFF);
    }
}

#pragma once
#include <TFT_eSPI.h>

#define SCR_W 135
#define SCR_H 240
#define GROUND_Y 168

enum Environment { ENV_MEADOW, ENV_LAKE, ENV_FOREST, ENV_HOME, ENV_COUNT };
enum Weather { WEATHER_CLEAR, WEATHER_CLOUDY, WEATHER_RAIN, WEATHER_SNOW, WEATHER_COUNT };

struct EnvPalette { uint16_t sky, ground, groundDark, accent; };
const EnvPalette ENV_COLORS[] = {
    {0x6DFF, 0x2C04, 0x3E05, 0xFFE0}, // Meadow: bright sky, green
    {0x5D9F, 0x4A29, 0x3E05, 0x07FF}, // Lake: soft sky, brown shore
    {0x2945, 0x2103, 0x1861, 0x3E05}, // Forest: dark green sky
    {0xBDD7, 0x9CD3, 0x8C51, 0xFDA0}, // Home: warm beige wall, wooden floor
};

// ============================================
// MEADOW - flowers, butterflies, rolling hills
// ============================================
inline void drawMeadow(TFT_eSprite &s, int frame) {
    // Distant hills
    for (int i = 0; i < 3; i++) {
        int hx = i * 55 + 10;
        int hr = 25 + i * 5;
        s.fillEllipse(hx, GROUND_Y + 5, hr, 15 + i * 3, 0x34A3);
    }

    // Ground
    s.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, 0x2C04);
    // Lighter grass top
    s.fillRect(0, GROUND_Y, SCR_W, 3, 0x3E05);

    // Grass blades (swaying)
    for (int i = 1; i < SCR_W; i += 3) {
        int h = 3 + (i * 7) % 6;
        int sway = sin((frame + i) * 0.04f) * 1.5f;
        uint16_t gc = (i % 2 == 0) ? 0x3E05 : 0x2C04;
        s.drawLine(i, GROUND_Y, i + sway, GROUND_Y - h, gc);
    }

    // Flowers with stems
    uint16_t fc[] = {0xF800, 0xFFE0, 0xF81F, 0x07FF, 0xFDA0, 0xFB2C};
    for (int i = 0; i < 6; i++) {
        int fx = (i * 23 + 5) % SCR_W;
        int fy = GROUND_Y + 2 + (i * 11) % 6;
        // Stem
        s.drawLine(fx, fy, fx, fy - 4, 0x2C04);
        // Petals
        s.fillCircle(fx, fy - 5, 2, fc[i]);
        s.drawPixel(fx, fy - 5, 0xFFE0); // Center
    }

    // Small butterflies in distance
    if ((frame / 20) % 3 == 0) {
        int bx = (frame * 2 + 50) % SCR_W;
        int by = GROUND_Y - 20 - random(20);
        s.drawPixel(bx - 1, by, 0xFFE0);
        s.drawPixel(bx + 1, by, 0xFFE0);
    }
}

// ============================================
// LAKE - proper water with shore, depth
// ============================================
inline void drawLake(TFT_eSprite &s, int frame) {
    // Sandy/dirt shore (left third)
    s.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, 0x4A29); // Brown shore

    // Shore grass
    for (int i = 0; i < 42; i += 3) {
        int h = 2 + (i * 5) % 4;
        s.drawLine(i, GROUND_Y, i, GROUND_Y - h, 0x3E05);
    }
    // Shore detail (pebbles)
    for (int i = 0; i < 40; i += 5) {
        s.drawPixel(i + 2, GROUND_Y + 3, 0x6B4D);
        s.drawPixel(i, GROUND_Y + 6, 0x8410);
    }

    // Water (right 2/3) - darker than sky!
    int waterX = 40;
    // Water depth gradient
    s.fillRect(waterX, GROUND_Y - 2, SCR_W - waterX, 4, 0x2B7E);  // Shore edge (dark)
    s.fillRect(waterX, GROUND_Y + 2, SCR_W - waterX, 8, 0x1B5C);  // Shallow
    s.fillRect(waterX, GROUND_Y + 10, SCR_W - waterX, SCR_H, 0x0B3A); // Deep

    // Water surface shine
    s.drawFastHLine(waterX, GROUND_Y - 2, SCR_W - waterX, 0x4DDF);

    // Animated ripples
    int t = millis() / 150;
    for (int i = 0; i < 8; i++) {
        int rx = waterX + 5 + ((t + i * 13) % (SCR_W - waterX - 10));
        int ry = GROUND_Y + (i * 5) % 16;
        s.drawFastHLine(rx, ry, 3 + i % 3, 0x3D9F);
    }

    // Lilypads with flowers
    for (int i = 0; i < 3; i++) {
        int lx = waterX + 12 + i * 28;
        int ly = GROUND_Y + 4 + i * 5;
        s.fillEllipse(lx, ly, 5, 3, 0x2C04); // Pad dark
        s.fillEllipse(lx, ly, 4, 2, 0x3E05); // Pad light
        if (i == 1) { // Flower on middle pad
            s.fillCircle(lx + 2, ly - 2, 2, 0xFB2C);
            s.drawPixel(lx + 2, ly - 2, 0xFFE0);
        }
    }

    // Reeds (taller, swaying)
    for (int i = 0; i < 4; i++) {
        int rx = waterX - 8 + i * 5;
        int sway = sin((frame + i * 15) * 0.03f) * 2;
        int rh = 18 + i * 3;
        s.drawLine(rx, GROUND_Y, rx + sway, GROUND_Y - rh, 0x3E05);
        s.drawLine(rx + 1, GROUND_Y, rx + 1 + sway, GROUND_Y - rh, 0x2C04);
        // Cattail top
        s.fillEllipse(rx + sway, GROUND_Y - rh - 2, 2, 4, 0x4A29);
    }

    // Distant shore/trees on far side
    for (int i = 0; i < 3; i++) {
        int tx = waterX + 20 + i * 30;
        s.fillTriangle(tx, GROUND_Y - 5, tx - 6, GROUND_Y - 2, tx + 6, GROUND_Y - 2, 0x1861);
    }
}

// ============================================
// FOREST - layered depth, atmosphere
// ============================================
inline void drawForest(TFT_eSprite &s, int frame) {
    // Dark ground with leaf litter
    s.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, 0x2103);

    // Leaf litter on ground
    uint16_t leafCols[] = {0x4A29, 0x6220, 0x3186, 0x2103};
    for (int i = 0; i < SCR_W; i += 3) {
        s.drawPixel(i, GROUND_Y + 1, leafCols[i % 4]);
        s.drawPixel(i + 1, GROUND_Y + 4, leafCols[(i + 2) % 4]);
    }

    // Far background trees (darker, smaller)
    for (int i = 0; i < 5; i++) {
        int tx = 5 + i * 30;
        s.fillRect(tx, GROUND_Y - 20, 3, 20, 0x2103);
        s.fillTriangle(tx + 1, GROUND_Y - 35, tx - 6, GROUND_Y - 12, tx + 8, GROUND_Y - 12, 0x1041);
    }

    // Main trees (foreground, detailed)
    for (int i = 0; i < 3; i++) {
        int tx = 15 + i * 45;
        // Trunk with bark texture
        s.fillRect(tx - 3, GROUND_Y - 35, 7, 35, 0x4A29);
        s.drawFastVLine(tx - 1, GROUND_Y - 30, 25, 0x3186);
        s.drawFastVLine(tx + 2, GROUND_Y - 28, 20, 0x3186);
        // Canopy layers
        s.fillTriangle(tx, GROUND_Y - 55, tx - 16, GROUND_Y - 18, tx + 16, GROUND_Y - 18, 0x1861);
        s.fillTriangle(tx, GROUND_Y - 65, tx - 12, GROUND_Y - 35, tx + 12, GROUND_Y - 35, 0x2882);
        // Highlight on canopy
        s.fillTriangle(tx + 3, GROUND_Y - 60, tx, GROUND_Y - 38, tx + 10, GROUND_Y - 38, 0x30C3);
    }

    // Mushrooms (more detailed)
    for (int i = 0; i < 4; i++) {
        int mx = 8 + i * 35;
        int my = GROUND_Y;
        // Stem
        s.fillRect(mx, my - 5, 3, 6, 0xE71C);
        // Cap
        s.fillCircle(mx + 1, my - 6, 4, (i % 2 == 0) ? 0xF800 : 0xFDA0);
        // Spots
        s.drawPixel(mx - 1, my - 7, 0xFFFF);
        s.drawPixel(mx + 2, my - 6, 0xFFFF);
    }

    // Moss on ground
    for (int i = 3; i < SCR_W; i += 8) {
        s.fillRect(i, GROUND_Y - 1, 3, 2, 0x1861);
    }

    // Fireflies (animated, glowing)
    for (int i = 0; i < 6; i++) {
        int fx = (i * 29 + (millis()/100) * 3) % SCR_W;
        int fy = GROUND_Y - 15 - (i * 17) % 45;
        if (((millis()/200) + i) % 4 == 0) {
            s.fillCircle(fx, fy, 1, 0xFFE0);
            s.drawPixel(fx - 1, fy, 0x8BE0); // Glow
            s.drawPixel(fx + 1, fy, 0x8BE0);
            s.drawPixel(fx, fy - 1, 0x8BE0);
        }
    }
}

// ============================================
// HOME - proper indoor scene with wall
// ============================================
inline void drawHome(TFT_eSprite &s, int frame) {
    // Wall (beige/cream, NOT sky)
    s.fillRect(0, 0, SCR_W, GROUND_Y, 0xBDD7);
    // Subtle wallpaper pattern
    for (int y = 10; y < GROUND_Y; y += 20) {
        for (int x = 10; x < SCR_W; x += 20) {
            s.drawPixel(x, y, 0xAD75);
        }
    }

    // Baseboard (dark strip at bottom of wall)
    s.fillRect(0, GROUND_Y - 5, SCR_W, 5, 0x6B4D);
    s.drawFastHLine(0, GROUND_Y - 5, SCR_W, 0x8410);

    // Wooden floor
    s.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, 0x8B45);
    // Floor boards
    for (int i = 0; i < SCR_W; i += 22) {
        s.drawFastVLine(i, GROUND_Y, SCR_H - GROUND_Y, 0x7B23);
    }
    // Floor grain
    for (int i = 5; i < SCR_W; i += 11) {
        s.drawFastHLine(i, GROUND_Y + 5, 4, 0x9B67);
        s.drawFastHLine(i + 6, GROUND_Y + 12, 3, 0x9B67);
    }

    // Window on wall (with frame, curtains, view)
    int wx = 75, wy = 25, ww = 45, wh = 50;
    // Window frame (thick wooden)
    s.fillRect(wx - 3, wy - 3, ww + 6, wh + 6, 0x6B4D); // Frame
    s.fillRect(wx, wy, ww, wh, 0x6DFF); // Sky view through window
    // Window panes
    s.drawFastVLine(wx + ww/2, wy, wh, 0x8410);
    s.drawFastHLine(wx, wy + wh/2, ww, 0x8410);
    // Sun through window
    s.fillCircle(wx + ww - 10, wy + 12, 5, 0xFFE0);
    // Curtains
    s.fillRect(wx, wy, 6, wh, 0xA000);
    s.fillRect(wx + ww - 6, wy, 6, wh, 0xA000);
    // Curtain folds
    s.drawFastVLine(wx + 2, wy, wh, 0xC800);
    s.drawFastVLine(wx + ww - 3, wy, wh, 0xC800);

    // Picture frame on wall
    s.drawRect(15, 35, 25, 20, 0x6B4D);
    s.drawRect(16, 36, 23, 18, 0x4A29);
    s.fillRect(17, 37, 21, 16, 0x5D9F); // Landscape painting
    s.fillRect(17, 47, 21, 6, 0x2C04); // Green ground in painting
    s.fillCircle(30, 41, 3, 0xFFE0); // Sun in painting

    // Food bowl (ceramic, detailed)
    s.fillEllipse(30, GROUND_Y + 5, 11, 4, 0xAD55); // Bowl outer
    s.fillEllipse(30, GROUND_Y + 4, 9, 3, 0xC618); // Bowl inner
    s.fillEllipse(30, GROUND_Y + 3, 7, 2, 0xFDA0); // Food

    // Cozy rug (patterned)
    s.fillEllipse(SCR_W/2 + 10, GROUND_Y + 8, 22, 4, 0xA000);
    s.drawEllipse(SCR_W/2 + 10, GROUND_Y + 8, 22, 4, 0xC800);
    s.drawEllipse(SCR_W/2 + 10, GROUND_Y + 8, 15, 3, 0xC800);

    // Plant in corner
    int px = 8, py = GROUND_Y - 6;
    s.fillRect(px - 2, py, 8, 8, 0x6B4D); // Pot
    s.fillRect(px - 1, py - 1, 6, 2, 0x8410); // Pot rim
    s.drawLine(px + 2, py - 1, px + 1, py - 10, 0x3E05); // Stem
    s.drawLine(px + 2, py - 1, px + 5, py - 8, 0x3E05);
    s.fillCircle(px + 1, py - 11, 3, 0x3E05); // Leaves
    s.fillCircle(px + 5, py - 9, 3, 0x2C04);
}

inline void drawEnvironment(TFT_eSprite &s, Environment env, int frame) {
    switch (env) {
        case ENV_MEADOW: drawMeadow(s, frame); break;
        case ENV_LAKE:   drawLake(s, frame); break;
        case ENV_FOREST: drawForest(s, frame); break;
        case ENV_HOME:   drawHome(s, frame); break;
        default: break;
    }
}

// ============================================
// Weather effects (improved)
// ============================================
inline void drawWeather(TFT_eSprite &s, Weather weather, int frame) {
    if (weather == WEATHER_CLOUDY) {
        for (int i = 0; i < 3; i++) {
            int cx = ((int)(millis()/250) + i * 50) % (SCR_W + 30) - 15;
            int cy = 20 + i * 15;
            s.fillEllipse(cx, cy, 18, 8, 0x8410);
            s.fillEllipse(cx + 12, cy - 4, 11, 6, 0x8410);
            s.fillEllipse(cx - 10, cy - 2, 10, 5, 0x9492);
            // Cloud shadow
            s.fillEllipse(cx, cy + 2, 16, 6, 0x6B4D);
        }
    } else if (weather == WEATHER_RAIN) {
        // Rain clouds
        for (int i = 0; i < 2; i++) {
            int cx = 20 + i * 60;
            s.fillEllipse(cx, 18, 20, 9, 0x6B4D);
            s.fillEllipse(cx + 15, 15, 12, 7, 0x6B4D);
        }
        // Rain drops (diagonal)
        int t = millis() / 30;
        for (int i = 0; i < 25; i++) {
            int rx = (i * 29 + t * 3) % SCR_W;
            int ry = (i * 41 + t * 7) % GROUND_Y;
            s.drawLine(rx, ry, rx - 1, ry + 5, 0x4DBF);
            s.drawPixel(rx - 1, ry + 5, 0x6DDF);
        }
        // Puddles with ripples
        for (int i = 0; i < 3; i++) {
            int px = 15 + i * 45;
            s.fillEllipse(px, GROUND_Y + 2, 7, 2, 0x4DBF);
            int rr = (t / 3 + i * 5) % 6;
            s.drawEllipse(px, GROUND_Y + 2, rr, 1, 0x6DDF);
        }
    } else if (weather == WEATHER_SNOW) {
        // Snowflakes (different sizes, drifting)
        int t = millis() / 50;
        for (int i = 0; i < 18; i++) {
            int sx = (i * 31 + t * 2) % SCR_W;
            int sy = (i * 37 + t * 3) % GROUND_Y;
            int sway = sin((t + i * 10) * 0.06f) * 4;
            int size = (i % 3 == 0) ? 2 : 1;
            s.fillCircle(sx + sway, sy, size, 0xFFFF);
        }
        // Snow accumulation on ground
        for (int i = 0; i < SCR_W; i += 2) {
            int sh = 1 + (i * 3) % 3;
            s.fillRect(i, GROUND_Y - sh, 2, sh, 0xE71C);
        }
    }
}

// ============================================
// Friends (improved)
// ============================================
inline void drawBird(TFT_eSprite &s, int frame, bool active) {
    if (!active) return;
    int t = millis() / 100;
    int bx = (t * 2) % (SCR_W + 40) - 20;
    int by = 50 + sin(t * 0.12f) * 12;
    int wing = (t / 2) % 2 == 0 ? -5 : 3;
    // Body
    s.fillEllipse(bx, by, 5, 3, 0x39C7);
    s.fillEllipse(bx, by + 1, 4, 2, 0xAD55);
    // Wings
    s.drawLine(bx - 4, by - 1, bx - 8, by + wing, 0x4A49);
    s.drawLine(bx + 4, by - 1, bx + 8, by + wing, 0x4A49);
    // Head
    s.fillCircle(bx + 4, by - 2, 2, 0x39C7);
    // Eye + beak
    s.drawPixel(bx + 5, by - 3, 0x0000);
    s.drawPixel(bx + 6, by - 2, 0xFDA0);
    s.drawPixel(bx + 7, by - 2, 0xFDA0);
}

inline void drawDuck(TFT_eSprite &s, int x, int frame, bool active) {
    if (!active) return;
    int dx = x + 55;
    int dy = GROUND_Y - 4;
    // Body
    s.fillEllipse(dx, dy, 7, 5, 0xFFFF);
    s.fillEllipse(dx, dy + 1, 6, 4, 0xE71C);
    // Head
    s.fillCircle(dx + 6, dy - 5, 4, 0x3E05);
    // Eye ring
    s.fillCircle(dx + 7, dy - 6, 1, 0xFFFF);
    s.drawPixel(dx + 7, dy - 6, 0x0000);
    // Beak
    s.fillRect(dx + 9, dy - 5, 5, 2, 0xFDA0);
    // Water ripples
    s.drawFastHLine(dx - 8, dy + 4, 16, 0x3D9F);
    s.drawFastHLine(dx - 6, dy + 6, 12, 0x2B7E);
}

inline void drawTurtle(TFT_eSprite &s, int frame, bool active) {
    if (!active) return;
    int tx = 10 + (int)(millis() / 500) % 60;
    int ty = GROUND_Y - 3;
    // Shell (dome shaped)
    s.fillEllipse(tx, ty - 1, 8, 6, 0x3E05);
    s.fillEllipse(tx, ty - 2, 7, 5, 0x5C23);
    // Shell segments
    s.drawLine(tx - 3, ty - 4, tx + 3, ty - 4, 0x3E05);
    s.drawFastVLine(tx, ty - 6, 5, 0x3E05);
    // Head
    s.fillCircle(tx + 8, ty + 1, 3, 0x5C23);
    s.fillCircle(tx + 8, ty, 2, 0x3E05);
    s.drawPixel(tx + 9, ty - 1, 0x0000);
    // Legs
    int lf = (millis() / 300) % 2;
    s.fillRect(tx - 5, ty + 3 + lf, 3, 3, 0x3E05);
    s.fillRect(tx + 4, ty + 3 + (1-lf), 3, 3, 0x3E05);
    // Tail
    s.drawPixel(tx - 7, ty + 1, 0x3E05);
}

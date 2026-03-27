#pragma once
#include <TFT_eSPI.h>
#include "environments.h"

#define GAME_GROUND 190

enum GameType {
    GAME_NONE = 0,
    GAME_RUNNER,
    GAME_CATCH,
    GAME_RHYTHM,
    GAME_TYPE_COUNT  // 3 games (memory removed)
};

enum GameState { GS_IDLE, GS_INVITE, GS_MENU, GS_COUNTDOWN, GS_PLAYING, GS_GAMEOVER };
struct GameResult { int score, foodBonus, happyBonus; };

class MiniGame {
public:
    GameState state = GS_IDLE;
    GameType type = GAME_NONE;
    int score = 0, lives = 3, frame = 0;
    float timer = 0, countdownTimer = 0, inviteTimer = 0, gameOverTimer = 0;
    bool rewardApplied = false;
    bool btnDown = false, btnPressed = false, _prevBtn = false;
    int nextGameIdx = 0;
    int menuSelection = 0; // 0=runner, 1=catch, 2=rhythm
    GameResult lastResult = {0, 0, 0};
    bool initialized = false;

    void showInvite() {
        if (state != GS_IDLE) return;
        state = GS_INVITE; inviteTimer = 8.0f;
        type = (GameType)(1 + nextGameIdx % (GAME_TYPE_COUNT - 1));
    }
    void startGame(GameType t) {
        type = t; state = GS_COUNTDOWN; countdownTimer = 3.0f;
        score = 0; lives = 3; frame = 0; timer = 0;
        initialized = false; rewardApplied = false;
        // Playing costs energy
        if (pEnergy) *pEnergy = max(0.0f, *pEnergy - 15.0f);
    }

    float *pEnergy = nullptr;
    // Pointers to pet stats - set from main
    float *pHappiness = nullptr;
    float *pHunger = nullptr;

    void endGame() {
        state = GS_GAMEOVER; gameOverTimer = 4.0f;
        lastResult.score = score;
        lastResult.foodBonus = min(score * 2, 30);
        lastResult.happyBonus = min(score * 3, 40);
        nextGameIdx++;
        // Apply rewards immediately
        if (pHappiness) *pHappiness = min(100.0f, *pHappiness + lastResult.happyBonus);
        if (pHunger) *pHunger = max(0.0f, *pHunger - lastResult.foodBonus);
        Serial.printf("[REWARD] score=%d +%dhp -%dhg\n", score, lastResult.happyBonus, lastResult.foodBonus);
    }
    void dismiss() { state = GS_IDLE; type = GAME_NONE; }
    void openMenu() { state = GS_MENU; menuSelection = 0; frame = 0; }
    void setButton(bool down) { btnDown = down; }

    bool update(float dt) {
        btnPressed = btnDown && !_prevBtn;
        _prevBtn = btnDown;
        frame++;
        switch (state) {
            case GS_INVITE:
                inviteTimer -= dt; if (inviteTimer <= 0) dismiss();
                if (btnPressed) startGame(type); return false;
            case GS_MENU:
                // Short press cycles selection, handled externally via menuSelect()
                return true;
            case GS_COUNTDOWN:
                countdownTimer -= dt; if (countdownTimer <= 0) state = GS_PLAYING; return true;
            case GS_PLAYING:
                timer += dt; updateGame(dt);
                if (lives <= 0 || timer > 60.0f) endGame(); return true;
            case GS_GAMEOVER:
                gameOverTimer -= dt;
                // Ignore button for first 1.5 sec (prevent accidental skip)
                if (gameOverTimer <= 0 || (gameOverTimer < 2.5f && btnPressed)) dismiss();
                return true;
            default: return false;
        }
    }

    void draw(TFT_eSprite &s) {
        switch (state) {
            case GS_INVITE: drawInvite(s); break;
            case GS_MENU: drawMenu(s); break;
            case GS_COUNTDOWN: drawCountdown(s); break;
            case GS_PLAYING: drawGame(s); break;
            case GS_GAMEOVER: drawGameOver(s); break;
            default: break;
        }
    }

private:
    // === RUNNER ===
    struct { float playerY, velY; bool jumping; float obstacles[4]; float speed; int passed; } runner;
    // === CATCH ===
    struct {
        float playerX; int playerDir;
        struct { float x, y; bool active; int type; } items[6]; // 0=orange,1=melon,2=heart,3=star,4=bomb,5=lightning
        float spawnTimer; int caught; int combo; float magnetTimer; bool magnet;
        float shieldTimer; bool shield; int coins;
    } catcher;
    // === RHYTHM ===
    struct {
        struct { float time; bool hit; bool missed; int lane; } notes[40]; // lane: 0=left,1=center,2=right
        int noteCount, nextNote; float songTime;
        int hits, combo, maxCombo, missPresses;
    } rhythm;

    void updateGame(float dt) {
        switch (type) {
            case GAME_RUNNER: updateRunner(dt); break;
            case GAME_CATCH: updateCatch(dt); break;
            case GAME_RHYTHM: updateRhythm(dt); break;
            default: break;
        }
    }

    // ==============================
    // RUNNER - spaced obstacles, fair gameplay
    // ==============================
    void updateRunner(float dt) {
        if (!initialized) {
            runner.playerY = 0; runner.velY = 0;
            runner.jumping = false; runner.speed = 50; runner.passed = 0;
            // 4 obstacles, well spaced apart
            for (int i = 0; i < 4; i++)
                runner.obstacles[i] = SCR_W + 50 + i * 70;
            initialized = true;
        }

        if (btnPressed && !runner.jumping) {
            runner.velY = -220.0f;
            runner.jumping = true;
        }

        runner.velY += 500.0f * dt;
        runner.playerY += runner.velY * dt;
        if (runner.playerY >= 0) { runner.playerY = 0; runner.velY = 0; runner.jumping = false; }

        // Gradual speed increase
        runner.speed = 50 + timer * 2;

        for (int i = 0; i < 4; i++) {
            runner.obstacles[i] -= runner.speed * dt;
            if (runner.obstacles[i] < -15) {
                // Respawn with guaranteed gap (min 60px between obstacles)
                float maxOther = 0;
                for (int j = 0; j < 4; j++) if (j != i && runner.obstacles[j] > maxOther) maxOther = runner.obstacles[j];
                runner.obstacles[i] = max((float)SCR_W + 20, maxOther + 60 + random(40));
                runner.passed++;
                score = runner.passed;
            }
            // Collision
            float ox = runner.obstacles[i];
            if (ox > 15 && ox < 35 && runner.playerY > -15) {
                lives--;
                runner.obstacles[i] = SCR_W + 80;
            }
        }
    }

    // ==============================
    // CATCH - powerups + better mechanics
    // ==============================
    void updateCatch(float dt) {
        if (!initialized) {
            catcher.playerX = SCR_W / 2; catcher.playerDir = 1;
            catcher.spawnTimer = 1.5f; catcher.caught = 0; catcher.combo = 0;
            catcher.magnetTimer = 0; catcher.magnet = false;
            catcher.shieldTimer = 0; catcher.shield = false; catcher.coins = 0;
            for (int i = 0; i < 6; i++) catcher.items[i].active = false;
            initialized = true;
        }

        if (btnPressed) catcher.playerDir *= -1;

        catcher.playerX += catcher.playerDir * 45 * dt;
        if (catcher.playerX < 10) { catcher.playerX = 10; catcher.playerDir = 1; }
        if (catcher.playerX > SCR_W - 10) { catcher.playerX = SCR_W - 10; catcher.playerDir = -1; }

        // Powerup timers
        if (catcher.magnet) { catcher.magnetTimer -= dt; if (catcher.magnetTimer <= 0) catcher.magnet = false; }
        if (catcher.shield) { catcher.shieldTimer -= dt; if (catcher.shieldTimer <= 0) catcher.shield = false; }

        // Spawn
        catcher.spawnTimer -= dt;
        if (catcher.spawnTimer <= 0) {
            for (int i = 0; i < 6; i++) {
                if (!catcher.items[i].active) {
                    catcher.items[i].x = 10 + random(SCR_W - 20);
                    catcher.items[i].y = -10;
                    catcher.items[i].active = true;
                    // Item types: 0=orange(common), 1=melon(common), 2=heart(rare), 3=star(rare), 4=bomb(danger)
                    int r = random(20);
                    if (r < 7) catcher.items[i].type = 0;      // orange 35%
                    else if (r < 13) catcher.items[i].type = 1; // melon 30%
                    else if (r < 15) catcher.items[i].type = 2; // heart 10%
                    else if (r < 17) catcher.items[i].type = 3; // star 10%
                    else catcher.items[i].type = 4;              // bomb 15%
                    break;
                }
            }
            catcher.spawnTimer = max(0.5f, 1.5f - timer * 0.015f);
        }

        float fallSpeed = 35 + timer * 1.5f;
        for (int i = 0; i < 6; i++) {
            if (!catcher.items[i].active) continue;

            // Magnet: attract food items towards player
            if (catcher.magnet && catcher.items[i].type < 2) {
                float dx = catcher.playerX - catcher.items[i].x;
                catcher.items[i].x += dx * 2.0f * dt;
            }

            catcher.items[i].y += fallSpeed * dt;

            // Catch
            if (catcher.items[i].y > GAME_GROUND - 15 &&
                abs(catcher.items[i].x - catcher.playerX) < 14) {
                catcher.items[i].active = false;
                int t = catcher.items[i].type;
                if (t == 0 || t == 1) { // Food
                    catcher.caught++; catcher.combo++;
                    score = catcher.caught + catcher.combo / 3;
                } else if (t == 2) { // Heart = +1 life
                    lives = min(lives + 1, 5);
                } else if (t == 3) { // Star = magnet powerup
                    catcher.magnet = true; catcher.magnetTimer = 5.0f;
                } else if (t == 4) { // Bomb
                    if (catcher.shield) { catcher.shield = false; } // Shield absorbs
                    else { lives--; catcher.combo = 0; }
                }
            }

            // Missed food = break combo
            if (catcher.items[i].y > GAME_GROUND + 10) {
                if (catcher.items[i].type < 2) catcher.combo = 0;
                catcher.items[i].active = false;
            }
        }

        // Combo bonus: every 5 catches = shield
        if (catcher.combo > 0 && catcher.combo % 5 == 0 && !catcher.shield) {
            catcher.shield = true; catcher.shieldTimer = 8.0f;
        }
    }

    // ==============================
    // RHYTHM - mis-press penalty + better visuals
    // ==============================
    void updateRhythm(float dt) {
        if (!initialized) {
            rhythm.noteCount = 0;
            float t = 2.0f;
            while (t < 30.0f && rhythm.noteCount < 40) {
                rhythm.notes[rhythm.noteCount].time = t;
                rhythm.notes[rhythm.noteCount].hit = false;
                rhythm.notes[rhythm.noteCount].missed = false;
                rhythm.notes[rhythm.noteCount].lane = random(3);
                rhythm.noteCount++;
                t += 0.6f + random(60) / 100.0f;
            }
            rhythm.nextNote = 0; rhythm.songTime = 0;
            rhythm.hits = 0; rhythm.combo = 0; rhythm.maxCombo = 0;
            rhythm.missPresses = 0;
            initialized = true;
        }

        rhythm.songTime += dt;

        // Missed notes = lose life
        while (rhythm.nextNote < rhythm.noteCount &&
               rhythm.songTime > rhythm.notes[rhythm.nextNote].time + 0.5f) {
            if (!rhythm.notes[rhythm.nextNote].hit) {
                rhythm.notes[rhythm.nextNote].missed = true;
                rhythm.combo = 0;
                lives--;
            }
            rhythm.nextNote++;
        }

        if (btnPressed) {
            bool hitSomething = false;
            for (int i = max(0, rhythm.nextNote - 1); i < rhythm.noteCount; i++) {
                if (rhythm.notes[i].hit || rhythm.notes[i].missed) continue;
                float diff = abs(rhythm.songTime - rhythm.notes[i].time);
                if (diff < 0.45f) {
                    rhythm.notes[i].hit = true;
                    rhythm.hits++; rhythm.combo++;
                    if (rhythm.combo > rhythm.maxCombo) rhythm.maxCombo = rhythm.combo;
                    score = rhythm.hits + rhythm.maxCombo / 2;
                    hitSomething = true;
                    break;
                }
            }
            if (!hitSomething) {
                // Mis-press = instant life loss
                lives--;
                rhythm.combo = 0;
            }
        }

        if (rhythm.nextNote >= rhythm.noteCount) endGame();
    }

    // ============================================
    // DRAWING
    // ============================================
    void drawMenu(TFT_eSprite &s) {
        s.fillSprite(0x0841);
        s.setTextDatum(MC_DATUM);
        s.setTextColor(0x07FF);
        s.drawString("GAMES", SCR_W/2, 15, 4);
        s.setTextColor(0x4A49);
        s.drawString("tap=next  hold=play", SCR_W/2, 38, 1);

        const char* names[] = {"RUNNER", "CATCH", "RHYTHM"};
        const char* descs[] = {"Jump obstacles!", "Catch fruit!", "Hit the beat!"};
        uint16_t cols[] = {0x07E0, 0xFDA0, 0xF81F};
        // Icons
        for (int i = 0; i < 3; i++) {
            int y = 60 + i * 55;
            bool sel = (i == menuSelection);
            // Box
            if (sel) {
                s.fillRoundRect(8, y-2, SCR_W-16, 46, 5, 0x10A2);
                s.drawRoundRect(8, y-2, SCR_W-16, 46, 5, cols[i]);
                // Animated arrow
                int ax = 3 + (frame/4)%3;
                s.fillTriangle(ax, y+18, ax+5, y+14, ax+5, y+22, cols[i]);
            } else {
                s.fillRoundRect(8, y-2, SCR_W-16, 46, 5, 0x0841);
                s.drawRoundRect(8, y-2, SCR_W-16, 46, 5, 0x2104);
            }
            // Mini icon
            if (i == 0) { // Runner: capybara + obstacle
                s.fillEllipse(25, y+20, 6, 4, 0x9B26);
                s.fillEllipse(40, y+22, 4, 3, 0x6B4D);
            } else if (i == 1) { // Catch: orange
                s.fillCircle(25, y+18, 5, 0xFD20);
                s.fillRect(24, y+11, 2, 3, 0x3E05);
            } else { // Rhythm: notes
                s.fillCircle(22, y+20, 3, 0x07FF);
                s.drawFastVLine(25, y+12, 8, 0x07FF);
                s.fillCircle(32, y+16, 3, 0xF81F);
                s.drawFastVLine(35, y+8, 8, 0xF81F);
            }
            // Text
            s.setTextDatum(TL_DATUM);
            s.setTextColor(sel ? cols[i] : 0x6B4D);
            s.drawString(names[i], 50, y+5, 2);
            s.setTextColor(sel ? 0xFFFF : 0x4A49);
            s.drawString(descs[i], 50, y+24, 1);
        }
        s.setTextDatum(MC_DATUM);
    }

    void drawInvite(TFT_eSprite &s) {
        int by = 15, bh = 55;
        // Glow border
        s.fillRoundRect(6, by-2, SCR_W-12, bh+4, 7, 0x0293);
        s.fillRoundRect(8, by, SCR_W-16, bh, 5, 0x0841);
        const char* names[] = {"", "RUNNER!", "CATCH!", "RHYTHM!"};
        s.setTextDatum(MC_DATUM);
        s.setTextColor(0xFFE0); s.drawString(names[type], SCR_W/2, by+15, 4);
        if ((frame/8)%2==0) { s.setTextColor(0xFFFF); s.drawString("BOOT=Play!", SCR_W/2, by+38, 2); }
        int barW = (int)((inviteTimer/8.0f)*(SCR_W-24));
        s.fillRect(12, by+bh-4, barW, 2, 0x07FF);
    }

    void drawCountdown(TFT_eSprite &s) {
        s.fillSprite(0x0000);
        // Radial glow effect
        int num = (int)countdownTimer + 1;
        uint16_t cols[] = {0xF800, 0xFFE0, 0x07E0};
        uint16_t c = cols[constrain(num-1,0,2)];
        int r = 30 + (int)((countdownTimer - (int)countdownTimer) * 15);
        s.drawCircle(SCR_W/2, SCR_H/2, r, c);
        s.drawCircle(SCR_W/2, SCR_H/2, r-1, c);
        s.setTextDatum(MC_DATUM); s.setTextColor(c);
        char buf[4]; snprintf(buf,4,"%d",num);
        s.drawString(buf, SCR_W/2, SCR_H/2, 7);
        const char* names[] = {"","RUNNER","CATCH","RHYTHM"};
        s.setTextColor(0x7BEF); s.drawString(names[type], SCR_W/2, SCR_H/2+50, 2);
    }

    void drawGameOver(TFT_eSprite &s) {
        s.fillSprite(0x0000);
        s.setTextDatum(MC_DATUM);
        // Animated title
        s.setTextColor((frame/5)%2==0 ? 0xF800 : 0xFDA0);
        s.drawString("GAME OVER", SCR_W/2, 30, 4);
        // Big score
        s.setTextColor(0xFFE0);
        char buf[16]; snprintf(buf,16,"%d",lastResult.score);
        s.drawString(buf, SCR_W/2, 75, 7);
        s.setTextColor(0x7BEF); s.drawString("score", SCR_W/2, 105, 1);
        // Rewards
        s.setTextColor(0x07E0);
        snprintf(buf,16,"+%dFood +%dHappy",lastResult.foodBonus,lastResult.happyBonus);
        s.drawString(buf, SCR_W/2, 125, 1);
        // Stars
        int stars = min(score/5+1, 5);
        for (int i=0;i<5;i++) {
            uint16_t sc = (i<stars) ? 0xFFE0 : 0x2104;
            int sx = SCR_W/2-40+i*18, sy = 155;
            s.fillTriangle(sx,sy-5,sx-4,sy+2,sx+4,sy+2,sc);
            s.fillTriangle(sx,sy+4,sx-5,sy-1,sx+5,sy-1,sc);
        }
        s.setTextColor(0x4A49); s.drawString("press to continue",SCR_W/2,185,1);
    }

    void drawGameHUD(TFT_eSprite &s, const char* name) {
        // Gradient top bar
        s.fillRect(0,0,SCR_W,16,0x0000);
        s.drawFastHLine(0,15,SCR_W,0x2104);
        s.setTextDatum(TL_DATUM); s.setTextColor(0x07FF); s.drawString(name,3,1,2);
        char buf[8]; snprintf(buf,8,"%d",score);
        s.setTextColor(0xFFE0); s.setTextDatum(TR_DATUM); s.drawString(buf,SCR_W-3,1,2);
        // Hearts
        for (int i=0;i<lives && i<5;i++) {
            int hx=SCR_W/2-lives*6+i*12;
            s.fillCircle(hx,6,2,0xF800); s.fillCircle(hx+3,6,2,0xF800);
            s.fillTriangle(hx-2,7,hx+5,7,hx+1,11,0xF800);
        }
        s.setTextDatum(TL_DATUM);
    }

    void drawGame(TFT_eSprite &s) {
        switch (type) {
            case GAME_RUNNER: drawRunner(s); break;
            case GAME_CATCH: drawCatch(s); break;
            case GAME_RHYTHM: drawRhythm(s); break;
            default: break;
        }
    }

    // ==============================
    // RUNNER DRAW - parallax, better gfx
    // ==============================
    void drawRunner(TFT_eSprite &s) {
        // Sky gradient
        for (int y=16;y<GAME_GROUND;y+=8) {
            uint16_t c = (y<80) ? 0x6DFF : (y<140) ? 0x5D9F : 0x4D3F;
            s.fillRect(0,y,SCR_W,8,c);
        }
        // Mountains (parallax slow)
        int mx = -(frame/3)%SCR_W;
        for (int i=0;i<3;i++) {
            int px = mx + i * 60;
            s.fillTriangle(px,GAME_GROUND-10, px+30,GAME_GROUND-40-i*8, px+60,GAME_GROUND-10, 0x3186);
        }
        // Ground
        s.fillRect(0,GAME_GROUND,SCR_W,SCR_H-GAME_GROUND,0x2C04);
        // Scrolling ground detail
        int sc = (frame*3)%16;
        for (int x=-sc;x<SCR_W;x+=16) { s.drawFastHLine(x,GAME_GROUND+3,6,0x3E05); s.drawFastHLine(x+8,GAME_GROUND+7,4,0x1C02); }
        // Background trees (parallax medium)
        for (int i=0;i<3;i++) {
            int tx = ((i*55+10)-(frame)%165+165)%(SCR_W+30)-15;
            s.fillRect(tx,GAME_GROUND-22,3,22,0x4A29);
            s.fillTriangle(tx+1,GAME_GROUND-35,tx-7,GAME_GROUND-12,tx+9,GAME_GROUND-12,0x1861);
        }

        // Capybara
        int px=25, py=GAME_GROUND-12+(int)runner.playerY;
        // Shadow
        if (runner.jumping) s.fillEllipse(px,GAME_GROUND-2,8,2,0x1861);
        s.fillEllipse(px,py,10,7,0x9B26);
        s.fillEllipse(px,py+3,7,4,0xBC89);
        s.fillRoundRect(px+7,py-4,9,8,2,0x9B26);
        // Flat head
        s.fillRoundRect(px+7,py-5,10,7,2,0x9B26);
        s.fillRect(px+14,py-2,4,3,0xBC89); // snout
        s.fillRect(px+16,py-2,2,1,0x3186); // nostril
        s.fillCircle(px+13,py-3,1,0x0000); // eye
        s.drawPixel(px+14,py-4,0xFFFF);
        // Ears
        s.fillCircle(px+9,py-6,2,0x9B26); s.drawPixel(px+9,py-6,0x6220);
        // Legs
        if (!runner.jumping) {
            int lf=(frame/3)%4; int l1=(lf<2)?0:2, l2=(lf<2)?2:0;
            s.fillRect(px-5,py+5+l1,3,5,0x6220); s.fillRect(px+3,py+5+l2,3,5,0x6220);
        } else {
            s.fillRect(px-5,py+4,3,3,0x6220); s.fillRect(px+3,py+4,3,3,0x6220);
        }

        // Obstacles
        for (int i=0;i<4;i++) {
            int ox=(int)runner.obstacles[i];
            if (ox<-15||ox>SCR_W+15) continue;
            if (i%3==0) { // Rock
                s.fillEllipse(ox,GAME_GROUND-5,7,5,0x6B4D);
                s.fillEllipse(ox-1,GAME_GROUND-7,4,3,0x8410);
                s.drawPixel(ox-2,GAME_GROUND-8,0xAD55);
            } else if (i%3==1) { // Mushroom
                s.fillRect(ox-1,GAME_GROUND-8,3,8,0xE71C);
                s.fillCircle(ox,GAME_GROUND-9,5,0xF800);
                s.drawPixel(ox-2,GAME_GROUND-10,0xFFFF); s.drawPixel(ox+2,GAME_GROUND-9,0xFFFF);
            } else { // Log
                s.fillRoundRect(ox-6,GAME_GROUND-6,12,6,2,0x4A29);
                s.drawCircle(ox-5,GAME_GROUND-3,2,0x6220);
            }
        }

        // Score popup on pass
        if (runner.passed > 0 && frame%30 < 15) {
            s.setTextColor(0xFFE0); s.setTextDatum(MC_DATUM);
            s.drawString("+1", 60, GAME_GROUND-30, 1);
        }

        drawGameHUD(s, "RUNNER");
    }

    // ==============================
    // CATCH DRAW - powerups visible, combo, effects
    // ==============================
    void drawCatch(TFT_eSprite &s) {
        // Sky
        s.fillRect(0,16,SCR_W,GAME_GROUND-16,0x5D9F);
        // Clouds
        int cx = (millis()/300)%(SCR_W+30)-15;
        s.fillEllipse(cx,40,10,4,0xFFFF); s.fillEllipse(cx+8,38,7,3,0xFFFF);
        // Ground
        s.fillRect(0,GAME_GROUND,SCR_W,SCR_H-GAME_GROUND,0x2C04);

        // Items
        for (int i=0;i<6;i++) {
            if (!catcher.items[i].active) continue;
            int ix=(int)catcher.items[i].x, iy=(int)catcher.items[i].y;
            switch (catcher.items[i].type) {
                case 0: // Orange
                    s.fillCircle(ix,iy,6,0xFD20); s.fillCircle(ix-1,iy-1,2,0xFEA0);
                    s.fillRect(ix-1,iy-8,2,3,0x3E05); s.drawPixel(ix+1,iy-7,0x07E0);
                    break;
                case 1: // Watermelon
                    s.fillCircle(ix,iy,6,0x3E05); s.fillCircle(ix,iy+1,5,0xF800);
                    s.drawPixel(ix-2,iy,0x0000); s.drawPixel(ix+1,iy+1,0x0000); s.drawPixel(ix,iy+2,0x0000);
                    break;
                case 2: // Heart (extra life)
                    s.fillCircle(ix-2,iy,3,0xF81F); s.fillCircle(ix+2,iy,3,0xF81F);
                    s.fillTriangle(ix-5,iy+1,ix+5,iy+1,ix,iy+7,0xF81F);
                    break;
                case 3: // Star (magnet powerup)
                    s.fillTriangle(ix,iy-6,ix-4,iy+2,ix+4,iy+2,0xFFE0);
                    s.fillTriangle(ix,iy+5,ix-5,iy-1,ix+5,iy-1,0xFFE0);
                    break;
                case 4: // Bomb
                    s.fillCircle(ix,iy,5,0x2104); s.fillCircle(ix,iy,4,0x39C7);
                    s.drawLine(ix+2,iy-5,ix+4,iy-8,0x8410);
                    s.fillCircle(ix+4,iy-8,2, (frame/3)%2==0 ? 0xF800 : 0xFDA0); // Sparking fuse
                    break;
            }
        }

        // Capybara with powerup indicators
        int px=(int)catcher.playerX, py=GAME_GROUND-12;
        // Shield glow
        if (catcher.shield) {
            s.drawCircle(px+2,py,14,(frame/3)%2==0?0x07FF:0x0293);
        }
        // Magnet indicator
        if (catcher.magnet) {
            s.setTextColor(0xF81F); s.setTextDatum(MC_DATUM);
            s.drawString("M",px,py-18,2);
        }
        // Body
        s.fillEllipse(px,py,9,7,0x9B26); s.fillEllipse(px,py+3,6,3,0xBC89);
        s.fillRoundRect(px+6,py-4,8,7,2,0x9B26);
        s.fillRect(px+12,py-2,3,2,0xBC89); s.fillRect(px+13,py-2,1,1,0x3186);
        s.fillCircle(px+11,py-3,1,0x0000);
        s.drawPixel(px+12,py-4,0xFFFF);
        // Direction arrow
        int ax=px+catcher.playerDir*16;
        s.fillTriangle(ax,py,ax-catcher.playerDir*6,py-3,ax-catcher.playerDir*6,py+3,0xFFE0);
        // Legs
        int lf=(frame/4)%4; int l1=(lf<2)?0:1;
        s.fillRect(px-4,py+5+l1,3,5,0x6220); s.fillRect(px+3,py+5+(1-l1),3,5,0x6220);

        // Combo display
        if (catcher.combo >= 3) {
            s.setTextDatum(MC_DATUM);
            uint16_t cc = catcher.combo>=10?0xF800:catcher.combo>=5?0xFFE0:0x07E0;
            s.setTextColor(cc);
            char buf[8]; snprintf(buf,8,"x%d",catcher.combo);
            s.drawString(buf, SCR_W/2, 25, 4);
        }

        drawGameHUD(s, "CATCH");
    }

    // ==============================
    // RHYTHM DRAW - lanes, better visuals
    // ==============================
    void drawRhythm(TFT_eSprite &s) {
        // Dark background with lane lines
        s.fillSprite(0x0841);
        int hitY = GAME_GROUND-10;

        // Lane dividers
        for (int y=20;y<hitY;y+=4) {
            s.drawPixel(SCR_W/3, y, 0x10A2);
            s.drawPixel(SCR_W*2/3, y, 0x10A2);
        }

        // Hit zone - glowing bar
        s.fillRect(0,hitY-4,SCR_W,9,0x10A2);
        s.drawFastHLine(0,hitY-5,SCR_W,0x07FF);
        s.drawFastHLine(0,hitY+5,SCR_W,0x07FF);

        // Three target circles
        int lanes[3] = {SCR_W/6, SCR_W/2, SCR_W*5/6};
        for (int l=0;l<3;l++) {
            s.drawCircle(lanes[l],hitY,10,0x2945);
            s.drawCircle(lanes[l],hitY,9,0x10A2);
        }

        // Notes
        uint16_t noteCols[3] = {0x07FF, 0x07E0, 0xF81F};
        for (int i=0;i<rhythm.noteCount;i++) {
            float relT = rhythm.notes[i].time - rhythm.songTime;
            if (relT < -0.8f || relT > 3.5f) continue;
            int ny = hitY - (int)(relT * 45);
            int lane = rhythm.notes[i].lane;
            int nx = lanes[lane];

            if (rhythm.notes[i].hit) {
                // Hit sparkle
                s.drawCircle(nx,ny,10,0x07E0);
                s.drawCircle(nx,ny,6,0x07E0);
            } else if (rhythm.notes[i].missed) {
                s.drawLine(nx-5,ny-5,nx+5,ny+5,0xF800);
                s.drawLine(nx+5,ny-5,nx-5,ny+5,0xF800);
            } else {
                // Approaching note with glow
                uint16_t col = noteCols[lane];
                if (relT < 0.3f) {
                    // Close! Pulse
                    int pr = 8 + (frame/2)%3;
                    s.fillCircle(nx,ny,pr,col);
                    s.drawCircle(nx,ny,pr+2,0xFFFF);
                } else {
                    s.fillCircle(nx,ny,7,col);
                }
            }
        }

        // Button flash
        if (btnPressed) {
            for (int l=0;l<3;l++) s.drawCircle(lanes[l],hitY,12,0xFFFF);
        }

        // Combo
        if (rhythm.combo > 2) {
            s.setTextDatum(MC_DATUM);
            uint16_t cc = rhythm.combo>=10?0xF800:rhythm.combo>=5?0xFFE0:0x07E0;
            s.setTextColor(cc);
            char buf[8]; snprintf(buf,8,"x%d",rhythm.combo);
            s.drawString(buf, SCR_W/2, 28, 4);
        }

        // Dancing capybara
        int bob = (frame/3)%2==0?-2:2;
        int cy = GAME_GROUND+15+bob;
        s.fillEllipse(SCR_W/2,cy,8,6,0x9B26);
        s.fillRoundRect(SCR_W/2+5,cy-3,8,6,2,0x9B26);
        // Happy eyes when combo
        if (rhythm.combo >= 3) {
            s.drawPixel(SCR_W/2+9,cy-3,0x0000); s.drawPixel(SCR_W/2+10,cy-2,0x0000); s.drawPixel(SCR_W/2+11,cy-3,0x0000);
        } else {
            s.fillCircle(SCR_W/2+10,cy-2,1,0x0000);
        }
        // Arms up when combo high
        if (rhythm.combo >= 5) {
            s.drawLine(SCR_W/2-5,cy-2,SCR_W/2-9,cy-8,0x6220);
            s.drawLine(SCR_W/2+15,cy-2,SCR_W/2+19,cy-8,0x6220);
        }

        drawGameHUD(s, "RHYTHM");
    }
};

#pragma once
#include <Arduino.h>
#include <time.h>
#include <Preferences.h>
#include "environments.h"
#include "brain_weights.h"

enum Mood {
    MOOD_HAPPY, MOOD_CONTENT, MOOD_BORED,
    MOOD_SLEEPY, MOOD_HUNGRY, MOOD_EXCITED, MOOD_CURIOUS,
    MOOD_COUNT
};

enum Behavior {
    BEH_IDLE, BEH_WALK_LEFT, BEH_WALK_RIGHT, BEH_EAT_GRASS,
    BEH_SWIM, BEH_SLEEP, BEH_YAWN, BEH_SCRATCH,
    BEH_HAPPY_JUMP, BEH_SIT_MIKAN, BEH_CURIOUS_SNIFF, BEH_STRETCH,
    BEH_PLAY_BUTTERFLY, BEH_EAT_WATERMELON, BEH_LOOK_AROUND,
    BEH_SHY, BEH_DOZE, BEH_RUN,
    // Transition behaviors (new!)
    BEH_TRANSITION,  // Generic transition between states
    BEH_COUNT
};

struct BehaviorInfo {
    const char* name;
    float minSec, maxSec;
};

const BehaviorInfo BEHAVIOR_INFO[] = {
    {"idle",4,12}, {"walk_L",5,12}, {"walk_R",5,12}, {"eat",4,8},
    {"swim",8,18}, {"sleep",15,40}, {"yawn",3,5}, {"scratch",3,6},
    {"jump",2,4}, {"mikan",8,20}, {"sniff",4,8}, {"stretch",3,5},
    {"butterfly",6,14}, {"melon",4,8}, {"look",4,10},
    {"shy",3,7}, {"doze",8,16}, {"run",3,7},
    {"trans",1,2},
};

// Idle sub-animations
enum IdleAnim {
    IDLE_STAND,       // Just standing
    IDLE_BLINK,       // Slow blink
    IDLE_EAR_TWITCH,  // Ears move
    IDLE_TAIL_WAG,    // Tiny tail wiggle
    IDLE_HEAD_TILT,   // Slight head tilt
    IDLE_SNIFF_AIR,   // Quick sniff up
    IDLE_SHIFT_WEIGHT,// Shift weight side to side
    IDLE_LOOK_UP,     // Look at sky
    IDLE_COUNT
};

// Transition types between behaviors
enum TransType {
    TRANS_NONE,
    TRANS_TURN_AROUND,  // Flip facing direction
    TRANS_SNIFF_BEFORE, // Sniff before eating
    TRANS_STAND_UP,     // Stand up from sleep
    TRANS_LIE_DOWN,     // Lie down to sleep
    TRANS_LOOK_THEN_GO, // Look in direction then walk
};

class PetBrain {
public:
    Mood mood = MOOD_CONTENT;
    Behavior behavior = BEH_IDLE;
    Behavior prevBehavior = BEH_IDLE;
    Behavior prevPrevBehavior = BEH_IDLE;
    Behavior nextBehavior = BEH_IDLE; // What we're transitioning TO
    int animFrame = 0;
    float behaviorTimeLeft = 10;

    float posX = 40, posY = 0;
    bool facingLeft = false;

    float happiness = 55, energy = 55, hunger = 55;
    float curiosity = 55, loneliness = 40;
    bool presenceDetected = false;

    Environment env = ENV_MEADOW;
    Weather weather = WEATHER_CLEAR;
    float envTimer = 60;

    bool birdActive = false, duckActive = false, turtleActive = false;
    float friendTimer = 20;

    int hourOfDay = 12;
    float gameTimeSec = 0;
    bool ntpSynced = false;
    int realHour = 12;
    float realTemp = 0;
    bool realWeatherActive = false;

    // Idle sub-animation state
    IdleAnim idleAnim = IDLE_STAND;
    float idleAnimTimer = 0;
    int idleAnimFrame = 0;

    // Transition state
    TransType transType = TRANS_NONE;
    float transProgress = 0; // 0-1

    // Behavior history (last 5 actions as NN input)
    int behaviorHistory[5] = {0,0,0,0,0};
    int historyIdx = 0;

    // Save/load
    Preferences prefs;
    float saveTimer = 60; // Save every 60 sec

    void saveState() {
        prefs.begin("capy", false);
        prefs.putFloat("hp", happiness);
        prefs.putFloat("en", energy);
        prefs.putFloat("hg", hunger);
        prefs.putFloat("cr", curiosity);
        prefs.putFloat("ln", loneliness);
        prefs.end();
        Serial.println("[SAVE] Stats saved to flash");
    }

    void loadState() {
        prefs.begin("capy", true);
        if (prefs.isKey("hp")) {
            happiness = prefs.getFloat("hp", 55);
            energy = prefs.getFloat("en", 55);
            hunger = prefs.getFloat("hg", 55);
            curiosity = prefs.getFloat("cr", 55);
            loneliness = prefs.getFloat("ln", 40);
            Serial.printf("[LOAD] hp=%.0f en=%.0f hg=%.0f cr=%.0f\n", happiness, energy, hunger, curiosity);
        } else {
            Serial.println("[LOAD] No save found, fresh start");
        }
        prefs.end();
    }

    // Smooth visual interpolation
    float visualHeadBob = 0;
    float visualBodyStretch = 0;
    float earWiggle = 0;
    float tailWag = 0;
    float breathe = 0; // Subtle breathing animation

    void begin() {
        randomSeed(analogRead(0) ^ micros());
        loadState();
        pickNewBehavior();
    }

    void update(float dt) {
        animFrame++;
        breathe = sin(animFrame * 0.08f) * 0.5f; // Constant subtle breathing

        // Stats
        hunger += dt * 0.18f;
        energy -= dt * 0.15f;
        happiness -= dt * 0.10f;
        curiosity += dt * 0.15f;
        loneliness += presenceDetected ? -dt * 0.5f : dt * 0.1f;

        if (hunger > 75) happiness -= dt * 0.15f;
        if (energy < 20) curiosity -= dt * 0.1f;
        if (loneliness > 70) happiness -= dt * 0.1f;
        if (hunger < 25 && energy > 50) happiness += dt * 0.03f;

        hunger = constrain(hunger, 0, 100);
        energy = constrain(energy, 0, 100);
        happiness = constrain(happiness, 0, 100);
        curiosity = constrain(curiosity, 0, 100);
        loneliness = constrain(loneliness, 0, 100);

        gameTimeSec += dt;
        if (ntpSynced) {
            struct tm t;
            if (getLocalTime(&t, 0)) hourOfDay = t.tm_hour;
        } else {
            hourOfDay = ((int)(gameTimeSec / 60)) % 24;
        }

        if (realTemp > 25 && behavior == BEH_RUN && energy < 50) behaviorTimeLeft = 0;
        if (realTemp < 0 && behavior == BEH_SWIM) behaviorTimeLeft = 0;

        updateMood();

        behaviorTimeLeft -= dt;
        if (behaviorTimeLeft <= 0) {
            if (behavior == BEH_TRANSITION) {
                // Transition done, start the actual behavior
                startBehavior(nextBehavior);
            } else {
                pickNewBehavior();
            }
        }

        // Update idle sub-animations
        if (behavior == BEH_IDLE) updateIdle(dt);

        // Smooth visual values
        earWiggle *= 0.9f;
        tailWag *= 0.92f;
        visualHeadBob *= 0.85f;
        visualBodyStretch *= 0.9f;

        // Environment
        envTimer -= dt;
        if (envTimer <= 0) { env = (Environment)(random(ENV_COUNT)); envTimer = 40 + random(80); }

        // Weather
        static float wTimer = 60;
        wTimer -= dt;
        if (wTimer <= 0) {
            if (!realWeatherActive) {
                int r = random(10);
                weather = (r<5)?WEATHER_CLEAR:(r<7)?WEATHER_CLOUDY:(r<9)?WEATHER_RAIN:WEATHER_SNOW;
            }
            wTimer = 60 + random(120);
        }

        // Auto-save every 60 sec
        saveTimer -= dt;
        if (saveTimer <= 0) { saveState(); saveTimer = 60; }

        // Friends
        friendTimer -= dt;
        if (friendTimer <= 0) {
            birdActive = random(4) == 0;
            duckActive = (env == ENV_LAKE) && random(2) == 0;
            turtleActive = (env != ENV_HOME) && random(4) == 0;
            friendTimer = 15 + random(25);
        }

        executeBehavior(dt);
    }

    void interact() {
        happiness = min(100.0f, happiness + 10);
        loneliness = max(0.0f, loneliness - 15);
        earWiggle = 1.0f;
        tailWag = 1.0f;

        int r = random(5);
        if (r == 0) startTransition(BEH_HAPPY_JUMP, TRANS_NONE);
        else if (r == 1) startTransition(BEH_SHY, TRANS_NONE);
        else if (r == 2) startTransition(BEH_SIT_MIKAN, TRANS_SNIFF_BEFORE);
        else if (r == 3) { facingLeft = !facingLeft; startBehavior(BEH_IDLE); }
        else startTransition(BEH_HAPPY_JUMP, TRANS_NONE);
    }

private:
    void startBehavior(Behavior b) {
        prevPrevBehavior = prevBehavior;
        prevBehavior = behavior;
        behavior = b;
        // Record in history ring buffer
        behaviorHistory[historyIdx % 5] = (int)b;
        historyIdx++;
        float dur = BEHAVIOR_INFO[b].minSec + random(1000)/1000.0f * (BEHAVIOR_INFO[b].maxSec - BEHAVIOR_INFO[b].minSec);
        behaviorTimeLeft = dur;
        animFrame = 0;
        transType = TRANS_NONE;
        transProgress = 0;

        // Reset idle animation
        if (b == BEH_IDLE) {
            idleAnim = IDLE_STAND;
            idleAnimTimer = 1.5f + random(100)/50.0f;
        }
    }

    void startTransition(Behavior target, TransType tt) {
        nextBehavior = target;
        transType = tt;
        transProgress = 0;

        if (tt == TRANS_NONE) {
            startBehavior(target);
            return;
        }

        behavior = BEH_TRANSITION;
        behaviorTimeLeft = 1.5f; // Transition duration
        animFrame = 0;

        // Trigger visual effects for transition
        if (tt == TRANS_TURN_AROUND) facingLeft = !facingLeft;
        if (tt == TRANS_SNIFF_BEFORE) visualHeadBob = -3.0f;
        if (tt == TRANS_STAND_UP) visualBodyStretch = 5.0f;
    }

    void updateIdle(float dt) {
        idleAnimTimer -= dt;
        idleAnimFrame++;

        if (idleAnimTimer <= 0) {
            // Pick new idle sub-animation
            idleAnim = (IdleAnim)(random(IDLE_COUNT));
            idleAnimTimer = 1.0f + random(100)/30.0f; // 1-4 sec per idle anim
            idleAnimFrame = 0;

            // Trigger visual effects
            switch (idleAnim) {
                case IDLE_EAR_TWITCH: earWiggle = 0.8f; break;
                case IDLE_TAIL_WAG: tailWag = 0.7f; break;
                case IDLE_HEAD_TILT: visualHeadBob = 2.0f; break;
                case IDLE_SNIFF_AIR: visualHeadBob = -2.0f; break;
                case IDLE_SHIFT_WEIGHT: visualBodyStretch = 2.0f; break;
                default: break;
            }
        }
    }

    void updateMood() {
        if (energy < 15) mood = MOOD_SLEEPY;
        else if (hunger > 80) mood = MOOD_HUNGRY;
        else if (happiness > 75 && energy > 40) mood = MOOD_EXCITED;
        else if (energy < 35) mood = MOOD_SLEEPY;
        else if (hunger > 60) mood = MOOD_HUNGRY;
        else if (happiness > 55) mood = MOOD_HAPPY;
        else if (curiosity > 65) mood = MOOD_CURIOUS;
        else if (happiness < 35) mood = MOOD_BORED;
        else mood = MOOD_CONTENT;
    }

    void pickNewBehavior() {
        // === DEEP NEURAL NETWORK FORWARD PASS ===
        // 21 inputs -> 48 hidden1 -> 24 hidden2 -> 18 outputs
        float inputs[BRAIN_N_IN];
        float hour_rad = hourOfDay / 24.0f * 6.28318f;
        bool isActive = (behavior==BEH_RUN||behavior==BEH_HAPPY_JUMP||behavior==BEH_SWIM);

        // Stats
        inputs[0] = happiness / 100.0f;
        inputs[1] = energy / 100.0f;
        inputs[2] = hunger / 100.0f;
        inputs[3] = curiosity / 100.0f;
        inputs[4] = loneliness / 100.0f;
        // Time
        inputs[5] = sin(hour_rad);
        inputs[6] = cos(hour_rad);
        inputs[7] = (hourOfDay >= 22 || hourOfDay < 6) ? 1.0f : 0.0f;
        // Environment
        inputs[8] = (env == ENV_LAKE) ? 1.0f : 0.0f;
        inputs[9] = (env == ENV_FOREST) ? 1.0f : 0.0f;
        inputs[10] = (env == ENV_HOME) ? 1.0f : 0.0f;
        // Context
        inputs[11] = presenceDetected ? 1.0f : 0.0f;
        inputs[12] = (behavior==BEH_SLEEP||behavior==BEH_DOZE) ? 1.0f : 0.0f;
        inputs[13] = (behavior==BEH_EAT_GRASS||behavior==BEH_EAT_WATERMELON) ? 1.0f : 0.0f;
        inputs[14] = isActive ? 1.0f : 0.0f;
        // Extended: temperature, weather, trends
        inputs[15] = constrain(realTemp / 30.0f, -1.0f, 1.0f);
        inputs[16] = (weather == WEATHER_RAIN) ? 1.0f : 0.0f;
        inputs[17] = hunger / 100.0f * 0.6f;
        inputs[18] = -(1.0f - energy/100.0f) * 0.4f;
        inputs[19] = (happiness - 50.0f) / 100.0f;
        inputs[20] = min(behaviorTimeLeft / 10.0f, 1.0f);

        // History features: categorize last 5 behaviors
        float hSleep=0, hEat=0, hActive=0, hExplore=0, hIdle=0;
        int hCount = min(historyIdx, 5);
        if (hCount > 0) {
            for (int i = 0; i < hCount; i++) {
                int b = behaviorHistory[(historyIdx - 1 - i + 500) % 5];
                if (b==5||b==16) hSleep++;
                else if (b==3||b==13) hEat++;
                else if (b==4||b==8||b==17) hActive++;
                else if (b==10||b==12||b==14) hExplore++;
                else if (b==0||b==7||b==9) hIdle++;
            }
            hSleep/=hCount; hEat/=hCount; hActive/=hCount; hExplore/=hCount; hIdle/=hCount;
        }
        inputs[21] = hSleep;
        inputs[22] = hEat;
        inputs[23] = hActive;
        inputs[24] = hExplore;
        inputs[25] = hIdle;

        // Hidden layer 1: ReLU(inputs @ W1 + b1)
        float h1[BRAIN_N_HIDDEN1];
        for (int j = 0; j < BRAIN_N_HIDDEN1; j++) {
            float sum = pgm_read_float(&brain_b1[j]);
            for (int i = 0; i < BRAIN_N_IN; i++)
                sum += inputs[i] * pgm_read_float(&brain_w1[i * BRAIN_N_HIDDEN1 + j]);
            h1[j] = sum > 0 ? sum : 0;
        }

        // Hidden layer 2: ReLU(h1 @ W2 + b2)
        float h2[BRAIN_N_HIDDEN2];
        for (int j = 0; j < BRAIN_N_HIDDEN2; j++) {
            float sum = pgm_read_float(&brain_b2[j]);
            for (int i = 0; i < BRAIN_N_HIDDEN1; i++)
                sum += h1[i] * pgm_read_float(&brain_w2[i * BRAIN_N_HIDDEN2 + j]);
            h2[j] = sum > 0 ? sum : 0;
        }

        // Output layer: h2 @ W3 + b3
        float scores[BRAIN_N_OUT];
        for (int j = 0; j < BRAIN_N_OUT; j++) {
            float sum = pgm_read_float(&brain_b3[j]);
            for (int i = 0; i < BRAIN_N_HIDDEN2; i++)
                sum += h2[i] * pgm_read_float(&brain_w3[i * BRAIN_N_OUT + j]);
            scores[j] = sum;
        }

        // Anti-repeat penalty (post-NN)
        scores[behavior] -= 3.0f;
        scores[prevBehavior] -= 1.5f;

        // Energy gate (safety)
        if (energy < 20) {
            scores[BEH_RUN] -= 5.0f;
            scores[BEH_HAPPY_JUMP] -= 4.0f;
            scores[BEH_SWIM] -= 3.0f;
        }

        // Add small noise for variety
        for (int i = 0; i < BRAIN_N_OUT; i++)
            scores[i] += random(100) / 150.0f;

        // Pick best
        int best = 0;
        for (int i = 1; i < BRAIN_N_OUT; i++)
            if (scores[i] > scores[best]) best = i;

        Behavior chosen = (Behavior)best;

        // Determine transition type based on current → next behavior
        TransType tt = TRANS_NONE;

        // Sleep transitions
        if (chosen == BEH_SLEEP || chosen == BEH_DOZE) {
            if (behavior != BEH_SLEEP && behavior != BEH_DOZE && behavior != BEH_YAWN) {
                // Chain: yawn first, then sleep
                if (random(3) > 0) {
                    startBehavior(BEH_YAWN);
                    nextBehavior = chosen;
                    return;
                }
                tt = TRANS_LIE_DOWN;
            }
        }
        // Waking up
        if ((behavior == BEH_SLEEP || behavior == BEH_DOZE) && chosen != BEH_SLEEP && chosen != BEH_DOZE) {
            tt = TRANS_STAND_UP;
        }
        // Eating: sniff before
        if ((chosen == BEH_EAT_GRASS || chosen == BEH_EAT_WATERMELON) && behavior != BEH_CURIOUS_SNIFF) {
            if (random(2) == 0) tt = TRANS_SNIFF_BEFORE;
        }
        // Walking: look in direction first
        if ((chosen == BEH_WALK_LEFT || chosen == BEH_WALK_RIGHT) && behavior == BEH_IDLE) {
            if (random(3) == 0) tt = TRANS_LOOK_THEN_GO;
            // Set facing for walk
            if (chosen == BEH_WALK_LEFT) facingLeft = true;
            if (chosen == BEH_WALK_RIGHT) facingLeft = false;
        }
        // Running: turn if needed
        if (chosen == BEH_RUN && random(2) == 0) {
            tt = TRANS_TURN_AROUND;
        }

        startTransition(chosen, tt);
    }

    void executeBehavior(float dt) {
        // Transition animation
        if (behavior == BEH_TRANSITION) {
            transProgress += dt / 1.5f;
            transProgress = constrain(transProgress, 0, 1);

            switch (transType) {
                case TRANS_SNIFF_BEFORE:
                    visualHeadBob = sin(transProgress * 6.28f) * 3;
                    break;
                case TRANS_LIE_DOWN:
                    posY = transProgress * 3;
                    visualBodyStretch = transProgress * -3;
                    break;
                case TRANS_STAND_UP:
                    posY = (1.0f - transProgress) * 3;
                    visualBodyStretch = (1.0f - transProgress) * -3;
                    earWiggle = transProgress * 0.5f;
                    break;
                case TRANS_LOOK_THEN_GO:
                    if (transProgress < 0.5f) visualHeadBob = sin(transProgress * 6.28f) * 2;
                    break;
                default: break;
            }
            return;
        }

        switch (behavior) {
            case BEH_WALK_LEFT:
                facingLeft = true; posX -= dt * 10;
                if (posX < -5) posX = SCR_W - 50;
                energy -= dt * 0.15f;
                break;
            case BEH_WALK_RIGHT:
                facingLeft = false; posX += dt * 10;
                if (posX > SCR_W - 45) posX = 0;
                energy -= dt * 0.15f;
                break;
            case BEH_RUN:
                posX += (facingLeft ? -1 : 1) * dt * 30;
                if (posX < -5) { posX = -5; facingLeft = false; }
                if (posX > SCR_W - 45) { posX = SCR_W - 45; facingLeft = true; }
                energy -= dt * 0.25f; happiness += dt * 0.5f;
                break;
            case BEH_EAT_GRASS: case BEH_EAT_WATERMELON:
                hunger -= dt * 2.5f; break;
            case BEH_SLEEP:
                energy += dt * 1.2f; break;
            case BEH_DOZE:
                energy += dt * 0.6f; break;
            case BEH_SWIM:
                happiness += dt * 0.8f; energy -= dt * 0.12f;
                posX += sin(animFrame * 0.03f) * dt * 6;
                break;
            case BEH_HAPPY_JUMP:
                happiness += dt * 1.2f; energy -= dt * 0.2f;
                posY = -abs(sin(animFrame * 0.15f)) * 12;
                break;
            case BEH_SIT_MIKAN: happiness += dt * 0.5f; energy += dt * 0.1f; break;
            case BEH_PLAY_BUTTERFLY: curiosity -= dt * 0.8f; happiness += dt * 0.4f; break;
            case BEH_CURIOUS_SNIFF: curiosity -= dt * 0.7f; break;
            case BEH_LOOK_AROUND:
                curiosity -= dt * 0.5f;
                if ((animFrame / 30) % 2 == 0) facingLeft = !facingLeft;
                break;
            case BEH_STRETCH: energy += dt * 0.3f; break;
            case BEH_YAWN: energy += dt * 0.2f; break;
            case BEH_SHY: loneliness -= dt * 0.3f; break;
            case BEH_IDLE:
                // Idle has sub-animations that produce visual effects
                break;
            default: break;
        }

        if (behavior != BEH_HAPPY_JUMP) posY *= 0.85f;

        // Lake: keep on shore unless swimming
        if (env == ENV_LAKE && behavior != BEH_SWIM) {
            posX = constrain(posX, -5, 25.0f);
        } else {
            posX = constrain(posX, -5, (float)SCR_W - 45);
        }

        // Can't swim if not at lake - force end swim
        if (behavior == BEH_SWIM && env != ENV_LAKE) {
            behaviorTimeLeft = 0;
        }
        hunger = constrain(hunger, 0, 100);
        energy = constrain(energy, 0, 100);
        happiness = constrain(happiness, 0, 100);
        curiosity = constrain(curiosity, 0, 100);
    }
};

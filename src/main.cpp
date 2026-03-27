#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "environments.h"
#include "pet_brain.h"
#include "minigames.h"

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);
PetBrain brain;
MiniGame game;

#define BTN_PIN 0
#define WIFI_SSID "NOKIA-C5EA"
#define WIFI_PASS "LTe4KG7gxa"

// Weather
struct RealWeather { float temp=0; int humidity=0; int code=0; bool valid=false; };
RealWeather realWeather;

Weather wmoToWeather(int c) {
    if(c==0) return WEATHER_CLEAR;
    if(c<=3||((c>=45)&&(c<=48))) return WEATHER_CLOUDY;
    if(c>=71&&c<=77) return WEATHER_SNOW;
    if(c>=85&&c<=86) return WEATHER_SNOW;
    if(c>=51) return WEATHER_RAIN;
    return WEATHER_CLEAR;
}
const char* wmoDesc(int c) {
    if(c==0) return "Clear"; if(c<=3) return "Cloudy"; if(c<=48) return "Fog";
    if(c<=57) return "Drizzle"; if(c<=67) return "Rain"; if(c<=77) return "Snow";
    if(c<=82) return "Showers"; return "Storm";
}
void fetchWeather() {
    if(WiFi.status()!=WL_CONNECTED) return;
    HTTPClient h;
    h.begin("http://api.open-meteo.com/v1/forecast?latitude=59.39&longitude=24.70&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Europe%2FTallinn");
    h.setTimeout(5000);
    if(h.GET()==200){
        JsonDocument d; if(!deserializeJson(d,h.getString())){
            JsonObject cur=d["current"];
            realWeather.temp=cur["temperature_2m"]|0.0f;
            realWeather.humidity=cur["relative_humidity_2m"]|0;
            realWeather.code=cur["weather_code"]|0;
            realWeather.valid=true;
            brain.weather=wmoToWeather(realWeather.code);
            brain.realTemp=realWeather.temp;
            brain.realWeatherActive=true;
            Serial.printf("[Weather] %.1fC %s\n",realWeather.temp,wmoDesc(realWeather.code));
        }
    }
    h.end();
}

// CSI
volatile float csiAmps[50]; volatile int csiIdx=0;
static void csi_cb(void*ctx,wifi_csi_info_t*info){
    if(!info||!info->buf||info->len<4)return;
    int8_t*buf=(int8_t*)info->buf;int p=info->len/2;if(p>30)p=30;
    float avg=0;for(int i=0;i<p;i++){float a=sqrtf(buf[i*2]*buf[i*2]+buf[i*2+1]*buf[i*2+1]);avg+=a;}
    avg/=p;csiAmps[csiIdx%50]=avg;csiIdx++;
}
float csiVariance(){
    int n=(csiIdx<50)?(int)csiIdx:50;if(n<10)return 0;
    float m=0;for(int i=0;i<n;i++)m+=csiAmps[i];m/=n;
    float v=0;for(int i=0;i<n;i++){float d=csiAmps[i]-m;v+=d*d;}return v/n;
}

void setupWiFi() {
    WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS);
    for(int i=0;i<15&&WiFi.status()!=WL_CONNECTED;i++) delay(500);
    if(WiFi.status()==WL_CONNECTED){
        Serial.println("[WiFi] "+WiFi.localIP().toString());
        configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4","pool.ntp.org","time.google.com");
        struct tm t; if(getLocalTime(&t,5000)){
            brain.realHour=t.tm_hour; brain.hourOfDay=t.tm_hour;
            brain.ntpSynced=true; Serial.printf("[NTP] %02d:%02d\n",t.tm_hour,t.tm_min);
        }
        fetchWeather();
        // CSI setup - order matters!
        delay(500); // Let WiFi stack stabilize
        esp_err_t e1 = esp_wifi_set_promiscuous(true);
        esp_err_t e2 = esp_wifi_set_csi(true);
        wifi_csi_config_t cfg={true,true,true,true,false,false,false};
        esp_err_t e3 = esp_wifi_set_csi_config(&cfg);
        esp_err_t e4 = esp_wifi_set_csi_rx_cb(csi_cb,NULL);
        Serial.printf("[CSI] promisc=%d csi=%d cfg=%d cb=%d\n",e1,e2,e3,e4);
    }
}

// Button state
bool btnDown = false, btnJustDown = false, btnJustUp = false;
bool lastBtn = false;
unsigned long btnDownSince = 0, btnHeldMs = 0;
bool longDone = false;

void drawCapy(TFT_eSprite &s, int x, int y, bool flip, int f, Behavior beh) {
    int dir = flip ? -1 : 1;
    int cx = flip ? x + 30 : x;
    int legPhase = 0;
    float headBob = brain.visualHeadBob;
    float bodyStretch = brain.visualBodyStretch;
    bool curled = (beh == BEH_SLEEP || beh == BEH_DOZE);
    bool swimming = (beh == BEH_SWIM);

    // Breathing body scale
    float breathScale = brain.breathe;

    if (beh == BEH_WALK_LEFT || beh == BEH_WALK_RIGHT) legPhase = (f / 8) % 4;
    else if (beh == BEH_RUN) legPhase = (f / 4) % 4;
    else if (beh == BEH_EAT_GRASS || beh == BEH_EAT_WATERMELON)
        headBob += (int[]){0, 5, 3, 1}[(f / 10) % 4];
    else if (beh == BEH_STRETCH) bodyStretch += ((f / 10) % 3 == 1) ? 5 : 0;
    else if (beh == BEH_CURIOUS_SNIFF) headBob += sin(f * 0.15f) * 2;
    else if (beh == BEH_DOZE) headBob += 2 + sin(f * 0.04f) * 3;

    // Transition visual
    if (beh == BEH_TRANSITION) {
        if (brain.transType == TRANS_LIE_DOWN) curled = brain.transProgress > 0.5f;
        if (brain.transType == TRANS_STAND_UP) curled = brain.transProgress < 0.5f;
    }

    if (swimming) {
        // Deep submerge: mostly underwater, just nose and top of head
        y += 32;
        int wy = y + 2; // Water surface cuts through mid-body

        // Water behind capybara (deep)
        s.fillRect(max(0, (int)(cx + dir * -10)), wy, 60, 35, 0x1B5C);

        // Face direction of movement
        float swimDir = sin(f * 0.03f);
        if (swimDir > 0.1f) brain.facingLeft = false;
        else if (swimDir < -0.1f) brain.facingLeft = true;
        flip = brain.facingLeft;
        dir = flip ? -1 : 1;
        cx = flip ? x + 30 : x;
    }

    uint16_t bC = 0x9B26, beC = 0xBC89;
    if (!swimming) {
        if (curled) {
            s.fillRoundRect(cx+dir*8, y+22, 5, 4, 1, 0x6220);
            s.fillRoundRect(cx+dir*26, y+22, 5, 4, 1, 0x6220);
        } else if (beh == BEH_HAPPY_JUMP) {
            for (int l : {8,13,26,31}) s.fillRect(cx+dir*l, y+22, 4, 6, 0x6220);
        } else {
            int offs[4][2] = {{0,2},{2,0},{0,-2},{-2,0}};
            int lx[] = {8, 13, 26, 31};
            for (int l = 0; l < 4; l++)
                s.fillRect(cx+dir*lx[l], y+20+offs[legPhase][l<2?0:1], 4, 9, 0x6220);
        }
    }

    // Tail (with wag animation)
    float tw = brain.tailWag * sin(f * 0.5f) * 3;
    s.fillCircle(cx+dir*2+(int)tw, y+18, 3, 0x6220);

    if (curled) { s.fillEllipse(cx+dir*18,y+14,15,10+(int)breathScale,bC); s.fillEllipse(cx+dir*18,y+17,10,5,beC); }
    else { s.fillEllipse(cx+dir*18,y+12,17+(int)bodyStretch,10+(int)breathScale,bC); s.fillEllipse(cx+dir*18,y+15,12,6,beC); }

    int hx = curled ? cx+dir*26 : cx+dir*34;
    int hy = (curled ? y+7 : y+3) + headBob;
    if (swimming) hy = y+1;
    s.fillRoundRect(hx-7, hy, 14, 13, 2, bC);
    int sx = hx+dir*7;
    s.fillRoundRect(sx-4, hy+5, 8, 6, 2, beC);
    s.fillRect(sx-2, hy+5, 2, 2, 0x3186); s.fillRect(sx+1, hy+5, 2, 2, 0x3186);

    bool perked = (beh==BEH_CURIOUS_SNIFF||beh==BEH_LOOK_AROUND||beh==BEH_PLAY_BUTTERFLY);
    int eo = perked ? -2 : 0;
    int ew = (int)(brain.earWiggle * sin(f * 0.8f) * 2); // Ear wiggle
    s.fillCircle(hx-4,hy-1+eo+ew,3,bC); s.fillCircle(hx-4,hy-1+eo+ew,1,0x6220);
    s.fillCircle(hx+4,hy-1+eo-ew,3,bC); s.fillCircle(hx+4,hy-1+eo-ew,1,0x6220);

    int ex=hx+dir*3, ey=hy+3, eS=0;
    if (beh==BEH_SLEEP) eS=2; else if (beh==BEH_DOZE) eS=(f%35<25)?2:1;
    else if (beh==BEH_HAPPY_JUMP||beh==BEH_SHY||swimming) eS=3;
    else if (beh==BEH_YAWN) eS=1; else if (f%55<3) eS=2;
    if (eS==0){s.fillCircle(ex,ey,2,0);s.drawPixel(ex+(flip?-1:1),ey-1,0xFFFF);}
    else if(eS<=2) s.drawFastHLine(ex-1,ey,3,0);
    else{s.drawPixel(ex-1,ey+1,0);s.drawPixel(ex,ey,0);s.drawPixel(ex+1,ey+1,0);}

    if(beh==BEH_YAWN&&(f/10)%3==1) s.fillEllipse(sx,hy+10,3,2,0x3186);
    if(beh==BEH_SLEEP){s.setTextColor(0x7BEF);int zp=(f/25)%3;s.drawString("z",hx+8,hy-8,1);if(zp>=1)s.drawString("Z",hx+12,hy-18,2);}
    if(beh==BEH_HAPPY_JUMP){int hp=(f/8)%4;s.fillCircle(hx+hp*2-2,hy-12-hp*4,2,0xF800);s.fillCircle(hx+hp*2+2,hy-12-hp*4,2,0xF800);}
    if(beh==BEH_SIT_MIKAN){s.fillCircle(hx,hy-5,4,0xFD20);s.fillRect(hx-1,hy-10,2,3,0x07E0);s.fillEllipse(hx+dir*5,hy+7,2,1,0xFB2C);}
    if(beh==BEH_SHY) s.fillEllipse(hx+dir*5,hy+7,2,1,0xFB2C);
    if(beh==BEH_PLAY_BUTTERFLY){float bfx=cx+dir*10+sin(f*0.05f)*18,bfy=y-10+cos(f*0.08f)*10;uint16_t bc=(uint16_t[]){0xF81F,0x07FF,0xFFE0,0xFDA0}[f/6%4];int ww=(f%2==0)?3:2;s.fillEllipse((int)bfx-ww,(int)bfy,ww,2,bc);s.fillEllipse((int)bfx+ww,(int)bfy,ww,2,bc);}
    if(beh==BEH_EAT_GRASS&&(f/10)%3!=0){s.drawLine(sx,hy+8,sx+dir*4,hy,0x07E0);s.drawLine(sx+1,hy+8,sx+dir*5,hy+1,0x07E0);}
    if(beh==BEH_EAT_WATERMELON){s.fillCircle(sx+dir*3,hy+7,5,0x07E0);s.fillCircle(sx+dir*3,hy+8,4,0xF800);}
    if(beh==BEH_CURIOUS_SNIFF){s.setTextColor(0xFFE0);s.drawString("?",hx+dir*8,hy-12,2);}
    if(beh==BEH_RUN){for(int i=0;i<3;i++)s.drawFastHLine(cx+(flip?38:-10),y+8+i*5,5+i*2,0x7BEF);}
    if(beh==BEH_DOZE&&(f/30)%3==0){s.setTextColor(0x7BEF);s.drawString("z",hx+6,hy-4,1);}

    // Swimming: draw water ON TOP of body to hide lower half
    if (swimming) {
        int wy = y + 8; // Water line cuts through body
        int wx = max(0, (int)(cx + dir * -10));
        // Water overlay
        s.fillRect(wx, wy, 60, 40, 0x1B5C);
        // Surface ripples
        for (int i = 0; i < 7; i++) {
            int rx = wx + 3 + ((f*2 + i*9) % 50);
            s.drawFastHLine(rx, wy + i*3, 3 + i%3, 0x3D9F);
        }
        // Surface highlight line
        s.drawFastHLine(wx, wy, 60, 0x4DDF);
        s.drawFastHLine(wx, wy+1, 60, 0x2B7E);
        // Circular ripples around capybara
        int rr = 5 + (f/3)%6;
        s.drawEllipse(cx + dir*20, wy+2, rr, 2, 0x3D9F);
    }
}

void drawIconHeart(TFT_eSprite&s,int x,int y,uint16_t c){s.fillCircle(x+1,y+1,1,c);s.fillCircle(x+4,y+1,1,c);s.fillRect(x+1,y+2,4,2,c);s.drawPixel(x+2,y+4,c);s.drawPixel(x+3,y+4,c);}
void drawIconBolt(TFT_eSprite&s,int x,int y,uint16_t c){s.drawLine(x+3,y,x+1,y+3,c);s.drawFastHLine(x+1,y+3,3,c);s.drawLine(x+3,y+3,x+1,y+6,c);}
void drawIconApple(TFT_eSprite&s,int x,int y,uint16_t c){s.fillCircle(x+2,y+3,2,c);s.fillCircle(x+4,y+3,2,c);s.drawLine(x+3,y,x+4,y-1,0x3E05);}
void drawIconEye(TFT_eSprite&s,int x,int y,uint16_t c){s.drawEllipse(x+3,y+2,3,2,c);s.fillCircle(x+3,y+2,1,c);}

void drawStatus(TFT_eSprite &s) {
    int y=185; s.fillRect(0,y,SCR_W,SCR_H-y,0x0841); s.drawFastHLine(0,y,SCR_W,0x2945);
    int r1=y+4;

    // Weather: real or simulated
    s.setTextDatum(TL_DATUM);
    if (realWeather.valid) {
        char tb[8]; snprintf(tb,8,"%.0fC",realWeather.temp);
        uint16_t tc=realWeather.temp<0?0x7DFF:realWeather.temp<10?0x07FF:realWeather.temp<20?0x07E0:realWeather.temp<30?0xFFE0:0xF800;
        s.setTextColor(tc,0x0841); s.drawString(tb,3,r1,2);
        s.setTextColor(0x6B4D,0x0841); s.drawString(wmoDesc(realWeather.code),3,r1+16,1);
    } else {
        const char* wn[]={"Clear","Cloudy","Rain","Snow"};
        s.setTextColor(0x07FF,0x0841); s.drawString(wn[brain.weather],3,r1,2);
    }

    // Time: real or simulated
    char tb[6]; struct tm t;
    if (brain.ntpSynced && getLocalTime(&t,10))
        snprintf(tb,6,"%02d:%02d",t.tm_hour,t.tm_min);
    else
        snprintf(tb,6,"%02d:00",brain.hourOfDay);
    s.setTextDatum(TR_DATUM); s.setTextColor(0xFFFF,0x0841); s.drawString(tb,SCR_W-3,r1,4);

    const char* mn[]={"Happy","Content","Bored","Sleepy","Hungry","Excited","Curious"};
    uint16_t mc[]={0x07E0,0xFFFF,0x7BEF,0x7BEF,0xFDA0,0xFFE0,0x07FF};
    s.setTextColor(mc[brain.mood],0x0841); s.drawString(mn[brain.mood],SCR_W-3,r1+18,1);
    s.setTextDatum(TL_DATUM);
    int dv=r1+27; for(int i=4;i<SCR_W-4;i+=3) s.drawPixel(i,dv,0x2104);
    int r2=dv+3;
    // Hunger inverted: 0 hunger = full bar (100% fed), 100 hunger = empty bar
    float foodDisplay = 100.0f - brain.hunger;
    struct{float v;uint16_t c;void(*ic)(TFT_eSprite&,int,int,uint16_t);}b[]={
        {brain.happiness,0xF800,drawIconHeart},{brain.energy,0xFFE0,drawIconBolt},
        {foodDisplay,0xFDA0,drawIconApple},{brain.curiosity,0x07FF,drawIconEye}};
    for(int i=0;i<4;i++){
        int bx=i*26+3;
        b[i].ic(s,bx,r2,b[i].c);
        s.fillRect(bx,r2+9,18,5,0x2104);
        int fw=(int)(b[i].v/100.0f*18);
        if(fw>0) s.fillRect(bx,r2+9,fw,5,b[i].c);
        s.drawRect(bx,r2+9,18,5,0x18C3);
    }

    // CSI presence indicator (right side)
    int px = SCR_W - 20;
    if (brain.presenceDetected) {
        // Pulsing WiFi icon = someone is nearby
        s.fillCircle(px+5, r2+6, 2, 0x07E0);
        s.drawCircle(px+5, r2+6, 5, 0x07E0);
        s.drawCircle(px+5, r2+6, 8, 0x07E0);
        // "HI" text
        s.setTextColor(0x07E0, 0x0841);
        s.setTextDatum(TL_DATUM);
        s.drawString("HI", px, r2+12, 1);
    } else {
        // Dim WiFi icon = alone
        s.fillCircle(px+5, r2+6, 2, 0x2104);
        s.drawCircle(px+5, r2+6, 5, 0x2104);
    }
}

void drawSky(TFT_eSprite &s) {
    int h = brain.hourOfDay;
    bool isHome = (brain.env == ENV_HOME);

    if (isHome) {
        // Home has its own wall - drawHome fills the background
        s.fillSprite(0xBDD7); // Warm wall color
        return;
    }

    uint16_t sky;
    if (h>=7&&h<17) sky = ENV_COLORS[brain.env].sky;
    else if ((h>=17&&h<20)||(h>=5&&h<7)) sky = 0xFA60;
    else sky = 0x0008;
    s.fillSprite(sky);

    // Sun
    if (h>=7&&h<17) {
        int sunY = 18 + abs(12-h)*2;
        s.fillCircle(110, sunY, 9, 0xFFE0);
        s.fillCircle(110, sunY, 6, 0xFFC0);
    }
    // Moon + stars
    else if (h>=20||h<5) {
        s.fillCircle(100,22,7,0xE71C); s.fillCircle(103,19,7,sky);
        for(int i=0;i<12;i++) {
            int sx=(i*29+3)%SCR_W, sy=4+(i*17)%55;
            s.drawPixel(sx,sy,0xFFFF);
            if(i%3==0) s.drawPixel(sx+1,sy,0xC618); // Brighter stars
        }
    }
    // Clouds (gentle)
    if (h>=5&&h<21) {
        int cx=(int)(millis()/250)%(SCR_W+40)-20;
        s.fillEllipse(cx,35,14,5,0xFFFF);
        s.fillEllipse(cx+10,32,9,4,0xFFFF);
        s.fillEllipse(cx-6,34,7,4,0xE71C);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Capybara Pet v3 ===");
    pinMode(32, OUTPUT); digitalWrite(32, HIGH);
    pinMode(BTN_PIN, INPUT_PULLUP);
    tft.init(); tft.setRotation(0); tft.fillScreen(0);
    spr.createSprite(SCR_W, SCR_H);

    // === ANIMATED BOOT SCREEN ===
    for (int bootFrame = 0; bootFrame < 40; bootFrame++) {
        spr.fillSprite(0x0000);

        // Gradient sky appearing
        int skyH = min(bootFrame * 5, 120);
        for (int y = 0; y < skyH; y++) {
            uint16_t c = (y < 40) ? 0x6DFF : (y < 80) ? 0x5D9F : 0x4D3F;
            spr.drawFastHLine(0, y, SCR_W, c);
        }

        // Ground sliding up
        if (bootFrame > 10) {
            int gY = 240 - (bootFrame - 10) * 4;
            if (gY < 168) gY = 168;
            spr.fillRect(0, gY, SCR_W, SCR_H - gY, 0x2C04);
            for (int i = 2; i < SCR_W; i += 5) spr.drawLine(i, gY, i, gY - 2 - (i%3), 0x3E05);
        }

        // Stars twinkling
        if (bootFrame < 20) {
            for (int i = 0; i < 15; i++) {
                int sx = (i * 31 + 7) % SCR_W;
                int sy = (i * 17 + 3) % 100;
                if ((bootFrame + i) % 3 == 0) spr.drawPixel(sx, sy, 0xFFFF);
            }
        }

        // Sun rising
        if (bootFrame > 15) {
            int sunY = 120 - (bootFrame - 15) * 4;
            if (sunY < 30) sunY = 30;
            int sunR = min((bootFrame - 15), 12);
            spr.fillCircle(SCR_W - 25, sunY, sunR, 0xFFE0);
            // Sun rays
            if (bootFrame > 25) {
                for (int r = 0; r < 6; r++) {
                    float a = r * 1.047f + bootFrame * 0.05f;
                    int dx = cos(a) * (sunR + 5);
                    int dy = sin(a) * (sunR + 5);
                    spr.drawLine(SCR_W-25, sunY, SCR_W-25+dx, sunY+dy, 0xFFE0);
                }
            }
        }

        // Capybara walking in from left
        if (bootFrame > 8) {
            int cx = -20 + (bootFrame - 8) * 3;
            if (cx > 30) cx = 30;
            int cy = 168 - 25;
            // Body
            spr.fillEllipse(cx+18, cy+12, 15, 9, 0x9B26);
            spr.fillEllipse(cx+18, cy+15, 10, 5, 0xBC89);
            // Head
            spr.fillRoundRect(cx+28, cy+3, 12, 11, 2, 0x9B26);
            spr.fillRoundRect(cx+35, cy+8, 7, 5, 2, 0xBC89);
            spr.fillRect(cx+38, cy+8, 2, 1, 0x3186);
            spr.fillCircle(cx+33, cy+6, 1, 0x0000);
            spr.drawPixel(cx+34, cy+5, 0xFFFF);
            // Ears
            spr.fillCircle(cx+30, cy+1, 2, 0x9B26);
            spr.fillCircle(cx+34, cy+1, 2, 0x9B26);
            // Legs walking
            int lf = (bootFrame / 3) % 4;
            spr.fillRect(cx+8, cy+18+(lf<2?0:2), 3, 7, 0x6220);
            spr.fillRect(cx+26, cy+18+(lf<2?2:0), 3, 7, 0x6220);
            // Orange on head (appears later)
            if (bootFrame > 25) {
                spr.fillCircle(cx+31, cy-3, 4, 0xFD20);
                spr.fillRect(cx+30, cy-8, 2, 3, 0x07E0);
            }
        }

        // Title text fading in
        if (bootFrame > 18) {
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(0xFFFF);
            spr.drawString("CAPYBARA", SCR_W/2, SCR_H/2 + 5, 4);
            if (bootFrame > 22) {
                spr.setTextColor(0x07FF);
                spr.drawString("AI", SCR_W/2, SCR_H/2 + 30, 4);
                spr.setTextColor(0xFD20);
                spr.drawString("PET", SCR_W/2, SCR_H/2 + 55, 4);
            }
        }

        // Version + instructions at end
        if (bootFrame > 30) {
            spr.setTextColor(0x4A49);
            spr.drawString("v3.0", SCR_W/2, SCR_H/2 + 75, 1);
            if ((bootFrame/3) % 2 == 0) {
                spr.setTextColor(0x07FF);
                spr.drawString("tap=pet  hold=games", SCR_W/2, SCR_H - 20, 1);
            }
        }

        spr.pushSprite(0, 0);
        delay(80);
    }
    delay(1500);
    setupWiFi();
    delay(500);
    brain.begin();
    // Connect game rewards to pet stats
    game.pHappiness = &brain.happiness;
    game.pHunger = &brain.hunger;
    game.pEnergy = &brain.energy;
}

void loop() {
    unsigned long now = millis();

    // Button - must re-init pin before reading because SPI clobbers GPIO 0
    pinMode(BTN_PIN, INPUT_PULLUP);
    delayMicroseconds(50); // Let pullup settle
    bool raw = (digitalRead(BTN_PIN) == LOW);
    btnJustDown = raw && !lastBtn;
    btnJustUp = !raw && lastBtn;
    if (btnJustDown) { btnDownSince = now; longDone = false; btnDown = true; }
    if (btnJustUp) { btnHeldMs = now - btnDownSince; btnDown = false; }
    lastBtn = raw;

    // Short press on release
    if (btnJustUp && btnHeldMs < 1500 && !longDone) {
        if (game.state == GS_MENU) {
            // Cycle menu selection
            game.menuSelection = (game.menuSelection + 1) % 3;
            Serial.printf("[MENU] Selected %d\n", game.menuSelection);
        } else if (game.state == GS_PLAYING || game.state == GS_INVITE || game.state == GS_GAMEOVER) {
            // game gets it via setButton
        } else {
            brain.interact();
        }
    }

    // Long press while held
    if (btnDown && !longDone && (now - btnDownSince > 1500)) {
        if (game.state == GS_MENU) {
            // Launch selected game
            GameType gt = (GameType)(game.menuSelection + 1); // 1=runner, 2=catch, 3=rhythm
            game.startGame(gt);
            Serial.printf("[GAME] Start %d!\n", gt);
        } else if (game.state == GS_IDLE || game.state == GS_INVITE) {
            // Open game menu
            game.openMenu();
            Serial.println("[MENU] Open");
        } else if (game.state == GS_PLAYING) {
            game.endGame();
        }
        longDone = true;
    }

    game.setButton(btnDown);

    // WiFi + NTP + Weather + CSI (separate timers!)
    static unsigned long tWifi=0, tNtp=0, tWeather=0, tCSI=0, tPing=0, tLog=0;

    if(now-tLog>10000){Serial.printf("[NET] wifi=%d ntp=%d weather=%d\n",WiFi.status(),brain.ntpSynced,realWeather.valid);tLog=now;}

    if(WiFi.status()!=WL_CONNECTED && now-tWifi>15000){WiFi.begin(WIFI_SSID,WIFI_PASS);tWifi=now;}

    if(WiFi.status()==WL_CONNECTED && !brain.ntpSynced && now-tNtp>3000){
        configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4","pool.ntp.org","time.google.com");
        struct tm t;if(getLocalTime(&t,500)){
            brain.realHour=t.tm_hour;brain.hourOfDay=t.tm_hour;brain.ntpSynced=true;
            Serial.printf("[NTP] %02d:%02d\n",t.tm_hour,t.tm_min);}
        tNtp=now;}

    if(WiFi.status()==WL_CONNECTED && !realWeather.valid && now-tWeather>5000){fetchWeather();tWeather=now;}
    if(realWeather.valid && now-tWeather>900000){fetchWeather();tWeather=now;}

    // CSI: retry init if no data coming + check variance
    static bool csiReady = false;
    if(WiFi.status()==WL_CONNECTED && !csiReady && now-tCSI>5000) {
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_csi(true);
        wifi_csi_config_t cfg={true,true,true,true,false,false,false};
        esp_wifi_set_csi_config(&cfg);
        esp_wifi_set_csi_rx_cb(csi_cb,NULL);
        Serial.println("[CSI] Retry init");
        tCSI=now;
        if(csiIdx > 0) csiReady = true;
    }
    if(now-tCSI>2000){
        float cv=csiVariance();
        brain.presenceDetected=(cv>5.0f);
        static bool lastPres = false;
        if (brain.presenceDetected != lastPres) {
            Serial.printf("[CSI] Presence: %s (var=%.1f)\n", brain.presenceDetected?"YES":"NO", cv);
            lastPres = brain.presenceDetected;
        }
        tCSI=now;
    }
    // UDP ping every 50ms + DNS every 2s = more CSI data
    static WiFiUDP pu; static bool pi2=false;
    if(WiFi.status()==WL_CONNECTED&&now-tPing>50){
        if(!pi2){pu.begin(12345);pi2=true;}
        pu.beginPacket(WiFi.gatewayIP(),53); // Port 53 = DNS, router will respond
        uint8_t d[]={0x00,0x01,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x67,0x6f,0x6f,0x67,0x6c,0x65,0x03,0x63,0x6f,0x6d,0x00,0x00,0x01,0x00,0x01};
        pu.write(d,sizeof(d)); pu.endPacket(); // DNS query for google.com
        tPing=now;
    }

    // 10 FPS
    static unsigned long lastDraw = 0;
    if (now - lastDraw < 100) { delay(10); return; }
    float dt = (now - lastDraw) / 1000.0f;
    if (dt > 0.5f) dt = 0.5f;
    lastDraw = now;

    brain.update(dt);

    static float boredT = 0;
    if (brain.happiness < 30 && game.state == GS_IDLE) { boredT += dt; if (boredT > 15) { game.showInvite(); boredT = 0; } } else boredT = 0;

    bool ga = game.update(dt);

    // Rewards applied inside game.endGame() directly

    if (ga) { game.draw(spr); }
    else {
        drawSky(spr);
        drawEnvironment(spr, brain.env, brain.animFrame);
        if (brain.env != ENV_HOME) drawWeather(spr, brain.weather, brain.animFrame);
        drawBird(spr, brain.animFrame, brain.birdActive);
        if (brain.env == ENV_LAKE) drawDuck(spr, (int)brain.posX, brain.animFrame, brain.duckActive);
        drawTurtle(spr, brain.animFrame, brain.turtleActive);
        drawCapy(spr, (int)brain.posX, GROUND_Y-30+(int)brain.posY, brain.facingLeft, brain.animFrame, brain.behavior);
        if (game.state == GS_INVITE) game.draw(spr);
        drawStatus(spr);
    }
    spr.pushSprite(0, 0);
}

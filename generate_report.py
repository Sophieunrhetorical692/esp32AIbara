#!/usr/bin/env python3
"""Capybara Pet PDF Report - warm capybara theme."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, black, white, Color
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                 TableStyle, PageBreak)
from reportlab.graphics.shapes import Drawing, Rect, Circle
from reportlab.pdfgen import canvas
from datetime import datetime

W, H = A4

# Capybara-themed palette (matches the game!)
BG = HexColor('#FFF8F0')         # Warm cream background (like sky)
BG_ALT = HexColor('#FFF0E0')     # Slightly darker cream
BROWN = HexColor('#9B6426')      # Capybara body color
BROWN_DARK = HexColor('#62200E') # Dark brown (outlines)
BROWN_LIGHT = HexColor('#BC8945')# Light belly
GREEN = HexColor('#2C6404')      # Grass green
GREEN_LIGHT = HexColor('#3E8505')# Light grass
BLUE_SKY = HexColor('#6DAFFF')   # Sky blue
BLUE_WATER = HexColor('#1B5C9E') # Water blue
ORANGE = HexColor('#FD8C20')     # Mikan orange
RED = HexColor('#E04040')        # Hearts
PINK = HexColor('#FB6C8C')       # Blush
YELLOW = HexColor('#FFD700')     # Stars/sun
TEXT = HexColor('#2A1810')        # Very dark brown text
TEXT_MID = HexColor('#5C3D2E')    # Medium brown
TEXT_LIGHT = HexColor('#8B6E5E')  # Light brown caption
BORDER = HexColor('#D4B896')      # Warm border
HEADER_BG = BROWN                 # Table headers = capybara brown


class CapyCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        canvas.Canvas.__init__(self, *args, **kwargs)
        self._saved = []

    def showPage(self):
        self._saved.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        n = len(self._saved)
        for s in self._saved:
            self.__dict__.update(s)
            # Top brown accent line
            self.setFillColor(BROWN)
            self.rect(0, H - 8*mm, W, 8*mm, fill=1, stroke=0)
            # Green grass strip at bottom
            self.setFillColor(GREEN)
            self.rect(0, 0, W, 5*mm, fill=1, stroke=0)
            self.setFillColor(GREEN_LIGHT)
            self.rect(0, 5*mm, W, 2*mm, fill=1, stroke=0)
            # Header text
            self.setFont('Helvetica-Bold', 8)
            self.setFillColor(white)
            self.drawString(20*mm, H - 6*mm, "CAPYBARA PET")
            self.setFont('Helvetica', 7)
            self.drawRightString(W - 20*mm, H - 6*mm, f"Page {self._pageNumber}/{n}")
            # Footer
            self.setFont('Helvetica', 6)
            self.setFillColor(white)
            self.drawCentredString(W/2, 1.5*mm, "Technical Report v3.0")
            canvas.Canvas.showPage(self)
        canvas.Canvas.save(self)


def S(name, **kw):
    d = {'fontName': 'Helvetica', 'fontSize': 10, 'textColor': TEXT,
         'leading': 14, 'spaceAfter': 2*mm}
    d.update(kw)
    return ParagraphStyle(name, **d)

ST = {
    'title': S('t', fontName='Helvetica-Bold', fontSize=32, textColor=BROWN,
               alignment=TA_CENTER, spaceAfter=3*mm, leading=36),
    'subtitle': S('st', fontSize=13, textColor=BLUE_SKY, alignment=TA_CENTER, spaceAfter=5*mm),
    'h1': S('h1', fontName='Helvetica-Bold', fontSize=17, textColor=BROWN,
            spaceBefore=6*mm, spaceAfter=3*mm),
    'h2': S('h2', fontName='Helvetica-Bold', fontSize=12, textColor=ORANGE,
            spaceBefore=4*mm, spaceAfter=2*mm),
    'h3': S('h3', fontName='Helvetica-Bold', fontSize=10, textColor=GREEN,
            spaceBefore=3*mm, spaceAfter=2*mm),
    'body': S('b', alignment=TA_JUSTIFY, textColor=TEXT),
    'bullet': S('bl', leftIndent=15, bulletIndent=5, spaceAfter=1.5*mm, textColor=TEXT),
    'code': S('c', fontName='Courier', fontSize=8, textColor=GREEN,
              backColor=BG_ALT, leftIndent=8, rightIndent=8, spaceAfter=3*mm,
              leading=11, borderWidth=0.5, borderColor=BORDER, borderPadding=6),
    'caption': S('cap', fontSize=8, textColor=TEXT_LIGHT, alignment=TA_CENTER, spaceAfter=4*mm),
    'footer': S('f', fontSize=8, textColor=TEXT_LIGHT, alignment=TA_CENTER),
    'cover_ver': S('cv', fontSize=11, textColor=TEXT_MID, alignment=TA_CENTER),
}

def brown_line():
    d = Drawing(170*mm, 4)
    d.add(Rect(0, 1, 170*mm, 2, fillColor=BROWN, strokeColor=None))
    return d

def green_line():
    d = Drawing(170*mm, 3)
    d.add(Rect(0, 1, 170*mm, 1, fillColor=GREEN_LIGHT, strokeColor=None))
    return d

def T(headers, rows, widths=None, hcolor=BROWN):
    data = [headers] + rows
    if not widths: widths = [170*mm/len(headers)]*len(headers)
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), hcolor),
        ('TEXTCOLOR', (0,0), (-1,0), white),
        ('FONTNAME', (0,0), (-1,0), 'Helvetica-Bold'),
        ('FONTSIZE', (0,0), (-1,0), 8),
        ('FONTNAME', (0,1), (-1,-1), 'Helvetica'),
        ('FONTSIZE', (0,1), (-1,-1), 8),
        ('TEXTCOLOR', (0,1), (-1,-1), TEXT),
        ('ALIGN', (0,0), (-1,-1), 'CENTER'),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('ROWBACKGROUNDS', (0,1), (-1,-1), [white, BG_ALT]),
        ('GRID', (0,0), (-1,-1), 0.5, BORDER),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 5),
        ('RIGHTPADDING', (0,0), (-1,-1), 5),
    ]))
    return t

def stat_card(val, label, c):
    return f'<font color="{c.hexval()}" size="22"><b>{val}</b></font><br/><font color="{TEXT_LIGHT.hexval()}" size="7">{label}</font>'

def build():
    doc = SimpleDocTemplate(
        '/home/definitelynotme/Desktop/esp32/capybara-pet/Capybara_Pet_Report.pdf',
        pagesize=A4, leftMargin=20*mm, rightMargin=20*mm,
        topMargin=14*mm, bottomMargin=12*mm)
    s = []

    # ===== COVER =====
    s.append(Spacer(1, 30*mm))
    # Decorative dots (capybara colors)
    d = Drawing(170*mm, 15)
    cols = [BROWN, ORANGE, GREEN, BLUE_SKY, RED]
    for i in range(5): d.add(Circle(30+i*70, 7, 5, fillColor=cols[i], strokeColor=None))
    s.append(d)
    s.append(Spacer(1, 8*mm))
    s.append(Paragraph("CAPYBARA PET", ST['title']))
    s.append(brown_line())
    s.append(Spacer(1, 4*mm))
    s.append(Paragraph("AI-Powered Virtual Pet on ESP32 Microcontroller", ST['subtitle']))
    s.append(Paragraph("Deep Neural Network · Real-Time Weather · WiFi Presence Detection · Mini-Games", ST['cover_ver']))
    s.append(Spacer(1, 12*mm))

    cards = Table([[
        Paragraph(stat_card("8,130", "NN Parameters", BROWN), ST['body']),
        Paragraph(stat_card("500K", "Training Samples", GREEN), ST['body']),
        Paragraph(stat_card("2,329", "Lines of Code", ORANGE), ST['body']),
        Paragraph(stat_card("80K+", "Unique States", BLUE_SKY), ST['body']),
    ]], colWidths=[42.5*mm]*4)
    cards.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), white),
        ('BOX', (0,0), (-1,-1), 1.5, BROWN),
        ('INNERGRID', (0,0), (-1,-1), 0.5, BORDER),
        ('ALIGN', (0,0), (-1,-1), 'CENTER'),
        ('TOPPADDING', (0,0), (-1,-1), 10),
        ('BOTTOMPADDING', (0,0), (-1,-1), 10),
    ]))
    s.append(cards)
    s.append(Spacer(1, 12*mm))

    badges = Table([["ESP32-WROOM-32", "ST7789 135x240", "Arduino+PlatformIO", "Open-Meteo API"]], colWidths=[42.5*mm]*4)
    badges.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (0,0), BROWN), ('BACKGROUND', (1,0), (1,0), GREEN),
        ('BACKGROUND', (2,0), (2,0), ORANGE), ('BACKGROUND', (3,0), (3,0), BLUE_SKY),
        ('TEXTCOLOR', (0,0), (-1,-1), white), ('FONTNAME', (0,0), (-1,-1), 'Helvetica-Bold'),
        ('FONTSIZE', (0,0), (-1,-1), 8), ('ALIGN', (0,0), (-1,-1), 'CENTER'),
        ('TOPPADDING', (0,0), (-1,-1), 5), ('BOTTOMPADDING', (0,0), (-1,-1), 5),
    ]))
    s.append(badges)
    s.append(Spacer(1, 20*mm))
    s.append(green_line())
    s.append(Spacer(1, 4*mm))
    s.append(Paragraph(f"Version 3.0 · {datetime.now().strftime('%B %d, %Y')} · Tallinn, Estonia", ST['footer']))
    s.append(PageBreak())

    # ===== TOC =====
    s.append(Paragraph("CONTENTS", ST['h1']))
    s.append(brown_line())
    s.append(Spacer(1, 4*mm))
    toc = [("01","Project Overview","Key capabilities and features"),("02","Hardware Platform","ESP32 specs and pin config"),
           ("03","Software Stack","Frameworks and source structure"),("04","Neural Network Model","Architecture, training, validation"),
           ("05","Pet Behavior System","Stats, moods, transitions, animations"),("06","Mini-Games","Runner, Catch, Rhythm"),
           ("07","Environments & Weather","4 scenes + real-time weather"),("08","WiFi Integration","NTP, API, CSI presence"),
           ("09","Technical Challenges","Problems solved"),("10","Performance","Resource usage and capacity")]
    for n,t2,d in toc:
        row = Table([[Paragraph(f'<font color="{ORANGE.hexval()}" size="13"><b>{n}</b></font>', ST['body']),
                      Paragraph(f'<b><font color="{BROWN.hexval()}">{t2}</font></b><br/><font color="{TEXT_LIGHT.hexval()}" size="8">{d}</font>', ST['body'])]],
                    colWidths=[14*mm, 156*mm])
        row.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),BG_ALT),('GRID',(0,0),(-1,-1),0.5,BORDER),
                                  ('VALIGN',(0,0),(-1,-1),'MIDDLE'),('TOPPADDING',(0,0),(-1,-1),4),('BOTTOMPADDING',(0,0),(-1,-1),4)]))
        s.append(row); s.append(Spacer(1,1*mm))
    s.append(PageBreak())

    # ===== 01 =====
    s.append(Paragraph('<font color="#FD8C20">01</font> PROJECT OVERVIEW', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(Paragraph('Capybara Pet is an AI-powered virtual pet running on a <b>$5 ESP32 microcontroller</b> with a 1.14" color TFT display. The capybara exhibits autonomous behavior driven by a <b>deep neural network</b> (26 inputs → 96 → 48 → 18 outputs) trained on 500,000 simulated decisions. It connects to WiFi for <b>real-time weather</b> data, <b>NTP time</b>, and uses <b>WiFi CSI</b> to detect human presence.', ST['body']))
    s.append(Paragraph("Key Features", ST['h2']))
    for f in [
        ("Neural Network Brain", "8,130 parameters, trained on 500K decisions, <0.1ms inference"),
        ("18 Behaviors", "With 8 idle variants, 5 transition types, breathing animation"),
        ("Real Weather", "Open-Meteo API — live temperature and conditions for Tallinn"),
        ("CSI Presence", "Detects nearby humans via WiFi signal analysis"),
        ("4 Environments", "Meadow, Lake, Forest, Home — each with unique visuals"),
        ("3 Mini-Games", "Runner, Catch (with powerups), Rhythm (with combos)"),
        ("Persistent Memory", "Stats saved to flash every 60 seconds"),
        ("Natural Chains", "Yawn→stretch→sleep, sniff→eat, wake→look around"),
    ]:
        s.append(Paragraph(f'<bullet><font color="{GREEN.hexval()}">●</font></bullet> <b>{f[0]}:</b> {f[1]}', ST['bullet']))
    s.append(PageBreak())

    # ===== 02 =====
    s.append(Paragraph('<font color="#FD8C20">02</font> HARDWARE PLATFORM', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Component","Specification"],
        [["Board","Ideaspark ESP32 with integrated TFT"],["MCU","ESP32-D0WD-V3 rev3.0 — Dual-core Xtensa LX6 @ 240MHz"],
         ["SRAM","520KB (327KB available)"],["Flash","8MB GigaDevice"],
         ["Display","ST7789 IPS — 135×240px, 65K colors, SPI @ 40MHz"],
         ["WiFi","802.11 b/g/n, 2.4GHz, WPA2"],["USB","Type-C, CH340"],
         ["Input","BOOT button (GPIO 0)"],["Power","5V USB-C (~200mA)"]],
        [50*mm,120*mm]))
    s.append(PageBreak())

    # ===== 03 =====
    s.append(Paragraph('<font color="#FD8C20">03</font> SOFTWARE STACK', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Layer","Technology","Purpose"],
        [["Platform","Arduino on ESP-IDF","Hardware abstraction"],
         ["Build","PlatformIO CLI","Compilation, dependencies, flashing"],
         ["Display","TFT_eSPI + Sprite","HW-accelerated SPI, double buffering"],
         ["AI Inference","Custom C++ forward pass","Matrix multiply + ReLU in PROGMEM"],
         ["AI Training","Python + NumPy","Behavioral cloning offline"],
         ["Networking","WiFi + HTTPClient","Weather, NTP, CSI"],
         ["JSON","ArduinoJson v7","API response parsing"],
         ["Storage","Preferences (NVS)","Persistent pet state"],
         ["Weather","Open-Meteo (free)","No API key required"],
         ["Time","NTP + configTzTime","POSIX timezone EET"]],
        [28*mm,42*mm,100*mm]))
    s.append(Paragraph("Source Files", ST['h2']))
    s.append(T(["File","Lines","Description"],
        [["main.cpp","~550","App loop, rendering, WiFi, buttons"],
         ["pet_brain.h","~560","NN brain, stats, behaviors, save/load"],
         ["environments.h","~400","4 environments, weather, friends"],
         ["minigames.h","~700","3 mini-games with full game loops"],
         ["brain_weights.h","~25","8,130 NN parameters (PROGMEM)"],
         ["train_brain.py","~200","Training: data gen, NN training, export"]],
        [35*mm,15*mm,120*mm], GREEN))
    s.append(PageBreak())

    # ===== 04 =====
    s.append(Paragraph('<font color="#FD8C20">04</font> NEURAL NETWORK MODEL', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(Paragraph('The capybara brain is a <b>deep feedforward neural network</b> with two hidden layers, trained offline via behavioral cloning. Deployed as PROGMEM float arrays. Inference: <b>&lt;100µs</b>.', ST['body']))
    s.append(Paragraph("Architecture", ST['h2']))
    s.append(Paragraph('<font name="Courier" size="9">  Input[26] ──→ Hidden1[96] ──→ Hidden2[48] ──→ Output[18]\n              ReLU           ReLU           argmax</font>', ST['code']))
    s.append(T(["Layer","Size","Activation","Parameters"],
        [["Input","26","—","0"],["Hidden 1","96","ReLU","2,592"],
         ["Hidden 2","48","ReLU","4,656"],["Output","18","argmax","882"],
         ["TOTAL","","","8,130 (32.5 KB)"]],
        [28*mm,28*mm,28*mm,86*mm]))

    s.append(Paragraph("26 Input Features", ST['h2']))
    s.append(T(["#","Feature","Range","Description"],
        [["0–4","Core Stats","[0,1]","Happiness, Energy, Hunger, Curiosity, Loneliness"],
         ["5–6","Time","[-1,1]","sin/cos of hour (circular encoding)"],
         ["7","Night","0/1","Nighttime flag (22:00–06:00)"],
         ["8–10","Environment","0/1","One-hot: Lake, Forest, Home"],
         ["11","Presence","0/1","WiFi CSI human detection"],
         ["12–14","Previous","0/1","Was sleeping, eating, or active"],
         ["15","Temperature","[-1,1]","Normalized real temperature"],
         ["16","Rain","0/1","Currently raining"],
         ["17–19","Trends","float","Stat rates of change"],
         ["20","Duration","[0,1]","Time in current behavior"],
         ["21–25","History","[0,1]","Category fractions in last 5 actions"]],
        [12*mm,28*mm,16*mm,114*mm], ORANGE))

    s.append(Paragraph("Training", ST['h2']))
    s.append(T(["Parameter","Value"],
        [["Paradigm","Behavioral Cloning from Expert Policy"],
         ["Samples","500,000 state-action pairs"],
         ["Labels","Soft: 20% distribution + 80% sampled"],
         ["Optimizer","Mini-batch SGD, LR 0.05→0.001"],
         ["Batch/Epochs","4,096 / 3,000"],
         ["Final Loss","2.587 (cross-entropy)"],
         ["Storage","PROGMEM (flash, 0% RAM)"],
         ["Inference","< 100 µs"]],
        [45*mm,125*mm]))

    s.append(Paragraph("Validation Results", ST['h2']))
    s.append(T(["Scenario","1st","2nd","3rd"],
        [["Starving (95%)","eat_grass 26%","melon 26%","doze 4%"],
         ["Exhausted night","sleep 21%","doze 12%","yawn 6%"],
         ["Bored+lake","swim 15%","sniff 12%","butterfly 9%"],
         ["After sleep","stretch 10%","look 10%","eat 9%"],
         ["Forest curious","sniff 23%","butterfly 14%","look 13%"],
         ["Cold rain home","sleep 31%","doze 13%","mikan 7%"],
         ["Owner+bored","sniff 11%","shy 11%","look 10%"],
         ["Slept too much","look 11%","mikan 9%","sniff 9%"]],
        [38*mm,44*mm,44*mm,44*mm], GREEN))
    s.append(PageBreak())

    # ===== 05 =====
    s.append(Paragraph('<font color="#FD8C20">05</font> PET BEHAVIOR SYSTEM', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Stat","Icon","Drain","Recovery","Cross-Effects"],
        [["Happiness","Heart","-0.10/s","Playing, petting","Drops if hungry/lonely"],
         ["Energy","Bolt","-0.15/s","Sleeping, dozing","Low → curiosity drops"],
         ["Hunger","Apple","+0.18/s","Eating, games","High → happiness drops"],
         ["Curiosity","Eye","+0.15/s","Sniffing, looking","Drops when exhausted"],
         ["Loneliness","WiFi","+0.10/s","CSI presence","High → happiness drops"]],
        [20*mm,14*mm,18*mm,42*mm,68*mm], RED))
    s.append(Paragraph("Transitions & Animations", ST['h2']))
    s.append(T(["Transition","Trigger","Visual Effect"],
        [["Lie Down","Before sleeping","Body compresses"],
         ["Stand Up","After waking","Body extends, ears wiggle"],
         ["Sniff Before Eat","50% before eating","Head bobs"],
         ["Look Then Walk","33% before walking","Looks in direction first"],
         ["Turn Around","50% before running","Flips direction"]],
        [30*mm,40*mm,100*mm], ORANGE))
    s.append(Paragraph('8 idle sub-animations cycle randomly: Standing, Blinking, Ear Twitching, Tail Wagging, Head Tilting, Sniffing Air, Shifting Weight, Looking Up. Constant breathing animation (sinusoidal body pulsation).', ST['body']))
    s.append(PageBreak())

    # ===== 06 =====
    s.append(Paragraph('<font color="#FD8C20">06</font> MINI-GAMES', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(Paragraph('3 games via long-press menu. Each costs <b>15 energy</b>. Rewards scale with score.', ST['body']))
    s.append(T(["Game","Mechanic","Special Features"],
        [["Runner","Jump obstacles","Parallax, 3 obstacle types, shadow, speed ramp"],
         ["Catch","Collect items","5 types: orange, melon, heart(+life), star(magnet), bomb. Combo→shield"],
         ["Rhythm","Hit notes","3 colored lanes, combo multiplier, mis-press = -1 life"]],
        [22*mm,30*mm,118*mm]))
    s.append(Paragraph("Controls", ST['h2']))
    s.append(T(["Context","Short Press","Long Press (1.5s)"],
        [["Normal","Pet (+10 happiness)","Open game menu"],
         ["Menu","Cycle games","Start selected"],
         ["In game","Action","Exit game"],
         ["Game over","Dismiss","—"]],
        [30*mm,70*mm,70*mm], BLUE_SKY))
    s.append(PageBreak())

    # ===== 07 =====
    s.append(Paragraph('<font color="#FD8C20">07</font> ENVIRONMENTS & WEATHER', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Env","Elements","Rules"],
        [["Meadow","Hills, swaying grass, flowers, butterflies","Default scene"],
         ["Lake","3-shade water, shore, reeds, lilypads","Swim only here; walking on shore"],
         ["Forest","2-layer trees, mushrooms, fireflies, moss","Boosts curiosity"],
         ["Home","Wall, floor, window+curtains, painting, plant","No weather; boosts comfort"]],
        [20*mm,75*mm,70*mm], GREEN))
    s.append(Paragraph("Real-Time Weather (Open-Meteo API)", ST['h2']))
    s.append(T(["Condition","WMO","Visual"],
        [["Clear","0","Sun with glow"],["Cloudy","1-3","Dark cloud layers"],
         ["Rain","51-82","Drops, puddle ripples"],["Snow","71-77","Drifting flakes, accumulation"]],
        [25*mm,20*mm,125*mm], BLUE_SKY))
    s.append(PageBreak())

    # ===== 08 =====
    s.append(Paragraph('<font color="#FD8C20">08</font> WIFI INTEGRATION', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Feature","Implementation","Details"],
        [["NTP","configTzTime + POSIX TZ","EET-2EEST, auto DST, retry on failure"],
         ["Weather","HTTP GET → ArduinoJson","api.open-meteo.com, every 15 min"],
         ["CSI","esp_wifi_set_csi callback","Promiscuous + DNS pings → variance > 5 = presence"],
         ["Reconnect","Separate timers","WiFi/NTP/Weather retry independently"],
         ["Presence UI","Green WiFi icon + HI","Visible on status bar when detected"]],
        [25*mm,42*mm,103*mm]))
    s.append(PageBreak())

    # ===== 09 =====
    s.append(Paragraph('<font color="#FD8C20">09</font> TECHNICAL CHALLENGES', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Problem","Cause","Solution"],
        [["GPIO 0 stuck LOW","SPI display clobbers GPIO 0 as SPI_HD","pinMode(INPUT_PULLUP) before every read"],
         ["Screen flicker","Direct draw-erase visible","64KB sprite double buffer"],
         ["CSI never fires","Init timing + needs promiscuous","Retry init in loop + enable promiscuous"],
         ["Rewards x5","Game over checked every frame","Apply in endGame() via pointer"],
         ["NTP off 1 hour","configTime offset overlap","Use configTzTime with POSIX string"],
         ["Swim on land","No env check","Force end swim if env != Lake"],
         ["Weather indoors","drawWeather on all scenes","Skip if env == Home"]],
        [35*mm,42*mm,88*mm], RED))
    s.append(PageBreak())

    # ===== 10 =====
    s.append(Paragraph('<font color="#FD8C20">10</font> PERFORMANCE & RESOURCES', ST['h1']))
    s.append(brown_line()); s.append(Spacer(1,3*mm))
    s.append(T(["Resource","Used","Total","Usage"],
        [["RAM","49 KB","328 KB","15%"],["Flash (firmware)","1,043 KB","3,146 KB","33%"],
         ["Flash (chip)","~1.1 MB","8 MB","14%"],["Sprite buffer","65 KB","RAM","20%"],
         ["NN weights","33 KB","PROGMEM","0% RAM"],["Frame rate","10 FPS","—","Stable"],
         ["NN inference","<0.1 ms","—","Negligible"]],
        [35*mm,30*mm,30*mm,30*mm]))

    s.append(Spacer(1,5*mm))
    cap = Table([[
        Paragraph(stat_card("85%", "RAM Free", GREEN), ST['body']),
        Paragraph(stat_card("86%", "Flash Free", BLUE_SKY), ST['body']),
        Paragraph(stat_card("10", "FPS", ORANGE), ST['body']),
        Paragraph(stat_card("<0.1", "ms AI", BROWN), ST['body']),
    ]], colWidths=[42.5*mm]*4)
    cap.setStyle(TableStyle([
        ('BACKGROUND',(0,0),(-1,-1),white),('BOX',(0,0),(-1,-1),1.5,GREEN),
        ('INNERGRID',(0,0),(-1,-1),0.5,BORDER),('ALIGN',(0,0),(-1,-1),'CENTER'),
        ('TOPPADDING',(0,0),(-1,-1),8),('BOTTOMPADDING',(0,0),(-1,-1),8),
    ]))
    s.append(cap)
    s.append(Spacer(1,8*mm))
    s.append(Paragraph('With 85% RAM and 86% flash free, substantial room remains for expansion: more games, TFLite models, web control, OTA updates, multiplayer, audio.', ST['body']))
    s.append(Spacer(1,10*mm))
    s.append(green_line())
    s.append(Spacer(1,3*mm))
    d = Drawing(170*mm, 10)
    for i in range(5): d.add(Circle(70+i*15, 5, 3, fillColor=[BROWN,ORANGE,GREEN,BLUE_SKY,RED][i], strokeColor=None))
    s.append(d)
    s.append(Spacer(1,3*mm))
    s.append(Paragraph(f'Generated with Claude Code · {datetime.now().strftime("%Y-%m-%d %H:%M")} · Ideaspark ESP32 + ST7789 · Tallinn, Estonia', ST['footer']))

    doc.build(s, canvasmaker=CapyCanvas)
    print("PDF generated!")

if __name__ == '__main__':
    build()

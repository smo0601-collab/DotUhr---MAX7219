// DotUhr – Hauptprogramm (Einstellungen: siehe Pinbelegung unten)
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ArduinoJson.h>
#include <time.h>
#include "webui.h"
#include "clockfonts.h"

// ---------------------------------------------------------------------------
//  Pinbelegung ESP32-C3 Super Mini  ->  MAX7219-Modul (Eingang, linke Seite)
// ---------------------------------------------------------------------------
#define PIN_CLK  4    // GPIO4  -> CLK
#define PIN_DIN  6    // GPIO6  -> DIN
#define PIN_CS   7    // GPIO7  -> CS
// VCC -> 5V-Pin,  GND -> GND

#define PIN_TOUCH   3   // TTP223 Touch-Sensor:  I/O -> GPIO3,  VCC -> 3V3,  GND -> GND
#define PIN_BUZZER 10   // Buzzer:  + (bzw. I/O) -> GPIO10,  - -> GND

#define FW_VERSION   "1.2.1"
#define AP_NAME      "DotUhr-Setup"    // Einrichtungs-WLAN (offen, ohne Passwort)

// ---------------------------------------------------------------------------
//  Konfiguration
// ---------------------------------------------------------------------------
struct Config {
  String ssid, pass;
  String host     = "dotuhr";
  String tz       = "CET-1CEST,M3.5.0,M10.5.0/3";   // Europa/Berlin
  uint8_t hw      = 0;      // 0=FC16 1=GENERIC 2=PAROLA 3=ICSTATION
  uint8_t modules = 4;      // Anzahl 8x8-Module
  bool lowTx      = true;   // reduzierte Sendeleistung (hilft vielen Super-Mini-Boards)

  uint8_t bright  = 3;      // 0..15
  bool nightEn    = true;
  uint8_t nightFrom = 22, nightTo = 7;
  uint8_t nightBright = 0;
  bool nightOff   = false;  // nachts Anzeige ganz aus

  bool showClock  = true;  uint16_t clockSec = 20;
  uint8_t clockStyle = 1;   // 0=HH:MM 1=HH:MM+Sekundenbalken 2=HH:MM:SS klein
  bool blink      = true;  bool h24 = true;
  uint8_t clockFont = 0;   // 0=Klassik 1=Rund

  // Wecker
  struct Alarm { bool en = false; uint8_t h = 7, m = 0; uint8_t days = 0x3E; } alarms[3];  // days: Bit0=So … Bit6=Sa
  uint8_t sound   = 0;      // 0=Piepen 1=Melodie 2=Klingel
  uint8_t buzType = 0;      // 0=passiv (Ton) 1=aktiv (an/aus)
  uint8_t snooze  = 9;      // Schlummerzeit [min]
  uint8_t ringMax = 10;     // max. Klingeldauer [min]
  // Touch
  bool touchEn = true, touchInv = false, touchBeep = true;
  bool touchToggle = false; // TTP223 im Selbsthalte-Modus (jede Berührung wechselt den Ausgang)

  bool showDate   = true;  uint16_t dateSec = 4;
  uint8_t dateStyle = 1;    // 0="12.11." 1="Do 12.11." 2=lang (Laufschrift)

  bool showWeather = true; uint16_t weatherSec = 6;
  bool weatherScroll = true;
  float lat = 52.52f, lon = 13.405f;
  String city = "Berlin";

  bool showText   = false;
  String text     = "Hallo von der DotUhr!";
  uint8_t speed   = 35;     // ms pro Scroll-Schritt (kleiner = schneller)
} cfg;

Preferences prefs;
WebServer   server(80);
DNSServer   dns;

MD_Parola*  P  = nullptr;
MD_MAX72XX* mx = nullptr;
int16_t     W  = 32;        // Breite in Pixeln

// ---------------------------------------------------------------------------
//  Laufzeit-Zustand
// ---------------------------------------------------------------------------
bool     apActive = false;
uint32_t apStopAt = 0;
bool     mdnsUp = false;
bool     timeValid = false;
bool     nightActive = false;
bool     displayOff = false;
bool     restartPending = false;
uint32_t restartAt = 0;

struct Weather {
  bool valid = false;
  float temp = 0, feels = 0, tmax = 0, tmin = 0, wind = 0;
  int hum = 0, code = 0;
  bool isDay = true;
  uint32_t fetchedAt = 0;
} wx;
uint32_t nextWeatherFetch = 0;
bool     weatherRequested = true;

enum Scene : int8_t { S_CLOCK = 0, S_DATE, S_WEATHER, S_WTEXT, S_TEXT, S_COUNT, S_MSG, S_INFO };
const char* sceneNames[] = {"Uhr", "Datum", "Wetter", "Wetter-Text", "Laufschrift"};
int8_t   scene = -1;          // aktuelle Szene im Ablauf
int8_t   active = -1;         // tatsächlich laufende Anzeige (inkl. MSG/INFO)
uint32_t sceneStart = 0;
bool     parolaRunning = false;
char     txtBuf[512];         // Puffer für Parola (Latin-1)

String   pendingMsg;  int pendingRepeat = 0;
String   infoMsg;     // z.B. Einrichtungs-Hinweis

bool     alarmRinging = false;
int8_t   ringIdx = -1;
uint32_t ringStart = 0, snoozeUntil = 0;
int      lastAlarmMin = -1;
uint32_t wakeUntil = 0;       // Anzeige trotz Nacht-Aus kurz eingeschaltet (Touch)
bool     manualOff = false;   // per Touch (lang drücken) ausgeschaltet

// ---------------------------------------------------------------------------
//  Hilfsfunktionen: Text / Zeichensatz
// ---------------------------------------------------------------------------
// UTF-8 (Browser) -> Latin-1 (MD_MAX72XX-Schriftart enthält ä ö ü ß ° usw.)
void utf8ToLatin1(const String& in, char* out, size_t outLen) {
  size_t o = 0;
  const uint8_t* s = (const uint8_t*)in.c_str();
  while (*s && o < outLen - 1) {
    uint8_t c = *s++;
    if (c < 0x80) { out[o++] = (char)c; continue; }
    if ((c & 0xE0) == 0xC0 && *s) {                     // 2-Byte-Sequenz
      uint16_t cp = ((c & 0x1F) << 6) | (*s++ & 0x3F);
      out[o++] = cp < 256 ? (char)cp : '?';
      continue;
    }
    // 3/4-Byte-Sequenzen: Sonderfälle, sonst überspringen
    uint32_t cp = 0; int extra = 0;
    if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    while (extra-- && *s) cp = (cp << 6) | (*s++ & 0x3F);
    if (cp == 0x20AC) { out[o++] = 'E'; }                // € -> E
    else if (cp == 0x2665 || cp == 0x2764) out[o++] = 3; // ♥ / ❤ -> Herz
    else if (cp == 0x2022) out[o++] = 7;                 // •
    else if (cp == 0x2013 || cp == 0x2014) out[o++] = '-';
    else if (cp == 0x201E || cp == 0x201C || cp == 0x201D) out[o++] = '"';
    else out[o++] = '?';
  }
  out[o] = 0;
}

int16_t textWidth(const char* s) {
  uint8_t buf[16]; int16_t w = 0;
  while (*s) { w += mx->getChar((uint8_t)*s++, sizeof(buf), buf) + 1; }
  return w > 0 ? w - 1 : 0;
}

// Pixel-Zeichnen mit x=0 links
inline void setCol(int16_t x, uint8_t v) { if (x >= 0 && x < W) mx->setColumn(W - 1 - x, v); }
inline void px(int16_t x, int16_t y, bool on = true) {
  if (x >= 0 && x < W && y >= 0 && y < 8) mx->setPoint(y, W - 1 - x, on);
}
int16_t drawChar(int16_t x, uint8_t c) {
  uint8_t buf[16];
  uint8_t w = mx->getChar(c, sizeof(buf), buf);
  for (uint8_t i = 0; i < w; i++) setCol(x + i, buf[i]);
  return w;
}
int16_t drawString(int16_t x, const char* s) {
  while (*s) x += drawChar(x, (uint8_t)*s++) + 1;
  return x;
}
// Zeichen zentriert in einer festen Zelle (Ziffern ohne "Springen")
void drawCharCell(int16_t x, uint8_t c, uint8_t cell = 5) {
  uint8_t buf[16];
  uint8_t w = mx->getChar(c, sizeof(buf), buf);
  int16_t off = (cell - w + 1) / 2;
  for (uint8_t i = 0; i < w; i++) setCol(x + off + i, buf[i]);
}

// Kleine 3x5-Ziffern für HH:MM:SS
const uint8_t DIGIT3x5[10][5] = {
  {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
  {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7}};
void drawSmallDigit(int16_t x, int16_t y, uint8_t d) {
  for (uint8_t r = 0; r < 5; r++)
    for (uint8_t c = 0; c < 3; c++)
      if (DIGIT3x5[d][r] & (4 >> c)) px(x + c, y + r);
}

// ---------------------------------------------------------------------------
//  Wettersymbole 8x8 ('#' = an)
// ---------------------------------------------------------------------------
const char* ICONS[][8] = {
  { "...#....", ".#...#..", "..###...", "#.###.#.", "..###...", ".#...#..", "...#....", "........" }, // 0 Sonne
  { "..###...", ".###....", "###.....", "###.....", "###.....", ".###....", "..###...", "........" }, // 1 Mond
  { "#.#.....", ".##.##..", "##.####.", "..######", ".#######", "########", ".######.", "........" }, // 2 heiter/wolkig
  { "........", "...##...", "..####..", ".######.", "########", "########", ".######.", "........" }, // 3 Wolke
  { "...##...", "..####..", ".######.", "########", "........", ".#..#..#", "#..#..#.", "........" }, // 4 Regen
  { "...#....", ".#.#.#..", "..###...", "###.###.", "..###...", ".#.#.#..", "...#....", "........" }, // 5 Schnee
  { "...##...", "..####..", ".######.", "########", "...##...", "..##....", "...##...", "..##...." }, // 6 Gewitter
  { "........", "#######.", "........", ".#######", "........", "#######.", "........", ".#######" }, // 7 Nebel
};
uint8_t iconFor(int code, bool day) {
  if (code <= 1) return day ? 0 : 1;
  if (code == 2) return day ? 2 : 3;
  if (code == 3) return 3;
  if (code == 45 || code == 48) return 7;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 5;
  if (code >= 95) return 6;
  return 4;   // Niesel, Regen, Schauer
}
void drawIcon(int16_t x, uint8_t idx) {
  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++)
      if (ICONS[idx][r][c] == '#') px(x + c, r);
}
const char* wmoText(int c) {
  switch (c) {
    case 0: return "klar";            case 1: return "überwiegend klar";
    case 2: return "teils bewölkt";   case 3: return "bedeckt";
    case 45: return "Nebel";          case 48: return "Reifnebel";
    case 51: case 53: case 55: return "Nieselregen";
    case 56: case 57: return "gefrierender Niesel";
    case 61: return "leichter Regen"; case 63: return "Regen";  case 65: return "starker Regen";
    case 66: case 67: return "Eisregen";
    case 71: return "leichter Schnee"; case 73: return "Schnee"; case 75: return "starker Schnee";
    case 77: return "Schneegriesel";
    case 80: case 81: case 82: return "Regenschauer";
    case 85: case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96: case 99: return "Gewitter mit Hagel";
  }
  return "?";
}

// ---------------------------------------------------------------------------
//  Konfiguration laden / speichern
// ---------------------------------------------------------------------------
void cfgToJson(JsonDocument& d, bool withSecrets) {
  d["ssid"] = cfg.ssid;  if (withSecrets) d["pass"] = cfg.pass;
  d["host"] = cfg.host;  d["tz"] = cfg.tz;  d["hw"] = cfg.hw;  d["modules"] = cfg.modules;
  d["lowTx"] = cfg.lowTx;
  d["bright"] = cfg.bright; d["nightEn"] = cfg.nightEn; d["nightFrom"] = cfg.nightFrom;
  d["nightTo"] = cfg.nightTo; d["nightBright"] = cfg.nightBright; d["nightOff"] = cfg.nightOff;
  d["showClock"] = cfg.showClock; d["clockSec"] = cfg.clockSec; d["clockStyle"] = cfg.clockStyle;
  d["blink"] = cfg.blink; d["h24"] = cfg.h24; d["clockFont"] = cfg.clockFont;
  JsonArray al = d["alarms"].to<JsonArray>();
  for (auto& a : cfg.alarms) { JsonObject o = al.add<JsonObject>(); o["en"] = a.en; o["h"] = a.h; o["m"] = a.m; o["days"] = a.days; }
  d["sound"] = cfg.sound; d["buzType"] = cfg.buzType; d["snooze"] = cfg.snooze; d["ringMax"] = cfg.ringMax;
  d["touchEn"] = cfg.touchEn; d["touchInv"] = cfg.touchInv; d["touchBeep"] = cfg.touchBeep; d["touchToggle"] = cfg.touchToggle;
  d["showDate"] = cfg.showDate; d["dateSec"] = cfg.dateSec; d["dateStyle"] = cfg.dateStyle;
  d["showWeather"] = cfg.showWeather; d["weatherSec"] = cfg.weatherSec; d["weatherScroll"] = cfg.weatherScroll;
  d["lat"] = cfg.lat; d["lon"] = cfg.lon; d["city"] = cfg.city;
  d["showText"] = cfg.showText; d["text"] = cfg.text; d["speed"] = cfg.speed;
}

template <typename T> void getIf(JsonVariantConst v, T& target) { if (!v.isNull()) target = v.as<T>(); }

void cfgFromJson(JsonVariantConst d) {
  getIf(d["ssid"], cfg.ssid);
  if (!d["pass"].isNull() && d["pass"].as<String>().length()) cfg.pass = d["pass"].as<String>();
  getIf(d["host"], cfg.host); getIf(d["tz"], cfg.tz); getIf(d["hw"], cfg.hw); getIf(d["modules"], cfg.modules);
  getIf(d["lowTx"], cfg.lowTx);
  getIf(d["bright"], cfg.bright); getIf(d["nightEn"], cfg.nightEn); getIf(d["nightFrom"], cfg.nightFrom);
  getIf(d["nightTo"], cfg.nightTo); getIf(d["nightBright"], cfg.nightBright); getIf(d["nightOff"], cfg.nightOff);
  getIf(d["showClock"], cfg.showClock); getIf(d["clockSec"], cfg.clockSec); getIf(d["clockStyle"], cfg.clockStyle);
  getIf(d["blink"], cfg.blink); getIf(d["h24"], cfg.h24); getIf(d["clockFont"], cfg.clockFont);
  if (d["alarms"].is<JsonArrayConst>()) {
    uint8_t i = 0;
    for (JsonVariantConst o : d["alarms"].as<JsonArrayConst>()) {
      if (i >= 3) break;
      auto& a = cfg.alarms[i++];
      getIf(o["en"], a.en); getIf(o["h"], a.h); getIf(o["m"], a.m); getIf(o["days"], a.days);
      a.h %= 24; a.m %= 60; a.days &= 0x7F;
    }
  }
  getIf(d["sound"], cfg.sound); getIf(d["buzType"], cfg.buzType); getIf(d["snooze"], cfg.snooze);
  getIf(d["ringMax"], cfg.ringMax);
  getIf(d["touchEn"], cfg.touchEn); getIf(d["touchInv"], cfg.touchInv); getIf(d["touchBeep"], cfg.touchBeep); getIf(d["touchToggle"], cfg.touchToggle);
  cfg.sound = min<uint8_t>(cfg.sound, 2); cfg.buzType = min<uint8_t>(cfg.buzType, 1);
  cfg.snooze = constrain(cfg.snooze, 1, 60); cfg.ringMax = constrain(cfg.ringMax, 1, 60);
  getIf(d["showDate"], cfg.showDate); getIf(d["dateSec"], cfg.dateSec); getIf(d["dateStyle"], cfg.dateStyle);
  getIf(d["showWeather"], cfg.showWeather); getIf(d["weatherSec"], cfg.weatherSec);
  getIf(d["weatherScroll"], cfg.weatherScroll);
  getIf(d["lat"], cfg.lat); getIf(d["lon"], cfg.lon); getIf(d["city"], cfg.city);
  getIf(d["showText"], cfg.showText); getIf(d["text"], cfg.text); getIf(d["speed"], cfg.speed);
  // Grenzen
  cfg.bright = min<uint8_t>(cfg.bright, 15); cfg.nightBright = min<uint8_t>(cfg.nightBright, 15);
  cfg.modules = constrain(cfg.modules, 1, 16); cfg.hw = min<uint8_t>(cfg.hw, 3);
  cfg.nightFrom %= 24; cfg.nightTo %= 24; cfg.clockStyle = min<uint8_t>(cfg.clockStyle, 2);
  cfg.clockFont = min<uint8_t>(cfg.clockFont, sizeof(CLOCK_FONTS) / sizeof(CLOCK_FONTS[0]));
  cfg.dateStyle = min<uint8_t>(cfg.dateStyle, 2);
  cfg.speed = constrain(cfg.speed, 10, 150);
  cfg.clockSec = max<uint16_t>(cfg.clockSec, 3); cfg.dateSec = max<uint16_t>(cfg.dateSec, 1);
  cfg.weatherSec = max<uint16_t>(cfg.weatherSec, 1);
  if (cfg.host.isEmpty()) cfg.host = "dotuhr";
}

void loadConfig() {
  prefs.begin("dotuhr", true);
  String s = prefs.getString("cfg", "");
  prefs.end();
  if (s.length()) {
    JsonDocument d;
    if (!deserializeJson(d, s)) cfgFromJson(d.as<JsonVariantConst>());
  }
}
void saveConfig() {
  JsonDocument d; cfgToJson(d, true);
  String s; serializeJson(d, s);
  prefs.begin("dotuhr", false);
  prefs.putString("cfg", s);
  prefs.end();
}

// ---------------------------------------------------------------------------
//  Anzeige: Text-Szenen über Parola
// ---------------------------------------------------------------------------
void scrollOut() {                         // alten Inhalt nach oben wegschieben
  for (uint8_t i = 0; i < 8; i++) { mx->transform(MD_MAX72XX::TSU); delay(18); }
}

// Text anzeigen: passt er auf die Anzeige -> statisch (mit Ein-/Ausblendung),
// sonst als Laufschrift
void startText(const String& utf8, uint16_t holdMs, bool forceScroll = false) {
  utf8ToLatin1(utf8, txtBuf, sizeof(txtBuf));
  P->displayClear();
  if (!forceScroll && textWidth(txtBuf) <= W) {
    P->displayText(txtBuf, PA_CENTER, cfg.speed, holdMs, PA_SCROLL_DOWN, PA_SCROLL_UP);
  } else {
    P->displayText(txtBuf, PA_LEFT, cfg.speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  }
  parolaRunning = true;
}

// ---------------------------------------------------------------------------
//  Anzeige: Uhr & Wetter (direkt gezeichnet)
// ---------------------------------------------------------------------------
void drawClock(bool invert = false) {
  struct tm t;
  bool ok = getLocalTime(&t, 0) && t.tm_year > 120;
  uint32_t ms = millis();
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx->clear();

  int h = ok ? t.tm_hour : 0, m = ok ? t.tm_min : 0, s = ok ? t.tm_sec : 0;
  if (!cfg.h24) { h %= 12; if (h == 0) h = 12; }
  bool colon = !cfg.blink || (ms % 1000) < 500 || !ok;

  if (cfg.clockStyle == 2) {                       // HH:MM:SS klein
    int16_t x = (W - 27) / 2;
    uint8_t d[6] = {(uint8_t)(h / 10), (uint8_t)(h % 10), (uint8_t)(m / 10), (uint8_t)(m % 10),
                    (uint8_t)(s / 10), (uint8_t)(s % 10)};
    for (int i = 0; i < 6; i++) {
      if (ok && !(i == 0 && !cfg.h24 && d[0] == 0)) drawSmallDigit(x, 1, d[i]);
      else if (!ok) { px(x, 3); px(x + 1, 3); px(x + 2, 3); }
      x += 4;
      if (i == 1 || i == 3) { if (colon) { px(x, 2); px(x, 4); } x += 2; }
    }
  } else {                                         // HH:MM groß
    // Schrift: 0 = Klassik (Bibliothek), sonst CLOCK_FONTS[clockFont-1]
    const ClockFont* f = cfg.clockFont ? &CLOCK_FONTS[cfg.clockFont - 1] : nullptr;
    uint8_t cw = f ? f->w : 5, fh = f ? f->h : 7;
    int16_t x = (W - (4 * cw + 5)) / 2;
    int16_t pos[4] = { x, (int16_t)(x + cw + 1), (int16_t)(x + 2 * cw + 4), (int16_t)(x + 3 * cw + 5) };
    int16_t colonX = x + 2 * cw + 2;
    uint8_t dig[4] = { (uint8_t)(h / 10), (uint8_t)(h % 10), (uint8_t)(m / 10), (uint8_t)(m % 10) };
    for (int i = 0; i < 4; i++) {
      if (i == 0 && !cfg.h24 && dig[0] == 0 && ok) continue;          // 12h: keine führende Null
      if (!ok) { for (uint8_t k = 0; k < cw; k++) px(pos[i] + k, 3); continue; }
      if (f) { for (uint8_t c = 0; c < cw; c++) setCol(pos[i] + c, f->col[dig[i]][c]); }
      else drawCharCell(pos[i], '0' + dig[i]);
    }
    if (colon) {
      if (fh >= 8) { px(colonX, 2); px(colonX, 5); }
      else { px(colonX, 1); px(colonX, 2); px(colonX, 4); px(colonX, 5); }
    }
    if (cfg.clockStyle == 1 && ok && fh <= 7) {    // Sekundenbalken in der untersten Zeile
      int16_t len = (int32_t)(s * 1000 + (ms % 1000)) * W / 60000;
      for (int16_t i = 0; i <= len && i < W; i++) px(i, 7);
    }
  }
  // Wecker-Anzeige: Punkt oben rechts (blinkt beim Schlummern)
  bool anyAlarm = false; for (auto& a : cfg.alarms) anyAlarm |= a.en;
  if (snoozeUntil ? (ms % 1000) < 500 : anyAlarm) px(W - 1, 0);
  if (invert) mx->transform(MD_MAX72XX::TINV);
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void drawWeather() {
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx->clear();
  char t[12];
  int temp = (int)lroundf(wx.temp);
  snprintf(t, sizeof(t), "%d\xB0" "C", temp);
  if (W < 16) {                                     // Mini-Anzeige: nur Temperatur
    snprintf(t, sizeof(t), "%d\xB0", temp);
    drawString((W - textWidth(t)) / 2, t);
  } else {
    drawIcon(0, iconFor(wx.code, wx.isDay));
    int16_t avail = W - 9;
    if (textWidth(t) > avail) snprintf(t, sizeof(t), "%d\xB0", temp);
    drawString(9 + (avail - textWidth(t)) / 2, t);
  }
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

// ---------------------------------------------------------------------------
//  Wetter abrufen (Open-Meteo)
// ---------------------------------------------------------------------------
bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();                 // kein Zertifikats-Pinning (öffentliche Wetterdaten)
  HTTPClient http;
  http.setTimeout(6000);
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(cfg.lat, 4) +
               "&longitude=" + String(cfg.lon, 4) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
               "wind_speed_10m,is_day&daily=temperature_2m_max,temperature_2m_min"
               "&timezone=auto&forecast_days=1";
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    JsonDocument filter;
    filter["current"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;
    JsonDocument d;
    if (!deserializeJson(d, http.getString(), DeserializationOption::Filter(filter))) {
      JsonObject c = d["current"];
      wx.temp  = c["temperature_2m"] | 0.0f;
      wx.feels = c["apparent_temperature"] | 0.0f;
      wx.hum   = c["relative_humidity_2m"] | 0;
      wx.code  = c["weather_code"] | 0;
      wx.wind  = c["wind_speed_10m"] | 0.0f;
      wx.isDay = (c["is_day"] | 1) == 1;
      wx.tmax  = d["daily"]["temperature_2m_max"][0] | 0.0f;
      wx.tmin  = d["daily"]["temperature_2m_min"][0] | 0.0f;
      wx.valid = true; wx.fetchedAt = millis();
      ok = true;
    }
  }
  http.end();
  Serial.printf("[Wetter] HTTP %d, ok=%d, %.1f°C code %d\n", code, ok, wx.temp, wx.code);
  return ok;
}

String weatherLine() {
  char b[200];
  snprintf(b, sizeof(b), "%s: %d°C, %s  -  gefühlt %d°  -  max %d° / min %d°  -  %d%%  -  Wind %d km/h",
           cfg.city.c_str(), (int)lroundf(wx.temp), wmoText(wx.code), (int)lroundf(wx.feels),
           (int)lroundf(wx.tmax), (int)lroundf(wx.tmin), wx.hum, (int)lroundf(wx.wind));
  return String(b);
}

String dateLine() {
  static const char* WD[]  = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  static const char* WDL[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  static const char* MON[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                              "August", "September", "Oktober", "November", "Dezember"};
  struct tm t; if (!getLocalTime(&t, 0)) return "";
  char b[64];
  if (cfg.dateStyle == 0)      snprintf(b, sizeof(b), "%02d.%02d.", t.tm_mday, t.tm_mon + 1);
  else if (cfg.dateStyle == 1) snprintf(b, sizeof(b), "%s %d.%d.", WD[t.tm_wday], t.tm_mday, t.tm_mon + 1);
  else snprintf(b, sizeof(b), "%s, %d. %s %d", WDL[t.tm_wday], t.tm_mday, MON[t.tm_mon], t.tm_year + 1900);
  return String(b);
}

// ---------------------------------------------------------------------------
//  Szenen-Ablauf
// ---------------------------------------------------------------------------
bool sceneEnabled(int8_t s) {
  switch (s) {
    case S_CLOCK:   return cfg.showClock;
    case S_DATE:    return cfg.showDate && timeValid;
    case S_WEATHER: return cfg.showWeather && wx.valid;
    case S_WTEXT:   return cfg.showWeather && cfg.weatherScroll && wx.valid;
    case S_TEXT:    return cfg.showText && cfg.text.length() > 0;
  }
  return false;
}

void startScene(int8_t s) {
  active = s; sceneStart = millis(); parolaRunning = false;
  switch (s) {
    case S_CLOCK:   drawClock(); break;
    case S_WEATHER: drawWeather(); break;
    case S_DATE:    startText(dateLine(), cfg.dateSec * 1000); break;
    case S_WTEXT:   startText(weatherLine(), 0, true); break;
    case S_TEXT:    startText(cfg.text, 3000); break;
    case S_INFO:    startText(infoMsg, 0, true); break;
    case S_MSG:     startText(pendingMsg, 3000); break;
  }
}

void nextScene() {
  // Wetter zwischen den Szenen holen (dann ruckelt keine Laufschrift)
  if (cfg.showWeather && WiFi.status() == WL_CONNECTED &&
      (weatherRequested || (int32_t)(millis() - nextWeatherFetch) >= 0)) {
    weatherRequested = false;
    nextWeatherFetch = millis() + (fetchWeather() ? 15UL * 60 * 1000 : 60UL * 1000);
  }
  if (active == S_CLOCK || active == S_WEATHER) scrollOut();

  if (pendingRepeat > 0) { pendingRepeat--; startScene(S_MSG); return; }
  if (infoMsg.length() && active != S_INFO) { startScene(S_INFO); return; }

  for (int i = 0; i < S_COUNT; i++) {
    scene = (scene + 1) % S_COUNT;
    if (sceneEnabled(scene)) { startScene(scene); return; }
  }
  scene = S_CLOCK; startScene(S_CLOCK);    // nichts aktiv -> Uhr
}

void updateDisplay() {
  static uint32_t lastDraw = 0;
  uint32_t now = millis();
  if (alarmRinging) {                       // Wecker: Uhr blinkt invertiert
    if (now - lastDraw >= 50) { lastDraw = now; drawClock((now / 500) % 2); }
    return;
  }
  if (displayOff) return;

  if (parolaRunning) {
    if (P->displayAnimate()) { parolaRunning = false; nextScene(); }
    return;
  }
  // direkt gezeichnete Szenen
  uint32_t dur = (active == S_WEATHER ? cfg.weatherSec : cfg.clockSec) * 1000UL;
  bool onlyClock = !sceneEnabled(S_DATE) && !sceneEnabled(S_WEATHER) && !sceneEnabled(S_WTEXT) &&
                   !sceneEnabled(S_TEXT) && !pendingRepeat && infoMsg.isEmpty() &&
                   !(cfg.showWeather && weatherRequested);
  if (active == S_CLOCK && onlyClock) dur = UINT32_MAX;      // Dauer-Uhr
  if (now - sceneStart >= dur || active < 0) { nextScene(); return; }
  if (active == S_CLOCK && now - lastDraw >= 50) { lastDraw = now; drawClock(); }
}

// Nachricht sofort anzeigen (unterbricht die aktuelle Szene)
void showMessage(const String& msg, int repeat) {
  pendingMsg = msg; pendingRepeat = constrain(repeat, 1, 20);
  if (displayOff) return;
  P->displayReset(); parolaRunning = false;
  pendingRepeat--; startScene(S_MSG);
}

// ---------------------------------------------------------------------------
//  Helligkeit / Nachtmodus
// ---------------------------------------------------------------------------
void applyBrightness(bool force = false) {
  static int lastLevel = -1;
  struct tm t;
  bool night = false;
  if (cfg.nightEn && getLocalTime(&t, 0) && t.tm_year > 120) {
    int h = t.tm_hour;
    night = cfg.nightFrom > cfg.nightTo ? (h >= cfg.nightFrom || h < cfg.nightTo)
                                        : (h >= cfg.nightFrom && h < cfg.nightTo);
  }
  bool woke = (int32_t)(millis() - wakeUntil) < 0;
  bool off = !alarmRinging && (manualOff || (night && cfg.nightOff && !apActive && !woke));
  uint8_t level = (night && !alarmRinging) ? cfg.nightBright : cfg.bright;
  nightActive = night;
  if (force || level != lastLevel) { P->setIntensity(level); lastLevel = level; }
  if (off != displayOff) {
    displayOff = off;
    P->displayShutdown(off);
    if (!off) active = -1;
  }
}

// ---------------------------------------------------------------------------
//  Buzzer, Wecker, Touch
// ---------------------------------------------------------------------------
struct Note { uint16_t f, ms; };
const Note SND_BEEP[] = { {2700,100},{0,100},{2700,100},{0,100},{2700,100},{0,100},{2700,100},{0,700} };
const Note SND_MELODY[] = { {1047,160},{1319,160},{1568,160},{2093,320},{0,160},{1568,160},{2093,450},{0,800} };
const Note SND_RING[] = { {2000,40},{2600,40},{2000,40},{2600,40},{2000,40},{2600,40},{2000,40},{2600,40},
                          {2000,40},{2600,40},{2000,40},{2600,40},{0,700} };
const Note* const SOUNDS[] = { SND_BEEP, SND_MELODY, SND_RING };
const uint8_t SOUND_LEN[] = { sizeof(SND_BEEP) / sizeof(Note), sizeof(SND_MELODY) / sizeof(Note),
                              sizeof(SND_RING) / sizeof(Note) };
uint8_t  soundPos = 0;
uint32_t soundNext = 0;

void buzz(uint16_t f) {
  if (cfg.buzType == 1) digitalWrite(PIN_BUZZER, f ? HIGH : LOW);   // aktiver Buzzer
  else if (f) tone(PIN_BUZZER, f);                                  // passiver Buzzer / Piezo
  else { noTone(PIN_BUZZER); digitalWrite(PIN_BUZZER, LOW); }
}
void click() { if (!cfg.touchBeep) return; buzz(3200); delay(25); buzz(0); }

void playSound() {
  if ((int32_t)(millis() - soundNext) < 0) return;
  const Note& n = SOUNDS[cfg.sound][soundPos];
  buzz(n.f);
  soundNext = millis() + n.ms;
  soundPos = (soundPos + 1) % SOUND_LEN[cfg.sound];
}

void startAlarm(int8_t idx) {
  alarmRinging = true; ringIdx = idx; ringStart = millis(); snoozeUntil = 0;
  soundPos = 0; soundNext = 0;
  P->displayReset(); parolaRunning = false; active = S_CLOCK; sceneStart = millis();
  applyBrightness(true);
  Serial.printf("[Wecker] klingelt (%d)\n", idx + 1);
}

void stopAlarm(bool snooze) {
  bool was = alarmRinging || snoozeUntil;
  alarmRinging = false; buzz(0);
  snoozeUntil = snooze ? millis() + cfg.snooze * 60000UL : 0;
  if (snooze && snoozeUntil == 0) snoozeUntil = 1;
  applyBrightness(true);
  active = -1;
  if (was) showMessage(snooze ? "Schlummern " + String(cfg.snooze) + " min" : "Wecker aus", 1);
}

void handleAlarms() {
  if (alarmRinging) {
    playSound();
    if (millis() - ringStart > cfg.ringMax * 60000UL) stopAlarm(false);
    return;
  }
  if (snoozeUntil && (int32_t)(millis() - snoozeUntil) >= 0) { startAlarm(ringIdx); return; }
  struct tm t;
  if (!getLocalTime(&t, 0) || t.tm_year <= 120) return;
  int cur = t.tm_hour * 60 + t.tm_min;
  if (cur == lastAlarmMin) return;
  lastAlarmMin = cur;
  for (int8_t i = 0; i < 3; i++) {
    auto& a = cfg.alarms[i];
    if (!a.en || a.h * 60 + a.m != cur) continue;
    if (a.days && !(a.days & (1 << t.tm_wday))) continue;
    if (!a.days) { a.en = false; saveConfig(); }     // einmaliger Wecker
    startAlarm(i);
    break;
  }
}

// Touch: kurz tippen / lange halten (1,5 s)
void onTap() {
  if (alarmRinging) { stopAlarm(true); return; }          // Schlummern
  click();
  if (displayOff) {                                        // Anzeige kurz aufwecken
    manualOff = false; wakeUntil = millis() + 15000; applyBrightness(true); return;
  }
  P->displayReset(); parolaRunning = false;                // nächste Anzeige
  nextScene();
}
void onLongPress() {
  if (alarmRinging || snoozeUntil) { stopAlarm(false); return; }   // Wecker ganz aus
  click(); delay(80); click();
  manualOff = !manualOff;                                   // Anzeige an/aus
  if (!manualOff) wakeUntil = 0;
  applyBrightness(true);
}
void handleTouch() {
  if (!cfg.touchEn) return;
  static bool state = false, longDone = false, init = false, pendingTap = false;
  static uint32_t pressStart = 0, lastChange = 0, tapTime = 0;
  bool raw = digitalRead(PIN_TOUCH) ^ cfg.touchInv;
  uint32_t now = millis();
  if (!init) { state = raw; init = true; }

  if (cfg.touchToggle) {
    // Selbsthalte-Modus: jede Pegeländerung = eine Berührung.
    // Einmal tippen = kurz, zweimal schnell tippen = "lang" (Anzeige an/aus bzw. Wecker aus)
    if (raw != state && now - lastChange > 40) {
      lastChange = now; state = raw;
      if (pendingTap && now - tapTime < 600) { pendingTap = false; onLongPress(); }
      else { pendingTap = true; tapTime = now; }
    }
    if (pendingTap && now - tapTime >= 600) { pendingTap = false; onTap(); }
    return;
  }

  // Punkt-Modus (Standard): Ausgang HIGH solange berührt
  if (raw != state && now - lastChange > 30) {
    lastChange = now; state = raw;
    if (state) { pressStart = now; longDone = false; }
    else if (!longDone && now - pressStart < 1000) onTap();
  }
  if (state && !longDone && now - pressStart >= 1500) { longDone = true; onLongPress(); }
}

// ---------------------------------------------------------------------------
//  WLAN
// ---------------------------------------------------------------------------
void setTx() { if (cfg.lowTx) WiFi.setTxPower(WIFI_POWER_8_5dBm); }

void startAP() {
  if (apActive) return;
  WiFi.mode(cfg.ssid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_NAME);                         // offenes WLAN, kein Passwort
  setTx();
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  apActive = true; apStopAt = 0;
  infoMsg = "WLAN \"" AP_NAME "\" verbinden  ->  http://" + WiFi.softAPIP().toString();
  Serial.println("[WLAN] Access-Point aktiv: " AP_NAME " (offen)");
}
void stopAP() {
  dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); setTx();
  apActive = false; infoMsg = "";
  Serial.println("[WLAN] Access-Point beendet");
}

void onConnected() {
  Serial.println("[WLAN] verbunden, IP " + WiFi.localIP().toString());
  configTzTime(cfg.tz.c_str(), "pool.ntp.org", "time.cloudflare.com", "ptbtime1.ptb.de");
  if (!mdnsUp && MDNS.begin(cfg.host.c_str())) { MDNS.addService("http", "tcp", 80); mdnsUp = true; }
  weatherRequested = true;
  if (apActive) apStopAt = millis() + 120000UL;     // AP nach 2 min abschalten
  showMessage("IP " + WiFi.localIP().toString() + "  -  http://" + cfg.host + ".local", 1);
}

void connectWiFi() {
  if (cfg.ssid.isEmpty()) { startAP(); return; }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.host.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  setTx();
  // kleine Warte-Animation, max. 20 s
  uint32_t t0 = millis(); uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    mx->clear();
    drawString(0, "WLAN");
    for (uint8_t k = 0; k < 3; k++) if (k <= i % 4) px(W - 6 + k * 2, 6);
    i++; delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) onConnected();
  else startAP();
}

void handleWiFi() {
  static bool wasConnected = false;
  bool c = WiFi.status() == WL_CONNECTED;
  if (c && !wasConnected) {
    // erste Verbindung wird in connectWiFi gemeldet; spätere Reconnects hier
    static bool first = true;
    if (!first || apActive) onConnected();
    first = false;
  }
  wasConnected = c;
  if (apActive) {
    dns.processNextRequest();
    if (apStopAt && (int32_t)(millis() - apStopAt) >= 0 && c && WiFi.softAPgetStationNum() == 0) stopAP();
  }
}

// ---------------------------------------------------------------------------
//  Webserver / API
// ---------------------------------------------------------------------------
void sendJson(JsonDocument& d, int code = 200) {
  String s; serializeJson(d, s);
  server.send(code, "application/json", s);
}

void apiStatus() {
  JsonDocument d;
  struct tm t; char b[32] = "--";
  if (getLocalTime(&t, 0) && t.tm_year > 120) strftime(b, sizeof(b), "%d.%m.%Y %H:%M:%S", &t);
  d["time"] = b; d["timeValid"] = timeValid;
  d["version"] = FW_VERSION;
  d["ip"] = WiFi.localIP().toString();
  d["ssid"] = WiFi.SSID(); d["rssi"] = WiFi.RSSI();
  d["connected"] = WiFi.status() == WL_CONNECTED;
  d["ap"] = apActive; d["uptime"] = millis() / 1000; d["heap"] = ESP.getFreeHeap();
  d["night"] = nightActive; d["off"] = displayOff;
  d["ringing"] = alarmRinging; d["snoozing"] = snoozeUntil != 0;
  d["touch"] = cfg.touchEn ? (bool)(digitalRead(PIN_TOUCH) ^ cfg.touchInv) : false;
  d["scene"] = active >= 0 && active < S_COUNT ? sceneNames[active] : (active == S_MSG ? "Nachricht" : "Info");
  if (wx.valid) {
    JsonObject w = d["weather"].to<JsonObject>();
    w["temp"] = wx.temp; w["feels"] = wx.feels; w["hum"] = wx.hum; w["wind"] = wx.wind;
    w["tmax"] = wx.tmax; w["tmin"] = wx.tmin; w["code"] = wx.code; w["text"] = wmoText(wx.code);
    w["age"] = (millis() - wx.fetchedAt) / 1000;
  }
  sendJson(d);
}

void apiFrame() {          // aktuelle Anzeige als Hex-String (Spalte links -> rechts)
  String s; s.reserve(W * 2);
  static const char hx[] = "0123456789abcdef";
  for (int16_t x = 0; x < W; x++) {
    uint8_t v = displayOff ? 0 : mx->getColumn(W - 1 - x);
    s += hx[v >> 4]; s += hx[v & 15];
  }
  server.send(200, "text/plain", s);
}

void apiGetConfig() { JsonDocument d; cfgToJson(d, false); d["hasPass"] = cfg.pass.length() > 0; sendJson(d); }

void apiSetConfig() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "JSON-Fehler"); return; }
  String oSsid = cfg.ssid, oPass = cfg.pass, oHost = cfg.host;
  uint8_t oHw = cfg.hw, oMod = cfg.modules; bool oTx = cfg.lowTx;
  float oLat = cfg.lat, oLon = cfg.lon; String oTz = cfg.tz;
  cfgFromJson(d.as<JsonVariantConst>());
  saveConfig();
  bool restart = oSsid != cfg.ssid || oPass != cfg.pass || oHost != cfg.host || oHw != cfg.hw ||
                 oMod != cfg.modules || oTx != cfg.lowTx;
  if (oTz != cfg.tz) { setenv("TZ", cfg.tz.c_str(), 1); tzset(); }
  if (oLat != cfg.lat || oLon != cfg.lon) { wx.valid = false; weatherRequested = true; }
  buzz(0);
  applyBrightness(true);
  P->setSpeed(cfg.speed);
  JsonDocument r; r["ok"] = true; r["restart"] = restart;
  sendJson(r);
  if (restart) { restartPending = true; restartAt = millis() + 1500; }
  else { P->displayReset(); parolaRunning = false; active = -1; }   // Ablauf neu starten
}

// Helligkeit live: /api/brightness?v=0..15[&night=1][&save=1]
void apiBrightness() {
  uint8_t v = constrain(server.arg("v").toInt(), 0, 15);
  bool night = server.arg("night") == "1";
  if (server.hasArg("save")) {
    if (night) cfg.nightBright = v; else cfg.bright = v;
    saveConfig();
    applyBrightness(true);                       // passende Stufe (Tag/Nacht) wieder herstellen
  } else {
    if (displayOff) { displayOff = false; P->displayShutdown(false); active = -1; }
    P->setIntensity(v);                          // Vorschau während des Schiebens
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void apiAlarm() {   // /api/alarm?action=test|stop|snooze
  String a = server.arg("action");
  if (a == "test") startAlarm(-1);
  else if (a == "snooze") stopAlarm(true);
  else stopAlarm(false);
  server.send(200, "application/json", "{\"ok\":true}");
}

void apiMessage() {
  String t = server.arg("text");
  if (t.isEmpty()) { server.send(400, "text/plain", "Parameter 'text' fehlt"); return; }
  int rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 1;
  showMessage(t, rep);
  server.send(200, "application/json", "{\"ok\":true}");
}

void apiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i); if (s.isEmpty()) continue;
    bool dup = false; for (JsonVariant v : a) if (v["ssid"] == s) { dup = true; break; }
    if (dup) continue;
    JsonObject o = a.add<JsonObject>(); o["ssid"] = s; o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(d);
}

void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/frame", HTTP_GET, apiFrame);
  server.on("/api/config", HTTP_GET, apiGetConfig);
  server.on("/api/config", HTTP_POST, apiSetConfig);
  server.on("/api/message", HTTP_ANY, apiMessage);
  server.on("/api/brightness", HTTP_ANY, apiBrightness);
  server.on("/api/alarm", HTTP_ANY, apiAlarm);
  server.on("/api/scan", HTTP_GET, apiScan);
  server.on("/api/weather/refresh", HTTP_POST, []() { weatherRequested = true; server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/restart", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":true}"); restartPending = true; restartAt = millis() + 800; });
  server.on("/api/reset", HTTP_POST, []() {
    prefs.begin("dotuhr", false); prefs.clear(); prefs.end();
    server.send(200, "application/json", "{\"ok\":true}"); restartPending = true; restartAt = millis() + 800; });

  // Firmware-Update (.bin aus "Sketch > Kompilierte Binärdatei exportieren")
  server.on("/update", HTTP_POST,
    []() {
      bool ok = !Update.hasError();
      server.sendHeader("Connection", "close");
      server.send(ok ? 200 : 500, "text/plain", ok ? "OK - Neustart..." : "Update fehlgeschlagen");
      if (ok) { restartPending = true; restartAt = millis() + 800; }
    },
    []() {
      HTTPUpload& up = server.upload();
      if (up.status == UPLOAD_FILE_START) {
        P->displayReset(); parolaRunning = false; displayOff = false;
        mx->clear(); drawString(0, "UPD..."); active = S_MSG;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
        int16_t p = (int32_t)Update.progress() * W / max<size_t>(Update.size(), 1);
        for (int16_t i = 0; i < p; i++) px(i, 7);
      } else if (up.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
      }
    });

  // Captive Portal: unbekannte Adressen auf die Startseite umleiten
  server.onNotFound([]() {
    if (apActive) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    } else server.send(404, "text/plain", "Nicht gefunden");
  });
  server.begin();
}

// ---------------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nDotUhr " FW_VERSION);
  loadConfig();
  pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_TOUCH, INPUT);

  static const MD_MAX72XX::moduleType_t HW[] = {
    MD_MAX72XX::FC16_HW, MD_MAX72XX::GENERIC_HW, MD_MAX72XX::PAROLA_HW, MD_MAX72XX::ICSTATION_HW};
  SPI.begin(PIN_CLK, -1, PIN_DIN, PIN_CS);      // eigene Pins; MD_MAX72XX nutzt dann diesen Bus
  P = new MD_Parola(HW[cfg.hw], PIN_CS, cfg.modules);
  P->begin();
  mx = P->getGraphicObject();
  W  = cfg.modules * 8;
  P->setIntensity(cfg.bright);
  P->setSpeed(cfg.speed);
  P->setCharSpacing(1);
  P->displayClear();

  // WLAN-Stack MUSS vor dem Webserver laufen (sonst Absturz "Invalid mbox")
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(50);
  connectWiFi();
  setupWeb();
  Serial.println(apActive ? "[Setup] Einrichtungs-WLAN 'DotUhr-Setup' aktiv"
                          : "[Setup] Webserver laeuft");
  setenv("TZ", cfg.tz.c_str(), 1); tzset();
  applyBrightness(true);
}

void loop() {
  server.handleClient();
  handleWiFi();

  static uint32_t lastSec = 0;
  if (millis() - lastSec >= 1000) {
    lastSec = millis();
    struct tm t;
    timeValid = getLocalTime(&t, 0) && t.tm_year > 120;
    applyBrightness();
  }

  handleTouch();
  handleAlarms();
  updateDisplay();

  if (restartPending && (int32_t)(millis() - restartAt) >= 0) {
    mx->clear(); ESP.restart();
  }
}

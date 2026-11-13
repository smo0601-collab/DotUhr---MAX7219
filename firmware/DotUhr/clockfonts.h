#pragma once
#include <stdint.h>
// Automatisch erzeugte Ziffern-Schriften für die Uhr (Spalten, Bit0 = oberste Zeile)
struct ClockFont { uint8_t w, h; uint8_t col[10][6]; };
const ClockFont CLOCK_FONTS[] = {
  { 5, 7, { {62,65,65,65,62,0}, {0,66,127,64,0,0}, {66,97,81,73,70,0}, {33,65,69,75,49,0}, {24,20,18,127,16,0}, {39,69,69,69,57,0}, {60,74,73,73,48,0}, {1,113,9,5,3,0}, {54,73,73,73,54,0}, {6,73,73,41,30,0} } },   // RUND
};

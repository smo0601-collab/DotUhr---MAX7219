/*
  ============================================================================
   DotUhr – Retro-Punktmatrix-Uhr
   ESP32-C3 Super Mini  +  MAX7219 8x8 Matrix-Module (kaskadierbar)
  ----------------------------------------------------------------------------
   Funktionen:
     - NTP-Uhr (Sommer-/Winterzeit automatisch), 3 Uhr-Stile
     - Datum (kurz / mit Wochentag / lang als Laufschrift)
     - Wetter von Open-Meteo (kein API-Key nötig): Symbol + Temperatur,
       optional ausführliche Wetter-Laufschrift
     - Eigene Laufschrift + Sofort-Nachrichten über Web/HTTP-API
     - Web-Oberfläche (http://dotuhr.local) mit Live-Vorschau der Anzeige
     - WLAN-Einrichtung über eigenen Access-Point (Captive Portal)
     - Helligkeit + Nachtmodus (dimmen oder aus)
     - Firmware-Update über die Web-Oberfläche (OTA)

   Arduino IDE:
     Board:            "ESP32C3 Dev Module"   (esp32 Core 2.0.x oder 3.x)
     USB CDC On Boot:  "Enabled"   (sonst keine serielle Ausgabe)
     Flash Size:       4MB
     Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA)"
   Bibliotheken (Bibliotheksverwalter):
     MD_Parola, MD_MAX72XX (beide von majicDesigns), ArduinoJson (v7)
  ============================================================================
*/

// Der Programmcode steht in DotUhr_main.cpp (gleicher Ordner).
// Die Arduino IDE kompiliert ihn automatisch mit. Pins dort ganz oben anpassen.

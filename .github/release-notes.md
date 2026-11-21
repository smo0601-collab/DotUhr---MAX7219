## DotUhr – Retro-Punktmatrix-Uhr (ESP32-C3 Super Mini + MAX7219)

### Firmware
| Datei | Verwendung |
|---|---|
| `DotUhr_komplett_0x0.bin` | **Erstinstallation** mit dem Web-Flasher https://espressif.github.io/esptool-js/ an Adresse **0x0** |
| `DotUhr_update_OTA.bin` | **Update** über die Web-Oberfläche: *System → Firmware-Update* (Einstellungen bleiben erhalten) |

### Gehäuse
`dotuhr_body.stl`, `dotuhr_cover.stl`, `dotuhr_diffuser.stl` (Streuscheibe) und die parametrische Quelle `dotuhr_gehaeuse.scad`.

### Funktionen
- Uhr (Klassik / Rund), Sekundenbalken, 12/24 h, Datum, Wetter (Open-Meteo), Laufschrift, Sofort-Nachrichten
- Wecker: 3 Weckzeiten mit Wochentagen, 3 Töne, Schlummern, passiver/aktiver Buzzer
- TTP223 Touch-Taste (Punkt- und Selbsthalte-Modus)
- Uhrzeit & Datum auch ohne Internet stellbar (automatisch vom Browser oder von Hand)
- Login für die Web-Oberfläche (Standard: admin, Passwort leer; Reset: Touch 10 s halten)
- Web-Oberfläche mit Reitern und Live-Vorschau, Helligkeit live, Nachtmodus, OTA-Update
- Offenes Einrichtungs-WLAN **DotUhr-Setup** → http://192.168.4.1

Verkabelung und Anleitung: siehe [README](../../blob/main/README.md).

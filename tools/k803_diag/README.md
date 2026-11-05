# K803-Diagnose: Sprung von „single“ auf „manual input“

Arbeitsstand der Fehlersuche, damit sie auf einem anderen Rechner weitergehen kann.
Nur für die Diagnose gedacht, nicht Teil der Rover-Firmware.

## Problem

SW Maps zeigt erst den Fix-Typ „single“ und springt nach einiger Zeit auf „manual input“.
Das entspricht GGA-Qualität 7 („Manual input mode (fixed position)“, Referenzhandbuch V1.8).

## Stand

- Board: XIAO ESP32-C6, Umgebung `xiao_esp32c6_ble`, Empfänger: K803, Firmware 7.4.2.
- **Auf dem Board ist die Diagnose-Firmware `passthrough/`.** Kein Bluetooth, also keine
  Verbindung mit SW Maps, bis die normale Firmware wieder geflasht ist (siehe unten).
- `GNSS_BOOT_DELAY_MS` (10 s) ist eingebaut. Laut Rückmeldung ändert sie nichts am Verhalten.

## Belegt

- Mit 10 s Verzögerung bleibt der Sprung auf „manual input“ bestehen.
- Der K803 wertet Befehle wirklich aus: Unsinn wird mit `Error! Invalid key word!` abgelehnt.
  `REFAUTOSETUP OFF`, `FIX NONE`, `FIX AUTO`, `FIX POSITION` und `SAVECONFIG` werden angenommen.
- `LOG REFMODE` meldet immer dieselbe Zeile `Base Mode: Auto ... RefID:398, MaxRefOffset:50`,
  auch direkt nach `FIX POSITION`, `FIX NONE` und `REFAUTOSETUP ON/OFF`.
  **Die Zeile taugt deshalb nicht als Nachweis für den Basis-Modus.**

## Noch offen

- Bisher nur Tests ohne Himmelssicht (0 bis 1 Satelliten, `COLD_START`, `NONE`).
  Der Sprung auf Qualität 7 wurde noch nie live mitgelesen.
- Dass der Selbststart als Basis die Ursache ist, ist nur eine Hypothese. Sie passt zum Ablauf,
  ist aber nicht bewiesen.
- `FIX POSITION` änderte ohne Satelliten weder `BESTPOSA` noch die GGA-Qualität. Ob es
  wirkt, zeigt sich erst mit Empfang.

## Nächster Test (Antenne mit freier Himmelssicht)

K803 und ESP32 einmal stromlos machen, damit nur gespeicherte Einstellungen aktiv sind.
Danach Port suchen und mitlesen. `COM3` ggf. anpassen, Python liegt in der PlatformIO-Umgebung:

```powershell
pio device list
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -I tools\k803_diag\k803.py COM3 "$env:TEMP\k803.log" "LOG BESTPOSA" "LOG LOGLIST" "wait:1200" "LOG BESTPOSA"
```

Das Skript gibt jede Änderung von GGA-Qualität oder Satellitenzahl aus. Abbruch mit Strg+C.
Kein Befehl wird gespeichert, solange `SAVECONFIG` nicht mitgeschickt wird.

**Logs nicht committen und nicht ins Repo legen: Mit Empfang enthalten sie die echte Position.**

Auswertung:

| Beobachtung | Bedeutung | Nächster Schritt |
|---|---|---|
| Sprung auf 7, `BESTPOSA` meldet `FIXEDPOS` o. Ä. | Empfänger legt seine Position selbst fest | Im Zustand 7 `FIX NONE`, dann `REFAUTOSETUP OFF` senden und prüfen, welcher Befehl die Qualität zurückholt. Danach mit `SAVECONFIG` + Stromlos-Test die Persistenz prüfen |
| Bleibt ohne Handy dauerhaft bei 1 | Auslöser liegt beim Betrieb mit Firmware oder Handy | Normale Firmware flashen, SW Maps testen. Dabei prüfen: mit oder ohne NTRIP? |

`FRESET` nur als letztes Mittel: Es löscht den nichtflüchtigen Speicher. Die Baudrate danach ist
widersprüchlich dokumentiert (Handbuch V1.8: 38400, K-Series-Guide: 115200), und die Firmware
erwartet 115200.

## Hinweise

- Der Kommentar in `src/gnss_init.cpp`, `INTERFACEMODE SAVECONFIG` sei ungültig, ist vermutlich
  falsch: Der K-Series-Guide nennt genau diesen Befehl für den Port, über den konfiguriert wird.
- `COMMANDS.md` nennt noch rund 13 s Boot-Sequenz. Mit Verzögerung sind es rund 24 s.
- Die `.gitignore` wurde lokal vermutlich von PlatformIO umgeschrieben und ist nicht committet.
- Handbücher: [Reference Manual V1.8](https://comnavtech.com/uploads/soft/20240530/13318ef4a1b9f626ada0ef3df759ec83.pdf),
  [K-Series Guide](https://comnavtech.com/uploads/soft/20240530/5af2a87fd8dac4ecff61ee14dc919470.pdf)

## Firmware flashen

```powershell
# Diagnose-Firmware (USB <-> K803)
pio run -d tools\k803_diag\passthrough -t upload

# Normale Rover-Firmware (Bluetooth, für SW Maps)
pio run -e xiao_esp32c6_ble -t upload
```

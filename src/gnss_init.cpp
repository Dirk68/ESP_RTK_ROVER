// ComNav K803 Lite - configure as an RTK rover over the same UART the bridge
// then uses. Port of the PC-side Python setup script, so the receiver no longer
// needs a laptop before a survey.

#include <Arduino.h>
#include <ctype.h>
#include <string.h>

#include "config.h"
#include "gnss_init.h"

#if GNSS_INIT_K803

#if DEBUG_USB
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

// Order matters. INTERFACEMODE ... AUTO AUTO ON is the point of no return: it
// puts the port into RTCM auto-detect, after which it no longer parses ASCII,
// so every LOG and every setting has to be in place before it.
static const char *const kCommands[] = {
    "UNLOGALL COM1",
    "UNLOGALL COM2",
    "UNLOGALL COM3",

    // Leave base mode. A K803 taken from a reference station has REFAUTOSETUP
    // ON and/or a FIX POSITION in flash; either makes it report GGA quality 7
    // ("manual input / fixed position") instead of computing a rover fix.
    "REFAUTOSETUP OFF",  // stop the base from self-starting
    "FIX NONE",          // clear a leftover base coordinate (NovAtel syntax,
                         // not in the ComNav manual - check the reply)
    "RTKOBSMODE 0",      // all frequencies
    "RTKREFMODE 0",      // corrections come from a FIXED base (NTRIP etc.)
    "RTKSOLUTION 0",     // auto -> aim for RTK fixed
    "RTKDYNAMICS FOOT",  // static / foot / land / air
    "UNLOCKOUTALL",

    "LOG " GNSS_CMD_PORT " GPGGA ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGST ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGSA ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGSV ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPRMC ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPVTG ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPZDA ONTIME 1",

    // Verify BEFORE the port goes into diff mode.
    "LOG REFMODE",   // base mode: auto-started / fixed position / none
    "LOG BESTPOSA",  // pos type must read SINGLE, not FIXEDPOS
    "LOG COMCONFIG",

    // Persist the rover settings while the port still parses ASCII. Sent after
    // AUTO it would be ignored, and the base config in flash would come back on
    // the next power-up. (The old "INTERFACEMODE SAVECONFIG" was not a valid
    // command either.)
    "SAVECONFIG",

    // LAST - after this the port no longer takes commands
    "INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON",
};

static char resp[512];  // a VERSION reply is a few hundred bytes

static bool containsNoCase(const char *hay, const char *needle) {
  for (; *hay; ++hay) {
    const char *h = hay;
    const char *n = needle;
    while (*n && toupper((unsigned char)*h) == toupper((unsigned char)*n)) {
      ++h;
      ++n;
    }
    if (!*n) return true;
  }
  return false;
}

// Mirrors the script's send(): drop whatever was streaming, write the command,
// then collect for the whole window rather than stopping at the first newline -
// the receiver may still be emitting a log while it answers.
static void sendCmd(HardwareSerial &ser, const char *cmd, uint32_t waitMs) {
  while (ser.available()) ser.read();

  ser.write((const uint8_t *)cmd, strlen(cmd));
  ser.write((const uint8_t *)"\r\n", 2);
  ser.flush();
  DBG("[>] %s\n", cmd);

  size_t n = 0;
  const uint32_t deadline = millis() + waitMs;
  while ((int32_t)(millis() - deadline) < 0) {
    while (ser.available()) {
      int c = ser.read();
      if (n < sizeof(resp) - 1) resp[n++] = (char)c;
    }
    delay(2);
  }
  resp[n] = '\0';
  while (n && (resp[n - 1] == '\r' || resp[n - 1] == '\n' || resp[n - 1] == ' '))
    resp[--n] = '\0';

  DBG("[<] %s\n", n ? resp : "(no response)");
  if (n && (strstr(resp, "Invalid") || containsNoCase(resp, "ERROR")))
    DBG("    ^^ REJECTED: %s\n", cmd);
}

void gnssInit(HardwareSerial &ser) {
  DBG("\n[GNSS] waiting for receiver boot (%d ms)\n", GNSS_BOOT_DELAY_MS);
  while (millis() < GNSS_BOOT_DELAY_MS) delay(10);

  DBG("[GNSS] K803 Lite rover init on %s\n", GNSS_CMD_PORT);

  // A previous run left the port in RTCM auto-detect, where it ignores ASCII.
  // This is the escape hatch back to the command console, so it has to go first
  // even on a cold receiver, where it is simply a no-op.
  // The manual's format is INTERFACEMODE <port> <in> <out> <switch>; without the
  // port the command may be rejected, and then every command below is ignored.
  sendCmd(ser, "INTERFACEMODE " GNSS_CMD_PORT " COMPASS COMPASS ON", 1000);
  sendCmd(ser, "LOG VERSION", 1000);

  for (size_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); ++i)
    sendCmd(ser, kCommands[i], GNSS_CMD_WAIT_MS);

  while (ser.available()) ser.read();  // drop the echo tail before bridging

  DBG("[GNSS] init done - %s is in RTCM auto-detect mode\n", GNSS_CMD_PORT);
}

#else   // GNSS_INIT_K803

void gnssInit(HardwareSerial &) {}

#endif  // GNSS_INIT_K803
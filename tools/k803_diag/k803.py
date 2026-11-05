"""Send commands to the K803 through the passthrough firmware and log everything.

usage: k803.py PORT LOGFILE STEP [STEP ...]
  a STEP is a command, or "wait:SECONDS" to just listen.
Prints non-NMEA replies, and one line whenever GGA quality or satellites change.
"""
import sys
import time

import serial

port, logpath, steps = sys.argv[1], sys.argv[2], sys.argv[3:]

s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 0.1
s.dtr = True
s.rts = False
s.open()

buf = b""
last = None
t0 = time.time()
log = open(logpath, "ab")


def handle(line):
    global last
    text = line.decode("ascii", "replace").strip()
    if not text:
        return
    if text[3:6] == "GGA" and text.startswith("$"):
        f = text.split(",")
        state = (f[6] if len(f) > 6 else "?", f[7] if len(f) > 7 else "?")
        if state != last:
            print("[%6.1fs] GGA quality=%s sats=%s  %s" % (time.time() - t0, state[0], state[1], text[:70]), flush=True)
            last = state
    elif not text.startswith("$"):
        print("[%6.1fs] %s" % (time.time() - t0, text[:200]), flush=True)


def listen(secs):
    global buf
    end = time.time() + secs
    while time.time() < end:
        data = s.read(4096)
        if not data:
            continue
        log.write(data)
        log.flush()
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            handle(line)


listen(1)
for step in steps:
    if step.startswith("wait:"):
        print("[%6.1fs] -- listening %s s" % (time.time() - t0, step[5:]), flush=True)
        listen(float(step[5:]))
    else:
        print("[%6.1fs] >> %s" % (time.time() - t0, step), flush=True)
        s.write(step.encode() + b"\r\n")
        listen(1.5)
s.close()
log.close()

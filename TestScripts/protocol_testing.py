#!/usr/bin/env python3
"""
LED Floor Test CLI
thx claude
===================
Interfaces with the STM32 controller over USB-CDC (virtual COM port).

Protocol (per Grid Message Protocol):
  Controller TX Frame -> Modules:
      Byte 0:      Header (0xFF)
      Byte 1..588: 49 modules x 12 bytes (R1,G1,B1, R2,G2,B2, R3,G3,B3, R4,G4,B4)
      Total: 589 bytes

  Module TX Switch Data -> Controller -> forwarded to us over USB-CDC:
      Byte 0: Module ID (1-49)
      Byte 1: bits 3-0 = Switch4,Switch3,Switch2,Switch1 ; bits 7-4 = empty
      Total: 2 bytes, sent once per module per frame cycle (asynchronously)

This test rig only has 2 real modules wired up (module IDs configurable below),
but the frame sent to the controller must still be the full 589-byte,
49-module frame (unused modules just stay blank/black).

Usage:
    python led_floor_test_cli.py [--port COM5] [--baud 460800]

Commands (once running):
    set <module_id> <pixel 1-4> <r> <g> <b>   e.g. "set 1 1 255 0 0"
    fill <module_id> <r> <g> <b>              set all 4 pixels on a module
    clear                                     blacks out the whole frame
    send                                      transmit the current frame
    switches                                  show last known switch states
    auto on|off                               auto-send frame after every 'set'/'fill'/'clear'
    quit
"""

import argparse
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    print("This script requires pyserial. Install with: pip install pyserial")
    sys.exit(1)

NUM_MODULES = 49
BYTES_PER_MODULE = 12
FRAME_HEADER = 0xFF
FRAME_LENGTH = 1 + NUM_MODULES * BYTES_PER_MODULE  # 589
SWITCH_MSG_LENGTH = 2

# Which module IDs actually exist on this 2-module test rig.
# (The frame/protocol still addresses all 49 -- these just help the CLI
#  warn you if you try to set/read a module that isn't physically present.)
TEST_MODULE_IDS = {1, 2}


class LedFloorInterface:
    def __init__(self, port, baud):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.frame = bytearray(FRAME_LENGTH)
        self.frame[0] = FRAME_HEADER
        # switch_states[module_id] = [sw1, sw2, sw3, sw4]  (0/1 each)
        self.switch_states = {}
        self._switch_lock = threading.Lock()
        self._rx_buf = bytearray()
        self._stop = threading.Event()
        self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._rx_thread.start()

    # ---------- Frame construction ----------

    def _module_offset(self, module_id):
        # Byte 0 is header; module N's 12 bytes start at 1 + (N-1)*12
        return 1 + (module_id - 1) * BYTES_PER_MODULE

    def set_pixel(self, module_id, pixel, r, g, b):
        if not (1 <= module_id <= NUM_MODULES):
            raise ValueError(f"module_id must be 1-{NUM_MODULES}")
        if not (1 <= pixel <= 4):
            raise ValueError("pixel must be 1-4")
        for v, name in ((r, "r"), (g, "g"), (b, "b")):
            if not (0 <= v <= 255):
                raise ValueError(f"{name} must be 0-255")

        base = self._module_offset(module_id) + (pixel - 1) * 3
        self.frame[base] = r
        self.frame[base + 1] = g
        self.frame[base + 2] = b

    def fill_module(self, module_id, r, g, b):
        for p in range(1, 5):
            self.set_pixel(module_id, p, r, g, b)

    def clear(self):
        for i in range(1, len(self.frame)):
            self.frame[i] = 0

    def send_frame(self):
        self.ser.write(bytes(self.frame))
        self.ser.flush()

    # ---------- Switch data receiving ----------

    def _rx_loop(self):
        """Continuously read bytes and pull out 2-byte switch messages."""
        while not self._stop.is_set():
            try:
                data = self.ser.read(64)
            except serial.SerialException:
                break
            if not data:
                continue
            self._rx_buf.extend(data)
            self._drain_switch_messages()

    def _drain_switch_messages(self):
        # Consume complete 2-byte switch messages; resync if a byte doesn't
        # look like a plausible module ID.
        while len(self._rx_buf) >= SWITCH_MSG_LENGTH:
            module_id = self._rx_buf[0]
            switch_byte = self._rx_buf[1]

            if not (1 <= module_id <= NUM_MODULES) or (switch_byte & 0xF0):
                # Misaligned -- drop the first byte and try again from the next one
                self._rx_buf.pop(0)
                continue

            sw1 = switch_byte & 0x01
            sw2 = (switch_byte >> 1) & 0x01
            sw3 = (switch_byte >> 2) & 0x01
            sw4 = (switch_byte >> 3) & 0x01

            with self._switch_lock:
                self.switch_states[module_id] = [sw1, sw2, sw3, sw4]

            del self._rx_buf[0:SWITCH_MSG_LENGTH]

    def get_switch_states(self):
        with self._switch_lock:
            return dict(self.switch_states)

    def close(self):
        self._stop.set()
        self._rx_thread.join(timeout=1)
        self.ser.close()


def print_help():
    print(__doc__.split("Commands")[1])


def main():
    parser = argparse.ArgumentParser(description="LED Floor Test CLI")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=460800)
    args = parser.parse_args()

    print(f"Connecting to {args.port} @ {args.baud} baud...")
    iface = LedFloorInterface(args.port, args.baud)
    print("Connected. Type 'help' for commands, 'quit' to exit.\n")

    auto_send = False

    try:
        while True:
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                break
            if not line:
                continue

            parts = line.split()
            cmd = parts[0].lower()

            try:
                if cmd == "help":
                    print_help()

                elif cmd == "set":
                    # set <module_id> <pixel> <r> <g> <b>
                    module_id, pixel, r, g, b = map(int, parts[1:6])
                    if module_id not in TEST_MODULE_IDS:
                        print(f"  (warning: module {module_id} isn't in the "
                              f"physical test rig {sorted(TEST_MODULE_IDS)}, "
                              f"but sending anyway)")
                    iface.set_pixel(module_id, pixel, r, g, b)
                    print(f"  Set module {module_id} pixel {pixel} -> ({r},{g},{b})")
                    if auto_send:
                        iface.send_frame()
                        print("  (frame sent)")

                elif cmd == "fill":
                    # fill <module_id> <r> <g> <b>
                    module_id, r, g, b = map(int, parts[1:5])
                    if module_id not in TEST_MODULE_IDS:
                        print(f"  (warning: module {module_id} isn't in the "
                              f"physical test rig {sorted(TEST_MODULE_IDS)}, "
                              f"but sending anyway)")
                    iface.fill_module(module_id, r, g, b)
                    print(f"  Filled module {module_id} -> ({r},{g},{b})")
                    if auto_send:
                        iface.send_frame()
                        print("  (frame sent)")

                elif cmd == "clear":
                    iface.clear()
                    print("  Frame cleared (all black)")
                    if auto_send:
                        iface.send_frame()
                        print("  (frame sent)")

                elif cmd == "send":
                    iface.send_frame()
                    print(f"  Sent {FRAME_LENGTH}-byte frame")

                elif cmd == "switches":
                    states = iface.get_switch_states()
                    if not states:
                        print("  No switch data received yet.")
                    else:
                        for mid in sorted(states):
                            tag = "" if mid in TEST_MODULE_IDS else "  (not on test rig?)"
                            print(f"  Module {mid}: {states[mid]}{tag}")

                elif cmd == "auto":
                    if len(parts) < 2 or parts[1] not in ("on", "off"):
                        print("  usage: auto on|off")
                    else:
                        auto_send = (parts[1] == "on")
                        print(f"  auto-send {'enabled' if auto_send else 'disabled'}")

                elif cmd in ("quit", "exit"):
                    break

                else:
                    print(f"  Unknown command: {cmd} (type 'help')")

            except ValueError as e:
                print(f"  Error: {e}")
            except IndexError:
                print("  Missing arguments (type 'help')")

    finally:
        iface.close()
        print("Disconnected.")


if __name__ == "__main__":
    main()
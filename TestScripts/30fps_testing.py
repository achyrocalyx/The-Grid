import serial
import threading
import time
import struct

PORT = "COM3"  # adjust
N_MODULES = 49
FRAME_LEN = 589
FPS = 30

ser = serial.Serial(PORT, 460800, timeout=0.01)

# Shared state
frame_buf = bytearray(FRAME_LEN)
frame_buf[0] = 0xFF
switch_state = {i: 0 for i in range(1, N_MODULES + 1)}  # node_id -> raw switch byte
lock = threading.Lock()
running = True


def hsv_to_rgb(h):
    """h in [0,1) -> (r,g,b) 0-254"""
    h6 = h * 6.0
    i = int(h6) % 6
    f = h6 - int(h6)
    q = int((1 - f) * 254)
    t = int(f * 254)
    v = 254
    return [
        (v, t, 0), (q, v, 0), (0, v, t),
        (0, q, v), (t, 0, v), (v, 0, q),
    ][i]


def set_module_leds(buf, module_id, colors):
    """colors: list of 4 (r,g,b) tuples for LED1..LED4"""
    offset = (module_id * 12) - 11  # matches firmware's indexing
    for led_idx, (r, g, b) in enumerate(colors):
        buf[offset + led_idx * 3 + 0] = r
        buf[offset + led_idx * 3 + 1] = g
        buf[offset + led_idx * 3 + 2] = b


def rainbow_frame(t):
    """Build one full frame: independent rainbow cycling per pixel."""
    buf = bytearray(FRAME_LEN)
    buf[0] = 0xFF
    total_pixels = N_MODULES * 4
    for module_id in range(1, N_MODULES + 1):
        colors = []
        for led_idx in range(4):
            pixel_index = (module_id - 1) * 4 + led_idx
            hue = ((pixel_index / total_pixels) + t * 0.2) % 1.0
            r, g, b = hsv_to_rgb(hue)
            colors.append((r, g, b))
        set_module_leds(buf, module_id, colors)
    return buf

def chase_frame(t, step_duration=0.3):
    buf = bytearray(FRAME_LEN)
    buf[0] = 0xFF
    total_pixels = N_MODULES * 4
    active = int(t / step_duration) % total_pixels
    module_id = (active // 4) + 1
    led_idx = active % 4
    colors = [(0,0,0)] * 4
    colors[led_idx] = (254, 254, 254)
    set_module_leds(buf, module_id, colors)
    # zero out the other module explicitly
    other_id = 2 if module_id == 1 else 1  # adjust for your actual 2 module IDs
    set_module_leds(buf, other_id, [(0,0,0)]*4)
    return buf

import random

CYCLE_EVERY_N_FRAMES = 2  # new color every 5 frames while held

_frame_counter = 0

def switch_reactive_frame(buf, switch_state, module_ids=(24, 25)):
    global _frame_counter
    buf[0] = 0xFF

    color_generation = _frame_counter // CYCLE_EVERY_N_FRAMES

    for module_id in module_ids:
        bits = switch_state.get(module_id, 0)
        colors = []
        for i in range(4):
            if bits & (1 << i):
                seed = color_generation * 1000 + module_id * 10 + i  # pack into single int
                rng = random.Random(seed)
                colors.append((
                    rng.randint(0, 254),
                    rng.randint(0, 254),
                    rng.randint(0, 254),
                ))
            else:
                colors.append((0, 0, 0))
        set_module_leds(buf, module_id, colors)

    _frame_counter += 1
    return buf

def tx_loop():
    frame_interval = 1.0 / FPS
    reusable_buf = bytearray(FRAME_LEN)
    while running:
        loop_start = time.perf_counter()

        with lock:
            current_switch_state = dict(switch_state)  # snapshot to avoid holding lock during frame build

        switch_reactive_frame(reusable_buf, current_switch_state, module_ids=(24, 25))
        ser.write(bytes(reusable_buf))

        elapsed = time.perf_counter() - loop_start
        sleep_time = frame_interval - elapsed
        if sleep_time > 0:
            time.sleep(sleep_time)
        else:
            print(f"[WARN] frame overrun by {-sleep_time*1000:.1f}ms")

def rx_loop():
    """Read switch-data replies (2 bytes each) as they stream in — print every one."""
    buf = bytearray()
    while running:
        chunk = ser.read(64)
        if chunk:
            buf.extend(chunk)
            while len(buf) >= 2:
                node_id, switch_bits = buf[0], buf[1]
                del buf[0:2]
                if 1 <= node_id <= N_MODULES:
                    with lock:
                        switch_state[node_id] = switch_bits
                    pressed = [i for i in range(4) if switch_bits & (1 << i)]
                    print(f"Module {node_id}: bits={switch_bits:04b} pressed={pressed}")


def main():
    global running
    tx_thread = threading.Thread(target=tx_loop, daemon=True)
    rx_thread = threading.Thread(target=rx_loop, daemon=True)
    tx_thread.start()
    rx_thread.start()

    print("Streaming rainbow at 30fps. Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Stopping...")
        running = False
        time.sleep(0.2)
        ser.close()


if __name__ == "__main__":
    main()
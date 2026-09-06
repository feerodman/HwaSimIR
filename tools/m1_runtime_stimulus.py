#!/usr/bin/env python3
"""Send a small, deterministic HwaSimIR UDP scene for M1 runtime smoke tests."""

from __future__ import annotations

import argparse
import math
import socket
import struct
import time


class Packet:
    def __init__(self) -> None: self.parts: list[bytes] = []
    def i(self, value: int) -> None: self.parts.append(struct.pack("<i", value))
    def b(self, value: bool) -> None: self.parts.append(struct.pack("<B", int(value)))
    def d(self, value: float) -> None: self.parts.append(struct.pack("<d", value))
    def spatial(self, lat: float, lon: float, alt: float, yaw=0.0, pitch=0.0, roll=0.0, speed=0.0) -> None:
        for value in (lat, lon, alt, yaw, pitch, roll, speed): self.d(value)
    def bytes(self) -> bytes: return b"".join(self.parts)


def init_packet(band: int, visibility_m: float, observer_alt_m: float) -> bytes:
    p = Packet()
    p.i(0x36); p.i(1); p.i(1); p.i(1)
    p.i(1); p.i(0x11); p.spatial(0.0, 0.0, observer_alt_m)
    p.b(True); p.i(0); p.i(0)
    for value in (0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 25.0, 25.0, 40.0, visibility_m, 0.0, 0.0): p.d(value)
    p.i(2); p.i(60)
    p.b(True); p.b(True); p.d(0.1); p.b(True); p.b(False)
    p.i(band); p.i(640); p.i(512); p.i(1); p.i(50000); p.d(0.0)
    for _ in range(14): p.d(0.0)
    p.i(0); p.d(0.0)
    for value in (3, 2, 0, 0, 0, 0, 0): p.i(value)
    return p.bytes()


def control_packet() -> bytes:
    p = Packet()
    for value in (0x41, 1, 1, 2, 1, 0): p.i(value)
    return p.bytes()


def display_packet(time_ms: float, range_km: float, observer_alt_km: float, target_alt_km: float) -> bytes:
    p = Packet()
    vertical_km = observer_alt_km - target_alt_km
    horizontal_km = math.sqrt(max(0.0, range_km * range_km - vertical_km * vertical_km))
    lon_offset = horizontal_km / 111.32
    p.i(0x38); p.i(1); p.i(1); p.d(time_ms); p.spatial(0.0, 0.0, observer_alt_km * 1000.0)
    p.i(0x22); p.i(1); p.i(0); p.d(0.0); p.d(0.0); p.b(True); p.b(False); p.d(0.0); p.d(0.0)
    p.b(True); p.i(0); p.b(False); p.i(0)
    p.i(1)
    targets = [(0x22, 0, True, lon_offset, target_alt_km * 1000.0),
               (0x22, 1, False, 0.015, 3000.0), (0x22, 2, False, 0.020, 3000.0),
               (0x33, 3, False, 0.025, 3000.0), (0x33, 4, False, 0.030, 3000.0)]
    for kind, ident, visible, lon, alt in targets:
        p.i(kind); p.i(1); p.i(ident); p.b(False); p.b(visible); p.spatial(0.0, lon, alt); p.i(0x01)
    return p.bytes()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=18888)
    ap.add_argument("--band", type=int, choices=[1, 2], required=True, help="Protocol band: 1=NIR, 2=MWIR")
    ap.add_argument("--visibility-km", type=float, default=23.0)
    ap.add_argument("--observer-alt-km", type=float, default=10.0)
    ap.add_argument("--target-alt-km", type=float, default=5.0)
    ap.add_argument("--range-km", type=float, default=10.0)
    ap.add_argument("--utc-hour", type=float, default=9.0)
    ap.add_argument("--frames", type=int, default=180)
    args = ap.parse_args()
    endpoint = ("127.0.0.1", args.port)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(init_packet(args.band, args.visibility_km*1000.0, args.observer_alt_km*1000.0), endpoint)
        time.sleep(0.5)
        sock.sendto(control_packet(), endpoint)
        time.sleep(1.0)
        base_ms = args.utc_hour * 3600000.0
        for frame in range(args.frames):
            sock.sendto(display_packet(base_ms + frame*1000.0/60.0, args.range_km,
                                       args.observer_alt_km, args.target_alt_km), endpoint)
            time.sleep(1.0/60.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

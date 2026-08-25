#!/usr/bin/env python3
"""Генерация иконки mav-voice-gcs (128x128 PNG) без внешних зависимостей.

Тёмно-синий квадрат со скруглением и белым силуэтом квадрокоптера
(корпус + четыре луча с винтами). Запуск: python3 packaging/make_icon.py
"""
import struct
import zlib

W = H = 128
BG = (15, 42, 67)      # тёмно-синий
FG = (240, 244, 248)   # белый
ACC = (255, 152, 0)    # оранжевый акцент


def in_rounded(x, y, m, r):
    return m <= x < W - m and m <= y < H - m and (
        m + r <= x < W - m - r or m + r <= y < H - m - r or
        (x - (m + r)) ** 2 + (y - (m + r)) ** 2 <= r * r and x < m + r and y < m + r or
        (W - m - r - x) ** 2 + (y - (m + r)) ** 2 <= r * r and x >= W - m - r and y < m + r or
        (x - (m + r)) ** 2 + (H - m - r - y) ** 2 <= r * r and x < m + r and y >= H - m - r or
        (W - m - r - x) ** 2 + (H - m - r - y) ** 2 <= r * r and x >= W - m - r and y >= H - m - r
    )


def drone(x, y):
    # корпус
    if 52 <= x < 76 and 56 <= y < 72:
        return True
    # лучи (крест)
    if 60 <= x < 68 and 36 <= y < 88:
        return True
    if 36 <= x < 92 and 60 <= y < 68:
        return True
    return False


def rotor(x, y, cx, cy):
    d2 = (x - cx) ** 2 + (y - cy) ** 2
    if 12 * 12 <= d2 <= 16 * 16:  # кольцо винта
        return True
    if abs(x - cx) + abs(y - cy) <= 5:  # втулка
        return True
    return False


rows = []
for y in range(H):
    row = bytearray([0])  # фильтр None
    for x in range(W):
        if not in_rounded(x, y, 8, 18):
            px = (0, 0, 0, 0)  # прозрачный фон
        else:
            color = BG
            if drone(x, y) or any(rotor(x, y, cx, cy)
                                  for cx, cy in ((40, 40), (88, 40), (40, 88), (88, 88))):
                color = FG
            # оранжевая «точка статуса» в центре корпуса
            if 60 <= x < 68 and 78 <= y < 86:
                color = ACC
            px = (*color, 255)
        row += bytes(px)
    rows.append(bytes(row))


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
png += chunk(b"IEND", b"")

import os
out = os.path.join(os.path.dirname(__file__), "mav-voice-gcs.png")
with open(out, "wb") as f:
    f.write(png)
print(f"иконка: {out} ({len(png)} байт)")

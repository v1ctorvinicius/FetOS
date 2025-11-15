# fethub/tv/oled_ssd1306.py
import pygame
import sys
from fethub.tv.base import TVBase

# ----------------------------------------------------------------------
# FONTE 6x8 — provisória (blocos simples)
# ----------------------------------------------------------------------

def make_font():
    """Fonte minimalista: cada char é um bloco 6x8."""
    font = {}
    for code in range(32, 128):
        glyph = []
        for _ in range(6):
            glyph.append(0b11111111)  # 8 bits ON → bloco cheio
        font[chr(code)] = glyph
    return font

FONT = make_font()


# ======================================================================
#                           OLED 128x64
# ======================================================================

class OLEDSSD1306(TVBase):
    """
    Televisão OLED SSD1306 simulada.
    Usa o framebuffer do TVBase (1 bit/pixel).
    """

    def __init__(self, width=128, height=64, zoom=4, title="OLED 128x64", **kwargs):
        super().__init__(width, height, zoom, title, **kwargs)

    # ==================================================================
    # PIXEL
    # ==================================================================
    def pixel(self, x, y, v=1):
        if 0 <= x < self.W and 0 <= y < self.H:
            self.fb[y][x] = 1 if v else 0

    # ==================================================================
    # RECT
    # ==================================================================
    def rect(self, x, y, w, h, fill=1):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.pixel(xx, yy, fill)

    # ==================================================================
    # TEXT
    # ==================================================================
    def text(self, x, y, msg: str):
        cx = x
        cy = y

        for ch in msg:
            if ch not in FONT:
                ch = "?"

            glyph = FONT[ch]  # lista com 6 colunas (bytes)

            for col_i, col_bits in enumerate(glyph):
                for row in range(8):  # 8px altura
                    bit = (col_bits >> row) & 1
                    px = cx + col_i
                    py = cy + row
                    if 0 <= px < self.W and 0 <= py < self.H:
                        self.fb[py][px] = bit

            cx += 6  # largura do char

    # ==================================================================
    # CLEAR
    # ==================================================================
    def clear(self):
        for y in range(self.H):
            for x in range(self.W):
                self.fb[y][x] = 0

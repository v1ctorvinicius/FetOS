import pygame
import sys
from fethub.tv.base import TVBase

class LCD16x2(TVBase):
    def __init__(self, cols=16, rows=2, zoom=1, title="LCD 16x2", **kwargs):
        self.cols = cols
        self.rows = rows
        w = cols * 6
        h = rows * 10
        super().__init__(w, h, zoom, title)

        self.buffer = [[" "]*cols for _ in range(rows)]

    def text(self, x, y, msg):
        if 0 <= y < self.rows:
            for i, ch in enumerate(msg):
                if 0 <= x+i < self.cols:
                    self.buffer[y][x+i] = ch

        self._render()

    def clear(self):
        self.buffer = [[" "]*self.cols for _ in range(self.rows)]
        self._render()

    def _render(self):
        # rasteriza buffer no framebuffer
        for y in range(self.rows):
            for x in range(self.cols):
                ch = self.buffer[y][x]
                px = x * 6
                py = y * 10
                # desenha um bloco (placeholder)
                for iy in range(8):
                    for ix in range(5):
                        self.fb[py+iy][px+ix] = 1 if ch != " " else 0

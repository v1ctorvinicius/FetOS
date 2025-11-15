# fethub/drivers/tv/rgb_matrix.py

import pygame
from fethub.fgl.fgl_surface import FGLSurface


class RGBMatrix:
    """
    TV simulada de matriz RGB, estilo NeoPixel 8x8 / 16x16 etc.
    Cada pixel recebe (x, y, r, g, b).
    """
    def __init__(self, width=16, height=16, zoom=20, title="RGB Matrix"):
        self.width = width
        self.height = height
        self.zoom = zoom
        self.title = title

        self.surface = FGLSurface(
            width=width,
            height=height,
            zoom=zoom,
            title=title
        )

        # matriz de pixels (r, g, b)
        self.buffer = [
            [(0, 0, 0) for _ in range(width)]
            for _ in range(height)
        ]

    # ----------------------------------------------------
    # UI dispatcher padrão
    # ----------------------------------------------------
    def handle_ui(self, subcmd, payload):
        if subcmd == "clear":
            self.clear()

        elif subcmd == "pixel":
            x = payload.get("x", 0)
            y = payload.get("y", 0)
            r = payload.get("r", 255)
            g = payload.get("g", 255)
            b = payload.get("b", 255)
            self.pixel(x, y, (r, g, b))

        elif subcmd == "rect":
            # desenha retângulo preenchido
            x = payload.get("x", 0)
            y = payload.get("y", 0)
            w = payload.get("w", 1)
            h = payload.get("h", 1)
            color = (
                payload.get("r", 255),
                payload.get("g", 255),
                payload.get("b", 255),
            )
            self.rect(x, y, w, h, color)

        self.update()

    # ----------------------------------------------------
    def clear(self):
        for y in range(self.height):
            for x in range(self.width):
                self.buffer[y][x] = (0, 0, 0)
                self.surface.draw_pixel(x, y, color=(0, 0, 0))
        self.surface.update()

    def pixel(self, x, y, color):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.buffer[y][x] = color
            self.surface.draw_pixel(x, y, color=color)

    def rect(self, x, y, w, h, color):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.pixel(xx, yy, color)

    def update(self):
        self.surface.update()

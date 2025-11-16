import pygame
import sys
import os
from PIL import Image

FONT_PATH = os.path.join(
    os.path.dirname(__file__),
    "..", "..", "assets", "font", "lcd_5x8.png"
)

class LCD16x2:
    """
    Simulador de LCD 16x2 HD44780 usando fonte bitmap 5x8 via PNG.
    """
    COLS = 16
    ROWS = 2

    CHAR_W = 5       # largura do char real HD44780
    CHAR_H = 8       # altura do char real

    GRID_COLS = 16   # no PNG
    GRID_ROWS = 6    # no PNG (6 * 16 = 96 chars = ASCII 32..127)

    def __init__(self, cols=16, lines=2, zoom=4, title="LCD 16x2", **kwargs):
        self.COLS = cols
        self.ROWS = lines
        self.zoom = zoom
        self.title = title

        # resolução lógica (80×16)
        self.WIDTH = cols * self.CHAR_W
        self.HEIGHT = lines * self.CHAR_H

        # carregar pygame
        pygame.init()
        self.surface = pygame.display.set_mode((self.WIDTH * zoom, self.HEIGHT * zoom))
        pygame.display.set_caption(self.title)

        print("LCD 16x2 CREATED:", self.surface)

        # framebuffer 1-bit
        self.fb = [[0 for _ in range(self.WIDTH)] for _ in range(self.HEIGHT)]

        # carregar PNG da fonte 5×8
        if not os.path.exists(FONT_PATH):
            raise FileNotFoundError(f"Fonte PNG não encontrada: {FONT_PATH}")

        self.font_img = Image.open(FONT_PATH).convert("1")

        self.clear()

    # =====================================================================
    # Eventos
    # =====================================================================
    def _process_events(self):
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                pygame.quit()
                sys.exit(0)

    # =====================================================================
    # Framebuffer
    # =====================================================================
    def clear(self):
        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                self.fb[y][x] = 0
        self.update()

    # =====================================================================
    # Desenho de texto a partir da fonte PNG
    # =====================================================================
    def text(self, col, row, msg):

        if row < 0 or row >= self.ROWS:
            return

        # posição em pixels
        x0 = col * self.CHAR_W
        y0 = row * self.CHAR_H

        for ch in msg:
            code = ord(ch)

            # mapa ASCII 32..127 (96 chars)
            if code < 32 or code > 127:
                code = ord("?")

            idx = code - 32

            # posição no PNG
            grid_x = (idx % self.GRID_COLS) * self.CHAR_W
            grid_y = (idx // self.GRID_COLS) * self.CHAR_H

            # copiar tile 5×8
            for yy in range(self.CHAR_H):
                for xx in range(self.CHAR_W):
                    p = self.font_img.getpixel((grid_x + xx, grid_y + yy))
                    print("pixel:", (grid_x+xx, grid_y+yy), p)
                    bit = p > 0
                    px = x0 + xx
                    py = y0 + yy
                    if 0 <= px < self.WIDTH and 0 <= py < self.HEIGHT:
                        self.fb[py][px] = 1 if bit else 0

            x0 += self.CHAR_W
            print("fb[0][0] =", self.fb[0][0])
            print("fb[7][4] =", self.fb[7][4])


    # =====================================================================
    # Atualização
    # =====================================================================
    def update(self):
        self._process_events()

        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                c = 255 if self.fb[y][x] else 0
                pygame.draw.rect(
                    self.surface,
                    (c, c, c),
                    (x * self.zoom, y * self.zoom, self.zoom, self.zoom)
                )

        pygame.display.update()

import pygame
import sys
import time
import os
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "font", "dejavu-sans-mono.bold.ttf")

class OLEDSSD1306:
    """
    Simulador de display OLED SSD1306 128x64 com fonte TTF real.
    """
    def __init__(self, width=128, height=64, zoom=4, title="OLED 128x64", font_size=10, **kwargs):
        self.WIDTH = width
        self.HEIGHT = height
        self.zoom = zoom
        self.title = title

        pygame.init()
        pygame.font.init()

        self.surface = pygame.display.set_mode((self.WIDTH * zoom, self.HEIGHT * zoom))
        pygame.display.set_caption(self.title)

        print("OLED DISPLAY CREATED:", self.surface)

        # Framebuffer 1-bit
        self.fb = [[0 for _ in range(self.WIDTH)] for _ in range(self.HEIGHT)]

        # Carrega fonte TTF com Pillow
        try:
            self.font = ImageFont.truetype(FONT_PATH, font_size)
            print(f"[OLED] Fonte carregada: {FONT_PATH}")
        except Exception as e:
            print(f"[OLED] ERRO ao carregar fonte {FONT_PATH}: {e}")
            sys.exit(1)

        self.clear()

    # ============================================================
    # Eventos
    # ============================================================
    def _process_events(self):
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                pygame.quit()
                sys.exit(0)

    # ============================================================
    # Framebuffer ops
    # ============================================================
    def clear(self):
        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                self.fb[y][x] = 0
        self.update()

    def pixel(self, x, y, v=1):
        if 0 <= x < self.WIDTH and 0 <= y < self.HEIGHT:
            self.fb[y][x] = 1 if v else 0

    def rect(self, x, y, w, h, fill=1):
        for yy in range(y, y+h):
            for xx in range(x, x+w):
                self.pixel(xx, yy, fill)

    # ============================================================
    # TEXTO COM TTF
    # ============================================================
    def text(self, x, y, msg):
        # Cria imagem temporária 1-bit usando Pillow
        img = Image.new("1", (self.WIDTH, self.HEIGHT), 0)
        draw = ImageDraw.Draw(img)

        # Desenha texto usando a fonte TTF
        draw.text((x, y), msg, fill=1, font=self.font)

        # Copia p/ framebuffer
        for py in range(self.HEIGHT):
            for px in range(self.WIDTH):
                self.fb[py][px] = img.getpixel((px, py))

    # ============================================================
    # UPDATE
    # ============================================================
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

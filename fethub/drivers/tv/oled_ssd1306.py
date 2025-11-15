# fethub/tv/oled_ssd1306.py
import pygame
import sys
import time

# Pixel font 6x8 ultra simples (ASCII 32–127)
FONT6x8 = [
    # cada char é uma lista de 6 colunas, cada coluna é um byte vertical
    # fonte minimalista – posso expandir depois
]

def make_font():
    # Gera uma fonte dummy (pontos quadrados)
    font = {}
    for code in range(32, 128):
        glyph = []
        for _ in range(6):
            glyph.append(0b11111111)  # bloco cheio temporário
        font[chr(code)] = glyph
    return font

FONT = make_font()


class OLEDSSD1306:
    """
    Simulador de display OLED SSD1306 128x64.
    - 1 bit por pixel
    - framebuffer 128x64
    - update() redesenha tela
    """

    def __init__(self, width=128, height=64, zoom=4, title="OLED 128x64", **kwargs):
        self.WIDTH = width
        self.HEIGHT = height
        self.zoom = zoom
        self.title = title

        pygame.init()
        self.surface = pygame.display.set_mode(
            (self.WIDTH * zoom, self.HEIGHT * zoom)
        )
        print("OLED DISPLAY CREATED:", self.surface)

        pygame.display.set_caption(self.title)

        self.fb = [[0 for _ in range(self.WIDTH)] for _ in range(self.HEIGHT)]
        self.clear()

    # ============================================================
    # Cleanup
    # ============================================================
    def _process_events(self):
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                pygame.quit()
                sys.exit(0)

    # ============================================================
    # Métodos gráficos
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
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.pixel(xx, yy, fill)

    def text(self, x, y, text):
        cx = x
        cy = y

        for ch in text:
            if ch not in FONT:
                ch = "?"  # fallback

            glyph = FONT[ch]

            for col_i, col_bits in enumerate(glyph):
                for row in range(8):  # 8 pixels de altura
                    bit = (col_bits >> row) & 1
                    px = cx + col_i
                    py = cy + row
                    if 0 <= px < self.WIDTH and 0 <= py < self.HEIGHT:
                        self.fb[py][px] = bit

            cx += 6  # cada char ocupa 6px em largura

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

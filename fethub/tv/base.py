# fethub/tv/base.py
import pygame
import sys

class TVBase:
    def __init__(self, width, height, zoom, title, **kwargs):
        self.W = width
        self.H = height
        self.zoom = zoom
        self.title = title

        pygame.init()
        self.surface = pygame.display.set_mode((self.W * zoom, self.H * zoom))
        pygame.display.set_caption(self.title)

        self.fb = [[0 for _ in range(self.W)] for _ in range(self.H)]

    def _process_events(self):
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                # impede o processo morrer
                print(f"[TV] Ignorando QUIT em {self.title}")
                pass

    def update(self):
        self._process_events()

        self.surface.fill((0, 0, 0))

        for y in range(self.H):
            for x in range(self.W):
                c = 255 if self.fb[y][x] else 0
                pygame.draw.rect(
                    self.surface,
                    (c, c, c),
                    (x * self.zoom, y * self.zoom, self.zoom, self.zoom)
                )

        pygame.display.update()

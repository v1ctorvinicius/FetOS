# fethub/fgl/fgl_surface.py
import pygame

class FGLSurface:
    def __init__(self, width, height, zoom=1, title="FGL Surface"):
        pygame.init()
        self.width = width
        self.height = height
        self.zoom = zoom
        self.title = title

        self.screen = pygame.display.set_mode((width * zoom, height * zoom))
        pygame.display.set_caption(title)

        pygame.font.init()
        self.font = pygame.font.SysFont("Courier", 12)

        self.surface = pygame.Surface((width, height))
        self.color = (0, 255, 0)

    def clear(self):
        self.surface.fill((0, 0, 0))

    def draw_text(self, x, y, text):
        surf = self.font.render(text, True, self.color)
        self.surface.blit(surf, (x, y))

    def draw_pixel(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.surface.set_at((x, y), self.color)

    def draw_line(self, x1, y1, x2, y2):
        pygame.draw.line(self.surface, self.color, (x1, y1), (x2, y2))

    def draw_rect(self, x, y, w, h, fill=1):
        rect = pygame.Rect(x, y, w, h)
        if fill:
            pygame.draw.rect(self.surface, self.color, rect, 0)
        else:
            pygame.draw.rect(self.surface, self.color, rect, 1)

    def update(self):
        # trata eventos básicos pra janela não "morrer"
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                return

        scaled = pygame.transform.scale(
            self.surface,
            (self.width * self.zoom, self.height * self.zoom),
        )
        self.screen.blit(scaled, (0, 0))
        pygame.display.flip()

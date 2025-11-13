import pygame
from fethub.fgl.fgl_surface import FGLSurface
from fethub.fgl.fgl_core import FetGLCore

class OLEDSSD1306:
    def __init__(self, width=128, height=64, zoom=3, title="OLED 128x64"):
        self.width = width
        self.height = height
        self.zoom = zoom
        self.title = title
        # nomear os parâmetros evita confusão
        self.surface = FGLSurface(width=width, height=height, zoom=zoom, title=title)
        self.fgl = FetGLCore(self.surface)

    def execute(self, cmd, payload):
        if cmd == "CLEAR":
            self.surface.clear()
        elif cmd == "TEXT":
            self.surface.draw_text(payload.get("x", 0), payload.get("y", 0), payload.get("text", ""))
        elif cmd == "PIXEL":
            self.surface.draw_pixel(payload.get("x", 0), payload.get("y", 0))
        elif cmd == "RECT":
            self.surface.draw_rect(payload.get("x", 0), payload.get("y", 0), payload.get("w", 1), payload.get("h", 1), payload.get("fill", 1))
        else:
            print(f"⚠️  Comando desconhecido: {cmd}")

    def update(self):
        self.surface.update()

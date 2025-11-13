# fethub/fgl/fgl_core.py
import json

class FetGLCore:
    """Core da Fet Graphics Layer: interpreta comandos UI_DRAW."""
    def __init__(self, surface):
        self.surface = surface

    def execute(self, payload: dict):
        cmd = payload.get("cmd")
        if cmd == "CLEAR":
            self.surface.clear()
        elif cmd == "TEXT":
            x, y = payload.get("x", 0), payload.get("y", 0)
            text = payload.get("text", "")
            self.surface.draw_text(x, y, text)
        elif cmd == "PIXEL":
            self.surface.draw_pixel(payload.get("x", 0), payload.get("y", 0))
        elif cmd == "SYNC":
            self.surface.refresh()

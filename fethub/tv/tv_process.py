# fethub/tv/tv_process.py
import time
import pygame
import sys
from multiprocessing import Queue

from fethub.drivers.tv.oled_ssd1306 import OLEDSSD1306
from fethub.drivers.tv.lcd_16x2 import LCD16x2
from fethub.drivers.tv.rgb_matrix import RGBMatrix

TV_TYPES = {
    "oled": OLEDSSD1306,
    "lcd": LCD16x2,
    "rgb": RGBMatrix,
}


def tv_process_main(tv_type: str, alias: str, kwargs: dict, queue: Queue):
    pygame.init()

    TVClass = TV_TYPES.get(tv_type)
    if not TVClass:
        print(f"[TVPROC] Tipo desconhecido '{tv_type}'")
        return

    tv = TVClass(**kwargs)
    print(f"{tv.title} CREATED: {tv.surface}")

    clock = pygame.time.Clock()
    running = True

    while running:
        # eventos do pygame
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False

        # comandos recebidos
        try:
            while True:
                msg = queue.get_nowait()
                op = msg.get("op")

                if op == "clear":
                    tv.clear()

                elif op == "text":
                    tv.text(msg.get("x", 0), msg.get("y", 0), msg.get("text", ""))

                elif op == "pixel":
                    tv.pixel(msg.get("x", 0), msg.get("y", 0), msg.get("v", 1))

                elif op == "rect":
                    tv.rect(
                        msg.get("x", 0),
                        msg.get("y", 0),
                        msg.get("w", 1),
                        msg.get("h", 1),
                        msg.get("fill", 1)
                    )

                elif op == "__quit__":
                    running = False

        except Exception:
            pass

        tv.update()
        clock.tick(120)

    pygame.quit()
    sys.exit(0)

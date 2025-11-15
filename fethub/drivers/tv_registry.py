##
# Registro global de tipos de TVs
##

from fethub.drivers.tv.oled_ssd1306 import OLEDSSD1306
from fethub.drivers.tv.lcd_16x2 import LCD16x2
from fethub.drivers.tv.rgb_matrix import RGBMatrix


def register_all_tv_types(manager):

    # ---------------- OLED ----------------
    manager.register_type(
        "oled",
        lambda **kw: OLEDSSD1306(
            width=128,
            height=64,
            zoom=kw.get("zoom", 4),
            title=kw.get("title", "OLED 128x64"),
        )
    )

    # ---------------- LCD 16x2 ----------------
    manager.register_type(
        "lcd",
        lambda **kw: LCD16x2(
            cols=16,
            rows=2,
            zoom=kw.get("zoom", 20),
            title=kw.get("title", "LCD 16x2"),
        )
    )

    # ---------------- RGB MATRIX ----------------
    manager.register_type(
        "rgb",
        lambda **kw: RGBMatrix(
            width=64,
            height=32,
            zoom=kw.get("zoom", 10),
            title=kw.get("title", "RGB LED Matrix"),
        )
    )

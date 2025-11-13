# fethub/tv_channels/tv_oled_ssd1306.py
from fethub.devices.oled_ssd1306 import OLEDSSD1306

def register_oled(manager):
    tv_id = "tv:OLED_SIM"
    # corrige a ordem dos argumentos: width, height, zoom, title
    oled = OLEDSSD1306(width=128, height=64, zoom=4, title="OLED SIM")
    manager.tvs[tv_id] = oled
    print(f"🖥️  Nova TV registrada: {tv_id} ({oled.width}x{oled.height} x{oled.zoom})")

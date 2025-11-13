#!/usr/bin/env python3
import sys
from fethub.core.fetlink_client import FetLinkClient
from fethub.tv_channels import tv_oled_ssd1306
import time

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    client = FetLinkClient(port)
    tv_oled_ssd1306.register_oled(client.manager)

    print("\n===========================================")
    print("  🧠 FetHub iniciado com TVs:")
    for tv_id in client.manager.tvs:
        print(f"   • {tv_id}")
    print("===========================================\n")

    try:
        while True:
            client.process_queue()
            client.manager.tick()
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("🛑 Encerrando FetHub...")
        client.running = False

if __name__ == "__main__":
    main()

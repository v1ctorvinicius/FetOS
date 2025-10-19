import serial, pygame, sys, time

# === CONFIGURAÇÕES INICIAIS ===
PORT = "COM5"
BAUD = 9600
MAX_LINES = 8
MAX_COLS = 16
ZOOM_LEVELS = [1, 1.5, 2, 3, 4]  # níveis 1–5 (1 = real)
ZOOM_INDEX = 0

# === INICIALIZA SERIAL ===
try:
    ser = serial.Serial(PORT, BAUD, timeout=0.05)
except serial.SerialException as e:
    print(f"[ERRO] Não foi possível abrir {PORT}: {e}")
    sys.exit(1)

# === INICIALIZA PYGAME ===
pygame.init()
font = pygame.font.SysFont("Courier", 12)
base_w, base_h = 128, 64

def update_screen_size():
    """Aplica o fator de zoom mantendo proporção."""
    global screen, zoom
    zoom = ZOOM_LEVELS[ZOOM_INDEX]
    screen = pygame.display.set_mode((int(base_w*zoom), int(base_h*zoom)))
    pygame.display.set_caption(f"FetOS Hub - Zoom x{zoom}")

update_screen_size()

lines = ["" for _ in range(MAX_LINES)]
running = True
last_key_send = 0

# === LOOP PRINCIPAL ===
while running:
    # --- Eventos ---
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

        elif event.type == pygame.KEYDOWN:
            key = event.unicode
            # zoom control
            if event.key == pygame.K_1: ZOOM_INDEX = 0; update_screen_size()
            elif event.key == pygame.K_2: ZOOM_INDEX = 1; update_screen_size()
            elif event.key == pygame.K_3: ZOOM_INDEX = 2; update_screen_size()
            elif event.key == pygame.K_4: ZOOM_INDEX = 3; update_screen_size()
            elif event.key == pygame.K_5: ZOOM_INDEX = 4; update_screen_size()
            elif event.key == pygame.K_ESCAPE:
                running = False
            else:
                # envia tecla pela serial (se possível)
                if ser.is_open and key and (time.time() - last_key_send > 0.05):
                    ser.write(key.encode())
                    last_key_send = time.time()

    # --- Leitura Serial ---
    try:
        line = ser.readline().decode(errors='ignore').strip()
        if line.startswith("LCD:PRINT,"):
            text = line.split(",", 1)[1]
            lines.append(text)
            lines = lines[-MAX_LINES:]
    except Exception:
        pass

    # --- Renderização ---
    screen.fill((0, 0, 0))
    green = (0, 255, 0)
    for i, txt in enumerate(lines):
        surf = font.render(txt, True, green)
        surf = pygame.transform.scale(surf, (int(surf.get_width()*zoom), int(surf.get_height()*zoom)))
        screen.blit(surf, (2*zoom, i*8*zoom))
    pygame.display.flip()
    time.sleep(0.05)

pygame.quit()
ser.close()

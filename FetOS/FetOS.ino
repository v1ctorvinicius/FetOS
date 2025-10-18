//FetOS - v0.1
#include <Arduino.h>


//// ---- global system variables ----
unsigned long system_start_ms = 0;  // momento em que o sistema ligou
int8_t menu_index = 0;
int8_t active_app = -1;   // executando logicamente
int8_t visible_app = -1;  // exibido no LCD (-1 = menu)

// LCD cache para otimização de escrita
static char prev_line1[17] = { 0 };
static char prev_line2[17] = { 0 };

//// ---- apps typedefs ----
typedef enum { APP_RUNNING,
               APP_SUSPENDED,
               APP_CLOSED } app_state_t;

typedef void (*app_run_t)();
typedef void (*app_bg_t)();
typedef void (*app_cleanup_t)();

typedef struct {
  const char *name;
  app_state_t state;
  bool is_visible;
  app_run_t run;  // foreground (writes LCD)
  app_bg_t bg;    // background minimal (no LCD writes)
  void (*fg)();   // interface (front)
  app_cleanup_t cleanup;
  char last_status[17];
} app_t;

//// ---- Icons (CGRAM custom characters (5x8)) ----
byte ICON_RUN[8] = {
  B00000,
  B01000,
  B01100,
  B01110,
  B01111,
  B01110,
  B01100,
  B01000
};

byte ICON_MIN[8] = {
  B00000,
  B01111,
  B01001,
  B11101,
  B11111,
  B11100,
  B00000,
  B00000
};

byte ICON_SUS[8] = {
  B00000,
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
  B00000,
  B00000
};

byte ICON_STOP[8] = {
  B00000,
  B01110,
  B11001,
  B10101,
  B10011,
  B01110,
  B00000,
  B00000
};

byte ICON_LIGHTNING[8] = {
  B00111,
  B01110,
  B01110,
  B11100,
  B11111,
  B00011,
  B00110,
  B00100
};

byte ICON_CHECK[8] = {
  B00000,
  B00000,
  B00001,
  B00010,
  B10100,
  B01000,
  B00000,
  B00000
};

//// ---- prototypes
void lcd_pulse_enable();
void lcd_send_nibble(uint8_t nibble);
void lcd_send(uint8_t value, uint8_t mode);
void lcd_init();
void lcd_clear();
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_print(const char *str);
void lcd_pwm_init();
void lcd_create_char(uint8_t location, byte charmap[]);
void icons_init();
void icons_reload();
void lcd_draw_lines(const char *line1, const char *line2);
char state_icon(app_state_t s);
void render_menu();
void render_status_bar_for_showing_app(uint8_t idx);
void render_header(const char *title);
void render_header_for_app(uint8_t idx, bool forceRedraw = false);
void handle_serial_char(char c);
void scheduler_tick();
void clock_app_run();
void clock_app_bg();
void clock_app_cleanup();
void fetOSHeart_app_fg();
void fetOSHeart_app_bg();
void fetOSHeart_app_cleanup();
void app_play(uint8_t idx);
void app_suspend(uint8_t idx);
void app_close(uint8_t idx);
void app_minimize(uint8_t idx);
void app_show(uint8_t idx);

//// ---- FetOS Core ---- #section

// ==== Pinagem ====
#define LCD_RS 8
#define LCD_EN 9
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 6
#define LCD_D7 7
#define D12 12
#define LCD_CONTRASTE 11  // V0 (PWM)
#define LCD_BACKLIGHT 10  // PWM
#define LED_PIN 13

// ---- Setup/loop ----
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(D12, OUTPUT);
  digitalWrite(D12, LOW);
  Serial.begin(9600);
  lcd_pwm_init();
  lcd_init();
  lcd_clear();
  icons_init();
  system_start_ms = millis();  // salva o tempo de início
  render_menu();
}

void loop() {
  static unsigned long last_tick = 0;
  if (millis() - last_tick >= 80) {
    last_tick = millis();
    scheduler_tick();
  }
  if (Serial.available()) handle_serial_char(Serial.read());
}


// ---- app subsystem ----
// registered apps
app_t apps[] = {
  { "Clock", APP_CLOSED, false, clock_app_run, clock_app_bg, clock_app_fg, clock_app_cleanup, "" },
  { "FetOS Heart", APP_CLOSED, false, fetOSHeart_app_run, fetOSHeart_app_bg, fetOSHeart_app_fg, fetOSHeart_app_cleanup, "" },
  { "Status", APP_CLOSED, false, status_app_run, status_app_bg, status_app_fg, status_app_cleanup, "" }
};
const uint8_t APP_COUNT = sizeof(apps) / sizeof(apps[0]);
bool cleanup_pending[APP_COUNT] = { false };
void app_play(uint8_t idx) {
  app_t *a = &apps[idx];
  a->state = APP_RUNNING;
}
void app_suspend(uint8_t idx) {
  app_t *a = &apps[idx];
  if (a->state == APP_RUNNING) {
    a->state = APP_SUSPENDED;
  } else {
    Serial.println("Erro: Não é possível suspender um aplicativo fechado!");
  }
}
void app_close(uint8_t idx) {
  app_t *a = &apps[idx];

  // muda estado lógico
  if (a->state != APP_CLOSED) {
    a->state = APP_CLOSED;
    cleanup_pending[idx] = true;
  }

  // limpa flags de visibilidade e execução
  a->is_visible = false;
  if (visible_app == idx) visible_app = -1;
  if (active_app == idx) active_app = -1;

  // limpa cache e força menu imediato
  lcd_clear();
  prev_line1[0] = '\0';
  prev_line2[0] = '\0';
  render_menu();
}


void app_minimize(uint8_t idx) {
  app_t *a = &apps[idx];
  a->is_visible = false;
  visible_app = -1;
  active_app = -1;

  lcd_clear();
  render_menu();
}

void app_show(uint8_t idx) {
  for (uint8_t i = 0; i < APP_COUNT; ++i)
    apps[i].is_visible = false;

  apps[idx].is_visible = true;
  visible_app = idx;
  active_app = idx;  // opcional: dá foco

  lcd_clear();
  render_header_for_app(idx);
  if (apps[idx].fg) apps[idx].fg();
}


// scheduler
void scheduler_tick() {
  static int8_t last_visible = -2;
  bool anyActive = false;
  bool anyVisible = false;

  // --- 1. Execução lógica (apps RUNNING) ---
  for (uint8_t i = 0; i < APP_COUNT; ++i) {
    app_t *a = &apps[i];
    if (a->state == APP_RUNNING) {
      if (a->bg) a->bg();
      if (a->run) a->run();
      anyActive = true;
    }
  }

  // --- 2. Redesenho de contexto (mudança de app visível) ---
  if (visible_app != last_visible) {
    lcd_clear();
    prev_line1[0] = '\0';
    prev_line2[0] = '\0';

    if (visible_app >= 0 && apps[visible_app].fg) {
      render_header(apps[visible_app].name);
      apps[visible_app].fg();
      anyVisible = true;
    } else {
      render_menu();
    }

    last_visible = visible_app;
  }

  // --- 3. Renderização contínua do app visível ---
  if (visible_app >= 0) {
    app_t *a = &apps[visible_app];
    if (a->fg && a->is_visible) {
      render_header(a->name);
      a->fg();
      anyVisible = true;
    }
  }

  // --- 4. Fallback visual (menu) ---
  // Renderiza o menu SOMENTE se nenhum app estiver visível
  if (visible_app == -1 && !anyActive) {
    render_menu();
  }

  // --- 5. Cleanup atrasado (apps encerrados) ---
  for (uint8_t i = 0; i < APP_COUNT; ++i) {
    if (cleanup_pending[i]) {
      if (apps[i].cleanup) apps[i].cleanup();
      cleanup_pending[i] = false;
    }
  }
}



//// ---- FetOS LCD ---- #section
void lcd_pulse_enable() {
  digitalWrite(LCD_EN, HIGH);
  delayMicroseconds(1);
  digitalWrite(LCD_EN, LOW);
  delayMicroseconds(50);
}

void lcd_send_nibble(uint8_t nibble) {
  digitalWrite(LCD_D4, nibble & 0x01);
  digitalWrite(LCD_D5, (nibble >> 1) & 0x01);
  digitalWrite(LCD_D6, (nibble >> 2) & 0x01);
  digitalWrite(LCD_D7, (nibble >> 3) & 0x01);
  lcd_pulse_enable();
}

void lcd_send(uint8_t value, uint8_t mode) {
  digitalWrite(LCD_RS, mode);
  lcd_send_nibble(value >> 4);
  lcd_send_nibble(value & 0x0F);
  delayMicroseconds(50);
}

void lcd_init() {
  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_EN, OUTPUT);
  pinMode(LCD_D4, OUTPUT);
  pinMode(LCD_D5, OUTPUT);
  pinMode(LCD_D6, OUTPUT);
  pinMode(LCD_D7, OUTPUT);
  delay(50);

  lcd_send_nibble(0x03);
  delay(5);
  lcd_send_nibble(0x03);
  delayMicroseconds(150);
  lcd_send_nibble(0x03);
  lcd_send_nibble(0x02);

  lcd_send(0x28, 0);  // 4-bit, 2 lines
  lcd_send(0x0C, 0);  // display ON, cursor off
  lcd_send(0x01, 0);  // clear
  delay(2);
}

void lcd_clear() {
  lcd_send(0x01, 0);
  delay(2);
  prev_line1[0] = '\0';
  prev_line2[0] = '\0';
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
  uint8_t row_offsets[] = { 0x00, 0x40 };
  lcd_send(0x80 | (col + row_offsets[row]), 0);
}

void lcd_print(const char *str) {
  while (*str) lcd_send(*str++, 1);
}

void lcd_pwm_init() {
  pinMode(LCD_CONTRASTE, OUTPUT);
  analogWrite(LCD_CONTRASTE, 60);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  analogWrite(LCD_BACKLIGHT, 120);
}

void lcd_create_char(uint8_t location, byte charmap[]) {
  location &= 0x7;                      // limita 0–7
  lcd_send(0x40 | (location << 3), 0);  // comando: definir endereço CGRAM
  for (int i = 0; i < 8; i++) {
    lcd_send(charmap[i], 1);  // grava cada linha (5x8 bits)
  }
}

void icons_init() {
  lcd_create_char(0, ICON_RUN);
  lcd_create_char(1, ICON_MIN);
  lcd_create_char(2, ICON_SUS);
  lcd_create_char(3, ICON_STOP);
  lcd_create_char(4, ICON_LIGHTNING);
}

void icons_reload() {
  lcd_create_char(0, ICON_RUN);
  lcd_create_char(1, ICON_MIN);
  lcd_create_char(2, ICON_SUS);
  lcd_create_char(3, ICON_STOP);
}

void lcd_draw_line1(const char *line1) {
  lcd_draw_lines(line1, NULL);
}

void lcd_draw_line2(const char *line2) {
  lcd_draw_lines(NULL, line2);
}

void lcd_draw_lines(const char *line1, const char *line2) {
  // Linha 1
  if (line1 && strncmp(prev_line1, line1, 16) != 0) {
    lcd_set_cursor(0, 0);
    lcd_print(line1);
    for (int i = strlen(line1); i < 16; ++i) lcd_print(" ");
    strncpy(prev_line1, line1, 16);
    prev_line1[16] = '\0';
  }

  // Linha 2
  if (line2 && strncmp(prev_line2, line2, 16) != 0) {
    lcd_set_cursor(0, 1);
    lcd_print(line2);
    for (int i = strlen(line2); i < 16; ++i) lcd_print(" ");
    strncpy(prev_line2, line2, 16);
    prev_line2[16] = '\0';
  }
}


//// ---- FetOS UI ---- #section
// ---- Ícones de estado ----
char state_icon(app_state_t s) {
  switch (s) {
    case APP_RUNNING: return 0;
    case APP_SUSPENDED: return 2;
    case APP_CLOSED: return 210;  // katakana built-in メ (me)
    default: return '?';
  }
}

// ---- Render do menu ----
void render_menu() {
  char line1[17], line2[17];

  snprintf(line1, 17, "FetOS Menu %d/%d", menu_index + 1, APP_COUNT);
  lcd_draw_lines(line1, "");

  lcd_set_cursor(0, 1);
  lcd_print("[");
  lcd_send(state_icon(apps[menu_index].state), 1);
  lcd_print("]");
  lcd_print(apps[menu_index].name);

  int len = 3 + strlen(apps[menu_index].name);
  for (int i = len; i < 16; ++i) lcd_print(" ");
}

void render_header_for_app(uint8_t idx, bool forceRedraw = false) {
  app_t *a = &apps[idx];
  char line1[17];

  char icon = state_icon(a->state);

  // Monta linha 1
  int name_len = strlen(a->name);
  int spaces = 15 - name_len; // reserva 1 para o ícone
  if (spaces < 1) spaces = 1;
  snprintf(line1, 17, "%s%*s", a->name, spaces, "");

  // Se for forçado, escreve tudo direto
  if (forceRedraw) {
    lcd_set_cursor(0, 0);
    lcd_print(line1);
    lcd_set_cursor(15, 0);
    lcd_send(icon, 1);
  } else {
    // Usa cache: redesenha só se mudou
    lcd_draw_lines(line1, NULL);
    lcd_set_cursor(15, 0);
    lcd_send(icon, 1);
  }
}


// ---- Render de app ativo ----
void render_status_bar_for_showing_app(uint8_t idx) {
  app_t *a = &apps[idx];
  char line1[17];

  char icon;
  switch (a->state) {
    case APP_RUNNING: icon = 0; break;
    case APP_SUSPENDED: icon = 2; break;
    case APP_CLOSED: icon = 210; break;
    default: icon = '?';
  }

  // Limpa a linha 1
  lcd_set_cursor(0, 0);
  for (int i = 0; i < 16; ++i) lcd_print(" ");

  // Nome do app alinhado à esquerda, ícone à direita
  int name_len = strlen(a->name);
  int spaces = 15 - name_len; // 1 espaço reservado ao ícone
  if (spaces < 1) spaces = 1;
  snprintf(line1, 17, "%s%*s", a->name, spaces, "");

  lcd_set_cursor(0, 0);
  lcd_print(line1);

  // Ícone de estado no canto direito
  lcd_set_cursor(15, 0);
  lcd_send(icon, 1);
}


void render_header(const char *title) {
  app_t *a = NULL;

  // Procura o app correspondente (opcional, só pra saber o estado)
  for (uint8_t i = 0; i < APP_COUNT; ++i) {
    if (strcmp(apps[i].name, title) == 0) {
      a = &apps[i];
      break;
    }
  }

  char line1[17];
  char icon = (a) ? state_icon(a->state) : 210;  // ícone de estado ou 'メ'

  // Monta a linha: Nome + ícone à direita
  int name_len = strlen(title);
  int right_len = 2;  // espaço + ícone
  int spaces = 16 - name_len - right_len;
  if (spaces < 1) spaces = 1;
  snprintf(line1, 17, "%s%*s", title, spaces, "");

  // Escreve linha 1
  lcd_draw_lines(line1, NULL);
  lcd_set_cursor(15, 0);
  lcd_send(icon, 1);  // desenha o ícone à direita
}


//// ---- FetOS IO ----
// ---- Serial Support ----
void handle_serial_char(char c) {
  if (active_app == -1) {
    // MENU
    if (c == 'd') {
      menu_index = (menu_index + 1) % APP_COUNT;
      render_menu();
    } else if (c == 'a') {
      menu_index = (menu_index - 1 + APP_COUNT) % APP_COUNT;
      render_menu();
    } else if (c == 'w') {
      // abrir app (só torna visível, não executa ainda)
      app_show(menu_index);
      active_app = menu_index;
      lcd_clear();
      render_header_for_app(menu_index, true);  // ✅ força redraw total
    }
  } else {
    // DENTRO DO APP
    switch (c) {
      case 'e':  // play / resume
        app_play(active_app);
        render_header_for_app(active_app, true);  // ✅ atualiza header
        break;

      case 'q':  // pause
        app_suspend(active_app);
        render_header_for_app(active_app, true);  // ✅ idem
        break;

      case 'x':  // close
        app_close(active_app);
        break;

      case 's':  // minimize
        app_minimize(active_app);
        break;

      case 'w':  // reabrir
        app_show(active_app);
        render_header_for_app(active_app, true);  // ✅ idem
        break;
    }
  }
}



//// ---- FetOS Apps ---- #section
// ---- Clock App ----
typedef struct {
  unsigned long uptime_secs;
} clock_state_t;

clock_state_t clock_internal = { 0 };

void clock_app_bg() {
  unsigned long now = millis();
  clock_internal.uptime_secs = (now - system_start_ms) / 1000UL;
}

void clock_app_run() {
  // poderia fazer algo adicional quando o app está ativo
  // (p. ex. ler sensores, atualizar outras métricas)
}

void clock_app_fg() {
  app_t *a = &apps[0];
  snprintf(a->last_status, 17, "Uptime: %lus", clock_internal.uptime_secs);
  lcd_draw_lines(NULL, a->last_status);
}

void clock_app_cleanup() {
  apps[0].last_status[0] = '\0';
}


// ---- FetOS Heart App ----
typedef struct {
  bool led_state;
  unsigned long last_toggle;
} fetOSHeart_state_t;

fetOSHeart_state_t fetOSHeart_internal = { false, 0 };

void fetOSHeart_app_bg() {
  unsigned long now = millis();
  if (now - fetOSHeart_internal.last_toggle >= 500) {
    fetOSHeart_internal.last_toggle = now;
    fetOSHeart_internal.led_state = !fetOSHeart_internal.led_state;
    digitalWrite(LED_PIN, fetOSHeart_internal.led_state ? HIGH : LOW);
  }
}

void fetOSHeart_app_run() {
  // poderíamos colocar lógica adicional aqui (ex: comunicação heartbeat)
}

void fetOSHeart_app_fg() {
  app_t *a = &apps[1];
  snprintf(a->last_status, 17, "%s", fetOSHeart_internal.led_state ? "LED: ON" : "LED: OFF");
  lcd_draw_lines(prev_line1, a->last_status);
}

void fetOSHeart_app_cleanup() {
  fetOSHeart_internal.led_state = false;
  digitalWrite(LED_PIN, LOW);
  apps[1].last_status[0] = '\0';
}

typedef struct {
  uint8_t selected_index;
} status_app_state_t;

status_app_state_t status_internal = { 0 };

void status_app_bg() {
  // nada necessário
}

void status_app_run() {
  // nada necessário, bg ou fg fazem a atualização visual
}

void status_app_fg() {
  char line2[17];
  snprintf(line2, 17, "[%c]%s",
           state_icon(apps[status_internal.selected_index].state),
           apps[status_internal.selected_index].name);
  lcd_draw_lines(NULL, line2);  // linha1 intacta
}

void status_app_cleanup() {
  // opcional: limpa linha2
  lcd_draw_lines(NULL, "                ");
}

// handle_serial dentro do Status App
void status_handle_input(char c) {
  if (c == 'd') {
    status_internal.selected_index = (status_internal.selected_index + 1) % APP_COUNT;
  } else if (c == 'a') {
    status_internal.selected_index = (status_internal.selected_index - 1 + APP_COUNT) % APP_COUNT;
  }
  status_app_fg();  // redesenha
}

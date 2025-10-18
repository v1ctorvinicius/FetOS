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
void render_status_bar_for_app(uint8_t idx);
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
#define LCD_CONTRASTE 11  // V0 (PWM)
#define LCD_BACKLIGHT 10  // PWM
#define LED_PIN 13

// ---- Setup/loop ----
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  lcd_pwm_init();
  lcd_init();
  lcd_clear();
  icons_init();
  system_start_ms = millis();  // salva o tempo de início
  render_menu();
}

void loop() {
  if (Serial.available()) handle_serial_char(Serial.read());
  scheduler_tick();
  delay(80);
}

// ---- app subsystem ----
// registered apps
app_t apps[] = {
  { "Clock", APP_CLOSED, false, clock_app_run, clock_app_bg, clock_app_cleanup, "" },
  { "FetOS Heart", APP_CLOSED, false, fetOSHeart_app_fg, fetOSHeart_app_bg, fetOSHeart_app_cleanup, "" }
};

const uint8_t APP_COUNT = sizeof(apps) / sizeof(apps[0]);

// ---- app management functions ----
void app_play(uint8_t idx) {
  app_t *a = &apps[idx];
  a->state = APP_RUNNING;
}

void app_suspend(uint8_t idx) {
  app_t *a = &apps[idx];
  if (a->state == APP_RUNNING) {  // Só permite suspender se estiver em execução
    a->state = APP_SUSPENDED;
  } else {
    Serial.println("Erro: Não é possível suspender um aplicativo fechado!");
  }
}

void app_close(uint8_t idx) {
  app_t *a = &apps[idx];
  if (a->state != APP_CLOSED) {
    a->state = APP_CLOSED;
    a->is_visible = false;
    if (a->cleanup) a->cleanup();
  }
}


void app_minimize(uint8_t idx) {
  app_t *a = &apps[idx];
  a->is_visible = false;
  visible_app = -1;
  render_menu();
}


void app_show(uint8_t idx) {
  for (uint8_t i = 0; i < APP_COUNT; ++i) {
    apps[i].is_visible = false;
  }
  apps[idx].is_visible = true;
  visible_app = idx;
}


// ---- scheduler ----
void scheduler_tick() {
  bool anyVisible = false;

  for (uint8_t i = 0; i < APP_COUNT; ++i) {
    app_t *a = &apps[i];

    switch (a->state) {
      case APP_RUNNING:
        if (a->bg) a->bg();
        if (a->run) a->run();
        if (a->is_visible && a->fg) {
          a->fg();
          anyVisible = true;
        }
        break;

      case APP_SUSPENDED:
        if (a->bg) a->bg();
        break;

      case APP_CLOSED:
        break;
    }
  }

  if (!anyVisible) render_menu();
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
  analogWrite(LCD_CONTRASTE, 100);
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

void lcd_draw_lines(const char *line1, const char *line2) {
  if (strncmp(prev_line1, line1, 16) != 0) {
    lcd_set_cursor(0, 0);
    lcd_print(line1);
    for (int i = strlen(line1); i < 16; ++i) lcd_print(" ");
    strncpy(prev_line1, line1, 16);
    prev_line1[16] = '\0';
  }
  if (strncmp(prev_line2, line2, 16) != 0) {
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

  // Linha 1: título + posição
  snprintf(line1, 17, "FetOS Menu %d/%d", menu_index + 1, APP_COUNT);
  lcd_draw_lines(line1, "");  // limpa só a linha 1

  // Linha 2: [status] nome do app
  lcd_set_cursor(0, 1);
  lcd_print("[");
  lcd_send(state_icon(apps[menu_index].state), 1);  // ícone custom direto
  lcd_print("]");
  lcd_print(apps[menu_index].name);

  // Preencher o restante da linha
  int len = 3 + strlen(apps[menu_index].name);  // [X] + nome
  for (int i = len; i < 16; ++i) lcd_print(" ");
}

// ---- Render de app ativo ----
void render_status_bar_for_app(uint8_t idx) {
  app_t *a = &apps[idx];
  char line1[17];

  char icon;
  switch (a->state) {
    case APP_RUNNING: icon = 0; break;
    case APP_SUSPENDED: icon = 2; break;
    case APP_CLOSED: icon = 210; break;
    default: icon = '?';
  }

  // Limpar linha 1 antes de escrever
  lcd_set_cursor(0, 0);
  for (int i = 0; i < 16; ++i) lcd_print(" ");

  // Construir linha 1: "Nome do app      posição/ícone"
  int name_len = strlen(a->name);
  int right_len = 3;  // "X/Í"
  int spaces = 16 - name_len - right_len;
  if (spaces < 1) spaces = 1;  // garante pelo menos 1 espaço
  snprintf(line1, 17, "%s%*s", a->name, spaces, "");

  lcd_set_cursor(0, 0);
  lcd_print(line1);

  // Posição no menu + ícone
  lcd_set_cursor(16 - right_len, 0);
  char pos_icon[3];
  pos_icon[0] = '0' + (idx + 1);  // posição 1-based
  pos_icon[1] = '/';
  pos_icon[2] = '\0';
  lcd_print(pos_icon);
  lcd_send(icon, 1);  // ícone do status à direita
}

//// ---- FetOS IO ----
// ---- Serial Support ----
void handle_serial_char(char c) {
  if (active_app == -1) {
    // MENU
    if (c == 'd') {
      menu_index = (menu_index - 1 + APP_COUNT) % APP_COUNT;
      render_menu();
    } else if (c == 'a') {
      menu_index = (menu_index + 1) % APP_COUNT;
      render_menu();
    } else if (c == 'w') {
      // abrir app (apenas torna visível, sem alterar estado)
      app_t *a = &apps[menu_index];
      app_show(menu_index);
      active_app = menu_index;
      // limpa a linha 2 imediatamente
      lcd_set_cursor(0, 1);
      for (int i = 0; i < 16; ++i) lcd_print(" ");
      prev_line1[0] = '\0';
      prev_line2[0] = '\0';
    }
  } else {
    // DENTRO DO APP
    switch (c) {
      case 'e':  // resume
        app_play(active_app);
        break;
      case 'q':  // pause
        app_suspend(active_app);
        break;
      case 'x':  // close
        app_close(active_app);
        // Forçar atualização da interface para refletir o estado APP_CLOSED
        render_status_bar_for_app(active_app);
        lcd_draw_lines(prev_line1, apps[active_app].last_status);
        break;
      case 's':  // minimize
        app_minimize(active_app);
        break;
      case 'w':  // show again
        app_show(active_app);
        break;
    }
  }
}

//// ---- FetOS Apps ---- #section
// ---- Clock App ----
typedef struct {
  unsigned long uptime_secs;  // segundos desde o início
} clock_state_t;

clock_state_t clock_internal = { 0 };

void clock_app_run() {
  app_t *a = &apps[0];  // Clock app

  // Atualiza a linha 2 com uptime
  snprintf(a->last_status, 17, "Uptime: %lus", clock_internal.uptime_secs);

  // Apenas desenha no LCD
  lcd_draw_lines(prev_line1, a->last_status);
}

void clock_app_bg() {
  unsigned long now = millis();
  clock_internal.uptime_secs = (now - system_start_ms) / 1000UL;
}

void clock_app_cleanup() {
  apps[0].last_status[0] = '\0';
}

// ---- FetOS Heart App ----
typedef struct {
  bool led_state;             // estado atual do LED
  unsigned long last_toggle;  // última vez que o LED mudou
} fetOSHeart_state_t;

fetOSHeart_state_t fetOSHeart_internal = { false, 0 };
static bool fetOSHeart_led_state = false;

void fetOSHeart_app_fg() {
  app_t *a = &apps[1];  // FetOS Heart app

  // Atualiza a linha 2 com o estado atual do LED
  snprintf(a->last_status, 17, "%s", fetOSHeart_internal.led_state ? "LED: ON" : "LED: OFF");

  // Apenas desenha no LCD
  lcd_draw_lines(prev_line1, a->last_status);
}

void fetOSHeart_app_bg() {
  app_t *a = &apps[1];
  if (a->state != APP_RUNNING) return;  // só roda quando RUNNING
  unsigned long now = millis();
  if (now - fetOSHeart_internal.last_toggle >= 500) {  // pisca a cada 500ms
    fetOSHeart_internal.last_toggle = now;
    fetOSHeart_internal.led_state = !fetOSHeart_internal.led_state;
    // Atualiza o pino físico do LED
    digitalWrite(LED_PIN, fetOSHeart_internal.led_state ? HIGH : LOW);
  }
}

void fetOSHeart_app_cleanup() {
  fetOSHeart_led_state = false;
  digitalWrite(LED_PIN, LOW);
  apps[1].last_status[0] = '\0';
}
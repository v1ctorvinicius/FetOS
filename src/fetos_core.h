/*
 * fetos_core.h
 *
 * Created: 18/10/2025 23:09:01
 *  Author: victo
 */ 


#ifndef FETOS_CORE_H_
#define FETOS_CORE_H_


#pragma once
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
	APP_RUNNING,
	APP_SUSPENDED,
	APP_CLOSED
} app_state_t;

typedef void (*app_run_t)();
typedef void (*app_bg_t)();
typedef void (*app_fg_t)();
typedef void (*app_cleanup_t)();

typedef struct {
	const char *name;
	app_state_t state;
	bool is_visible;
	app_run_t run;
	app_bg_t bg;
	app_fg_t fg;
	app_cleanup_t cleanup;
	char last_status[17];
} app_t;

extern unsigned long system_start_ms;
extern int8_t menu_index;
extern int8_t active_app;
extern int8_t visible_app;
extern app_t apps[];
extern const uint8_t APP_COUNT;

void fetos_core_init(void);
void scheduler_tick(void);
void app_play(uint8_t idx);
void app_suspend(uint8_t idx);
void app_close(uint8_t idx);
void app_minimize(uint8_t idx);
void app_show(uint8_t idx);

// synchronization between logical and visual state of apps may depend on hardware timing
// (LCD update vs scheduler tick). Ensure sequential calls avoid bus contention.



#endif /* FETOS_CORE_H_ */
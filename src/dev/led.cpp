//
// Created by root on 11/17/17.
//
#include "pp.h"
#include "led.h"

void led_set(uint16_t G, uint16_t R)
{
	// Set the brightnesss of the LED within range(0, 100).
	G = G < 100 ? G : 100;
	R = R < 100 ? R : 100;
	controller.set(CTL_LED_RED, R & 0xff);
	controller.set(CTL_LED_GREEN, G & 0xff);
}

void led_set_mode(uint8_t type, uint8_t color, uint16_t time_ms)
{
	robotbase_led_type = type;
	robotbase_led_color = color;
	robotbase_led_cnt_for_switch = time_ms / 20;
	live_led_cnt_for_switch = 0;
	robotbase_led_update_flag = true;
}


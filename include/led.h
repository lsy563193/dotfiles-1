//
// Created by root on 11/17/17.
//

#ifndef PP_LED_H
#define PP_LED_H


class Led {

};


#endif //PP_LED_H

void led_set(uint16_t G, uint16_t R);
// time_ms is used for both LED_FLASH type and LED_BREATH type, the default value is for LED_BREATH.
void led_set_mode(uint8_t type, uint8_t color, uint16_t time_ms = 3000);

//
// Created by lsy563193 on 12/4/17.
//

#include <state.hpp>
#include <action.hpp>
#include <mode.hpp>

#include "key_led.h"

void StateExploration::init() {
	led.set_mode(LED_STEADY, LED_ORANGE);
}



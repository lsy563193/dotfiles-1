//
// Created by lsy563193 on 12/4/17.
//
#include "pp.h"
#include "arch.hpp"

void StateResumeLowBatteryCharge::init() {
	led.set_mode(LED_STEADY, LED_GREEN);
}

//bool StateTrapped::isFinish() {
//	return false;
//}


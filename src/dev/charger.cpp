//
// Created by root on 11/20/17.
//
#include "pp.h"
#include "charger.h"
#include "serial.h"

Charger charger;
/*-------------------------------Check if at charger stub------------------------------------*/
bool Charger::isOnStub(void) {
	// 1: On charger stub and charging.
	// 2: On charger stub but not charging.
	if (status_ == 2 || status_ == 1)
		return true;
	else
		return false;
}

bool Charger::isDirected(void) {
	// 3: Direct connect to charge line but not charging.
	// 4: Direct connect to charge line and charging.
	if (status_ == 3 || status_ == 4)
		return true;
	else
		return false;
}

void Charger::setStart(void) {
	// This function will turn on the charging function.
	serial.setSendData(CTL_CHARGER, 0x01);
}

void Charger::setStop(void) {
	// Set the flag to false so that it can quit charger mode_.
	serial.setSendData(CTL_CHARGER, 0x00);
}



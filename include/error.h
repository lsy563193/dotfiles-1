//
// Created by root on 11/20/17.
//

#ifndef PP_ERROR_H
#define PP_ERROR_H

#include "stdint.h"

// Definition for error code.
enum
{
	// 0
	RTC_ALARM_ERROR,
	RTC_TIME_ERROR,
	GYRO_SERIAL_ERROR,
	OBS_VERSION_ERROR,
	SERIAL_ERROR,
	// 5
	RAM_ERROR,
	R16_FLASH_ERROR,
	M0_FLASH_ERROR,
	LIDAR_ERROR,
	LIDAR_BUMPER_ERROR,
	// 10
	BASELINE_VOLTAGE_ERROR,
	BATTERY_ERROR,
	BATTERY_LOW,
	BASELINE_CURRENT_ERROR,
	BASELINE_CURRENT_LOW,
	// 15
	GYRO_ERROR,
	LEFT_OBS_ERROR,
	FRONT_OBS_ERROR,
	RIGHT_OBS_ERROR,
	LEFT_WALL_ERROR,
	// 20
	RIGHT_WALL_ERROR,
	OBS_ENABLE_ERROR,
	LEFT_BUMPER_ERROR,
	RIGHT_BUMPER_ERROR,
	LEFT_CLIFF_ERROR,
	// 25
	FRONT_CLIFF_ERROR,
	RIGHT_CLIFF_ERROR,
	LEFT_WHEEL_SW_ERROR,
	RIGHT_WHEEL_SW_ERROR,
	BLRCON_ERROR,
	// 30
	LRCON_ERROR,
	FL2RCON_ERROR,
	FLRCON_ERROR,
	FRRCON_ERROR,
	FR2RCON_ERROR,
	// 35
	RRCON_ERROR,
	BRRCON_ERROR,
	LEFT_WHEEL_FORWARD_CURRENT_ERROR,
	LEFT_WHEEL_FORWARD_PWM_ERROR,
	LEFT_WHEEL_FORWARD_ENCODER_FAIL,
	// 40
	LEFT_WHEEL_FORWARD_ENCODER_ERROR,
	LEFT_WHEEL_BACKWARD_CURRENT_ERROR,
	LEFT_WHEEL_BACKWARD_PWM_ERROR,
	LEFT_WHEEL_BACKWARD_ENCODER_FAIL,
	LEFT_WHEEL_BACKWARD_ENCODER_ERROR,
	// 45
	LEFT_WHEEL_STALL_ERROR,
	RIGHT_WHEEL_FORWARD_CURRENT_ERROR,
	RIGHT_WHEEL_FORWARD_PWM_ERROR,
	RIGHT_WHEEL_FORWARD_ENCODER_FAIL,
	RIGHT_WHEEL_FORWARD_ENCODER_ERROR,
	// 50
	RIGHT_WHEEL_BACKWARD_CURRENT_ERROR,
	RIGHT_WHEEL_BACKWARD_PWM_ERROR,
	RIGHT_WHEEL_BACKWARD_ENCODER_FAIL,
	RIGHT_WHEEL_BACKWARD_ENCODER_ERROR,
	RIGHT_WHEEL_STALL_ERROR,
	// 55
	LEFT_BRUSH_CURRENT_ERROR,
	LEFT_BRUSH_STALL_ERROR,
	RIGHT_BRUSH_CURRENT_ERROR,
	RIGHT_BRUSH_STALL_ERROR,
	MAIN_BRUSH_CURRENT_ERROR,
	// 60
	MAIN_BRUSH_STALL_ERROR,
	VACUUM_CURRENT_ERROR,
	VACUUM_PWM_ERROR,
	VACUUM_ENCODER_FAIL,
	VACUUM_ENCODER_ERROR,
	// 65
	VACUUM_STALL_ERROR,
	CHARGE_PWM_ERROR,
	CHARGE_CURRENT_ERROR,
	SWING_MOTOR_ERROR,
	SERIAL_WIFI_ERROR,
};

typedef enum {
	ERROR_CODE_NONE,
	ERROR_CODE_LEFTWHEEL,
	ERROR_CODE_RIGHTWHEEL,
	ERROR_CODE_LEFTBRUSH,
	ERROR_CODE_RIGHTBRUSH,
	ERROR_CODE_PICKUP,
	ERROR_CODE_CLIFF,
	ERROR_CODE_BUMPER,
	ERROR_CODE_STUCK,
	ERROR_CODE_MAINBRUSH,
	ERROR_CODE_VACUUM,
	ERROR_CODE_WATERTANK,
	ERROR_CODE_BTA,
	ERROR_CODE_OBS,
	ERROR_CODE_BATTERYLOW,
	ERROR_CODE_DUSTBIN,
	ERROR_CODE_GYRO,
	ERROR_CODE_ENCODER,
	ERROR_CODE_SLAM,
	ERROR_CODE_LIDAR,
	ERROR_CODE_AIRPUMP,
	ERROR_CODE_FILTER,
	ERROR_CODE_OTHER,
	ERROR_CODE_TEST,
	ERROR_CODE_TEST_NULL
}ErrorType;



class Error {
public:
	Error()
	{
		error_code_ = ERROR_CODE_NONE;
	}
	void set(ErrorType code)
	{
		error_code_ = code;
	}

	ErrorType get()
	{
		return error_code_;
	}

	void alarm(void);

	bool clear(uint8_t code);

private:
	ErrorType error_code_;
};

extern Error error;
#endif //PP_ERROR_H

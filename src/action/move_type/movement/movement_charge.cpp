//
// Created by austin on 17-12-7.
//

#include <movement.hpp>
#include "dev.h"

MovementCharge::MovementCharge()
{
	ROS_INFO("%s %d: Start charge action. Battery voltage \033[32m%5.2f V\033[0m.", __FUNCTION__, __LINE__, (float)battery.getVoltage()/100.0);
	led.setMode(LED_BREATH, LED_ORANGE);
	wheel.stop();
	brush.stop();
	vacuum.stop();

	if(lidar.isScanOriginalReady())
	{
		lidar.motorCtrl(OFF);
		lidar.setScanOriginalReady(0);
	}

	charger.setStart();
	usleep(30000);

	show_battery_info_time_stamp_ = time(NULL);

	speaker.play(VOICE_BATTERY_CHARGE);

}

MovementCharge::~MovementCharge()
{
	wheel.stop();
	charger.setStop();
	ROS_INFO("%s %d: End movement charge.", __FUNCTION__, __LINE__);
}

bool MovementCharge::isFinish()
{

	if (battery.isFull())
		return true;

	if (!turn_for_charger_)
	{
		// Check for charger connection.
		if (charger.getChargeStatus())
			disconnect_charger_count_ = 0;
		else
			disconnect_charger_count_++;

		if (disconnect_charger_count_ > 15)
		{
			if (directly_charge_)
			{
				charger.setStop();
				return true;
			}
			else
			{
				led.setMode(LED_STEADY, LED_ORANGE);
				turn_for_charger_ = true;
				start_turning_time_stamp_ = ros::Time::now().toSec();
				turn_right_finish_ = false;
				ROS_INFO("%s %d: Start turn for charger.", __FUNCTION__, __LINE__);
			}
		}
	}

	if (turn_for_charger_)
	{
		if (charger.getChargeStatus())
			turn_for_charger_ = false;
		if (ros::Time::now().toSec() - start_turning_time_stamp_ > 3)
			return true;
		if (cliff.getStatus() == BLOCK_ALL)
			return true;
	}

	return false;
}

void MovementCharge::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
	if (turn_for_charger_)
	{
		if (ros::Time::now().toSec() - start_turning_time_stamp_ > 1)
			turn_right_finish_ = true;

		if (turn_right_finish_)
		{
			wheel.setDirectionRight();
			left_speed = right_speed = 5;
		} else
		{
			wheel.setDirectionLeft();
			left_speed = right_speed = 5;
		}
	}
	else
		left_speed = right_speed = 0;
}

void MovementCharge::run()
{
	// Debug for charge info
	if (time(NULL) - show_battery_info_time_stamp_ > 5)
	{
		ROS_INFO("%s %d: battery voltage \033[32m%5.2f V\033[0m.", __FUNCTION__, __LINE__, (float)battery.getVoltage()/100.0);
		show_battery_info_time_stamp_ = time(NULL);
	}

	IMovement::run();
}

//
// Created by lsy563193 on 11/29/17.
//

#include "dev.h"
#include "action.hpp"

//IAction::IAction(Mode* p_mode) {
//	sp_mode_ = p_mode;
//}

ActionOpenGyro::ActionOpenGyro()
{
	ROS_WARN("%s %d: Enter action open gyro.", __FUNCTION__, __LINE__);
	gyro.reOpen();
}

bool ActionOpenGyro::isFinish()
{
	if (gyro.isOn())
	{
		ROS_WARN("%s %d: Open gyro succeeded.", __FUNCTION__, __LINE__);
		gyro.setAccInitData();
		gyro.setAngleVOffset();
		gyro.setAngleROffset();
		gyro.resetKalmanParam();
		gyro.setTiltCheckingEnable(true);
		return true;
	}
	return false;
}

void ActionOpenGyro::run()
{
	wheel.setPidTargetSpeed(0, 0);
	gyro.waitForOn();
}

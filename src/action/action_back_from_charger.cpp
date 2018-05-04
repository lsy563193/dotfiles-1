//
// Created by lsy563193 on 11/29/17.
//

#include "dev.h"
#include "action.hpp"
ActionBackFromCharger::ActionBackFromCharger()
{
	ROS_INFO("%s %d, Init action back from charger.", __FUNCTION__, __LINE__);
	wheel.setDirectionBackward();
	// This time out interval is just for checking whether the switch is on.
	timeout_interval_ = 1;
};

ActionBackFromCharger::~ActionBackFromCharger()
{
	ROS_INFO("%s %d, Finish action back from charger.", __FUNCTION__, __LINE__);
}

bool ActionBackFromCharger::isFinish() {
	bool val = false;
	double back_distance = two_points_distance_double(start_point_.x, start_point_.y, odom.getOriginX(), odom.getOriginY());
	double angle_diff = fabs(odom.getRadian() - M_PI);
	double forward_distance = two_points_distance_double(start_point_.x, start_point_.y, odom.getOriginX(), odom.getOriginY());

	switch(flag_){
		case BACK:
		{
			if(back_distance > BACK_DIST)
				flag_ = TURN;
			break;
		}
		case TURN:
		{
			if(angle_diff < 0.174) //Radian(0.174) is degree(10),because robot can not stop immediately
			{
				flag_ = FORWARD;
				start_point_.x = odom.getOriginX();
				start_point_.y = odom.getOriginY();
			}
			break;
		}
		case FORWARD:
		{
			if(bumper.getStatus())
			{
				flag_ = BUMPER_BACK;
				start_point_.x = odom.getOriginX();
				start_point_.y = odom.getOriginY();
			}
			else if(forward_distance > FORWARD_DIST)
				val = true;
			break;
		}
		case BUMPER_BACK:
		{
			if(back_distance > 0.01)
				val = true;
			break;
		}
	}
	return val;
}

void ActionBackFromCharger::run() {
	switch(flag_){
		case BACK:
			wheel.setDirectionBackward();
			wheel.setPidTargetSpeed(20, 20);
			break;
		case TURN:
			wheel.setDirectionLeft();
			wheel.setPidTargetSpeed(20, 20);
			break;
		case FORWARD:
			wheel.setDirectionForward();
			wheel.setPidTargetSpeed(25, 25);
			break;
		case BUMPER_BACK:
			wheel.setDirectionBackward();
			wheel.setPidTargetSpeed(BACK_MAX_SPEED, BACK_MAX_SPEED);
			break;
	}
}

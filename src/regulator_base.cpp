//
// Created by lsy563193 on 6/28/17.
//

#include <map.h>
#include <gyro.h>
#include <movement.h>
#include <wall_follow_slam.h>
#include <robot.hpp>
#include <core_move.h>
#include "regulator_base.h"
#include "ros/ros.h"
#include <event_manager.h>

bool RegulatorBase::isStop()
{
//	ROS_INFO("reg_base isStop");
	return g_fatal_quit_event || g_key_clean_pressed  || g_oc_wheel_left || g_oc_wheel_right;
}

//FollowWallRegulator

FollowWallRegulator::FollowWallRegulator(CMMoveType type):type_(type),previous_(0){
//	ROS_INFO("FollowWallRegulator init");
};
bool FollowWallRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{
//	ROS_INFO("FollowWallRegulator adjustSpeed");
	if (get_right_wheel_step() < (uint32_t) g_straight_distance)
	{
		int32_t speed;
		if (get_left_wheel_step() < 500)
		{
			if (get_right_wheel_step() < 100) speed = 10;
			else speed = 15;
		} else
			speed = 23;
		l_speed = r_speed = speed;
//		ROS_INFO("get_right_wheel_step() < (uint32_t) g_straight_distance", get_right_wheel_step(), g_straight_distance);
	}
	else
	{

		if (get_front_obs() < get_front_obs_value())
		{
//			ROS_INFO("get_front_obs() < get_front_obs_value()", get_front_obs(), get_front_obs_value());
			auto wheel_speed_base = 15 + get_right_wheel_step() / 150;
			if (wheel_speed_base > 28)wheel_speed_base = 28;

			auto proportion = robot::instance()->getLeftWall();

			proportion = proportion * 100 / g_wall_distance;

			proportion -= 100;

			auto delta = proportion - previous_;

			if (g_wall_distance > 200)
			{//over left

				l_speed = wheel_speed_base + proportion / 8 + delta / 3; //12
				r_speed = wheel_speed_base - proportion / 9 - delta / 3; //10

				if (wheel_speed_base < 26)
				{
					if (r_speed > wheel_speed_base + 6)
					{
						r_speed = 34;
						l_speed = 4;
					} else if (l_speed > wheel_speed_base + 10)
					{
						r_speed = 5;
						l_speed = 30;
					}
				} else
				{
					if (r_speed > 35)
					{
						r_speed = 35;
						l_speed = 4;
					}
				}
			} else
			{

				l_speed = wheel_speed_base + proportion / 10 + delta / 3;//16
				r_speed = wheel_speed_base - proportion / 10 - delta / 4; //11

				if (wheel_speed_base < 26)
				{
					if (r_speed > wheel_speed_base + 4)
					{
						r_speed = 34;
						l_speed = 4;
					}
				} else
				{
					if (r_speed > 32)
					{
						r_speed = 36;
						l_speed = 4;
					}
				}
			}

			previous_ = proportion;

			if (l_speed > 39)l_speed = 39;
			if (l_speed < 0)l_speed = 0;
			if (r_speed > 35)r_speed = 35;
			if (r_speed < 5)r_speed = 5;

		}
		 else
		{
			if (get_right_wheel_step() < 2000) jam_++;
			g_turn_angle = 920;
//			reset_wheel_step();
			g_wall_distance = Wall_High_Limit;
		}
	}

	if(type_ != CM_FOLLOW_LEFT_WALL) std::swap(l_speed, r_speed);
	set_dir_forward();
	return true;
}

bool FollowWallRegulator::isReach()
{
//	ROS_INFO("FollowWallRegulator isReach");
	bool ret = false;
	auto start_y = origin_.Y;
	if (get_clean_mode() == Clean_Mode_WallFollow)
	{
		if (wf_is_end())
		{
			wf_break_wall_follow();
			ret = true;
		}
	} else if (get_clean_mode() == Clean_Mode_Navigation)
	{

		if ((start_y < target_.Y ^ map_get_y_count() < target_.Y))
		{
			ROS_WARN("Robot has reach the target.");
			ROS_WARN("%s %d:start_y(%d), target.Y(%d),curr_y(%d)", __FUNCTION__, __LINE__, start_y, target_.Y,
							 map_get_y_count());
			ret = true;
		}

		if ((target_.Y > start_y && (start_y - map_get_y_count()) > 120) ||
				(target_.Y < start_y && (map_get_y_count() - start_y) > 120))
		{
			ROS_WARN("Robot has round to the opposite direcition.");
			ROS_WARN("%s %d:start_y(%d), target.Y(%d),curr_y(%d)", __FUNCTION__, __LINE__, start_y, target_.Y,
							 map_get_y_count());
			map_set_cell(MAP, map_get_relative_x(Gyro_GetAngle(), CELL_SIZE_3, 0),
									 map_get_relative_y(Gyro_GetAngle(), CELL_SIZE_3, 0), CLEANED);
			ret = true;
		}
	}
	return ret;
}

bool FollowWallRegulator::isSwitch()
{
//	ROS_INFO("FollowWallRegulator isSwitch");
	return g_bumper_hitted || g_cliff_triggered || g_turn_angle != 0;
}

//BackRegulator
BackRegulator::BackRegulator():speed_(8),counter_(0),pos_x_(0),pos_y_(0)
{
//	ROS_INFO("BackRegulator init");
}

bool BackRegulator::isSwitch()
{
//	ROS_INFO("BackRegulator::isSwitch");
	auto distance = sqrtf(powf(pos_x_ - robot::instance()->getOdomPositionX(), 2) + powf(pos_y_ -
																																								 robot::instance()->getOdomPositionY(), 2));
	return fabsf(distance) > 0.02f;
}

bool BackRegulator::isReach()
{

//	ROS_INFO("reg_back isReach");
	return false;
}

bool BackRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{
//	ROS_INFO("reg_back adjustSpeed");
	set_dir_backward();
	speed_ += counter_ / 100;
	speed_ = (speed_ > 18) ? 18 : speed_;
	reset_wheel_step();
	l_speed = r_speed = speed_;
	return true;
}

//TurnRegulator
TurnRegulator::TurnRegulator(uint16_t speed_max):speed_max_(speed_max),target_angle_(0)
{
//	ROS_INFO("TurnRegulator init");
	accurate_ = speed_max_ > 30 ? 30 : 10;
}

bool TurnRegulator::isSwitch()
{
//	ROS_INFO("TurnRegulator::isSwitch");
	return (abs(target_angle_ - Gyro_GetAngle()) < accurate_);
}
bool TurnRegulator::isReach()
{
//	ROS_INFO("TurnRegulator::isReach");
	return false;
}

bool TurnRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{

	(g_cm_move_type == CM_FOLLOW_LEFT_WALL) ? set_dir_right() : set_dir_left();
//	ROS_INFO("TurnRegulator::adjustSpeed");
	auto speed = speed_max_;
		if (abs(target_angle_ - Gyro_GetAngle()) < 50) {
			speed = std::min((uint16_t)5, speed);
		} else if (abs(target_angle_ - Gyro_GetAngle()) < 200) {
			speed = std::min((uint16_t)10, speed);
		}
	l_speed = r_speed = speed;
	return true;
}

//RegulatorManage

RegulatorProxy::RegulatorProxy(RegulatorBase* p_reg):p_reg_(p_reg)
{
//	ROS_INFO("RegulatorProxy init");
}

bool RegulatorProxy::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
	if(p_reg_ != nullptr)
		return p_reg_->adjustSpeed(left_speed, right_speed);
	return false;
}

bool RegulatorProxy::isSwitch()
{
	if(p_reg_ != nullptr)
		return p_reg_->isSwitch();
	return false;
}
bool RegulatorProxy::isReach()
{
	if(p_reg_ != nullptr)
		return p_reg_->isReach();
	return false;
}
/*
void RegulatorProxy::switchTo()
{
	switch(getType()){
		case RegBack:
			break;
	}
}*/

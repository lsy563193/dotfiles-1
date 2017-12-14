//
// Created by lsy563193 on 11/29/17.
//

#include <arch.hpp>
#include <error.h>
#include <pp.h>
#include "dev.h"

MovementExceptionResume::MovementExceptionResume()
{
	PP_INFO();
	if (ev.bumper_jam || ev.cliff_jam || ev.lidar_stuck)
	{
		// Save current position for moving back detection.
		s_pos_x = odom.getX();
		s_pos_y = odom.getY();
	}

	wheel_current_sum_ = 0;
	wheel_current_sum_cnt_ = 0;
	wheel_resume_cnt_ = 0;
	resume_wheel_start_time_ = ros::Time::now().toSec();
	bumper_jam_state_ = 1;

}

MovementExceptionResume::~MovementExceptionResume()
{

}

void MovementExceptionResume::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{
	l_speed = r_speed = 0;
}

bool MovementExceptionResume::isFinish()
{
	if (!(ev.bumper_jam || ev.cliff_jam || ev.oc_wheel_left || ev.oc_wheel_right || ev.oc_suction || ev.lidar_stuck))
	{
		PP_INFO();
		return true;
	}

	// Check for right wheel.
	if (ev.oc_wheel_left || ev.oc_wheel_right)
	{
		if (ros::Time::now().toSec() - resume_wheel_start_time_ >= 1)
		{
			wheel_current_sum_ /= wheel_current_sum_cnt_;
			if (wheel_current_sum_ > Wheel_Stall_Limit)
			{
				if (wheel_resume_cnt_ >= 3)
				{
					wheel.stop();
					brush.stop();
					vacuum.stop();
					if (ev.oc_wheel_left)
					{
						ROS_WARN("%s,%d Left wheel stall maybe, please check!!\n", __FUNCTION__, __LINE__);
						error.set(ERROR_CODE_LEFTWHEEL);
					} else
					{
						ROS_WARN("%s,%d Right wheel stall maybe, please check!!\n", __FUNCTION__, __LINE__);
						error.set(ERROR_CODE_RIGHTWHEEL);
					}
					ev.fatal_quit = true;
					return true;
				}
				else
				{
					resume_wheel_start_time_ = time(NULL);
					wheel_resume_cnt_++;
					wheel_current_sum_ = 0;
					wheel_current_sum_cnt_ = 0;
					ROS_WARN("%s %d: Failed to resume for %d times.", __FUNCTION__, __LINE__, wheel_resume_cnt_);
				}
			}
			else
			{
				if (ev.oc_wheel_left)
				{
					ROS_WARN("%s %d: Left wheel resume succeeded.", __FUNCTION__, __LINE__);
					ev.oc_wheel_left = false;
					vacuum.setMode(Vac_Save);
					brush.setSidePwm(50, 50);
					brush.setMainPwm(30);
				} else
				{
					ROS_WARN("%s %d: Left wheel resume succeeded.", __FUNCTION__, __LINE__);
					ev.oc_wheel_right = false;
					vacuum.setMode(Vac_Save);
					brush.setSidePwm(50, 50);
					brush.setMainPwm(30);
				}
			}
		}
		else
		{
			if (ev.oc_wheel_left)
				wheel_current_sum_ += (uint32_t) wheel.getLeftWheelCurrent();
			else
				wheel_current_sum_ += (uint32_t) wheel.getRightWheelCurrent();
			wheel_current_sum_cnt_++;
		}
	}
	else if (ev.bumper_jam)
	{
		if (!bumper.get_status())
		{
			ROS_WARN("%s %d: Bumper resume succeeded.", __FUNCTION__, __LINE__);
			ev.bumper_jam = false;
			ev.bumper_triggered = 0;
			g_bumper_cnt = 0;
		}

		switch (bumper_jam_state_)
		{
			case 1: // Move back for the first time.
			case 2: // Move back for the second time.
			case 3: // Move back for the third time.
			{
				float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getX(), odom.getY());
				if (fabsf(distance) > 0.05f)
				{
					wheel.stop();
					// If cliff jam during bumper self resume.
					if (cliff.get_status() && ++g_cliff_cnt > 2)
					{
						ev.cliff_jam = true;
						wheel_resume_cnt_ = 0;
					} else
					{
						bumper_jam_state_++;
						ROS_WARN("%s %d: Try bumper resume state %d.", __FUNCTION__, __LINE__, bumper_jam_state_);
						if (bumper_jam_state_ == 4)
							resume_wheel_start_time_ = ros::Time::now().toSec();
					}
					s_pos_x = odom.getX();
					s_pos_y = odom.getY();
				}
				break;
			}
			case 4:
			case 5:
			{
				ROS_DEBUG("%s %d: robot::instance()->getPoseAngle(): %d", __FUNCTION__, __LINE__,
						  robot::instance()->getPoseAngle());
				// If cliff jam during bumper self resume.
				if (cliff.get_status() && ++g_cliff_cnt > 2)
				{
					ev.cliff_jam = true;
					wheel_resume_cnt_ = 0;
				}
				else if (ros::Time::now().toSec() - resume_wheel_start_time_ >= 2)
				{
					bumper_jam_state_++;
					resume_wheel_start_time_ = ros::Time::now().toSec();
					ROS_WARN("%s %d: Try bumper resume state %d.", __FUNCTION__, __LINE__, bumper_jam_state_);
				}
				break;
			}
			default: //case 6:
			{
				ROS_WARN("%s %d: Bumper jamed.", __FUNCTION__, __LINE__);
				ev.fatal_quit = true;
				error.set(ERROR_CODE_BUMPER);
				break;
			}
		}
	}

	ROS_INFO("%s %d: fatal:%d", __FUNCTION__, __LINE__, ev.fatal_quit);
	return ev.fatal_quit;
}

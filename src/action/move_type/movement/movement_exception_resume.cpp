//
// Created by lsy563193 on 11/29/17.
//

#include <movement.hpp>
#include <move_type.hpp>
#include <error.h>
#include <event_manager.h>
#include <robot.hpp>
#include <mode.hpp>
#include "dev.h"

double MovementExceptionResume::slip_start_turn_time_ = 0;
bool MovementExceptionResume::is_slip_last_turn_left_ = false;
MovementExceptionResume::MovementExceptionResume()
{
	ROS_INFO("%s %d: Entering movement exception resume.", __FUNCTION__, __LINE__);

	// Save current position for moving back detection.
	s_pos_x = odom.getOriginX();
	s_pos_y = odom.getOriginY();

	//For slip
	if(ros::Time::now().toSec() - slip_start_turn_time_ < 5){
		robot_slip_flag_ = static_cast<uint8_t>(is_slip_last_turn_left_ ? 2 : 1);
		slip_start_turn_time_ = ros::Time::now().toSec();
	}else{
		robot_slip_flag_ = 0;
		slip_start_turn_time_ = 0;
	}

	resume_wheel_start_time_ = ros::Time::now().toSec();
	resume_main_bursh_start_time_ = ros::Time::now().toSec();
	resume_vacuum_start_time_ = ros::Time::now().toSec();
	resume_slip_start_time_ = ros::Time::now().toSec();
}

MovementExceptionResume::~MovementExceptionResume()
{
	ROS_INFO("%s %d: Exiting movement exception resume.", __FUNCTION__, __LINE__);
}

void MovementExceptionResume::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
	if (ev.oc_vacuum)
		left_speed = right_speed = 0;
	else if (ev.oc_wheel_left || ev.oc_wheel_right)
	{
		if (ev.oc_wheel_right)
			wheel.setDirectionRight();
		else
			wheel.setDirectionLeft();
		left_speed = 30;
		right_speed = 30;
	}
	else if (ev.robot_stuck || ev.cliff_jam || ev.cliff_all_triggered )
	{
		wheel.setDirectionBackward();
		left_speed = right_speed = RUN_TOP_SPEED;
	}
	else if(ev.oc_brush_main)
	{
		if(main_brush_resume_state_ == 1){
			wheel.setDirectionBackward();
			left_speed = right_speed = BACK_MAX_SPEED;
		}
		else
			left_speed = right_speed = 0;
	}

	else if (ev.bumper_jam)
	{
		switch (bumper_jam_state_)
		{
			case 1:
			case 2:
			case 3:
			{
				// Quickly move back for a distance.
				wheel.setDirectionBackward();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 4:
			{
				// Quickly turn right for 90 degrees.
				wheel.setDirectionRight();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 5:
			{
				// Quickly turn left for 180 degrees.
				wheel.setDirectionLeft();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
		}
	}
	else if (ev.lidar_stuck)
	{
		wheel.setDirectionBackward();
		left_speed = right_speed = 2;
	}
	else if (ev.robot_slip)
	{
		switch(robot_slip_flag_){
			case 0:
			{
				wheel.setDirectionBackward();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 1:
			{
				wheel.setDirectionRight();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 2:
			{
				wheel.setDirectionLeft();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
		}
	}
}

bool MovementExceptionResume::isFinish()
{
	updatePosition();
	if (!(ev.bumper_jam || ev.cliff_jam || ev.cliff_all_triggered || ev.oc_wheel_left || ev.oc_wheel_right
		  || ev.oc_vacuum || ev.lidar_stuck || ev.robot_stuck || ev.oc_brush_main || ev.robot_slip))
	{
		ROS_INFO("%s %d: All exception cleared.", __FUNCTION__, __LINE__);
		return true;
	}

	// Check for right wheel.
	if (ev.oc_wheel_left || ev.oc_wheel_right)
	{
		if (ros::Time::now().toSec() - resume_wheel_start_time_ >= 1)
		{
			if (wheel.getLeftWheelOc() || wheel.getRightWheelOc())
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
					ROS_WARN("%s %d: Failed to resume for %d times.", __FUNCTION__, __LINE__, wheel_resume_cnt_);
				}
			}
			else
			{
				if (ev.oc_wheel_left)
				{
					ROS_WARN("%s %d: Left wheel resume succeeded.", __FUNCTION__, __LINE__);
					ev.oc_wheel_left = false;
					if (!water_tank.checkEquipment(true))
						vacuum.setCleanState();
//					brush.normalOperate();

				} else
				{
					ROS_WARN("%s %d: Right wheel resume succeeded.", __FUNCTION__, __LINE__);
					ev.oc_wheel_right = false;
					if (!water_tank.checkEquipment(true))
						vacuum.setCleanState();
//					brush.normalOperate();
				}
			}
		}
	}
	else if (ev.oc_brush_main)
	{
		if (oc_main_brush_cnt_ < 1)
		{
			switch (main_brush_resume_state_)
			{
				case 1:
				{
					if (brush.isOn())
						brush.stop();
					float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getOriginX(), odom.getOriginY());
					if (std::abs(distance) >= CELL_SIZE * 2)
					{
						ROS_INFO("%s %d: Move back finish!", __FUNCTION__, __LINE__);
						brush.mainBrushResume();
						main_brush_resume_state_++;
						resume_main_bursh_start_time_ = ros::Time::now().toSec();
					}
					break;
				}
				case 2:
				{
					if ((ros::Time::now().toSec() - resume_main_bursh_start_time_) >= 1 && !brush.getMainOc())
					{
						ROS_INFO("%s %d: main brush over current resume succeeded!", __FUNCTION__, __LINE__);
						brush.normalOperate();
						ev.oc_brush_main = false;
					}
					else if ((ros::Time::now().toSec() - resume_main_bursh_start_time_) >= 3)
					{
						oc_main_brush_cnt_++;
						main_brush_resume_state_ = 1;
					}
					break;
				}
				default:
					main_brush_resume_state_ = 1;
					break;
			}
		}
		else
		{
			ROS_WARN("%s %d: Main brush stuck.", __FUNCTION__, __LINE__);
			ev.oc_brush_main = false;
			ev.fatal_quit = true;
			error.set(ERROR_CODE_MAINBRUSH);
		}
	}
	else if (ev.robot_stuck)
	{
		if (!lidar.isRobotSlip())
		{
			ROS_INFO("%s %d: Cliff resume succeeded.", __FUNCTION__, __LINE__);
			ev.robot_slip = false;
			ev.robot_stuck = false;
		}
		else if (robot_stuck_resume_cnt_ < 5)
		{
			float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getOriginX(), odom.getOriginY());
			if (std::abs(distance) > 0.05f)
			{
				wheel.stop();
				robot_stuck_resume_cnt_++;
				ROS_WARN("%s %d: Try robot stuck resume for the %d time.", __FUNCTION__, __LINE__, robot_stuck_resume_cnt_);
				s_pos_x = odom.getOriginX();
				s_pos_y = odom.getOriginY();
			}
		}
		else
		{
			ROS_WARN("%s %d: Robot stuck.", __FUNCTION__, __LINE__);
			ev.fatal_quit = true;
			error.set(ERROR_CODE_STUCK);
		}
	}
	else if (ev.cliff_all_triggered)
	{
		if (cliff.getStatus() != BLOCK_ALL)
		{
			ROS_INFO("%s %d: Cliff all resume succeeded.", __FUNCTION__, __LINE__);
			ev.cliff_all_triggered = false;
			ev.cliff_triggered = 0;
			g_cliff_cnt = 0;
		}
		else if (cliff_all_resume_cnt_ < 2)
		{
			float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getOriginX(), odom.getOriginY());
			if (std::abs(distance) > 0.02f)
			{
				wheel.stop();
				cliff_all_resume_cnt_++;
				if (cliff_all_resume_cnt_ < 2)
					ROS_WARN("%s %d: Resume failed, try cliff all resume for the %d time.",
							 __FUNCTION__, __LINE__, cliff_all_resume_cnt_ + 1);
				s_pos_x = odom.getOriginX();
				s_pos_y = odom.getOriginY();
			}
		}
		else
		{
			ROS_WARN("%s %d: Cliff all triggered.", __FUNCTION__, __LINE__);
			ev.fatal_quit = true;
		}
	}
	else if (ev.cliff_jam)
	{
		if (!cliff.getStatus())
		{
			ROS_INFO("%s %d: Cliff resume succeeded.", __FUNCTION__, __LINE__);
			ev.cliff_jam = false;
			ev.cliff_triggered = 0;
			g_cliff_cnt = 0;
		}
		else if (cliff_resume_cnt_ < 5)
		{
			float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getOriginX(), odom.getOriginY());
			if (std::abs(distance) > 0.02f)
			{
				wheel.stop();
				cliff_resume_cnt_++;
				if (cliff_resume_cnt_ < 5)
					ROS_WARN("%s %d: Resume failed, try cliff resume for the %d time.",
							 __FUNCTION__, __LINE__, cliff_resume_cnt_ + 1);
				s_pos_x = odom.getOriginX();
				s_pos_y = odom.getOriginY();
			}
		}
		else
		{
			ROS_WARN("%s %d: Cliff jamed.", __FUNCTION__, __LINE__);
			ev.fatal_quit = true;
			error.set(ERROR_CODE_CLIFF);
		}
	}
	else if (ev.bumper_jam)
	{
		if (!bumper.getStatus())
		{
			ROS_INFO("%s %d: Bumper resume succeeded.", __FUNCTION__, __LINE__);
			ev.bumper_jam = false;
			ev.bumper_triggered = 0;
			g_bumper_cnt = 0;
		}
		else
		{
			switch (bumper_jam_state_)
			{
				case 1: // Move back for the first time.
				case 2: // Move back for the second time.
				case 3: // Move back for the third time.
				{
					float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getOriginX(), odom.getOriginY());
					if (std::abs(distance) > 0.05f)
					{
						wheel.stop();
						// If cliff jam during bumper self resume.
						if (cliff.getStatus() && ++g_cliff_cnt > 2)
						{
							ROS_WARN("%s %d: Triggered cliff jam during resuming bumper.", __FUNCTION__, __LINE__);
							ev.cliff_jam = true;
							bumper_jam_state_ = 1;
							wheel_resume_cnt_ = 0;
							g_cliff_cnt = 0;
						} else
						{
							bumper_jam_state_++;
							ROS_WARN("%s %d: Try bumper resume state %d.", __FUNCTION__, __LINE__, bumper_jam_state_);
							if (bumper_jam_state_ == 4)
								bumper_resume_start_radian_ = odom.getRadian();
						}
						s_pos_x = odom.getOriginX();
						s_pos_y = odom.getOriginY();
					}
					break;
				}
				case 4:
				case 5:
				{
//					ROS_DEBUG("%s %d: robot::instance()->getWorldPoseRadian(): %d", __FUNCTION__, __LINE__,
//							  robot::instance()->getWorldPoseRadian());
					// If cliff jam during bumper self resume.
					if (cliff.getStatus() && ++g_cliff_cnt > 2)
					{
						ROS_WARN("%s %d: Triggered cliff jam during resuming bumper.", __FUNCTION__, __LINE__);
						ev.cliff_jam = true;
						bumper_jam_state_ = 1;
						wheel_resume_cnt_ = 0;
						g_cliff_cnt = 0;
					} else if (fabs(ranged_radian(odom.getRadian() - bumper_resume_start_radian_)) > degree_to_radian(90))
					{
						bumper_jam_state_++;
						bumper_resume_start_radian_ = odom.getRadian();
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
	}
	else if (ev.oc_vacuum)
	{
		if (!vacuum.getOc())
		{
			ROS_INFO("%s %d: Vacuum over current resume succeeded!", __FUNCTION__, __LINE__);
			brush.normalOperate();
			if (!water_tank.checkEquipment(true))
				vacuum.setCleanState();
			vacuum.resetExceptionResume();
			ev.oc_vacuum = false;
		}
		else if (ros::Time::now().toSec() - resume_vacuum_start_time_ > 10)
		{
			ROS_WARN("%s %d: Vacuum resume failed..", __FUNCTION__, __LINE__);
			ev.oc_vacuum = false;
			ev.fatal_quit = true;
			vacuum.resetExceptionResume();
			error.set(ERROR_CODE_VACUUM);
		}
		else if (oc_vacuum_resume_cnt_ == 0)
		{
			brush.stop();
			vacuum.stop();
			vacuum.startExceptionResume();
			oc_vacuum_resume_cnt_++;
		}
	}
	else if(ev.robot_slip)
	{
		ACleanMode* p_mode = dynamic_cast<ACleanMode*>(sp_mt_->sp_mode_);
		auto isExitSlipBlock = p_mode->clean_map_.getCell(CLEAN_MAP,getPosition().toCell().x,getPosition().toCell().y);

		if(ros::Time::now().toSec() - resume_slip_start_time_ > 60){
			ev.robot_slip = false;
			ev.fatal_quit = true;
			error.set(ERROR_CODE_STUCK);

		}
		switch(robot_slip_flag_){
			case 0:{
				float distance = two_points_distance_double(s_pos_x, s_pos_y, odom.getX(), odom.getY());
				if ((isExitSlipBlock != BLOCKED_SLIP && std::abs(distance) > 0.15f) || lidar.getObstacleDistance(1, ROBOT_RADIUS) < 0.06)
				{
					if(!lidar.isRobotSlip())
					{
						ev.robot_slip = false;
						slip_start_turn_time_ = ros::Time::now().toSec();//in this place,slip_start_turn_time_ record the slip end time
					}
					else{
						robot_slip_flag_ = static_cast<uint8_t>(is_slip_last_turn_left_ ? 2 : 1);
						slip_start_turn_time_ = ros::Time::now().toSec();
					}
				}
				break;
			}
			case 1:{
				if(ros::Time::now().toSec() - slip_start_turn_time_ > 1) {
					s_pos_x = odom.getOriginX();
					s_pos_y = odom.getOriginY();
					is_slip_last_turn_left_ = true;
					robot_slip_flag_ = 0;
				}
				break;
			}
			case 2:{
				if(ros::Time::now().toSec() - slip_start_turn_time_ > 1)
				{
					s_pos_x = odom.getOriginX();
					s_pos_y = odom.getOriginY();
					is_slip_last_turn_left_ = false;
					robot_slip_flag_ = 0;
				}
				break;
			}
		}
	}

//	if (ev.fatal_quit)
//		ROS_INFO("%s %d: ev.fatal_quit is set to %d.", __FUNCTION__, __LINE__, ev.fatal_quit);
	return ev.fatal_quit;
}


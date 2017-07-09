//
// Created by lsy563193 on 6/28/17.
//

#include <map.h>
#include <gyro.h>
#include <movement.h>
#include <wall_follow_slam.h>
#include <robot.hpp>
#include <core_move.h>
#include "regulator.h"
#include "ros/ros.h"
#include <event_manager.h>
#include <mathematics.h>
#include <motion_manage.h>
#include <wav.h>
#include <spot.h>

Point32_t RegulatorBase::s_target = {0,0};
Point32_t RegulatorBase::s_origin = {0,0};


bool RegulatorBase::isStop()
{
//	ROS_INFO("reg_base isStop");
	return g_battery_home || !g_go_home && g_remote_home || cm_should_self_check();
}


BackRegulator::BackRegulator() : speed_(8), counter_(0), pos_x_(0), pos_y_(0)
{
//	ROS_WARN("%s, %d: ", __FUNCTION__, __LINE__);
}

bool BackRegulator::isReach()
{
	auto distance = sqrtf(powf(pos_x_ - robot::instance()->getOdomPositionX(), 2) +
			   	powf(pos_y_ - robot::instance()->getOdomPositionY(), 2));
	if(fabsf(distance) > 0.02f){
//		ROS_WARN("%s, %d: BackRegulator ");
		if(get_bumper_status()!= 0) {
			g_bumper_cnt++ ;
			if(g_bumper_cnt >=2) g_bumper_jam=true;
		}else g_bumper_cnt = 0;

		if(get_cliff_trig() != 0)
		{
			g_cliff_cnt++;
			if(g_cliff_cnt >=2) g_cliff_jam=true;
		}else g_cliff_cnt = 0;

		if(g_bumper_cnt == 0 && g_cliff_cnt == 0){
			g_bumper_hitted = g_cliff_triggered = 0;// node  can reset hear, switchToNext do not need this two flag
			return true;
		}
		else
			setOrigin();
	}
	return false;
}

bool BackRegulator::isSwitch()
{
//	ROS_INFO("BackRegulator::isSwitch");
	if(mt_is_fallwall())
		return isReach();
	if(mt_is_linear())
		return false;

	return false;
}

bool BackRegulator::isStop()
{
	return false;
}

void BackRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{
//	ROS_INFO("BackRegulator::adjustSpeed");
	set_dir_backward();
	speed_ += counter_ / 100;
	speed_ = (speed_ > 18) ? 18 : speed_;
	reset_wheel_step();
	l_speed = r_speed = speed_;
}


TurnRegulator::TurnRegulator() : speed_max_(13)
{
//	ROS_WARN("%s, %d: ", __FUNCTION__, __LINE__);
	accurate_ = speed_max_ > 30 ? 30 : 10;
	target_angle_ = calc_target(g_turn_angle);
}

bool TurnRegulator::isReach()
{
	if (abs(target_angle_ - Gyro_GetAngle()) < accurate_){
		ROS_WARN("%s, %d: TurnRegulator target,curr (%d,%d)", __FUNCTION__, __LINE__,target_angle_, Gyro_GetAngle());
		g_obs_triggered = g_rcon_triggered = 0; //should clear when turn
		return true;
	}

	return false;
}

bool TurnRegulator::isSwitch()
{
//	ROS_INFO("TurnRegulator::isSwitch");
	if(isReach() || g_bumper_hitted || g_cliff_triggered)
	{
		reset_sp_turn_count();
		reset_wheel_step();
		return true;
	}
	return false;
}

bool TurnRegulator::isStop()
{
	return false;
}

void TurnRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{

	auto diff = ranged_angle(target_angle_ - Gyro_GetAngle());
//	ROS_WARN("TurnRegulator::adjustSpeed diff(%d)", diff);
	if(mt_is_fallwall())
		(mt_is_left()) ? set_dir_right() : set_dir_left();
	else
	{
		if ((diff >= 0) && (diff <= 1800))
			set_dir_left();
		else if ((diff <= 0) && (diff >= (-1800)))
			set_dir_right();
	}

//	ROS_INFO("TurnRegulator::adjustSpeed");
	auto speed = speed_max_;

	if (std::abs(diff) < 50)
	{
		speed = std::min((uint16_t) 5, speed);
	} else if (std::abs(diff) < 200)
	{
		speed = std::min((uint16_t) 10, speed);
	}
	l_speed = r_speed = speed;
}


bool TurnSpeedRegulator::adjustSpeed(int16_t diff, uint8_t& speed)
{
	if ((diff >= 0) && (diff <= 1800))
		set_dir_left();
	else if ((diff <= 0) && (diff >= (-1800)))
		set_dir_right();

	tick_++;
	if (tick_ > 2)
	{
		tick_ = 0;
		if (std::abs(diff) > 350){
			speed_ = std::min(++speed_, speed_max_);
		}
		else{
			--speed_;
			speed_ = std::max(--speed_, speed_min_);
		}
	}
	speed = speed_;
	return true;
}


LinearRegulator::LinearRegulator():
				speed_max_(40),integrated_(0),base_speed_(BASE_SPEED),integration_cycle_(0),tick_(0),turn_speed_(4)
{
	g_should_follow_wall = false;
	g_turn_angle = uranged_angle(course2dest(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y) - Gyro_GetAngle());
	ROS_WARN("%s %d: angle(%d),curr(%d,%d),targ(%d,%d) ", __FUNCTION__, __LINE__, g_turn_angle, map_get_x_count(),map_get_y_count(), s_target.X,s_target.Y);
	ROS_ERROR("angle:%d(%d,%d) ", g_turn_angle, course2dest(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y),Gyro_GetAngle());
}

bool LinearRegulator::isReach()
{
	if (std::abs(map_get_x_count() - s_target.X) < 150 && std::abs(map_get_y_count() - s_target.Y) < 150)
	{
		ROS_INFO("%s, %d: LinearRegulator.", __FUNCTION__, __LINE__);
		return true;
	}

	return false;
}

bool LinearRegulator::isSwitch()
{
	if (g_bumper_hitted || g_cliff_triggered){
			ROS_WARN("%s, %d:LinearRegulator g_bumper_hitted || g_cliff_triggered.", __FUNCTION__, __LINE__);
		return true;
	}

	return false;
}

bool LinearRegulator::isStop()
{
	if (g_obs_triggered || g_rcon_triggered)
	{
		ROS_WARN("%s, %d: g_obs_triggered || g_rcon_triggered.", __FUNCTION__, __LINE__);
		g_should_follow_wall = true;
		SpotType spt = SpotMovement::instance()->getSpotType();
		if (spt == CLEAN_SPOT || spt == NORMAL_SPOT)
			SpotMovement::instance()->setDirectChange();
		return true;
	}

	if (is_map_front_block(2))
	{
		ROS_WARN("%s, %d: Blocked boundary.", __FUNCTION__, __LINE__);
		return true;
	}

	auto diff = ranged_angle(course2dest(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y) - Gyro_GetAngle());
	if ( std::abs(diff) > 300)
	{
		ROS_WARN("%s %d: warning: angle is too big, angle: %d", __FUNCTION__, __LINE__, diff);
		return true;
	}

	if(g_remote_spot)
	{
		ROS_WARN("%s, %d: g_remote_spot.", __FUNCTION__, __LINE__);
		return true;
	}

	return false;
}

void LinearRegulator::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
//	ROS_INFO("BackRegulator::adjustSpeed");
	set_dir_forward();

	auto diff = ranged_angle(course2dest(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y) - Gyro_GetAngle());

//	ROS_WARN("%s %d: angle %d(%d,%d) ", __FUNCTION__, __LINE__, diff,course2dest(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y),Gyro_GetAngle());
	if (integration_cycle_++ > 10)
	{
		integration_cycle_ = 0;
		integrated_ += diff;
		check_limit(integrated_, -150, 150);
	}

	auto distance = TwoPointsDistance(map_get_x_count(), map_get_y_count(), s_target.X, s_target.Y);
	auto obstcal_detected = MotionManage::s_laser->laserObstcalDetected(0.2, 0, -1.0);

	if (get_obs_status() || is_obs_near() || (distance < SLOW_DOWN_DISTANCE) || is_map_front_block(3) || obstcal_detected)
	{
		integrated_ = 0;
		diff = 0;
		base_speed_ -= 1;
		base_speed_ = base_speed_ < BASE_SPEED ? BASE_SPEED : base_speed_;
	} else
	if (base_speed_ < (int32_t) speed_max_)
	{
		if (tick_++ > 5)
		{
			tick_ = 0;
			base_speed_++;
		}
		integrated_ = 0;
	}

	left_speed = base_speed_ - diff / 20 - integrated_ / 150; // - Delta / 20; // - Delta * 10 ; // - integrated_ / 2500;
	right_speed = base_speed_ + diff / 20 + integrated_ / 150; // + Delta / 20;// + Delta * 10 ; // + integrated_ / 2500;

	check_limit(left_speed, BASE_SPEED, speed_max_);
	check_limit(right_speed, BASE_SPEED, speed_max_);
	base_speed_ = (left_speed + right_speed) / 2;
//	ROS_ERROR("left_speed(%d),right_speed(%d), base_speed_(%d), slow_down(%d),diff(%d)",left_speed, right_speed, base_speed_, is_map_front_block(3),diff);
}


FollowWallRegulator::FollowWallRegulator() : previous_(0)
{
	g_wall_distance = 400;
	g_straight_distance = 300;
	ROS_WARN("%s, %d: ", __FUNCTION__, __LINE__);
}

bool FollowWallRegulator::isReach()
{
//	ROS_INFO("FollowWallRegulator isReach");
//	ROS_INFO("target_(%d,%d)",s_target.X,s_target.Y);
	bool ret = false;
	auto start_y = s_origin.Y;
	if (get_clean_mode() == Clean_Mode_WallFollow)
	{
		if (wf_is_end())
		{
			//wf_break_wall_follow();
			ret = true;
		}
	} else if (get_clean_mode() == Clean_Mode_Navigation)
	{

		extern int g_trapped_mode;
		if (g_trapped_mode != 0)
		{
			extern uint32_t g_escape_trapped_timer;
			if (g_trapped_mode == 2 || (time(NULL) - g_escape_trapped_timer) > ESCAPE_TRAPPED_TIME)
			{
//				wav_play(WAV_CLEANING_START);
//				g_trapped_mode = 0;
				ret = true;
			}
		} else
		{
			if ((start_y < s_target.Y ^ map_get_y_count() < s_target.Y))
			{
				ROS_WARN("%s, %d: BackRegulator ");
				ROS_WARN("Robot has reach the target.");
				ROS_WARN("%s %d:start_y(%d), target.Y(%d),curr_y(%d)", __FUNCTION__, __LINE__, start_y, s_target.Y,
								 map_get_y_count());
//			if(s_origin.X == map_get_x_count() && s_origin.Y == map_get_y_count()){
//				ROS_WARN("direcition is wrong, swap");
//				extern uint16_t g_last_dir;
//				g_last_dir = (g_last_dir == POS_X) ? NEG_X : POS_X;
//			}
				ret = true;
			}

			if ((s_target.Y > start_y && (start_y - map_get_y_count()) > 120) ||
					(s_target.Y < start_y && (map_get_y_count() - start_y) > 120))
			{
				ROS_WARN("%s, %d: BackRegulator ");
				ROS_WARN("Robot has round to the opposite direcition.");
				ROS_WARN("%s %d:start_y(%d), target.Y(%d),curr_y(%d)", __FUNCTION__, __LINE__, start_y, s_target.Y,
								 map_get_y_count());
				map_set_cell(MAP, map_get_relative_x(Gyro_GetAngle(), CELL_SIZE_3, 0),
										 map_get_relative_y(Gyro_GetAngle(), CELL_SIZE_3, 0), CLEANED);

//			if(s_origin.X == map_get_x_count() && s_origin.Y == map_get_y_count()){
//				ROS_WARN("direcition is wrong, swap");
//				extern uint16_t g_last_dir;
//				g_last_dir = (g_last_dir == POS_X) ? NEG_X : POS_X;
//			}
				ret = true;
			}
		}
	}
	return ret;
}

bool FollowWallRegulator::isSwitch()
{
//	ROS_INFO("FollowWallRegulator isSwitch");
	return g_bumper_hitted || g_cliff_triggered || g_turn_angle != 0;
}

bool FollowWallRegulator::isStop()
{
//	ROS_INFO("FollowWallRegulator isSwitch");
	return false;
}

void FollowWallRegulator::adjustSpeed(int32_t &l_speed, int32_t &r_speed)
{
	ROS_INFO("BackRegulator::adjustSpeed");
//	uint32_t l_step = (get_right_wheel_step() / 100) * 11 ;
	auto _l_step = get_left_wheel_step();
	auto _r_step = get_right_wheel_step();
	auto &l_step = (mt_is_left()) ? _l_step :_r_step ;
	auto &r_step = (mt_is_left()) ? _r_step :_l_step ;
//	ROS_INFO("FollowWallRegulator adjustSpeed");
//	ROS_INFO("l_step: %d < g_straight_distance : %d", l_step, g_straight_distance);
	if ((l_step) < (uint32_t) g_straight_distance)
	{
		int32_t speed;
		if (l_step < 500)
		{
			if (l_step < 100) speed = 10;
			else speed = 15;
		} else
			speed = 23;
		l_speed = r_speed = speed;
//		ROS_WARN("l_step: %d < g_straight_distance : %d", l_step, g_straight_distance);
//		ROS_INFO("Wf_1, speed = %d, g_wall_distance = %d", speed, g_wall_distance, g_wall_distance);
	} else
	{

		if (get_front_obs() < get_front_obs_value())
		{
//			ROS_INFO("get_front_obs() < get_front_obs_value()", get_front_obs(), get_front_obs_value());
			auto wheel_speed_base = 15 + r_step / 150;
			if (wheel_speed_base > 28)wheel_speed_base = 28;

			auto proportion = (mt_is_left()) ? robot::instance()->getLeftWall() : robot::instance()->getRightWall();

			proportion = proportion * 100 / g_wall_distance;

			proportion -= 100;

			auto delta = proportion - previous_;

			if (g_wall_distance > 200)
			{//over left

				l_speed = wheel_speed_base + proportion / 8 + delta / 3; //12
				r_speed = wheel_speed_base - proportion / 9 - delta / 3; //10

				if (wheel_speed_base < 26)
				{
					reset_sp_turn_count();
					if (r_speed > wheel_speed_base + 6)
					{
						r_speed = 34;
						l_speed = 4;
//						ROS_INFO("Wf_2, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
					} else if (l_speed > wheel_speed_base + 10)
					{
						r_speed = 5;
						l_speed = 30;
//						ROS_INFO("Wf_3, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
					}
				} else
				{
					if (r_speed > 35)
					{
						add_sp_turn_count();
						r_speed = 35;
						l_speed = 4;
//						ROS_INFO("Wf_4, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
//						ROS_WARN("get_sp_turn_count() = %d", get_sp_turn_count());
					} else {
						reset_sp_turn_count();
					}
				}
			} else
			{

				l_speed = wheel_speed_base + proportion / 10 + delta / 3;//16
				r_speed = wheel_speed_base - proportion / 10 - delta / 4; //11

				if (wheel_speed_base < 26)
				{
					reset_sp_turn_count();
					if (r_speed > wheel_speed_base + 4)
					{
						r_speed = 34;
						l_speed = 4;
//						ROS_INFO("Wf_5, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
					}
				} else
				{
					if (r_speed > 32)
					{
						add_sp_turn_count();
						r_speed = 36;
						l_speed = 4;
//						ROS_INFO("Wf_6, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
//						ROS_WARN("g_sp_turn_count() = %d",get_sp_turn_count());
					} else {
						reset_sp_turn_count();
					}
				}
			}

			previous_ = proportion;

			if (l_speed > 39)l_speed = 39;
			if (l_speed < 0)l_speed = 0;
			if (r_speed > 35)r_speed = 35;
			if (r_speed < 5)r_speed = 5;
//			ROS_INFO("Wf_7, l_speed = %d, r_speed = %d, g_wall_distance = %d", l_speed, r_speed, g_wall_distance);
		} else
		{
			if (r_step < 2000) jam_++;
			g_turn_angle = 920;
//			reset_wheel_step();
			g_wall_distance = Wall_High_Limit;
		}
	}

	if (! mt_is_left()) std::swap(l_speed, r_speed);
	set_dir_forward();
}


void SelfCheckRegulator::adjustSpeed(uint8_t bumper_jam_state)
{
	uint8_t left_speed;
	uint8_t right_speed;
	if (g_oc_suction)
		left_speed = right_speed = 0;
	else if (g_oc_wheel_left || g_oc_wheel_right)
	{
		if (g_oc_wheel_right) {
			set_dir_right();
		} else {
			set_dir_left();
		}
		left_speed = 30;
		right_speed = 30;
	}
	else if (g_cliff_jam)
	{
		set_dir_backward();
		left_speed = right_speed = 18;
	}
	else //if (g_bumper_jam)
	{
		switch (bumper_jam_state)
		{
			case 1:
			case 2:
			case 3:
			{
				// Quickly move back for a distance.
				set_dir_backward();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 4:
			{
				// Quickly turn right for 90 degrees.
				set_dir_right();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
			case 5:
			{
				// Quickly turn left for 180 degrees.
				set_dir_left();
				left_speed = right_speed = RUN_TOP_SPEED;
				break;
			}
		}
	}

	set_wheel_speed(left_speed, right_speed);
}


//RegulatorManage
RegulatorProxy::RegulatorProxy(Point32_t origin, Point32_t target)
{
	g_obs_triggered = g_rcon_triggered = false;
	g_bumper_hitted =  g_cliff_triggered = false;
	g_obs_triggered = g_rcon_triggered = false;
	g_bumper_cnt = g_cliff_cnt =0;
	reset_rcon_status();

	s_target = target;
	s_origin = origin;
//	ROS_INFO("target(%d,%d)",target.X,target.Y);
//	ROS_INFO("target_(%d,%d)",s_target.X,s_target.Y);

	back_reg_ = new BackRegulator();

	if (mt_is_fallwall()){
		mt_reg_ = new FollowWallRegulator();
		turn_reg_ = new TurnRegulator();
		p_reg_ = mt_reg_;
	}
	else{
		mt_reg_ = new LinearRegulator();
		turn_reg_ = new TurnRegulator();
		p_reg_ = turn_reg_;
	}

	cm_set_event_manager_handler_state(true);

}

RegulatorProxy::~RegulatorProxy()
{
	delete turn_reg_;
	delete back_reg_;
	delete mt_reg_;
	set_wheel_speed(0,0);
	cm_set_event_manager_handler_state(false);
}
void RegulatorProxy::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
	if (p_reg_ != nullptr)
		p_reg_->adjustSpeed(left_speed, right_speed);
}

bool RegulatorProxy::isReach()
{
	if ( (mt_is_linear() && p_reg_ == back_reg_) || (mt_is_fallwall() && p_reg_ == mt_reg_) )
		return p_reg_->isReach();
	return false;
}

bool RegulatorProxy::isSwitch()
{
	if (p_reg_ != nullptr)
		return p_reg_->isSwitch();
	return false;
}

bool RegulatorProxy::isStop()
{
	if (p_reg_ != nullptr)
		return p_reg_->isStop();
	return false;
}

void RegulatorProxy::switchToNext()
{
	if (p_reg_ == mt_reg_)
	{
		if (g_turn_angle != 0)
		{
			turn_reg_->setTarget();
			p_reg_ = turn_reg_;
		} else if (g_bumper_hitted || g_cliff_triggered)
		{
			g_bumper_hitted = g_cliff_triggered = false;

			auto mt = mt_get();
			if(mt == CM_LINEARMOVE)
				mt_set(CM_FOLLOW_LEFT_WALL);

			g_turn_angle = bumper_turn_angle();

			mt_set(mt);

			back_reg_->setOrigin();
			p_reg_ = back_reg_;
		}
	}
	else if (p_reg_ == back_reg_)
	{
		turn_reg_->setTarget();
		p_reg_ = turn_reg_;
	}
	else if (p_reg_ == turn_reg_)
	{
		if(g_bumper_hitted || g_cliff_triggered)
		{
			g_bumper_hitted = g_cliff_triggered = false;
			back_reg_->setOrigin();
			p_reg_ = back_reg_;
		}
			else
		{
			g_turn_angle = 0;
			p_reg_ = mt_reg_;
		}
	}
}

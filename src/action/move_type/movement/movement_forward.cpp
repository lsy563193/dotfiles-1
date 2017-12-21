//
// Created by lsy563193 on 12/5/17.
//

#include "pp.h"
#include "arch.hpp"

MovementFollowPointLinear::MovementFollowPointLinear()
{
//	sp_mt_->sp_cm_->plan_path_ = path;
//	s_target_p = GridMap::cellToPoint(sp_mt_->sp_cm_->plan_path_.back());
	base_speed_ = LINEAR_MIN_SPEED,
	tmp_target_ = calcTmpTarget();
//	sp_mt_->sp_cm_->plan_path_display_sp_mt_->sp_cm_->plan_path_points();
//	g_is_should_follow_wall = false;
//	s_target = target;
//	sp_mt_->sp_cm_->plan_path_ = path;
}

Point32_t MovementFollowPointLinear::calcTmpTarget()
{
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);
	auto new_dir = p_clean_mode->new_dir_;
	auto curr = nav_map.getCurrPoint();
	auto tmp_target = nav_map.cellToPoint(p_clean_mode->plan_path_.front());
	auto curr_xy = (GridMap::isXDirection(new_dir)) ? curr.X : curr.Y;
	auto &target_xy = (GridMap::isXDirection(new_dir)) ? tmp_target.X : tmp_target.Y;

	int16_t dis = std::min(std::abs(curr_xy - target_xy), (int32_t) (1.5 * CELL_COUNT_MUL));
	if (!GridMap::isPositiveDirection(new_dir))
		dis *= -1;
	target_xy = curr_xy + dis;

	return tmp_target;
}

void MovementFollowPointLinear::adjustSpeed(int32_t &left_speed, int32_t &right_speed)
{
//	PP_INFO();
//	ROS_WARN("%s,%d: g_p_clean_mode->plan_path_size(%d)",__FUNCTION__, __LINE__,p_clean_mode->plan_path_.size());
	wheel.setDirectionForward();
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);

	tmp_target_ = calcTmpTarget();

	auto new_dir = p_clean_mode->new_dir_;

	auto target = GridMap::cellToPoint(p_clean_mode->plan_path_.front());

	auto tmp_xy = (GridMap::isXDirection(new_dir)) ? tmp_target_.X : tmp_target_.Y;
	auto target_xy = (GridMap::isXDirection(new_dir)) ? target.X : target.Y;
	auto is_beyond = (GridMap::isPositiveDirection(new_dir)) ? target_xy <= tmp_xy : target_xy >= tmp_xy;
//	ROS_ERROR("%s,%d,target,tmp(%d,%d)",__FUNCTION__,__LINE__,target_xy, tmp_xy);
	if(is_beyond && p_clean_mode->plan_path_.size()>1)
	{
		p_clean_mode->old_dir_ = p_clean_mode->new_dir_;
		p_clean_mode->new_dir_ = (MapDirection)p_clean_mode->plan_path_.front().TH;
		p_clean_mode->plan_path_.pop_front();
		tmp_target_ = calcTmpTarget();
		ROS_INFO("%s,%d,dir(%d,%d)target(%d,%d)",__FUNCTION__,__LINE__,p_clean_mode->old_dir_,p_clean_mode->new_dir_,(MapDirection)p_clean_mode->plan_path_.front().X,(MapDirection)p_clean_mode->plan_path_.front().Y);
		ROS_INFO("%s,%d,target,tmp(%d,%d)",__FUNCTION__,__LINE__,target_xy, tmp_xy);
	}

	auto curr_p = nav_map.getCurrPoint();
	auto angle_diff = ranged_angle(
					course_to_dest(curr_p.X, curr_p.Y, tmp_target_.X, tmp_target_.Y) - robot::instance()->getPoseAngle());

//	ROS_WARN("tmp_xy(%d),x?(%d),pos(%d),dis(%d), target_p(%d,%d)", tmp_xy, GridMap::isXDirection(new_dir), GridMap::isPositiveDirection(new_dir), dis, target_p.X, target_p.Y);
//	auto dis_diff = GridMap::isXDirection(new_dir) ? curr_p.Y - cm_target_p_.Y : curr_p.X - cm_target_p_.X;
//	dis_diff = GridMap::isPositiveDirection(new_dir) ^ GridMap::isXDirection(new_dir) ? dis_diff :  -dis_diff;

	if (integration_cycle_++ > 10) {
		auto t = nav_map.pointToCell(tmp_target_);
		robot::instance()->pubCleanMapMarkers(nav_map, p_clean_mode->plan_path_, &t);
		integration_cycle_ = 0;
		integrated_ += angle_diff;
		check_limit(integrated_, -150, 150);
	}
	auto distance = two_points_distance(curr_p.X, curr_p.Y, tmp_target_.X, tmp_target_.Y);
	auto obstalce_distance_front = lidar.getObstacleDistance(0,ROBOT_RADIUS);
	uint8_t obs_state = obs.getStatus();
	bool is_decrease_blocked = decrease_map.isFrontBlocked();
	if (obs_state > 0 || (distance < SLOW_DOWN_DISTANCE) || nav_map.isFrontBlockBoundary(3) || (obstalce_distance_front < 0.25) || is_decrease_blocked)
	{
//		ROS_WARN("decelarate");
		if (distance < SLOW_DOWN_DISTANCE)
			angle_diff = 0;
		integrated_ = 0;
		if (base_speed_ > (int32_t) LINEAR_MIN_SPEED){
			base_speed_--;
			/*if(obstalce_distance_front > 0.025 && obstalce_distance_front < 0.125 && (left_speed > 20 || right_speed > 20)) {
				base_speed_ -= 2;
			}
			else if(obs_state & BLOCK_FRONT)
				base_speed_ -=2;
			else if(obs_state & (BLOCK_LEFT | BLOCK_RIGHT))
				base_speed_ --;*/
		}
	}
	else if (base_speed_ < (int32_t) LINEAR_MAX_SPEED) {
		if (tick_++ > 1) {
			tick_ = 0;
			base_speed_++;
		}
		integrated_ = 0;
	}

	left_speed =
					base_speed_ - angle_diff / 20 - integrated_ / 150; // - Delta / 20; // - Delta * 10 ; // - integrated_ / 2500;
	right_speed =
					base_speed_ + angle_diff / 20 + integrated_ / 150; // + Delta / 20;// + Delta * 10 ; // + integrated_ / 2500;
//		ROS_ERROR("left_speed(%d),right_speed(%d),angle_diff(%d), intergrated_(%d)",left_speed, right_speed, angle_diff, integrated_);

#if LINEAR_MOVE_WITH_PATH
	check_limit(left_speed, 0, LINEAR_MAX_SPEED);
	check_limit(right_speed, 0, LINEAR_MAX_SPEED);
#else
	check_limit(left_speed, LINEAR_MIN_SPEED, LINEAR_MAX_SPEED);
	check_limit(right_speed, LINEAR_MIN_SPEED, LINEAR_MAX_SPEED);
#endif
	base_speed_ = (left_speed + right_speed) / 2;
}

bool MovementFollowPointLinear::isFinish()
{
	return isPoseReach() || shouldMoveBack() || isBoundaryStop() || isLidarStop() || isOBSStop() || isRconStop() || isPassTargetStop() ||
				 is_robotbase_stop();
}

bool MovementFollowPointLinear::isCellReach()
{
	// Checking if robot has reached target cell.
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);
	auto s_curr_p = nav_map.getCurrPoint();
	auto target_p = nav_map.cellToPoint(p_clean_mode->plan_path_.back());
	if (std::abs(s_curr_p.X - target_p.X) < CELL_COUNT_MUL_1_2 &&
		std::abs(s_curr_p.Y - target_p.Y) < CELL_COUNT_MUL_1_2)
	{
		ROS_INFO("\033[1m""%s, %d: MovementFollowPointLinear, reach the target cell (%d,%d)!!""\033[0m", __FUNCTION__, __LINE__,
						 p_clean_mode->plan_path_.back().X, p_clean_mode->plan_path_.back().Y);
//		g_turn_angle = ranged_angle(new_dir - robot::instance()->getPoseAngle());
		return true;
	}

	return false;
}

bool MovementFollowPointLinear::isPoseReach()
{
	// Checking if robot has reached target cell and target angle.
//	PP_INFO();
	auto target_angle = s_target_p.TH;
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);
	if (isCellReach() && std::abs(ranged_angle(robot::instance()->getPoseAngle() - target_angle)) < 200)
	{
		ROS_INFO("\033[1m""%s, %d: MovementFollowPointLinear, reach the target cell and pose(%d,%d,%d)!!""\033[0m", __FUNCTION__, __LINE__,
				 p_clean_mode->plan_path_.back().X, p_clean_mode->plan_path_.back().Y, p_clean_mode->plan_path_.back().TH);
		return true;
	}
	return false;
}

bool MovementFollowPointLinear::isNearTarget()
{
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);
	auto new_dir = p_clean_mode->new_dir_;
	auto s_curr_p = nav_map.getCurrPoint();
	auto curr = (GridMap::isXDirection(new_dir)) ? s_curr_p.X : s_curr_p.Y;
	auto target_p = nav_map.cellToPoint(p_clean_mode->plan_path_.front());
	auto &target = (GridMap::isXDirection(new_dir)) ? target_p.X : target_p.Y;
	//ROS_INFO("%s %d: s_curr_p(%d, %d), target_p(%d, %d), dir(%d)",
	//		 __FUNCTION__, __LINE__, s_curr_p.X, s_curr_p.Y, target_p.X, target_p.Y, new_dir);
	if ((GridMap::isPositiveDirection(new_dir) && (curr > target - 1.5 * CELL_COUNT_MUL)) ||
		(!GridMap::isPositiveDirection(new_dir) && (curr < target + 1.5 * CELL_COUNT_MUL))) {
		if(p_clean_mode->plan_path_.size() > 1)
		{
			// Switch to next target for smoothly turning.
			new_dir = static_cast<MapDirection>(p_clean_mode->plan_path_.front().TH);
			p_clean_mode->plan_path_.pop_front();
			ROS_INFO("%s %d: Curr(%d, %d), switch next cell(%d, %d), new dir(%d).", __FUNCTION__, __LINE__,
					 nav_map.getXCell(),
					 nav_map.getYCell(), p_clean_mode->plan_path_.front().X, p_clean_mode->plan_path_.front().Y, new_dir);
		}
		else if(p_clean_mode->plan_path_.front() != g_zero_home && g_allow_check_path_in_advance)
		{
			g_check_path_in_advance = true;
			ROS_INFO("%s %d: Curr(%d, %d), target(%d, %d), dir(%d), g_check_path_in_advance(%d)",
					 __FUNCTION__, __LINE__, nav_map.getXCell(), nav_map.getYCell(),
					 p_clean_mode->plan_path_.front().X, p_clean_mode->plan_path_.front().Y, new_dir, g_check_path_in_advance);
			return true;
		}
	}
	return false;
}

bool MovementFollowPointLinear::shouldMoveBack()
{
	// Robot should move back for these cases.
	ev.bumper_triggered = bumper.get_status();
	ev.cliff_triggered = cliff.get_status();
	ev.tilt_triggered = gyro.getTiltCheckingStatus();

	if (ev.bumper_triggered || ev.cliff_triggered || ev.tilt_triggered || g_robot_slip)
	{
		ROS_WARN("%s, %d,ev.bumper_triggered(%d) ev.cliff_triggered(%d) ev.tilt_triggered(%d) g_robot_slip(%d)."
				, __FUNCTION__, __LINE__,ev.bumper_triggered,ev.cliff_triggered,ev.tilt_triggered,g_robot_slip);
		return true;
	}

	return false;
}

bool MovementFollowPointLinear::isRconStop()
{
//	PP_INFO();
	ev.rcon_triggered = c_rcon.getTrig();
	if(ev.rcon_triggered)
	{
//		g_turn_angle = rcon_turn_angle();
//		ROS_INFO("%s, %d: ev.rcon_triggered(%d), turn for (%d).", __FUNCTION__, __LINE__, ev.rcon_triggered, g_turn_angle);
//		return true;
	}

	return false;
}

bool MovementFollowPointLinear::isOBSStop()
{
	// Now OBS sensor is just for slowing down.
//	PP_INFO();
	return false;
/*
	ev.obs_triggered = obs.get_status(200, 1700, 200);
	if(ev.obs_triggered)
	{
		turn_angle = obs_turn_angle();
		ROS_INFO("%s, %d: ev.obs_triggered(%d), turn for (%d).", __FUNCTION__, __LINE__, ev.obs_triggered, turn_angle);
		return true;
	}

	return false;*/
}

bool MovementFollowPointLinear::isLidarStop()
{
//	PP_INFO();
	ev.lidar_triggered = lidar_get_status();
	if (ev.lidar_triggered)
	{
		// Temporary use OBS to get angle.
//		ev.obs_triggered = ev.lidar_triggered;
//		g_turn_angle = obs_turn_angle();
//		ROS_WARN("%s, %d: ev.lidar_triggered(%d), turn for (%d).", __FUNCTION__, __LINE__, ev.lidar_triggered, g_turn_angle);
		return true;
	}

	return false;
}

bool MovementFollowPointLinear::isBoundaryStop()
{
//	PP_INFO();
	if (nav_map.isFrontBlockBoundary(2))
	{
		ROS_INFO("%s, %d: MovementFollowPointLinear, Blocked boundary.", __FUNCTION__, __LINE__);
		return true;
	}

	return false;
}

bool MovementFollowPointLinear::isPassTargetStop()
{
//	PP_INFO();
	// Checking if robot has reached target cell.
	auto p_clean_mode = boost::dynamic_pointer_cast<ACleanMode>(sp_mt_->sp_mode_);
	auto new_dir = p_clean_mode->new_dir_;
	auto s_curr_p = nav_map.getCurrPoint();
	auto curr = (GridMap::isXDirection(new_dir)) ? s_curr_p.X : s_curr_p.Y;
	auto target_p = nav_map.cellToPoint((p_clean_mode->plan_path_.back()));
	auto target = (GridMap::isXDirection(new_dir)) ? target_p.X : target_p.Y;
	if ((GridMap::isPositiveDirection(new_dir) && (curr > target + CELL_COUNT_MUL / 4)) ||
		(!GridMap::isPositiveDirection(new_dir) && (curr < target - CELL_COUNT_MUL / 4)))
	{
		ROS_INFO("%s, %d: MovementFollowPointLinear, pass target: new_dir(\033[32m%d\033[0m),is_x_axis(\033[32m%d\033[0m),is_pos(\033[32m%d\033[0m),curr(\033[32m%d\033[0m),target(\033[32m%d\033[0m)",
				 __FUNCTION__, __LINE__, new_dir, GridMap::isXDirection(new_dir), GridMap::isPositiveDirection(new_dir), curr, target);
		return true;
	}
	return false;
}

//void MovementFollowPointLinear::setTarget()
//{
//	turn_angle = ranged_angle(
//						course_to_dest(s_curr_p.X, s_curr_p.Y, cm_target_p_.X, cm_target_p_.Y) - robot::instance()->getPoseAngle());
//	s_target_p = nav_map.cellToPoint(p_clean_mode->plan_path_.back());
//	p_clean_mode->plan_path_ = p_clean_mode->plan_path_;
//}

void MovementFollowPointLinear::setBaseSpeed()
{
	base_speed_ = LINEAR_MIN_SPEED;
}
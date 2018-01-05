// // Created by lsy563193 on 12/5/17.  //

#include <event_manager.h>
#include "pp.h"
#include "arch.hpp"

MovementFollowPointLinear::MovementFollowPointLinear()
{
	min_speed_ = LINEAR_MIN_SPEED;
	max_speed_ = LINEAR_MAX_SPEED;
//	sp_mt_->sp_cm_->tmp_plan_path_ = path;
//	s_target_p = GridMap::cellToPoint(sp_mt_->sp_cm_->tmp_plan_path_.back());
	base_speed_ = LINEAR_MIN_SPEED;
	tick_limit_ = 1;
	auto p_clean_mode = (ACleanMode*)(sp_mt_->sp_mode_);
	sp_mt_->target_point_ = p_clean_mode->plan_path_.front();
//	sp_mt_->sp_cm_->plan_path_display_sp_mt_->sp_cm_->plan_path_points();
//	g_is_should_follow_wall = false;
//	s_target = target;
//	sp_mt_->sp_cm_->tmp_plan_path_ = path;
}

bool MovementFollowPointLinear::calcTmpTarget()
{
	auto curr = getPosition();
	auto curr_xy = (isXAxis(sp_mt_->dir_)) ? curr.x : curr.y;
	auto tmp_target_xy = (isXAxis(sp_mt_->dir_)) ? tmp_target_.x :tmp_target_.y;
	if(fabs(curr_xy - tmp_target_xy) > LINEAR_NEAR_DISTANCE && tmp_target_xy != 0) {
		return false;
	}

	auto &target_xy = (isXAxis(sp_mt_->dir_)) ? sp_mt_->target_point_.x : sp_mt_->target_point_.y;
	tmp_target_.x = isXAxis(sp_mt_->dir_) ? curr_xy : sp_mt_->target_point_.x;
	tmp_target_.y = isXAxis(sp_mt_->dir_) ? sp_mt_->target_point_.y : curr_xy;
	auto &tmp_xy = (isXAxis(sp_mt_->dir_)) ? tmp_target_.x : tmp_target_.y;
//	ROS_WARN("curr_xy(%d), target_xy(%d)", curr_xy, target_xy);
	auto dis = std::min(std::abs(curr_xy - target_xy), (int32_t) (LINEAR_NEAR_DISTANCE + CELL_COUNT_MUL));
//	ROS_INFO("dis(%d)",dis);
	if (!isPos(sp_mt_->dir_))
		dis *= -1;
	tmp_xy = curr_xy + dis;
//	ROS_WARN("tmp(%d,%d)",tmp_target_.x, tmp_target_.y);
//	ROS_WARN("dis(%d),dir(%d), curr(%d, %d), tmp_target(%d, %d)", dis, sp_mt_->dir_, curr.x, curr.y, tmp_target.x, tmp_target.y);
	return true;
}

bool MovementFollowPointLinear::isFinish()
{
	return sp_mt_->shouldMoveBack();
}

bool MovementFollowPointLinear::is_near()
{
	bool is_decrease_blocked = decrease_map.isFrontBlocked();
//	auto curr_p = getPosition();
//	auto distance = two_points_distance(curr_p.x, curr_p.y, s_target_p.x, s_target_p.y);
	auto obstacle_distance_front = lidar.getObstacleDistance(0,ROBOT_RADIUS);
	return obs.getStatus() > 0 || /*(distance < SLOW_DOWN_DISTANCE) ||*/  (obstacle_distance_front < 0.25) || is_decrease_blocked;
}

//
// Created by lsy563193 on 12/4/17.
//
#include "pp.h"
#include "arch.hpp"



MoveTypeLinear::MoveTypeLinear() {
	resetTriggeredValue();

	auto p_clean_mode = (ACleanMode*)sp_mode_;
	target_point_ = p_clean_mode->plan_path_.front();
	turn_target_angle_ = p_clean_mode->new_dir_;
	ROS_INFO("%s,%d: mt_is_linear,turn(%d)", __FUNCTION__, __LINE__, turn_target_angle_);
	movement_i_ = mm_turn;
	sp_movement_.reset(new MovementTurn(turn_target_angle_, ROTATE_TOP_SPEED));
//	ROS_INFO("%s,%d: mt_is_linear,turn(%d)", __FUNCTION__, __LINE__, turn_target_angle_);
//	ROS_ERROR("%s,%d: mt_is_linear,turn(%d)", __FUNCTION__, __LINE__, turn_target_angle_);
	IMovement::sp_mt_ = this;
//	ROS_WARN("%s,%d: mt_is_linear,turn(%d)", __FUNCTION__, __LINE__, turn_target_angle_);
}

//MoveTypeLinear::MoveTypeLinearar() {
//
//}
bool MoveTypeLinear::isFinish()
{
	if (IMoveType::isFinish())
	{
		PP_INFO();
		return true;
	}

	if (sp_movement_->isFinish()) {
		PP_INFO();

		if (movement_i_ == mm_turn) {
			PP_INFO();
			movement_i_ = mm_forward;
			resetTriggeredValue();
			sp_movement_.reset(new MovementFollowPointLinear());
		}
		else if (movement_i_ == mm_forward) {
			PP_INFO();
			if (ev.bumper_triggered || ev.cliff_triggered || ev.tilt_triggered) {
				movement_i_ = mm_back;
				sp_movement_.reset(new MovementBack(0.01, BACK_MAX_SPEED));
			}
			else {
//				resetTriggeredValue();
				return true;
			}
		}
		else {//back
//			resetTriggeredValue();
			PP_INFO();
			return true;
		}
	}
	return false;
}

MoveTypeLinear::~MoveTypeLinear()
{
//	PP_WARN();
}



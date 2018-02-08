//
// Created by pierre on 17-12-20.
//
#include <event_manager.h>
#include <dev.h>
#include <error.h>
#include <map.h>

#include "mode.hpp"

CleanModeExploration::CleanModeExploration()
{
	ROS_INFO("%s %d: Entering Exploration mode\n=========================" , __FUNCTION__, __LINE__);
	speaker.play(VOICE_EXPLORATION_START, false);
	mode_i_ = cm_exploration;
	clean_path_algorithm_.reset(new NavCleanPathAlgorithm());
	IMoveType::sp_mode_ = this;
	go_home_path_algorithm_.reset();
	clean_map_.mapInit();
}

CleanModeExploration::~CleanModeExploration()
{
}

bool CleanModeExploration::mapMark()
{
	clean_map_.mergeFromSlamGridMap(slam_grid_map, true, true, false, false, false, false);
	clean_map_.setCircleMarkers(getPosition(),false,10,CLEANED);
	clean_map_.setBlocks();
	if(mark_robot_)
		clean_map_.markRobot(CLEAN_MAP);
//	passed_path_.clear();
	return false;
}

// event
void CleanModeExploration::keyClean(bool state_now, bool state_last) {
	ROS_WARN("%s %d: key clean.", __FUNCTION__, __LINE__);

	beeper.beepForCommand(VALID);

	// Wait for key released.
	bool long_press = false;
	while (key.getPressStatus())
	{
		if (!long_press && key.getPressTime() > 3)
		{
			ROS_WARN("%s %d: key clean long pressed.", __FUNCTION__, __LINE__);
			beeper.beepForCommand(VALID);
			long_press = true;
		}
		usleep(20000);
	}

	if (long_press)
		ev.key_long_pressed = true;
	else
		ev.key_clean_pressed = true;
	ROS_WARN("%s %d: Key clean is released.", __FUNCTION__, __LINE__);

	key.resetTriggerStatus();
}

void CleanModeExploration::remoteClean(bool state_now, bool state_last) {
	ROS_WARN("%s %d: remote clean.", __FUNCTION__, __LINE__);

	beeper.beepForCommand(VALID);
	ev.key_clean_pressed = true;
	remote.reset();
}

void CleanModeExploration::overCurrentWheelLeft(bool state_now, bool state_last) {
	ROS_WARN("%s %d: Left wheel oc.", __FUNCTION__, __LINE__);
	ev.oc_wheel_left = true;
}

void CleanModeExploration::overCurrentWheelRight(bool state_now, bool state_last) {
	ROS_WARN("%s %d: Right wheel oc.", __FUNCTION__, __LINE__);
	ev.oc_wheel_right = true;
}

void CleanModeExploration::chargeDetect(bool state_now, bool state_last) {
	ROS_WARN("%s %d: Charge detect!.", __FUNCTION__, __LINE__);
	if (charger.getChargeStatus() >= 1)
	{
		ROS_WARN("%s %d: Set ev.chargeDetect.", __FUNCTION__, __LINE__);
		ev.charge_detect = charger.getChargeStatus();
	}
}

/*void CleanModeExploration::printMapAndPath()
{
	clean_path_algorithm_->displayCellPath(pointsGenerateCells(passed_path_));
	clean_map_.print(CLEAN_MAP,getPosition().toCell().x,getPosition().toCell().y);
}*/

//state GoToCharger
void CleanModeExploration::switchInStateGoToCharger() {
	PP_INFO();
	if (ev.charge_detect && charger.isOnStub()) {
		ev.charge_detect = 0;
		sp_state = nullptr;
		return;
	}
	else
		sp_state = state_exploration;
	sp_state->init();
}

//state Init
void CleanModeExploration::switchInStateInit() {
	PP_INFO();
	action_i_ = ac_null;
	sp_action_ = nullptr;
	sp_state = state_exploration;
	sp_state->init();
}

//state GoHomePoint
void CleanModeExploration::switchInStateGoHomePoint() {
	PP_INFO();
	sp_state = nullptr;
}
/*

bool CleanModeExploration::moveTypeFollowWallIsFinish(IMoveType *p_move_type, bool is_new_cell) {
	if(action_i_ == ac_follow_wall_left || action_i_ == ac_follow_wall_right)
	{
		auto p_mt = dynamic_cast<MoveTypeFollowWall *>(p_move_type);
		return p_mt->isBlockCleared(clean_map_, passed_path_);
	}
	return false;
}
*/

bool CleanModeExploration::markMapInNewCell() {
	if(sp_state == state_folllow_wall)
	{
		mark_robot_ = false;
		mapMark();
		mark_robot_ = true;
	}
	else
		mapMark();
	return true;
}


//
// Created by lsy563193 on 12/4/17.
//

#include <mathematics.h>
#include <event_manager.h>
#include "dev.h"
#include "robot.hpp"

#include <move_type.hpp>
#include <state.hpp>
#include <mode.hpp>

boost::shared_ptr<IMovement> IMoveType::sp_movement_ = nullptr;
Mode* IMoveType::sp_mode_ = nullptr;
int IMoveType::movement_i_ = mm_null;

IMoveType::IMoveType() {
//	resetTriggeredValue();
	last_ = start_point_ = getPosition();
	c_rcon.resetStatus();
	robot::instance()->obsAdjustCount(20);
}
IMoveType::~IMoveType() {
//	resetTriggeredValue();
	wheel.stop();
}
bool IMoveType::shouldMoveBack()
{
	// Robot should move back for these cases.
	ev.bumper_triggered = bumper.getStatus();
	ev.cliff_triggered = cliff.getStatus();
	ev.tilt_triggered = gyro.getTiltCheckingStatus();

	if (ev.bumper_triggered || ev.cliff_triggered || ev.tilt_triggered)
	{
		ROS_WARN("%s, %d,ev.bumper_triggered(%d) ev.cliff_triggered(%d) ev.tilt_triggered(%d)."
				, __FUNCTION__, __LINE__, ev.bumper_triggered, ev.cliff_triggered, ev.tilt_triggered);
		return true;
	}

	return false;
}

bool IMoveType::isOBSStop()
{
	// Now OBS sensor is just for slowing down.
//	PP_INFO();
	return false;
/*
	ev.obs_triggered = obs.getStatus(200, 1700, 200);
	if(ev.obs_triggered)
	{
		turn_angle = obsTurnAngle();
		ROS_INFO("%s, %d: ev.obs_triggered(%d), turn for (%d).", __FUNCTION__, __LINE__, ev.obs_triggered, turn_angle);
		return true;
	}

	return false;*/
}

bool IMoveType::isLidarStop()
{
//	PP_INFO();
//	ev.lidar_triggered = lidar.getObstacleDistance(0,0.056) < 0.04 ? BLOCK_FRONT : 0;
	auto p_mode = dynamic_cast<ACleanMode*> (sp_mode_);
	auto p_mt = boost::dynamic_pointer_cast<IMoveType>(p_mode->sp_action_);
	ev.lidar_triggered = lidar.lidar_get_status(p_mt->movement_i_, sp_mode_->action_i_);
	if (ev.lidar_triggered)
	{
		ROS_WARN("%s, %d: ev.lidar_triggered(%d).", __FUNCTION__, __LINE__, ev.lidar_triggered);
		return true;
	}

	return false;
}

bool IMoveType::shouldTurn()
{
//	ev.lidar_triggered = lidar_get_status();
//	if (ev.lidar_triggered)
//	{
//		// Temporary use bumper as lidar triggered.
//		ev.bumper_triggered = ev.lidar_triggered;
//		g_turn_angle = bumperTurnAngle();
//		ev.bumper_triggered = 0;
//		ROS_WARN("%s %d: Lidar triggered, turn_angle: %d.", __FUNCTION__, __LINE__, g_turn_angle);
//		return true;
//	}

//	ev.obs_triggered = (obs.getFront() > obs.getFrontTrigValue() + 1700);
//	if (ev.obs_triggered)
//	{
////		ev.obs_triggered = BLOCK_FRONT;
////		g_turn_angle = obsTurnAngle();
//		ROS_WARN("%s %d: OBS triggered.", __FUNCTION__, __LINE__);
//		return true;
//	}

	return false;
}

bool IMoveType::RconTrigger()
{
	ev.rcon_status = c_rcon.getWFRcon();
	if (ev.rcon_status) {
		ROS_WARN("%s, %d: ev.rcon_status(%d).", __FUNCTION__, __LINE__, ev.rcon_status);
		return true;
	}
	return false;
}

void IMoveType::resetTriggeredValue()
{
//	PP_INFO();
	ev.lidar_triggered = 0;
//	ev.rcon_status = 0;
	ev.bumper_triggered = 0;
	ev.obs_triggered = 0;
	ev.cliff_triggered = 0;
	ev.tilt_triggered = 0;
	ev.robot_slip = false;
}

bool IMoveType::isFinish()
{
	updatePosition();
	auto curr = getPosition();
	auto p_cm = dynamic_cast<ACleanMode*> (sp_mode_);
	if (!curr.isCellAndAngleEqual(last_))
	{
		last_ = curr;
		if(p_cm->moveTypeNewCellIsFinish(this))
			return true;
	}
	if(p_cm->moveTypeRealTimeIsFinish(this)) {
//		wheel.stop();
		return true;
	}

	return false;
}

void IMoveType::run()
{
	sp_movement_->run();
}

uint32_t IMoveType::countRconTriggered(uint32_t rcon_value, int max_cnt)
{
	if(rcon_value == 0)
		return 0;

	if ( rcon_value& RconL_HomeT)
		rcon_cnt[Rcon::left]++;
	if ( rcon_value& RconFL_HomeT)
		rcon_cnt[Rcon::fl]++;
	if ( rcon_value& RconFL2_HomeT)
		rcon_cnt[Rcon::fl2]++;
	if ( rcon_value& RconFR2_HomeT)
		rcon_cnt[Rcon::fr2]++;
	if ( rcon_value& RconFR_HomeT)
		rcon_cnt[Rcon::fr]++;
	if ( rcon_value& RconR_HomeT)
		rcon_cnt[Rcon::right]++;
	uint32_t ret = 0;
	for (int i = Rcon::enum_start; i < Rcon::enum_end; i++){
		if ( (i == Rcon::fl || i == Rcon::fr ) && rcon_cnt[i] > max_cnt) {
				
			for (int j = Rcon::enum_start; j < Rcon::enum_end; j++)
				rcon_cnt[j] =0;
			ret = c_rcon.convertFromEnum(i);
			break;
		}
		else if( (i == Rcon::left || i == Rcon::right || i == Rcon::fl2 || i == Rcon::fr2) && rcon_cnt[i] > (max_cnt + 2)){
			for (int j = Rcon::enum_start; j < Rcon::enum_end; j++)
				rcon_cnt[j] =0;
			ret = c_rcon.convertFromEnum(i);
			break;
		}
	}
	return ret;
}

bool IMoveType::isRconStop()
{
	bool ret = false;
	ev.rcon_status = countRconTriggered(c_rcon.getNavRcon(), 3);
	if(ev.rcon_status)
	{
		ROS_INFO("\033[1;40;35m""Rcon triggered and stop %x.""\033[0m",ev.rcon_status);
		ret = true;
	}

	return ret;
}

bool IMoveType::isBlockCleared(GridMap &map, Points &passed_path)
{
	if (!passed_path.empty())
	{
//		ROS_INFO("%s %d: passed_path.back(%d %d)", __FUNCTION__, __LINE__, passed_path.back().x, passed_path.back().y);
		return !map.isBlockAccessible(passed_path.back().toCell().x, passed_path.back().toCell().y);
	}

	return false;
}


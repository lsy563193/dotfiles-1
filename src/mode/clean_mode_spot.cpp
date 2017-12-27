//
// Created by lsy563193 on 12/9/17.
//
#include "dev.h"
#include "arch.hpp"
#include "path_algorithm.h"
#include "pp.h"
#include "robot.hpp"

CleanModeSpot::CleanModeSpot()
{
	event_manager_register_handler(this);
	event_manager_set_enable(true);
	IMoveType::sp_mode_ = this;
	speaker.play(VOICE_CLEANING_SPOT);
	clean_path_algorithm_.reset(new SpotCleanPathAlgorithm());
	go_home_path_algorithm_.reset();
	has_aligned_and_open_slam = false;
	clean_map_ = &nav_map;
}

CleanModeSpot::~CleanModeSpot()
{
	IMoveType::sp_mode_ = nullptr;
	wheel.stop();
	brush.stop();
	vacuum.stop();
	lidar.motorCtrl(OFF);
	lidar.setScanOriginalReady(0);

	robot::instance()->setBaselinkFrameType( ODOM_POSITION_ODOM_ANGLE);
	slam.stop();
	odom.setAngleOffset(0);
}

bool CleanModeSpot::isFinish()
{
	if(action_i_ == ac_open_slam){
		vacuum.setMode(Vac_Save);
		brush.setLeftPwm(50);
		brush.setRightPwm(50);
		brush.setMainPwm(50);
	}
	return ACleanMode::isFinish();
}

bool CleanModeSpot::mapMark()
{

	clean_path_algorithm_->displayPath(passed_path_);
	if (action_i_ == ac_linear) {
		PP_INFO();
		nav_map.setCleaned(passed_path_);
	}

	if (state_i_ == st_trapped)
		nav_map.markRobot(CLEAN_MAP);

	nav_map.setBlocks();
	PP_INFO();
	nav_map.print(CLEAN_MAP, nav_map.getXCell(), nav_map.getYCell());

	passed_path_.clear();
	return false;
}

bool CleanModeSpot::isExit()
{
	if (action_i_ == ac_pause && sp_action_->isTimeUp())
	{
		ROS_WARN("%s %d:.", __FUNCTION__, __LINE__);
		setNextMode(md_sleep);
		return true;
	}

	if (action_i_ == ac_pause && sp_action_->isExit())
	{
		ROS_WARN("%s %d:.", __FUNCTION__, __LINE__);
		setNextMode(md_idle);
		return true;
	}

	if (ev.fatal_quit || ev.key_long_pressed || ev.cliff_all_triggered || sp_action_->isExit())
	{
		ROS_WARN("%s %d:.", __FUNCTION__, __LINE__);
		setNextMode(md_idle);
		return true;
	}

	if (ev.charge_detect >= 3)
	{
		ROS_WARN("%s %d: Exit for directly charge.", __FUNCTION__, __LINE__);
		setNextMode(md_charge);
		return true;
	}

	return false;
}

bool CleanModeSpot::setNextAction()
{
	if (!isInitFinished_)
		return ACleanMode::setNextAction();
	else if (state_i_ == st_clean)
	{
		PP_INFO();
		if(plan_path_.size() >= 2)
			action_i_ = ac_linear;
		else
			action_i_ = ac_null;
	}
	genNextAction();

	return action_i_ != ac_null;
}

bool CleanModeSpot::setNextState()
{
	PP_INFO();

	if (!isInitFinished_)
		return true;

	bool state_confirm = false;
	while (ros::ok() && !state_confirm)
	{
		if (state_i_ == st_null)
		{
			auto curr = nav_map.updatePosition();
			passed_path_.push_back(curr);

			home_cells_.back().TH = robot::instance()->getPoseAngle();
			PP_INFO();

			state_i_ = st_clean;
			stateInit(state_i_);
			action_i_ = ac_null;
		}
		else if (isExceptionTriggered())
		{
			ROS_INFO("%s %d: Pass this state switching for exception cases.", __FUNCTION__, __LINE__);
			// Apply for all states.
			// If all these exception cases happens, directly set next action to exception resume action.
			// BUT DO NOT CHANGE THE STATE!!! Because after exception resume it should restore the state.
			action_i_ = ac_null;
			state_confirm = true;
		}
		else if(state_i_ == st_clean)
		{
			PP_INFO();
			old_dir_ = new_dir_;
			ROS_ERROR("old_dir_(%d)", old_dir_);
			if (clean_path_algorithm_->generatePath(nav_map, nav_map.getCurrCell(), old_dir_, plan_path_))
			{
				new_dir_ = (MapDirection)plan_path_.front().TH;
				ROS_ERROR("new_dir_(%d)", new_dir_);
				PP_INFO();
				plan_path_.pop_front();
				clean_path_algorithm_->displayPath(plan_path_);
				state_confirm = true;
			}
			else
			{
				state_i_ = st_go_home_point;
				stateInit(state_i_);
				action_i_ = ac_null;
			}
		}
		else if (state_i_ == st_go_home_point)
		{
			PP_INFO();
			old_dir_ = new_dir_;
			plan_path_.clear();
			state_i_ = st_null;
#if 0
			if (go_home_path_algorithm_->generatePath(nav_map, nav_map.getCurrCell(),old_dir_, plan_path_))
			{
				// Reach home cell or new path to home cell is generated.
				if (plan_path_.empty())
				{
					// Reach home cell.
					PP_INFO();
					if (nav_map.getCurrCell() == g_zero_home)
					{
						PP_INFO();
						state_i_ = st_null;
					}
					else
					{
						PP_INFO();
						state_i_ = st_go_to_charger;
						stateInit(state_i_);
					}
					action_i_ = ac_null;
				}
				else
				{
					new_dir_ = (MapDirection)plan_path_.front().TH;
					plan_path_.pop_front();
					go_home_path_algorithm_->displayPath(plan_path_);
				}
			}
			else
			{
				// No more paths to home cells.
				PP_INFO();
				state_i_ = st_null;
			}
#endif
			state_confirm = true;
		}
	}

	return state_i_ != st_null;
}


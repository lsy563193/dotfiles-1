//
// Created by austin on 17-12-8.
//

#include <event_manager.h>
#include <robot.hpp>
#include "dev.h"
#include "mode.hpp"
#include "appointment.h"

ModeRemote::ModeRemote()
{//use dynamic then you can limit using derived class member
	ROS_INFO("%s %d: Entering remote mode\n=========================" , __FUNCTION__, __LINE__);
	event_manager_register_handler(this);
	event_manager_set_enable(true);

	serial.setWorkMode(WORK_MODE);
	if (gyro.isOn())
	{
		sp_state = st_clean.get();
		sp_state->init();
		action_i_ = ac_remote;
	}
	else
	{
		sp_state = st_init.get();
		sp_state->init();
		key_led.setMode(LED_FLASH, LED_GREEN, 600);
		action_i_ = ac_open_gyro;
	}
	genNextAction();
	key.resetTriggerStatus();
	c_rcon.resetStatus();
	remote.reset();
	appmt_obj.resetPlanStatus();
	event_manager_reset_status();

	remote_mode_time_stamp_ = ros::Time::now().toSec();

	s_wifi.appendTask(S_Wifi::ACT::ACT_UPLOAD_STATUS);
	mode_i_ = md_remote;
	IMoveType::sp_mode_ = this;
}

ModeRemote::~ModeRemote()
{
	event_manager_set_enable(false);
	sp_action_.reset();

	wheel.stop();
	brush.stop();
	vacuum.stop();
	water_tank.stop(WaterTank::tank_pump);

	ROS_INFO("%s %d: Exit remote mode.", __FUNCTION__, __LINE__);
}

bool ModeRemote::isExit()
{
	if (ev.key_clean_pressed || ev.remote_spot || ev.remote_home || ev.remote_follow_wall)
	{
		ROS_WARN("%s %d: Exit to idle mode.", __FUNCTION__, __LINE__);
		setNextMode(md_idle);
		return true;
	}

	if (ev.charge_detect)
	{
		ROS_WARN("%s %d: Exit to charge mode.", __FUNCTION__, __LINE__);
		setNextMode(md_charge);
		return true;
	}

	return false;
}

bool ModeRemote::isFinish()
{
	if ((action_i_ != ac_exception_resume) && isExceptionTriggered())
	{
		ROS_WARN("%s %d: Exception triggered.", __FUNCTION__, __LINE__);
		action_i_ = ac_exception_resume;
		genNextAction();
	}

	if (sp_action_->isFinish())
	{
		PP_INFO();
		action_i_ = getNextAction();
		genNextAction();
		if (sp_action_ == nullptr)
		{
			setNextMode(md_idle);
			return true;
		}
	}

	return false;
}

int ModeRemote::getNextAction()
{
	if(action_i_ == ac_open_gyro || (action_i_ == ac_exception_resume && !ev.fatal_quit))
	{
		sp_state = st_clean.get();
		sp_state->init();
		return ac_remote;
	}

	return ac_null;
}

void ModeRemote::remoteClean(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote clean.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.key_clean_pressed = true;
	remote.reset();
}

void ModeRemote::remoteDirectionForward(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote forward.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_direction_forward = true;
	remote.reset();
}

void ModeRemote::remoteDirectionLeft(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote left.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_direction_left = true;
	remote.reset();
}

void ModeRemote::remoteDirectionRight(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote right.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_direction_right = true;
	remote.reset();
}

void ModeRemote::remoteMax(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote max is pressed.", __FUNCTION__, __LINE__);
	if(water_tank.checkEquipment(false)){
		beeper.beepForCommand(INVALID);
	}
	else{
		beeper.beepForCommand(VALID);
		vacuum.isMaxInClean(!vacuum.isMaxInClean());
		speaker.play(vacuum.isMaxInClean() ? VOICE_VACCUM_MAX : VOICE_CLEANING_NAVIGATION,false);
		vacuum.setCleanState();
	}
	remote.reset();
}

void ModeRemote::keyClean(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: key clean.", __FUNCTION__, __LINE__);

	beeper.beepForCommand(VALID);

	// Wait for key released.
	while (key.getPressStatus())
		usleep(20000);

	ev.key_clean_pressed = true;
	ROS_WARN("%s %d: Key clean is released.", __FUNCTION__, __LINE__);

	key.resetTriggerStatus();
}

void ModeRemote::chargeDetect(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Charge detect.", __FUNCTION__, __LINE__);
	ev.charge_detect = charger.getChargeStatus();
}

void ModeRemote::remoteWallFollow(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote wall follow.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_follow_wall = true;
	remote.reset();
}

void ModeRemote::remoteSpot(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote spot.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_spot = true;
	remote.reset();
}

void ModeRemote::remoteHome(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: remote home.", __FUNCTION__, __LINE__);
	beeper.beepForCommand(VALID);
	ev.remote_home = true;
	remote.reset();
}

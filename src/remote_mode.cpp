
 /**
  ******************************************************************************
  * @file	 AI Cleaning Robot
  * @author  ILife Team Dxsong
  * @version V1.0
  * @date	 17-Nov-2011
  * @brief	 this mode the robot follows the command of the remote ,
			   Upkey : move forward untill stop command or obstacle event
  ******************************************************************************
  * <h2><center>&copy; COPYRIGHT 2011 ILife CO.LTD</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "movement.h"
#include "gyro.h"
#include "remote_mode.h"
#include <ros/ros.h>
#include "wav.h"
#include "robot.hpp"
#include "robotbase.h"
#include "event_manager.h"

extern volatile uint32_t Left_Wheel_Step,Right_Wheel_Step;

RemoteModeMoveType move_flag = REMOTE_MODE_STAY;
boost::mutex move_flag_mutex;
float pos_x, pos_y;

void Remote_Mode(void)
{
	uint32_t Moving_Speed=0;
	uint8_t Dec_Counter=0;
	uint32_t OBS_Stop=0;
	bool eh_status_now=false, eh_status_last=false;
  //Display_Clean_Status(Display_Remote);

	set_led(100, 0);
	reset_wheel_step();
	reset_stop_event_status();
	work_motor_configure();
	reset_rcon_remote();
	set_move_flag_(REMOTE_MODE_STAY);
//    set_vacmode(Vac_Normal);

	remote_mode_register_events();

	while(ros::ok())
	{
		if (event_manager_check_event(&eh_status_now, &eh_status_last) == 1) {
			continue;
		}
#ifdef OBS_DYNAMIC_MOVETOTARGET
		/* Dyanmic adjust obs trigger val . */
		robotbase_obs_adjust_count(20);
#endif

		if (get_clean_mode() != Clean_Mode_Remote)
			break;

		switch (get_move_flag_())
		{
			case REMOTE_MODE_FORWARD:
			{
				if(get_obs_status())
				{
					Dec_Counter++;
					if(Moving_Speed>10)Moving_Speed--;
					move_forward(Moving_Speed, Moving_Speed);
				}
				else
				{
					Moving_Speed=(get_right_wheel_step()/80)+25;
					if(Moving_Speed<25)Moving_Speed=25;
					if(Moving_Speed>42)Moving_Speed=42;
					move_forward(Moving_Speed, Moving_Speed);
					//work_motor_configure();
					OBS_Stop=0;
				}
				break;
			}
			case REMOTE_MODE_BACKWARD:
			{
				move_forward(-Moving_Speed, -Moving_Speed);
				break;
			}
			case REMOTE_MODE_STAY:
			{
				set_wheel_speed(0, 0);
				break;
			}

			case REMOTE_MODE_LEFT:
			{
				turn_left(Turn_Speed, 300);
				set_move_flag_(REMOTE_MODE_STAY);
				break;
			}
			case REMOTE_MODE_RIGHT:
			{
				Turn_Right(Turn_Speed,300);
				set_move_flag_(REMOTE_MODE_STAY);
				break;
			}
		}

		/*------------------------------------------------------Check Battery-----------------------*/
		if(check_bat_stop())
		{
			set_clean_mode(Clean_Mode_Userinterface);
			break;
		}
		/*-------------------------------------------Bumper  and cliff Event-----------------------*/
		if(get_cliff_trig())
		{
			move_back();
			if(get_cliff_trig()){
				move_back();
			}
			stop_brifly();
			disable_motors();
			wav_play(WAV_ERROR_LIFT_UP);
			set_clean_mode(Clean_Mode_Userinterface);
			break;
		}
		if(get_bumper_status())
		{
			random_back();
			is_bumper_jamed();
			break;
		}
		if(get_cliff_trig() == (Status_Cliff_All)){
			quick_back(20,20);
			stop_brifly();
			if(get_cliff_trig() == (Status_Cliff_All)){
				quick_back(20,20);
				stop_brifly();
			}
			if(get_cliff_trig() == Status_Cliff_All){
				quick_back(20,20);
				stop_brifly();
				disable_motors();
				ROS_INFO("Cliff trigger three times stop robot ");
				wav_play(WAV_ERROR_LIFT_UP);
				set_clean_mode(Clean_Mode_Userinterface);
				break;
			}
		}
		/*------------------------------------------------check motor over current event ---------*/
		uint8_t octype =0;
		octype = check_motor_current();
		if(octype){
			if(self_check(octype)){
				set_clean_mode(Clean_Mode_Userinterface);
				break;
			}
		}
	}
	disable_motors();
	remote_mode_unregister_events();
}

void set_move_flag_(RemoteModeMoveType flag)
{
	move_flag_mutex.lock();
	move_flag = flag;
	move_flag_mutex.unlock();
}

RemoteModeMoveType get_move_flag_(void)
{
	RemoteModeMoveType flag;
	move_flag_mutex.lock();
	flag = move_flag;
	move_flag_mutex.unlock();
	return flag;
}

void remote_mode_register_events(void)
{
	ROS_WARN("%s %d: Register events", __FUNCTION__, __LINE__);
	event_manager_set_current_mode(EVT_MODE_REMOTE);

#define event_manager_register_and_enable_x(name, y, enabled) \
	event_manager_register_handler(y, &remote_mode_handle_ ##name); \
	event_manager_enable_handler(y, enabled);

	///* Cliff */
	//event_manager_register_and_enable_x(cliff_all, EVT_CLIFF_ALL, true);
	///* Rcon */
	//event_manager_register_and_enable_x(rcon, EVT_RCON, true);
	///* Battery */
	//event_manager_register_and_enable_x(battery_low, EVT_BATTERY_LOW, true);
	/* Key */
	event_manager_register_and_enable_x(key_clean, EVT_KEY_CLEAN, true);
	/* Remote */
	event_manager_register_and_enable_x(remote_direction_forward, EVT_REMOTE_DIRECTION_FORWARD, true);
	event_manager_register_and_enable_x(remote_direction_left, EVT_REMOTE_DIRECTION_LEFT, true);
	event_manager_register_and_enable_x(remote_direction_right, EVT_REMOTE_DIRECTION_RIGHT, true);
	event_manager_register_and_enable_x(remote_max, EVT_REMOTE_MAX, true);
	event_manager_register_and_enable_x(remote_exit, EVT_REMOTE_CLEAN, true);
	event_manager_register_and_enable_x(remote_exit, EVT_REMOTE_SPOT, true);
	event_manager_register_and_enable_x(remote_exit, EVT_REMOTE_WALL_FOLLOW, true);
	event_manager_register_and_enable_x(remote_exit, EVT_REMOTE_HOME, true);
	event_manager_enable_handler(EVT_REMOTE_PLAN, true);
	/* Charge Status */
	event_manager_register_and_enable_x(charge_detect, EVT_CHARGE_DETECT, true);
}

void remote_mode_unregister_events(void)
{
	ROS_WARN("%s %d: Unregister events", __FUNCTION__, __LINE__);
#define event_manager_register_and_disable_x(x) \
	event_manager_register_handler(x, NULL); \
	event_manager_enable_handler(x, false);

	///* Cliff */
	//event_manager_register_and_disable_x(EVT_CLIFF_ALL);
	///* Rcon */
	//event_manager_register_and_disable_x(EVT_RCON);
	///* Battery */
	//event_manager_register_and_disable_x(EVT_BATTERY_LOW);
	/* Key */
	event_manager_register_and_disable_x(EVT_KEY_CLEAN);
	/* Remote */
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_FORWARD);
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_LEFT);
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_RIGHT);
	event_manager_register_and_disable_x(EVT_REMOTE_MAX);
	event_manager_register_and_disable_x(EVT_REMOTE_CLEAN);
	event_manager_register_and_disable_x(EVT_REMOTE_SPOT);
	event_manager_register_and_disable_x(EVT_REMOTE_WALL_FOLLOW);
	event_manager_register_and_disable_x(EVT_REMOTE_HOME);
	event_manager_register_and_disable_x(EVT_REMOTE_PLAN);
	/* Charge Status */
	event_manager_register_and_disable_x(EVT_CHARGE_DETECT);
}

void remote_mode_handle_remote_direction_forward(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote forward is pressed.", __FUNCTION__, __LINE__);
	if (get_move_flag_() == REMOTE_MODE_BACKWARD)
		beep_for_command(false);
	else if (get_move_flag_() == REMOTE_MODE_STAY)
	{
		set_move_flag_(REMOTE_MODE_FORWARD);
		beep_for_command(true);
	}
	else
	{
		set_move_flag_(REMOTE_MODE_STAY);
		beep_for_command(true);
	}
	reset_rcon_remote();
}

void remote_mode_handle_remote_direction_left(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote left is pressed.", __FUNCTION__, __LINE__);
	if (get_move_flag_() == REMOTE_MODE_BACKWARD)
		beep_for_command(false);
	else if (get_move_flag_() == REMOTE_MODE_STAY)
	{
		beep_for_command(true);
		set_move_flag_(REMOTE_MODE_LEFT);
	}
	else
	{
		beep_for_command(true);
		set_move_flag_(REMOTE_MODE_STAY);
	}
	reset_rcon_remote();
}

void remote_mode_handle_remote_direction_right(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote right is pressed.", __FUNCTION__, __LINE__);
	if (get_move_flag_() == REMOTE_MODE_BACKWARD)
		beep_for_command(false);
	else if (get_move_flag_() == REMOTE_MODE_STAY)
	{
		beep_for_command(true);
		set_move_flag_(REMOTE_MODE_RIGHT);
	}
	else
	{
		beep_for_command(true);
		set_move_flag_(REMOTE_MODE_STAY);
	}
	reset_rcon_remote();
}

void remote_mode_handle_remote_max(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote max is pressed.", __FUNCTION__, __LINE__);
	beep_for_command(true);
	switch_vac_mode(true);
	reset_rcon_remote();
}

void remote_mode_handle_remote_exit(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote %x is pressed.", __FUNCTION__, __LINE__, get_rcon_remote());
	beep_for_command(true);
	disable_motors();
	if (get_rcon_remote() == Remote_Home)
		set_clean_mode(Clean_Mode_GoHome);
	else
		set_clean_mode(Clean_Mode_Userinterface);
	reset_rcon_remote();
}

void remote_mode_handle_key_clean(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Key clean is pressed.", __FUNCTION__, __LINE__);
	beep_for_command(true);
	while (get_key_press() == KEY_CLEAN)
	{
		ROS_WARN("%s %d: User hasn't release the key.", __FUNCTION__, __LINE__);
		usleep(40000);
	}
	set_clean_mode(Clean_Mode_Userinterface);
	reset_touch();
}

void remote_mode_handle_charge_detect(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Detect charging.", __FUNCTION__, __LINE__);
	if (robot::instance()->getChargeStatus() == 3)
	{
		set_clean_mode(Clean_Mode_Charging);
		disable_motors();
	}
}

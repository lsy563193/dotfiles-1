/**
  ******************************************************************************
  * @file    AI Cleaning Robot
  * @author  ILife Team Dxsong
  * @version V1.0
  * @date    17-Nov-2011
  * @brief   UserInterface Fuction
	           Display Button lights and waiting for user to select cleaning mode
						 Plan setting , set hours and minutes
  ******************************************************************************
  * <h2><center>&copy; COPYRIGHT 2011 ILife CO.LTD</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "movement.h"
#include "user_interface.h"
#include <ros/ros.h>
#include "config.h"
#include "wav.h"
#include "robot.hpp"
#include "event_manager.h"

uint8_t Temp_Mode=0;
time_t charger_signal_start_time;
uint16_t charger_signal_delay = 20;
time_t battery_low_start_time;
uint16_t battery_low_delay = 10;
uint8_t Error_Alarm_Counter = 2;
/*------------------------------------------------------------User Interface ----------------------------------*/
void User_Interface(void)
{
	static volatile uint8_t Press_time=0;
	time_t start_time;

#ifdef ONE_KEY_DISPLAY
	uint16_t LedBreathCount=100;
	uint8_t breath =1;
#endif
	bool Battery_Ready_to_clean = true;
	bool eh_status_now=false, eh_status_last=false;

	// Count for error alarm.
	Error_Alarm_Counter = 2;
	charger_signal_delay = 0;
	battery_low_delay = 0;
	start_time = time(NULL);
	Press_time=0;
	Temp_Mode=0;

	user_interface_register_events();
	Disable_Motors();
	Reset_Rcon_Remote();
	Set_Plan_Status(0);
	Reset_Stop_Event_Status();
	Reset_Rcon_Status();
	Set_VacMode(Vac_Save);

	ROS_INFO("%s,%d ,BatteryVoltage = %dmv.",__FUNCTION__,__LINE__, GetBatteryVoltage());
	// Check the battery to warn the user.
	if(!Check_Bat_Ready_To_Clean() && !robot::instance()->isManualPaused())
	{
		ROS_WARN("%s %d: Battery level low %4dmV(limit in %4dmV).", __FUNCTION__, __LINE__, GetBatteryVoltage(), (int)BATTERY_READY_TO_CLEAN_VOLTAGE);
		Battery_Ready_to_clean = false;
		wav_play(WAV_BATTERY_LOW);
	}

	while(ros::ok())
	{
		usleep(10000);

		if (event_manager_check_event(&eh_status_now, &eh_status_last) == 1) {
			continue;
		}

		if (charger_signal_delay > 0)
			charger_signal_delay--;

		if (battery_low_delay > 0)
			battery_low_delay--;

		if(!Check_Bat_Ready_To_Clean() && !robot::instance()->isManualPaused())
		{
			Battery_Ready_to_clean = false;
		}

		if(time(NULL) - start_time > USER_INTERFACE_TIMEOUT)
		{
			ROS_WARN("%s %d: Userinterface mode didn't receive any command in 10mins, go to sleep mode.", __FUNCTION__, __LINE__);
			Set_Clean_Mode(Clean_Mode_Sleep);
			break;
		}

#ifdef ONE_KEY_DISPLAY

		//ROS_INFO("One key min_distant_segment logic. odc = %d", LedBreathCount);
		if(breath){
			LedBreathCount--;
			if(LedBreathCount<=0)
				breath = 0;
		}
		else{
			LedBreathCount++;
			if(LedBreathCount >=100)
				breath = 1;
		}

		if(Get_Error_Code())
		{
			// Red
			Set_LED(0,100);
		}
		else if(!Battery_Ready_to_clean)
		{
			// Orange
			Set_LED(LedBreathCount,LedBreathCount);
		}
		else
		{
			// Green
			Set_LED(LedBreathCount,0);
		}

#endif

		// Alarm for error.
		if (Get_Error_Code())
			if ((Error_Alarm_Counter == 2 && (time(NULL) - start_time) > 10) || (Error_Alarm_Counter == 1 && (time(NULL) - start_time) > 20))
			{
				Error_Alarm_Counter--;
				Alarm_Error();
			}

		/*-------------------------------If has error, only clean key or remote key clean will reset it--------------*/
		if (Get_Error_Code() != Error_Code_None)
		{
			if (Get_Key_Press() & KEY_CLEAN)
			{
				Beep(2, 2, 0, 1);//Beep for useless remote command
				// Wait for user to release the key.
				while (Get_Key_Press() & KEY_CLEAN)
				{
					ROS_INFO("User still holds the key.");
					usleep(100000);
				}
				// Key relaesed, then the touch status and stop event status should be cleared.
				Reset_Stop_Event_Status();
				wav_play(WAV_CLEAR_ERROR);
				Set_Error_Code(Error_Code_None);
				Reset_Rcon_Remote();
			}
			continue;
		}

		/*--------------------------------------------------------If manual pause cleaning, check cliff--------------*/
		if (robot::instance()->isManualPaused())
		{
			if (Get_Cliff_Trig() & (Status_Cliff_Left|Status_Cliff_Front|Status_Cliff_Right))
			{
				ROS_WARN("%s %d: Robot lifted up during manual pause, reset manual pause status.", __FUNCTION__, __LINE__);
				wav_play(WAV_ERROR_LIFT_UP);
				Clear_Manual_Pause();
			}
		}

		/*--------------------------------------------------------Check if on the charger stub--------------*/
		if(is_on_charger_stub() || is_direct_charge())//on base but miss charging , adjust position to charge
		{
			ROS_WARN("%s %d: Detect charging.", __FUNCTION__, __LINE__);
			if (is_direct_charge())
				Temp_Mode = Clean_Mode_Charging;
			else if(Turn_Connect())
				Temp_Mode = Clean_Mode_Charging;
			Disable_Motors();
		}

		/* -----------------------------Check if key clean event ----------------------------*/
		if(Get_Key_Press() & KEY_CLEAN)//                                    Check Key Clean
		{
			Press_time=Get_Key_Time(KEY_CLEAN);
			// Long press on the clean button means let the robot go to sleep mode.
			if(Press_time>151)
			{
				ROS_INFO("%s %d: Long press and go to sleep mode.", __FUNCTION__, __LINE__);
				Beep(1, 4, 0, 1);
				usleep(100000);
				Beep(2,4,0,1);
				usleep(100000);
				Beep(3,4,0,1);
				usleep(100000);
				Beep(5,4,4,1);
				// Wait for beep finish.
				usleep(200000);
				// Wait for user to release the key.
				while (Get_Key_Press() & KEY_CLEAN)
				{
					ROS_INFO("User still holds the key.");
					usleep(100000);
				}
				// Key relaesed, then the touch status and stop event status should be cleared.
				Reset_Stop_Event_Status();
				Temp_Mode=Clean_Mode_Sleep;
			}
			else
				Temp_Mode=Clean_Mode_Navigation;
			Reset_MoveWithRemote();
		}

		if(Temp_Mode)
		{
			if((Temp_Mode==Clean_Mode_Sleep)||(Temp_Mode==Clean_Mode_Charging))
			{
				Set_Clean_Mode(Temp_Mode);
				break;
			}
			if((Temp_Mode==Clean_Mode_GoHome)||(Temp_Mode==Clean_Mode_WallFollow)||(Temp_Mode==Clean_Mode_Spot)||(Temp_Mode==Clean_Mode_RandomMode)||(Temp_Mode==Clean_Mode_Navigation)||(Temp_Mode==Clean_Mode_Remote))
			{
				ROS_INFO("[user_interface.cpp] GetBatteryVoltage = %dmV.", GetBatteryVoltage());
				if((Temp_Mode != Clean_Mode_GoHome && Temp_Mode != Clean_Mode_Remote) && !Battery_Ready_to_clean)
				{
					ROS_WARN("%s %d: Battery level low %4dmV(limit in %4dmV)", __FUNCTION__, __LINE__, robot::instance()->getBatteryVoltage(), (int)BATTERY_READY_TO_CLEAN_VOLTAGE);
					wav_play(WAV_BATTERY_LOW);
					Temp_Mode=0;
					charger_signal_delay = 0;
				}
				else
				{
					Set_Clean_Mode(Temp_Mode);
					Reset_Rcon_Remote();
					break;
				}
			}
			Temp_Mode=0;
		}
	}

	user_interface_unregister_events();
}

void user_interface_register_events(void)
{
	ROS_WARN("%s %d: Register events", __FUNCTION__, __LINE__);
	event_manager_set_current_mode(EVT_MODE_USER_INTERFACE);

#define event_manager_register_and_enable_x(name, y, enabled) \
	event_manager_register_handler(y, &user_interface_handle_ ##name); \
	event_manager_enable_handler(y, enabled);

	/* Rcon */
	event_manager_register_and_enable_x(rcon, EVT_RCON, true);
	/* Battery */
	event_manager_register_and_enable_x(battery_low, EVT_BATTERY_LOW, true);
	/* Remote */
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_DIRECTION_FORWARD, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_DIRECTION_LEFT, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_DIRECTION_RIGHT, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_CLEAN, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_SPOT, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_HOME, true);
	event_manager_register_and_enable_x(remote_cleaning, EVT_REMOTE_WALL_FOLLOW, true);
	event_manager_register_and_enable_x(remote_plan, EVT_REMOTE_PLAN, true);
}

void user_interface_unregister_events(void)
{
	ROS_WARN("%s %d: Unregister events", __FUNCTION__, __LINE__);
#define event_manager_register_and_disable_x(x) \
	event_manager_register_handler(x, NULL); \
	event_manager_enable_handler(x, false);

	/* Rcon */
	event_manager_register_and_disable_x(EVT_RCON);
	/* Battery */
	event_manager_register_and_disable_x(EVT_BATTERY_LOW);
	/* Remote */
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_FORWARD);
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_LEFT);
	event_manager_register_and_disable_x(EVT_REMOTE_DIRECTION_RIGHT);
	event_manager_register_and_disable_x(EVT_REMOTE_CLEAN);
	event_manager_register_and_disable_x(EVT_REMOTE_SPOT);
	event_manager_register_and_disable_x(EVT_REMOTE_HOME);
	event_manager_register_and_disable_x(EVT_REMOTE_WALL_FOLLOW);
	event_manager_register_and_disable_x(EVT_REMOTE_PLAN);
}

void user_interface_handle_rcon(bool state_now, bool state_last)
{
	if (robot::instance()->isManualPaused())
	{
		Reset_Rcon_Status();
		ROS_DEBUG("%s %d: User_Interface detects charger signal, but ignore for manual pause.", __FUNCTION__, __LINE__);
		return;
	}

	ROS_DEBUG("%s %d: User_Interface detects charger signal for %ds.", __FUNCTION__, __LINE__, (int)(time(NULL) - charger_signal_start_time));
	if (charger_signal_delay == 0)
		charger_signal_start_time = time(NULL);

	if (time(NULL) - charger_signal_start_time > 180)// 3 mins
	{
		if (Get_Error_Code())
		{
			ROS_WARN("%s %d: Rcon set go home not valid because of error %d.", __FUNCTION__, __LINE__, Get_Error_Code());
			Error_Alarm_Counter = 0;
			Alarm_Error();
			wav_play(WAV_BACK_TO_CHARGER_FAILED);
			charger_signal_delay = 0;
			Reset_Rcon_Status();
			return;
		}

		if(Get_Cliff_Trig() & (Status_Cliff_Left|Status_Cliff_Front|Status_Cliff_Right))
		{
			ROS_WARN("%s %d: Rcon set go home not valid because of robot lifted up.", __FUNCTION__, __LINE__);
			wav_play(WAV_ERROR_LIFT_UP);
			wav_play(WAV_BACK_TO_CHARGER_FAILED);
			charger_signal_delay = 0;
			Reset_Rcon_Status();
			return;
		}

		Temp_Mode = Clean_Mode_GoHome;
		Reset_Rcon_Status();
		return;
	}

	charger_signal_delay = 20;
	Reset_Rcon_Status();

}

void user_interface_handle_battery_low(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: User_Interface detects battery low %dmv for %ds.", __FUNCTION__, __LINE__, robot::instance()->getBatteryVoltage(), (int)(time(NULL) - battery_low_start_time));
	if (battery_low_delay == 0)
		battery_low_start_time = time(NULL);

	if (time(NULL) - battery_low_start_time > 5)// 5 seconds
	{
		Temp_Mode = Clean_Mode_Sleep;
		return;
	}

	battery_low_delay = 10;
}

void user_interface_handle_remote_cleaning(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Remote key %x has been pressed.", __FUNCTION__, __LINE__, Get_Rcon_Remote());

	if (Get_Error_Code())
	{
		if (Get_Rcon_Remote() == Remote_Clean)
		{
			ROS_WARN("%s %d: Clear the error %x.", __FUNCTION__, __LINE__, Get_Rcon_Remote());
			beep_for_command(true);
			wav_play(WAV_CLEAR_ERROR);
			Set_Error_Code(Error_Code_None);
			Error_Alarm_Counter = 0;
			Reset_Rcon_Remote();
			return;
		}
		ROS_WARN("%s %d: Remote key %x not valid because of error %d.", __FUNCTION__, __LINE__, Get_Rcon_Remote(), Get_Error_Code());
		beep_for_command(false);
		Error_Alarm_Counter = 0;
		Alarm_Error();
		Reset_Rcon_Remote();
		return;
	}

	if(Get_Cliff_Trig() & (Status_Cliff_Left|Status_Cliff_Front|Status_Cliff_Right))
	{
		ROS_WARN("%s %d: Remote key %x not valid because of robot lifted up.", __FUNCTION__, __LINE__, Get_Rcon_Remote());
		beep_for_command(false);
		wav_play(WAV_ERROR_LIFT_UP);
		Reset_Rcon_Remote();
		return;
	}

	beep_for_command(true);
	switch (Get_Rcon_Remote())
	{
		case Remote_Forward:
		case Remote_Left:
		case Remote_Right:
		{
			Temp_Mode = Clean_Mode_Remote;
			break;
		}
		case Remote_Clean:
		{
			Temp_Mode = Clean_Mode_Navigation;
			break;
		}
		case Remote_Spot:
		{
			Temp_Mode = Clean_Mode_Spot;
			break;
		}
		case Remote_Home:
		{
			Temp_Mode = Clean_Mode_GoHome;
			break;
		}
		case Remote_Wall_Follow:
		{
			Temp_Mode = Clean_Mode_WallFollow;
			break;
		}
	}
	Reset_Rcon_Remote();
}

void user_interface_handle_remote_plan(bool state_now, bool state_last)
{
	ROS_DEBUG("%s %d: Remote key plan has been pressed.", __FUNCTION__, __LINE__);
	/* -----------------------------Check if plan event ----------------------------------*/
	switch (Get_Plan_Status())
	{
		case 1:
		{
			ROS_WARN("%s %d: Remote key plan has been pressed. Plan received.", __FUNCTION__, __LINE__);
			beep_for_command(true);
			break;
		}
		case 2:
		{
			ROS_WARN("%s %d: Plan canceled.", __FUNCTION__, __LINE__);
			wav_play(WAV_CANCEL_APPOINTMENT);
			break;
		}
		case 3:
		{
			ROS_WARN("%s %d: Plan activated.", __FUNCTION__, __LINE__);
			if (Get_Error_Code() != Error_Code_None)
			{
				ROS_INFO("%s %d: Error exists, so cancel the appointment.", __FUNCTION__, __LINE__);
				Alarm_Error();
				wav_play(WAV_CANCEL_APPOINTMENT);
				Set_Plan_Status(0);
				break;
			}
			else if(Get_Cliff_Trig() & (Status_Cliff_Left|Status_Cliff_Front|Status_Cliff_Right))
			{
				ROS_WARN("%s %d: Plan not activated not valid because of robot lifted up.", __FUNCTION__, __LINE__);
				wav_play(WAV_ERROR_LIFT_UP);
				wav_play(WAV_CANCEL_APPOINTMENT);
				Set_Plan_Status(0);
				break;
			}
			else
			{
				// Sleep for 50ms cause the status 3 will be sent for 3 times.
				usleep(50000);
				if (!robot::instance()->isManualPaused())
					Temp_Mode=Clean_Mode_Navigation;
				break;
			}
		}
		case 4:
		{
			ROS_WARN("%s %d: Plan confirmed.", __FUNCTION__, __LINE__);
			wav_play(WAV_APPOINTMENT_DONE);
			break;
		}
	}

	Set_Plan_Status(0);
}

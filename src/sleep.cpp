#include <stdint.h>
#include <unistd.h>
#include <ros/ros.h>

#include "sleep.h"
#include "movement.h"
#include "wav.h"
#include "event_manager.h"

uint8_t sleep_plan_reject_reason = 0; // 1 for error exist, 2 for robot lifted up, 3 for battery low, 4 for key clean clear the error.
bool sleep_rcon_triggered = false;
/*----------------------------------------------------------------Sleep mode---------------------------*/
void sleep_mode(void)
{
	time_t check_battery_time = time(NULL);
	bool eh_status_now=false, eh_status_last=false;
	sleep_plan_reject_reason = 0;
	sleep_rcon_triggered = false;

	beep(1, 80, 0, 1);
	usleep(100000);
	beep(2, 80, 0, 1);
	usleep(100000);
	beep(3, 80, 0, 1);
	usleep(100000);
	beep(4, 80, 0, 1);
	usleep(100000);
	set_led_mode(LED_STEADY, LED_OFF);

	disable_motors();
	set_main_pwr_byte(POWER_DOWN);
	usleep(20000);
	ROS_INFO("%s %d,power status %u ",__FUNCTION__,__LINE__, get_main_pwr_byte());

	reset_stop_event_status();
	reset_rcon_status();
	reset_rcon_remote();
	reset_touch();
	set_plan_status(0);

	event_manager_reset_status();
	sleep_register_events();
	while(ros::ok())
	{
		usleep(20000);

		if (event_manager_check_event(&eh_status_now, &eh_status_last) == 1) {
			continue;
		}

		switch (sleep_plan_reject_reason)
		{
			case 1:
				alarm_error();
				wav_play(WAV_CANCEL_APPOINTMENT);
				sleep_plan_reject_reason = 0;
				break;
			case 2:
				wav_play(WAV_ERROR_LIFT_UP);
				wav_play(WAV_CANCEL_APPOINTMENT);
				sleep_plan_reject_reason = 0;
				break;
			case 3:
				wav_play(WAV_BATTERY_LOW);
				wav_play(WAV_CANCEL_APPOINTMENT);
				sleep_plan_reject_reason = 0;
				break;
		}
		/*--- Wake up events---*/
		if(g_key_clean_pressed)
			cm_set(Clean_Mode_Userinterface);
		else if(g_charge_detect)
			cm_set(Clean_Mode_Charging);
		else if(g_plan_activated)
			cm_set(Clean_Mode_Navigation);
		else if(sleep_rcon_triggered)
			cm_set(Clean_Mode_GoHome);

		if (cm_get() != Clean_Mode_Sleep)
			break;

		if (time(NULL) - check_battery_time > 30)
		{
			// Check the battery for every 30s. If battery below 12.5v, power of core board will be cut off.
			reset_sleep_mode_flag();
			ROS_WARN("Wake up robotbase to check if battery too low(<12.5v).");
			// Wait for 40ms to make sure base board has finish checking.
			usleep(40000);
			check_battery_time = time(NULL);
		}
		else if(!get_sleep_mode_flag())
			set_sleep_mode_flag();
	}

	sleep_unregister_events();

	beep(4, 80, 0, 1);
	usleep(100000);
	beep(3, 80, 0, 1);
	usleep(100000);
	beep(2, 80, 0, 1);
	usleep(100000);
	beep(1, 80, 4, 1);

	// Wait 1.5s to avoid gyro can't open if switch to navigation mode too soon after waking up.
	usleep(1500000);

	reset_rcon_status();
	reset_rcon_remote();
	reset_stop_event_status();
	set_plan_status(0);
}

void sleep_register_events(void)
{
	ROS_WARN("%s %d: Register events", __FUNCTION__, __LINE__);
	event_manager_set_current_mode(EVT_MODE_SLEEP);

#define event_manager_register_and_enable_x(name, y, enabled) \
	event_manager_register_handler(y, &sleep_handle_ ##name); \
	event_manager_enable_handler(y, enabled);

	/* Rcon */
	event_manager_register_and_enable_x(rcon, EVT_RCON, true);
	/* Remote */
	event_manager_register_and_enable_x(remote_clean, EVT_REMOTE_CLEAN, true);
	event_manager_register_and_enable_x(remote_plan, EVT_REMOTE_PLAN, true);
	/* Key */
	event_manager_register_and_enable_x(key_clean, EVT_KEY_CLEAN, true);
	/* Charge Status */
	event_manager_register_and_enable_x(charge_detect, EVT_CHARGE_DETECT, true);

#undef event_manager_register_and_enable_x

	event_manager_set_enable(true);
}

void sleep_unregister_events(void)
{
	ROS_WARN("%s %d: Unregister events", __FUNCTION__, __LINE__);
#define event_manager_register_and_disable_x(x) \
	event_manager_register_handler(x, NULL); \
	event_manager_enable_handler(x, false);

	/* Rcon */
	event_manager_register_and_disable_x(EVT_RCON);
	/* Remote */
	event_manager_register_and_disable_x(EVT_REMOTE_CLEAN);
	event_manager_register_and_disable_x(EVT_REMOTE_PLAN);
	/* Key */
	event_manager_register_and_disable_x(EVT_KEY_CLEAN);
	/* Charge Status */
	event_manager_register_and_disable_x(EVT_CHARGE_DETECT);
#undef event_manager_register_and_disable_x

	event_manager_set_enable(false);
}

void sleep_handle_rcon(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Waked up by rcon signal.", __FUNCTION__, __LINE__);
	if (get_error_code() == Error_Code_None)
	{
		set_main_pwr_byte(Clean_Mode_GoHome);
		sleep_rcon_triggered = true;
	}
	reset_sleep_mode_flag();
}

void sleep_handle_remote_clean(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Waked up by remote key clean.", __FUNCTION__, __LINE__);
	set_main_pwr_byte(Clean_Mode_Userinterface);
	g_key_clean_pressed = true;
	reset_sleep_mode_flag();
}

void sleep_handle_remote_plan(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Waked up by plan.", __FUNCTION__, __LINE__);
	if (get_plan_status() == 3)
	{
		if (get_error_code() != Error_Code_None)
		{
			ROS_WARN("%s %d: Error exists, so cancel the appointment.", __FUNCTION__, __LINE__);
			sleep_plan_reject_reason = 1;
		}
		else if(get_cliff_status() & (BLOCK_LEFT|BLOCK_FRONT|BLOCK_RIGHT))
		{
			ROS_WARN("%s %d: Plan not activated not valid because of robot lifted up.", __FUNCTION__, __LINE__);
			sleep_plan_reject_reason = 2;
		}
		else
		{
			set_main_pwr_byte(Clean_Mode_Navigation);
			g_plan_activated = true;
		}
	}
	reset_sleep_mode_flag();
	set_plan_status(0);
}

void sleep_handle_key_clean(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Waked up by key clean.", __FUNCTION__, __LINE__);
	g_key_clean_pressed = true;
	set_main_pwr_byte(Clean_Mode_Userinterface);
	reset_sleep_mode_flag();
	usleep(20000);

	beep_for_command(VALID);

	while (get_key_press() & KEY_CLEAN)
		usleep(20000);

	ROS_WARN("%s %d: Key clean is released.", __FUNCTION__, __LINE__);
	reset_touch();
}

void sleep_handle_charge_detect(bool state_now, bool state_last)
{
	ROS_WARN("%s %d: Detect charge!", __FUNCTION__, __LINE__);
	set_main_pwr_byte(Clean_Mode_Charging);
	g_charge_detect = true;
	reset_sleep_mode_flag();
}

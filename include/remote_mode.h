/**
  ******************************************************************************
  * @file    AI Cleaning Robot
  * @author  ILife Team Dxsong
  * @version V0.0
  * @date    11-July-2011
  * @brief   System Initialize
  * @define a lot of IO function for a easier look
  ******************************************************************************
  * Initialize the System Clock.ADC converter.EXTI.Timer and USART3
  * <h2><center>&copy; COPYRIGHT 2011 ILife CO.LTD</center></h2>
  ******************************************************************************
  */

#ifndef __Remote_Mode_H
#define __Remote_Mode_H

typedef enum {
	REMOTE_MODE_FORWARD = 0,
	REMOTE_MODE_LEFT,
	REMOTE_MODE_RIGHT,
	REMOTE_MODE_BACKWARD,
	REMOTE_MODE_STAY,
} RemoteModeMoveType;

void Remote_Mode(void);
void set_move_flag_(RemoteModeMoveType flag);
RemoteModeMoveType get_move_flag_(void);
void remote_mode_register_events(void);
void remote_mode_unregister_events(void);

#define define_remote_mode_handle_func(name) \
	void remote_mode_handle_ ## name(bool state_now, bool state_last);

/* Bumper */
define_remote_mode_handle_func(bumper)
///* OBS */
//define_remote_mode_handle_func(obs)
///* Cliff */
//define_remote_mode_handle_func(cliff_all)
///* Over Current */
//define_remote_mode_handle_func(over_current_brush_left)
//define_remote_mode_handle_func(over_current_brush_right)
//define_remote_mode_handle_func(over_current_wheel_left)
//define_remote_mode_handle_func(over_current_wheel_right)
//define_remote_mode_handle_func(over_current_suction)
/* Key */
define_remote_mode_handle_func(key_clean)
/* Remote */
define_remote_mode_handle_func(remote_direction_forward)
define_remote_mode_handle_func(remote_direction_left)
define_remote_mode_handle_func(remote_direction_right)
define_remote_mode_handle_func(remote_exit)
define_remote_mode_handle_func(remote_max)
///* Battery */
//define_remote_mode_handle_func(battery_low)
/* Charge Status */
define_remote_mode_handle_func(charge_detect)

#endif /*----Behaviors------*/

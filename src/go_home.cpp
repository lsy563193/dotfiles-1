#include <ros/ros.h>
#include <ros/console.h>
#include <stdio.h>
#include <wav.h>

#include "movement.h"
#include "go_home.hpp"
#include "robot.hpp"
#include "gyro.h"
#include "random_runing.h"
#include "core_move.h"
#include "event_manager.h"
#include "charger.hpp"

const uint16_t TURN_SPEED = 18;
/*----------------------------------------------------------------GO Home  ----------------*/
uint8_t g_dir_around_cs = 0;
uint8_t g_dir_check_position = 0;
uint8_t g_position_far = 1;
/*	meaning of g_go_home_state_now 		*
 *	0: go_home				main_while	*
 *	1: around_chargestation	initial		*
 *	2: around_chargestation	main_while	*
 *	3: check_position		initial		*
 *	4: check_position		main_while	*
 *	5: by_path				initial		*
 *	6: by_path				main_while	*
 *	7: move back 						*/
uint8_t g_go_home_state_now = 0;
uint8_t g_go_home_state_last = 0;
bool g_bumper_left = false, g_bumper_right = false;
void go_home(void)
{
	/*	meaning of entrance_to_check_position	*
	 *	0: around_chargestation					*
	 *	1: by_path								*/
	uint8_t entrance_to_check_position = 0;
	uint8_t entrance_to_move_back = 0;
	uint32_t receive_code = 0;
	uint8_t ret = 0;
	/*---variable for around_chargestation---*/
	uint8_t signal_counter=0;
	uint32_t no_signal_counter=0;
	uint32_t n_around_lrsignal=0;
	uint8_t bumper_counter=0;
	uint8_t cliff_counter = 0;
	/*---variable for by_path---*/
	uint32_t temp_code =0 ;
	uint16_t nosignal_counter=0;
	uint8_t temp_check_position=0;
	uint8_t near_counter=0;
	uint8_t side_counter=0;
	bool eh_status_now=false, eh_status_last=false;
	/*---variable for turn_connect---*/
	int16_t tc_target_angle;
	int8_t tc_speed = 5;

	reset_rcon_status();
	// This is for calculating the robot turning.
	float current_angle;
	float last_angle;
	float angle_offset;
	// This step is for counting angle change when the robot turns.
	float gyro_step = 0;

	g_go_home_state_now = 0;
	g_go_home_state_last = 0;
	set_led(100, 100);
	set_side_brush_pwm(30, 30);
	set_main_brush_pwm(30);

	stop_brifly();
	reset_rcon_status();
	// Save the start angle.
	last_angle = robot::instance()->getAngle();
	// Enable the charge function
	set_start_charge();

	go_home_register_events();

	while(gyro_step < 360)
	{
		if(event_manager_check_event(&eh_status_now, &eh_status_last) == 1)
		{
			continue;
		}
		if(g_charge_detect && g_go_home_state_now != 4)
		{
			g_charge_detect = 0;
			disable_motors();
			set_clean_mode(Clean_Mode_Charging);
			break;
		}
		if(g_key_clean_pressed || g_cliff_all_triggered)
		{
			g_key_clean_pressed = false;
			set_clean_mode(Clean_Mode_Userinterface);
			disable_motors();
			if (! robot::instance()->isLowBatPaused())
				if (! robot::instance()->isManualPaused())
				{
					g_key_clean_pressed = 0;
					g_cliff_all_triggered = false;
				}
			break;
		}						
		if(g_go_home_state_now == 0)
		{
			if(g_bumper_left || g_bumper_right)
			{
				ROS_INFO("bumper back 0");
				random_back();
				g_bumper_left = false;
				g_bumper_right = false;
			}
			receive_code = get_rcon_status();
			if(receive_code&RconFL_HomeR)//FL H_R
			{
				ROS_INFO("Start with FL-R.");
				turn_left(TURN_SPEED, 900);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR_HomeL)//FR H_L
			{
				ROS_INFO("Start with FR-L.");
				Turn_Right(TURN_SPEED,900);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconFL_HomeL)//FL H_L
			{
				ROS_INFO("Start with FL-L.");
				Turn_Right(TURN_SPEED,900);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR_HomeR)//FR H_R
			{
				ROS_INFO("Start with FR-R.");
				turn_left(TURN_SPEED, 900);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFL2_HomeR)//FL2 H_R
			{
				ROS_INFO("Start with FL2-R.");
				turn_left(TURN_SPEED, 850);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR2_HomeL)//FR2 H_L
			{
				ROS_INFO("Start with FR2-L.");
				Turn_Right(TURN_SPEED,850);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconFL2_HomeL)//FL2 H_L
			{
				ROS_INFO("Start with FL2-L.");
				Turn_Right(TURN_SPEED,600);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR2_HomeR)//FR2 H_R
			{
				ROS_INFO("Start with FR2-R.");
				turn_left(TURN_SPEED, 600);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconL_HomeL)// L  H_L
			{
				ROS_INFO("Start with L-L.");
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconR_HomeR)// R  H_R
			{
				ROS_INFO("Start with R-R.");
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconL_HomeR)// L  H_R
			{
				ROS_INFO("Start with L-R.");
				turn_left(TURN_SPEED, 1500);
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconR_HomeL)// R  H_L
			{
				ROS_INFO("Start with R-L.");
				Turn_Right(TURN_SPEED,1500);
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
	/*--------------------------HomeT-----------------*/
			if(receive_code&RconFL_HomeT)//FL H_T
			{
				ROS_INFO("Start with FL-T.");
				Turn_Right(TURN_SPEED,600);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR_HomeT)//FR H_T
			{
				ROS_INFO("Start with FR-T.");
				Turn_Right(TURN_SPEED,800);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconFL2_HomeT)//FL2 H_T
			{
				ROS_INFO("Start with FL2-T.");
				Turn_Right(TURN_SPEED,600);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconFR2_HomeT)//FR2 H_T
			{
				ROS_INFO("Start with FR2-T.");
				Turn_Right(TURN_SPEED,800);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}

			if(receive_code&RconL_HomeT)// L  H_T
			{
				ROS_INFO("Start with L-T.");
				Turn_Right(TURN_SPEED,1200);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if(receive_code&RconR_HomeT)// R  H_T
			{
				ROS_INFO("Start with R-T.");
				Turn_Right(TURN_SPEED,1200);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}

	/*--------------BL BR---------------------*/
			if((receive_code&RconBL_HomeL))//BL H_L    //OK
			{
				ROS_INFO("Start with BL-L.");
				turn_left(30, 800);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if((receive_code&RconBR_HomeR))//BR H_L R  //OK
			{
				ROS_INFO("Start with BR-R.");
				Turn_Right(30,800);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}

			if((receive_code&RconBL_HomeR))//BL H_R
			{
				ROS_INFO("Start with BL-R.");
				turn_left(30, 800);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if((receive_code&RconBR_HomeL))//BL H_L R
			{
				ROS_INFO("Start with BR-L.");
				Turn_Right(30,800);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}

			if((receive_code&RconBL_HomeT))//BL H_T
			{
				ROS_INFO("Start with BL-T.");
				turn_left(30, 300);
				stop_brifly();
				g_dir_around_cs = 1;
				g_go_home_state_now = 1;
				continue;
			}
			if((receive_code&RconBR_HomeT))//BR H_T
			{
				ROS_INFO("Start with BR-T.");
				Turn_Right(30,300);
				stop_brifly();
				g_dir_around_cs = 0;
				g_go_home_state_now = 1;
				continue;
			}
			current_angle = robot::instance()->getAngle();
			angle_offset = current_angle - last_angle;
			ROS_DEBUG("Current_Angle = %f, Last_Angle = %f, Angle_Offset = %f, Gyro_Step = %f.", current_angle, last_angle, angle_offset, gyro_step);
			if (angle_offset > 0)
			{
				// For passing the boundary of angle range. e.g.(179 - (-178))
				if (angle_offset >= 180)
					angle_offset -= 360;
				else
				// For sudden change of angle, normally it shouldn't turn back for a few degrees, however if something hit robot to opposit degree, we can skip that angle change.
					angle_offset = 0;
			}
			gyro_step += (-angle_offset);
			last_angle = current_angle;

			set_dir_right();
			set_wheel_speed(10, 10);
		}
		/*-----around_chargestation init-----*/
		else if(g_go_home_state_now == 1)
		{
			move_forward(9, 9);
			set_side_brush_pwm(30, 30);
			set_main_brush_pwm(30);
			set_bldc_speed(Vac_Speed_NormalL);
			reset_rcon_status();
			reset_wheel_step();
			reset_move_distance();
			ROS_INFO("%s, %d: Call Around_ChargerStation with dir = %d.", __FUNCTION__, __LINE__, g_dir_around_cs);
			g_go_home_state_now = 2;
			continue;
		}
		/*-------around_chargestation-------*/
		else if(g_go_home_state_now == 2)
		{
			if(g_cliff_triggered)
			{
				g_cliff_triggered = false;
				if(g_cliff_cnt == 0)
				{
					move_back();
					ROS_INFO("cliff back 2");
					g_cliff_cnt++;
					continue;
				}
				else
				{
					g_cliff_cnt++;
					if(g_cliff_cnt >= 3)
					{
						g_cliff_cnt = 0;
						set_clean_mode(Clean_Mode_Userinterface);
						break;
					}
				}
			}
			else
			{
				if(g_cliff_cnt != 0)
				{
					turn_left(TURN_SPEED, 1750);
					move_forward(9,9);
					set_clean_mode(Clean_Mode_GoHome);
					g_cliff_cnt = 0;
					break;
				}
			}
			if(g_bumper_left || g_bumper_right)
			{
				ROS_INFO("bumper back 2");
				random_back();
				g_bumper_left = false;
				g_bumper_right = false;
				if(g_dir_around_cs)
					turn_left(TURN_SPEED, 1800);
				else
					Turn_Right(TURN_SPEED, 1800);
				move_forward(10,10);
				g_dir_around_cs = 1 - g_dir_around_cs;
				set_clean_mode(Clean_Mode_GoHome);
				break;
			}
			receive_code = get_rcon_status();
			if(receive_code)
			{
				no_signal_counter=0;
				reset_rcon_status();
			}
			else
			{
				no_signal_counter++;
				if(no_signal_counter>80)
				{
					ROS_WARN("No charger signal received.");
					set_clean_mode(Clean_Mode_GoHome);
					break;
				}
			}
			ROS_DEBUG("%s %d Check DIR: %d, and do something", __FUNCTION__, __LINE__, g_dir_around_cs);
			if(g_dir_around_cs == 1)//10.30
			{
				if(receive_code&RconL_HomeT)  //L_T
				{
					ROS_DEBUG("%s, %d: Detect L-T.", __FUNCTION__, __LINE__);
					move_forward(19, 5);
					usleep(100000);
				}
				else if(receive_code&RconL_HomeL)  //L_L  9 18
				{
					ROS_DEBUG("%s, %d: Detect L-L.", __FUNCTION__, __LINE__);
					move_forward(19, 5);
					usleep(100000);
				}
				else if(receive_code&RconL_HomeR)  //L_R  9 18
				{
					ROS_DEBUG("%s, %d: Detect L-R.", __FUNCTION__, __LINE__);
					move_forward(17, 9);
					usleep(100000);
				}
				else if(receive_code&RconFL2_HomeT)  //FL2_T
				{
					ROS_DEBUG("%s, %d: Detect FL2-T.", __FUNCTION__, __LINE__);
					move_forward(16, 19);
					usleep(100000);
				}
				else if(receive_code&RconFL2_HomeL)  //FL_HL
				{
					ROS_DEBUG("%s, %d: Detect FL2-L.", __FUNCTION__, __LINE__);
					move_forward(15, 11);
					usleep(100000);
				}
				else  if(receive_code&RconFL2_HomeR)//FL2_HR
				{
					ROS_DEBUG("%s, %d: Detect FL2-R.", __FUNCTION__, __LINE__);
					move_forward(9, 15);
					usleep(100000);
				}
				else if(receive_code&RconFL_HomeL)	//FR_HL
				{
					ROS_DEBUG("%s, %d: Detect FL-L.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,500);
					move_forward(5, 5);
				}
				else if(receive_code&RconFL_HomeR)	 //FR_HL
				{
					ROS_DEBUG("%s, %d: Detect FL-R.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 600);
					move_forward(5, 5);
				}
				else if(receive_code&RconFL_HomeT)	 //FR_HT
				{
					ROS_DEBUG("%s, %d: Detect FL-T.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,500);
					move_forward(5, 5);
				}
				else if(receive_code&RconFR_HomeT)	 //R_HT
				{
					ROS_DEBUG("%s, %d: Detect FR-T.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,800);
					move_forward(5, 5);
				}
				else if(receive_code&RconFR2_HomeT) //FR2_T //OK
				{
					ROS_DEBUG("%s, %d: Detect FR2-T.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,900);
					move_forward(5, 5);
				}
				else if(receive_code&RconR_HomeT)  //OK
				{
					ROS_DEBUG("%s, %d: Detect R-T.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,1100);
					move_forward(5, 5);
					g_dir_around_cs = 0;
				}
				else
				{
					ROS_DEBUG("%s, %d: Else.", __FUNCTION__, __LINE__);
					move_forward(16, 34);  //0K (16,35)	  1100
					usleep(100000);
				}

				if(receive_code&(RconFR_HomeR))
				{
					ROS_DEBUG("%s, %d: Detect FR-R, call By_Path().", __FUNCTION__, __LINE__);
					g_go_home_state_now = 5;
					continue;
				}
				if(receive_code&(RconFL_HomeR))
				{
					ROS_DEBUG("%s, %d: Detect FL-R, call By_Path().", __FUNCTION__, __LINE__);
					g_go_home_state_now = 5;
					continue;
				}
				if(receive_code&(RconL_HomeR))
				{
					ROS_DEBUG("%s, %d: Detect L-R, call By_Path().", __FUNCTION__, __LINE__);
					signal_counter++;
					n_around_lrsignal=0;
					if(signal_counter>0)
					{
						ROS_DEBUG("%s %d signal_counter>0, check position.", __FUNCTION__, __LINE__);
						signal_counter=0;
						stop_brifly();
						g_dir_check_position = ROUND_LEFT;
						g_go_home_state_last = 0;
						g_go_home_state_now = 3;
						continue;
					}
				}
				else
				{
					n_around_lrsignal++;
					if(n_around_lrsignal>4)
					{
						if(signal_counter>0)signal_counter--;
					}
				}
			}
			else
			{
				if(receive_code&RconR_HomeT)   // OK ,(10,26)
				{
					ROS_DEBUG("%s %d Detect R-T.", __FUNCTION__, __LINE__);
					move_forward(5, 19);
					usleep(100000);
				}
				else if(receive_code&RconR_HomeR)  //ok 18 13
				{
					ROS_DEBUG("%s %d Detect R-R.", __FUNCTION__, __LINE__);
					move_forward(5, 19);
					usleep(100000);
				}
				else if(receive_code&RconR_HomeL)  //ok 18 13
				{
					ROS_DEBUG("%s %d Detect R-L.", __FUNCTION__, __LINE__);
					move_forward(9, 17);
					usleep(100000);
				}
				else if(receive_code&RconFR2_HomeT)   //turn left
				{
					ROS_DEBUG("%s %d Detect FR2-T.", __FUNCTION__, __LINE__);
					move_forward(19, 17);
					usleep(100000);
				}
				else if(receive_code&RconFR2_HomeR)  //OK
				{
					ROS_DEBUG("%s %d Detect FR2-R.", __FUNCTION__, __LINE__);
					move_forward(11, 15);
					usleep(100000);
				}
				else if(receive_code&RconFR2_HomeL)  //
				{
					ROS_DEBUG("%s, %d: Detect FL2-R.", __FUNCTION__, __LINE__);
					move_forward(15, 9);
					usleep(100000);
				}
				else if(receive_code&RconFR_HomeR)	//OK
				{
					ROS_DEBUG("%s, %d: Detect FR-R.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 500);
					move_forward(5, 5);
				}
				else if(receive_code&RconFR_HomeL)	//OK
				{
					ROS_DEBUG("%s, %d: Detect FR-L.", __FUNCTION__, __LINE__);
					Turn_Right(TURN_SPEED,600);
					move_forward(5, 5);
				}
				else if(receive_code&RconFR_HomeT)	//ok
				{
					ROS_DEBUG("%s, %d: Detect FR-T.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 500);
					move_forward(5, 5);
				}
				else if(receive_code&RconFL_HomeT)	//OK
				{
					ROS_DEBUG("%s, %d: Detect FL-T.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 800);
					move_forward(5, 5);
				}
				else if(receive_code&RconFL2_HomeT)  //OK
				{
					ROS_DEBUG("%s, %d: Detect FL2-T.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 900);
					move_forward(5, 5);
				}
				else if(receive_code&RconL_HomeT)  //OK
				{
					ROS_DEBUG("%s, %d: Detect L-T.", __FUNCTION__, __LINE__);
					turn_left(TURN_SPEED, 1100);
					move_forward(5, 5);
					g_dir_around_cs = 1;
				}
				else
				{
					ROS_DEBUG("%s, %d: Else.", __FUNCTION__, __LINE__);
					move_forward(34, 16);
					usleep(100000);
				}

				if(receive_code&(RconFL_HomeL))
				{
					ROS_DEBUG("%s, %d: Detect FL-L, call By_Path().", __FUNCTION__, __LINE__);
					g_go_home_state_now = 5;
					continue;
				}
				if(receive_code&(RconFR_HomeL))
				{
					ROS_DEBUG("%s, %d: Detect FR-L, call By_Path().", __FUNCTION__, __LINE__);
					g_go_home_state_now = 5;
					continue;
				}
				if((receive_code&(RconR_HomeL)))
				{
					ROS_DEBUG("%s, %d: Detect R-L, call By_Path().", __FUNCTION__, __LINE__);
					n_around_lrsignal=0;
					signal_counter++;
					if(signal_counter>0)
					{
						ROS_DEBUG("%s %d Signal_Counter>0, check position.", __FUNCTION__, __LINE__);
						signal_counter=0;
						stop_brifly();
						g_dir_check_position = ROUND_RIGHT;
						g_go_home_state_last = 0;
						g_go_home_state_now = 3;
						continue;
					}
				}
				else
				{
					n_around_lrsignal++;
					if(n_around_lrsignal>4)
					{
						if(signal_counter>0)signal_counter--;
					}
				}
			}
		}
		/*---check_position initial---*/
		else if(g_go_home_state_now == 3)
		{
			receive_code = 0;
			gyro_step = 0;
			if(g_dir_check_position == ROUND_LEFT)
			{
				ROS_DEBUG("Check position Dir = left");
				set_dir_left();
			}
			else if(g_dir_check_position == ROUND_RIGHT)
			{
				ROS_DEBUG("Check position Dir = right");
				set_dir_right();
			}
			set_wheel_speed(10, 10);

			last_angle = robot::instance()->getAngle();
			ROS_DEBUG("Last_Angle = %f.", last_angle);
			g_go_home_state_now = 4;
			continue;
		}
		/*---check_position main while---*/
		else if(g_go_home_state_now == 4)
		{
			if(g_charge_detect)
			{
				g_charge_detect = 0;
				if(g_charge_detect_cnt == 0)g_charge_detect_cnt++;
				else
				{
					disable_motors();
					set_clean_mode(Clean_Mode_Charging);
					break;
				}
			}
			else if(g_charge_detect_cnt > 0)
			{
				g_charge_detect_cnt = 0;
				ret = turn_connect();
				if(ret == 1)
				{
					disable_motors();
					set_clean_mode(Clean_Mode_Charging);
					break;
				}
				else if(ret == 2)
				{
					ROS_INFO("ret == 2 in check position");
					break;
				}
				else
				{
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(0);
					////Back(30,800);
					//Back(30,300);
					ROS_INFO("turn_connect back 4");
					quick_back(30,300);
					set_main_brush_pwm(30);
					stop_brifly();
				}
			}
			ROS_INFO("check_position main while");
			if(gyro_step < 360)
			{
				current_angle = robot::instance()->getAngle();
				angle_offset = current_angle - last_angle;
				ROS_DEBUG("Current_Angle = %f, Last_Angle = %f, Angle_Offset = %f, Gyro_Step = %f.", current_angle, last_angle, angle_offset, gyro_step);
				if (g_dir_check_position == ROUND_LEFT)
				{
					if (angle_offset < 0)
						angle_offset += 360;
					gyro_step += angle_offset;
				}
				if (g_dir_check_position == ROUND_RIGHT)
				{
					if (angle_offset > 0)
						angle_offset -= 360;
					gyro_step += (-angle_offset);
				}
				last_angle = current_angle;

				receive_code = (get_rcon_status()&(RconL_HomeL|RconL_HomeR|RconFL_HomeL|RconFL_HomeR|RconR_HomeL|RconR_HomeR|RconFR_HomeL|RconFR_HomeR));
				ROS_DEBUG("Check_Position get_rcon_status() == %x, R... == %x, receive code: %x.", get_rcon_status(), (RconL_HomeL|RconL_HomeR|RconFL_HomeL|RconFL_HomeR|RconR_HomeL|RconR_HomeR|RconFR_HomeL|RconFR_HomeR), receive_code);
				if(receive_code)
				{
					reset_rcon_status();
					if (receive_code & RconL_HomeL)ROS_DEBUG("Check_Position get L-L");
					if (receive_code & RconL_HomeR)ROS_DEBUG("Check_Position get L-R");
					if (receive_code & RconFL_HomeL)ROS_DEBUG("Check_Position get FL-L");
					if (receive_code & RconFL_HomeR)ROS_DEBUG("Check_Position get FL-R");
					if (receive_code & RconR_HomeL)ROS_DEBUG("Check_Position get R-L");
					if (receive_code & RconR_HomeR)ROS_DEBUG("Check_Position get R-R");
					if (receive_code & RconFR_HomeL)ROS_DEBUG("Check_Position get FR-L");
					if (receive_code & RconFR_HomeR)ROS_DEBUG("Check_Position get FR-R");
				}

				if(g_dir_check_position == ROUND_LEFT)
				{
					if(receive_code & (RconFR_HomeL|RconFR_HomeR))
					{
						g_go_home_state_now = 5;
						continue;
					}
				}
				if(g_dir_check_position == ROUND_RIGHT)
				{
					if(receive_code & (RconFL_HomeL|RconFL_HomeR))
					{
						g_go_home_state_now = 5;
						continue;
					}
				}
			}
			if(gyro_step >= 360)
			{
				if(entrance_to_check_position == 0)
					g_go_home_state_now = 2;
				else
				{
					ROS_INFO("%s, %d: Robot can't see charger, return to gohome mode.", __FUNCTION__, __LINE__);
					stop_brifly();
					Turn_Right(TURN_SPEED,1000);
					stop_brifly();
					move_forward(10, 10);
					set_clean_mode(Clean_Mode_GoHome);
					break;
				}
			}
		}
		/*---by_path initial---*/
		else if(g_go_home_state_now == 5)
		{
			receive_code=0;
			temp_code =0 ;
			g_position_far=1;
			nosignal_counter=0;
			temp_check_position=0;
			near_counter=0;
			bumper_counter=0;
			side_counter=0;

			reset_stop_event_status();
			set_start_charge();
			move_forward(9, 9);
			set_side_brush_pwm(30, 30);
			set_main_brush_pwm(30);
			set_bldc_speed(Vac_Speed_NormalL);

			g_go_home_state_now = 6;
			continue;
		}
		/*---by_path main while---*/
		else if(g_go_home_state_now == 6)
		{
			ROS_INFO("by_path main while");
			if(g_charge_detect)
			{
				g_charge_detect = 0;
				if(g_charge_detect_cnt == 0)g_charge_detect_cnt++;
				else
				{
					disable_motors();
					set_clean_mode(Clean_Mode_Charging);
					break;
				}
			}
			else if(g_charge_detect_cnt > 0)
			{
				g_charge_detect_cnt = 0;
				ret = turn_connect();
				if(ret == 1)
				{
					disable_motors();
					set_clean_mode(Clean_Mode_Charging);
					break;
				}
				else if(ret == 2)
				{
					break;
				}
				else
				{
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(0);
					////Back(30,800);
					//Back(30,300);
					ROS_INFO("turn_connect back 6");
					quick_back(30,300);
					set_main_brush_pwm(30);
					stop_brifly();
				}
			}
			if(g_bumper_left)
			{
				if(!g_position_far)
				{
					stop_brifly();
					ret = turn_connect();
					if(ret == 1)
					{
						disable_motors();
						set_clean_mode(Clean_Mode_Charging);
						break;
					}
					else if(ret == 2)
					{
						break;
					}
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(0);
					ROS_INFO("lbumper turn_connect back 6");
					quick_back(30,300);
					g_bumper_left = false;
					g_bumper_right = false;
					ROS_DEBUG("%d: quick_back in !position_far", __LINE__);
					set_main_brush_pwm(30);
					stop_brifly();
					if(bumper_counter>0)
					{
						 move_forward(0, 0);
						 set_clean_mode(Clean_Mode_GoHome);
						 ROS_DEBUG("%d, Return from LeftBumperTrig.", __LINE__);
						 break;
					}
				}
				else if((get_rcon_status()&(RconFL2_HomeL|RconFL2_HomeR|RconFR2_HomeL|RconFR2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR))==0)
				{
					ROS_INFO("lbumper rcon back 6");
					random_back();
					g_bumper_left = false;
					g_bumper_right = false;
					Turn_Right(TURN_SPEED,1100);
					move_forward(8, 8);
					set_clean_mode(Clean_Mode_GoHome);
					ROS_DEBUG("%d, Return from LeftBumperTrig.", __LINE__);
					break;
				}
				else
				{
					ROS_INFO("lbumper back 6");
					random_back();
					g_bumper_left = false;
					g_bumper_right = false;
					Turn_Right(TURN_SPEED,1100);
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(30);
					move_forward(8, 8);
				}
				bumper_counter++;
			}
			else if(g_bumper_right)
			{
				if(!g_position_far)
				{
					stop_brifly();
					ret = turn_connect();
					if(ret == 1)
					{
						disable_motors();
						set_clean_mode(Clean_Mode_Charging);
						break;
					}
					else if(ret == 2)
					{
						break;
					}
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(0);
					ROS_INFO("rbumper turn_connect back 6");
					quick_back(30,300);
					g_bumper_left = false;
					g_bumper_right = false;
					ROS_DEBUG("%d: quick_back in !position_far", __LINE__);
					set_main_brush_pwm(30);
					stop_brifly();
					if(bumper_counter>0)
					{
						 move_forward(0, 0);
						 set_clean_mode(Clean_Mode_GoHome);
						 ROS_DEBUG("%d, Return from LeftBumperTrig.", __LINE__);
						 break;
					}
				}
				else if((get_rcon_status()&(RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFL2_HomeL|RconFL2_HomeR|RconFR2_HomeL|RconFR2_HomeR))==0)
				{
					ROS_INFO("rbumper rcon back 6");
					random_back();
					g_bumper_left = false;
					g_bumper_right = false;
					turn_left(TURN_SPEED,1100);
					move_forward(8, 8);
					set_clean_mode(Clean_Mode_GoHome);
					ROS_DEBUG("%d, Return from LeftBumperTrig.", __LINE__);
					break;
				}
				else
				{
					ROS_INFO("rbumper back 6");
					random_back();
					g_bumper_left = false;
					g_bumper_right = false;
					turn_left(TURN_SPEED,1100);
					set_side_brush_pwm(30, 30);
					set_main_brush_pwm(30);
					move_forward(8, 8);
				}
				bumper_counter++;
			}
			if(g_cliff_triggered)
			{
				g_cliff_triggered = false;
				ROS_INFO("cleff back 6");
				move_back();
				move_back();
				turn_left(TURN_SPEED, 1750);
				move_forward(9, 9);
				set_clean_mode(Clean_Mode_GoHome);
				break;
			}

			if(entrance_to_check_position == 0)
			{
				receive_code = get_rcon_status();
				temp_code = receive_code;
				temp_code &= (	RconL_HomeL|RconL_HomeT|RconL_HomeR| \
								RconFL2_HomeL|RconFL2_HomeT|RconFL2_HomeR| \
								RconFL_HomeL|RconFL_HomeT|RconFL_HomeR| \
								RconFR_HomeL|RconFR_HomeT|RconFR_HomeR| \
								RconFR2_HomeL|RconFR2_HomeT|RconFR2_HomeR| \
								RconR_HomeL|RconR_HomeT|RconR_HomeR \
							 );
				if(receive_code)
				{
					if((receive_code&(RconFR_HomeT|RconFL_HomeT)) == (RconFR_HomeT|RconFL_HomeT))
					{
						g_position_far = 0;
						ROS_INFO("%s, %d: Robot face HomeT, g_position_far = 0.", __FUNCTION__, __LINE__);
					}
					if(receive_code&(RconFR2_HomeT|RconFL2_HomeT|RconR_HomeT|RconL_HomeT))
					{
						g_position_far = 0;
						ROS_DEBUG("%s, %d: Robot side face HomeT, g_position_far = 0.", __FUNCTION__, __LINE__);
					}
					if(receive_code&(RconFR_HomeT|RconFL_HomeT|RconFR2_HomeT|RconFL2_HomeT|RconR_HomeT|RconL_HomeT))
					{
						near_counter++;
						if(near_counter > 1)
						{
							g_position_far = 0;
							ROS_DEBUG("%s, %d: Robot near HomeT counter > 1, g_position_far = 0.", __FUNCTION__, __LINE__);
						}
						if((receive_code&(RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR)) == 0)
						{
							side_counter++;
							if(side_counter > 5)
							{
								move_forward(9, 9);
								ROS_INFO("%s, %d: Robot goes far, back to gohome mode.", __FUNCTION__, __LINE__);
								set_clean_mode(Clean_Mode_GoHome);
								break;	
							}
						}
						else
						{
							side_counter = 0;
						}
					}

					if((receive_code&(RconFL_HomeL|RconFR_HomeR))==(RconFL_HomeL|RconFR_HomeR))
					{
						ROS_DEBUG("%s, %d: Robot sees HomeL or HomeR, g_position_far = 0.", __FUNCTION__, __LINE__);
						g_position_far=0;
					}
					reset_rcon_status();
					nosignal_counter = 0;
				}
				else
				{
					near_counter = 0;
					nosignal_counter++;
					if(nosignal_counter>50)
					{
						nosignal_counter = 0;
						stop_brifly();
						g_dir_check_position = ROUND_LEFT;
						entrance_to_check_position = 1;
						g_go_home_state_now = 3;
						continue;
					}
				}
			}

			temp_code &= (RconL_HomeL|RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL|RconFR2_HomeL|RconR_HomeL| \
						RconL_HomeR|RconFL2_HomeR|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR|RconR_HomeR);
			if(g_position_far)
			{
				switch(temp_code)
				{
					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR2_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR|RconFR_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR2_R/FR_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(13, 11);
						break;

					case (RconFL2_HomeL|RconFR2_HomeR|RconFR_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR2_R/FR_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(13, 9);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 9);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 9);
						break;

					case (RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 8);
						break;

					case (RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 8);
						break;

					case (RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FR_L.", __FUNCTION__, __LINE__);
						move_forward(11, 8);
						break;

					case (RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 7);
						break;

					case (RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 7);
						break;

					case (RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 7);
						break;

					case (RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 6);
						break;

					case (RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						Turn_Right(20,350);
						stop_brifly();
						move_forward(10, 3);
						break;

					case (RconFL_HomeL|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L.", __FUNCTION__, __LINE__);
						move_forward(11, 10);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 9);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 10);
						break;

					case (RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FR_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(12, 7);
						break;

					case (RconFL2_HomeR|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_R/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 10);
						break;

					case (RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 8);
						break;

					case (RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 10);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 10);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 7);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 7);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 9);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 8);
						break;

					case (RconFL2_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(13, 9);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(13, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR_L.", __FUNCTION__, __LINE__);
						move_forward(13, 12);
						break;

					case (RconR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
							ROS_DEBUG("%s, %d: g_position_far, R_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
							Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
							break;

					case (RconR_HomeR|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,400);
						stop_brifly();
						move_forward(8, 3);
						break;

					case (RconR_HomeR|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(10, 4);
						break;

					case (RconR_HomeR|RconR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_R/R_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,450);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_R/R_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,500);
						stop_brifly();
						move_forward(9, 6);
						break;

					case (RconR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,550);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FR_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,500);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 2);
						break;

					case (RconR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 0);
						break;

					case (RconL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 1);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 6);
						break;

					case (RconR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, R_R.", __FUNCTION__, __LINE__);
						move_forward(10, 0);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(13, 12);
						break;

					case (RconFL_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(13, 12);
						break;

					case (RconFL2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(14, 4);
						break;

					case (RconFL_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(12, 11);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 13);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(9, 13);
						break;

					case (RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_R.", __FUNCTION__, __LINE__);
						move_forward(7, 11);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(9, 13);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						move_forward(8, 13);
						break;

					case (RconFL2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L.", __FUNCTION__, __LINE__);
						move_forward(8, 14);
						break;

					case (RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 350);
						stop_brifly();
						move_forward(3, 10);
						break;

					case (RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_R/FL_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(9, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL_L.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_L/FL_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(10, 12);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FL_R/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_L/FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 12);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 400);
						stop_brifly();
						move_forward(3, 8);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(4, 10);
						break;

					case (RconL_HomeR|RconL_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_R/L_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 450);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeR|RconL_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, L_R/L_L.", __FUNCTION__, __LINE__);
						turn_left(20, 500);
						stop_brifly();
						move_forward(6, 9);
						break;

					case (RconL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_R.", __FUNCTION__, __LINE__);
						turn_left(20, 550);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeR|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, L_R/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 500);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFL_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_L/FL_L.", __FUNCTION__, __LINE__);
						move_forward(2, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, L_L/FL2_L.", __FUNCTION__, __LINE__);
						move_forward(0, 9);
						break;

					case (RconR_HomeR|RconFL2_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FL2_L.", __FUNCTION__, __LINE__);
						move_forward(1, 9);
						break;

					case (RconR_HomeR|RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, R_R/FL2_L/FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(6, 9);
						break;

					case (RconL_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, L_L.", __FUNCTION__, __LINE__);
						move_forward(0, 10);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					case (RconFL_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: g_position_far, FL_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(7, 12);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: g_position_far, FL2_L/FL2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(11, 12);
						break;

					default:
						ROS_DEBUG("%s, %d: g_position_far, else:%x.", __FUNCTION__, __LINE__, temp_code);
						move_forward(10, 11);
						break;
				}
			}
			else
			{
				switch(temp_code)
				{
					case (RconFL_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL-L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 8);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(9, 9);
						break;
					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR2_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(9, 9);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR|RconFR_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR2_R/FR_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFR2_HomeR|RconFR_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR2_R/FR_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(10, 8);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 7);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 6);
						break;

					case (RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 6);
						break;

					case (RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 6);
						break;

					case (RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L.", __FUNCTION__, __LINE__);
						move_forward(7, 4);
						break;

					case (RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 3);
						break;

					case (RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 5);
						break;

					case (RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 3);
						break;

					case (RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 3);
						break;

					case (RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						Turn_Right(20,350);
						stop_brifly();
						move_forward(10, 3);
						break;

					case (RconFL_HomeL|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 6);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 7);
						break;

					case (RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FR_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(9, 4);
						break;

					case (RconFL2_HomeR|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_R/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 6);
						break;

					case (RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 7);
						break;

					case (RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 6);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 3);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 3);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_L/FR_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR_L.", __FUNCTION__, __LINE__);
						move_forward(9, 6);
						break;

					case (RconR_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,400);
						stop_brifly();
						move_forward(8, 3);
						break;

					case (RconR_HomeR|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(10, 4);
						break;

					case (RconR_HomeR|RconR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/R_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,450);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/R_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,500);
						stop_brifly();
						move_forward(9, 6);
						break;

					case (RconR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,550);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,250);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_L/FR2_L.", __FUNCTION__, __LINE__);
						Turn_Right(20,500);
						stop_brifly();
						move_forward(9, 3);
						break;

					case (RconR_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 2);
						break;

					case (RconR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 0);
						break;

					case (RconL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 1);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 6);
						break;

					case (RconR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R.", __FUNCTION__, __LINE__);
						move_forward(10, 0);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 7);
						break;

					case (RconFL_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 2);
						break;

					case (RconFL_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(9, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 10);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 9);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(6, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(6, 9);
						break;

					case (RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(6, 8);
						break;

					case (RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_R.", __FUNCTION__, __LINE__);
						move_forward(4, 7);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(3, 7);
						break;

					case (RconFL2_HomeL|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_R.", __FUNCTION__, __LINE__);
						move_forward(5, 9);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						move_forward(3, 8);
						break;

					case (RconFL2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L.", __FUNCTION__, __LINE__);
						move_forward(3, 9);
						break;

					case (RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 350);
						stop_brifly();
						move_forward(3, 10);
						break;

					case (RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(6, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(7, 9);
						break;

					case (RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_R/FL_R.", __FUNCTION__, __LINE__);
						move_forward(4, 9);
						break;

					case (RconFL_HomeL|RconFR_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(6, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L.", __FUNCTION__, __LINE__);
						move_forward(7, 9);
						break;

						ROS_DEBUG("%s, %d: !g_position_far, FL_L.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR2_L.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(6, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(3, 8);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_L/FL_R/FR2_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(3, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeL|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_L/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case (RconFL2_HomeL|RconFR_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FR_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FL_R/FR_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL_HomeL|RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_L/FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeL|RconFL_HomeL|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL_L/FR_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL_HomeR|RconFR_HomeL|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_R/FR_L/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case 0x15:
						move_forward(6, 9);break;			  //FL_R/FR_R/FR2_R
					case (RconFL_HomeR|RconFR_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL_R/FR_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(6, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 400);
						stop_brifly();
						move_forward(3, 8);
						break;

					case (RconL_HomeL|RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(4, 10);
						break;

					case (RconL_HomeR|RconL_HomeL|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_R/L_L/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 450);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeR|RconL_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, L_R/L_L.", __FUNCTION__, __LINE__);
						turn_left(20, 500);
						stop_brifly();
						move_forward(6, 9);
						break;

					case (RconL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_R.", __FUNCTION__, __LINE__);
						turn_left(20, 550);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeR|RconFL_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_R/FL_R.", __FUNCTION__, __LINE__);
						turn_left(20, 250);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeR|RconFL2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, L_R/FL2_R.", __FUNCTION__, __LINE__);
						turn_left(20, 500);
						stop_brifly();
						move_forward(3, 9);
						break;

					case (RconL_HomeL|RconFL2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, L_L/FL2_L.", __FUNCTION__, __LINE__);
						move_forward(0, 9);
						break;

					case (RconR_HomeR|RconFL2_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FL2_L.", __FUNCTION__, __LINE__);
						move_forward(1, 9);
						break;

					case (RconR_HomeR|RconFL2_HomeL|RconFL_HomeL|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, R_R/FL2_L/FL_L/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(6, 9);
						break;

					case (RconL_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, L_L.", __FUNCTION__, __LINE__);
						move_forward(0, 10);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFL_HomeR|RconFR2_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FL_R/FR2_R.", __FUNCTION__, __LINE__);
						move_forward(7, 8);
						break;

					case (RconFL2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					case (RconFL_HomeR|RconFR_HomeL):
						ROS_DEBUG("%s, %d: !g_position_far, FL_R/FR_L.", __FUNCTION__, __LINE__);
						move_forward(2, 9);
						break;

					case (RconFL2_HomeL|RconFL2_HomeR|RconFR_HomeR):
						ROS_DEBUG("%s, %d: !g_position_far, FL2_L/FL2_R/FR_R.", __FUNCTION__, __LINE__);
						move_forward(8, 9);
						break;

					default:
						ROS_DEBUG("%s, %d: !g_position_far, else:%x.", __FUNCTION__, __LINE__, temp_code);
						move_forward(7, 8);
						break;
				}
			}
		}
		usleep(500000);
	}
	if (gyro_step >= 360)
		set_clean_mode(Clean_Mode_Userinterface);

	// If robot didn't reach the charger, go back to userinterface mode.
	if(get_clean_mode() != Clean_Mode_Charging && get_clean_mode() != Clean_Mode_GoHome)
	{
		extern std::list <Point32_t> g_home_point;
		if (!stop_event() && g_home_point.empty())
		{
			set_led(100, 0);
			stop_brifly();
			wav_play(WAV_BACK_TO_CHARGER_FAILED);
		}
	}
	go_home_unregister_events();
}

/*------------------------------------------------*/
uint8_t home_check_current(void)
{
	uint8_t motor_check_code= check_motor_current();
	if(motor_check_code)
	{
		if(self_check(motor_check_code))
		{
			set_clean_mode(Clean_Mode_Userinterface);
			return 1;
		}
		else
		{
			home_motor_set();
			set_clean_mode(Clean_Mode_GoHome);
			return 1;
		}
	}
	return 0;
}

/*-------------------Turn OFF the Vaccum-----------------------------*/
void home_motor_set(void)
{
	set_bldc_speed(0);
	set_main_brush_pwm(20);
	set_side_brush_pwm(20, 20);
	move_forward(20, 20);
	//Reset_WheelSLow();
	//reset_bumper_error();

}

void go_home_register_events(void)
{
	ROS_INFO("%s, %d: Register events.", __FUNCTION__, __LINE__);
	event_manager_set_current_mode(EVT_MODE_HOME);
#define event_manager_register_and_enable_x(name, y, enabled) \
	event_manager_register_handler(y, &go_home_handle_ ##name); \
	event_manager_enable_handler(y, enabled);

	/*---charge detect---*/
	event_manager_register_and_enable_x(charge_detect, EVT_CHARGE_DETECT, true);
	/*---key---*/
	event_manager_register_and_enable_x(key_clean, EVT_KEY_CLEAN, true);
	/*---remote---*/
	event_manager_register_and_enable_x(remote_clean, EVT_REMOTE_CLEAN, true);
	event_manager_enable_handler(EVT_REMOTE_HOME, true);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_FORWARD, true);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_LEFT, true);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_RIGHT, true);
	event_manager_enable_handler(EVT_REMOTE_WALL_FOLLOW, true);
	event_manager_enable_handler(EVT_REMOTE_SPOT, true);
	event_manager_enable_handler(EVT_REMOTE_MAX, true);
	/*---cliff---*/
	event_manager_register_and_enable_x(cliff_all, EVT_CLIFF_ALL, true);
	event_manager_register_and_enable_x(cliff, EVT_CLIFF_LEFT, true);
	event_manager_register_and_enable_x(cliff, EVT_CLIFF_FRONT, true);
	event_manager_register_and_enable_x(cliff, EVT_CLIFF_RIGHT, true);
	/*---bumper---*/
	event_manager_register_and_enable_x(bumper_right, EVT_BUMPER_RIGHT, true);
	event_manager_register_and_enable_x(bumper_left, EVT_BUMPER_LEFT, true);
}

void go_home_unregister_events(void)
{
	ROS_WARN("%s, %d: Unregister events.", __FUNCTION__, __LINE__);
	#define event_manager_register_and_disable_x(x) \
	event_manager_register_handler(x, NULL); \
	event_manager_enable_handler(x, false);

	/*---charge detect---*/
	event_manager_register_and_disable_x(EVT_CHARGE_DETECT);
	/*---key---*/
	event_manager_register_and_disable_x(EVT_KEY_CLEAN);
	/*---remote---*/
	event_manager_register_and_disable_x(EVT_REMOTE_CLEAN);
	event_manager_enable_handler(EVT_REMOTE_HOME, false);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_FORWARD, false);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_LEFT, false);
	event_manager_enable_handler(EVT_REMOTE_DIRECTION_RIGHT, false);
	event_manager_enable_handler(EVT_REMOTE_WALL_FOLLOW, false);
	event_manager_enable_handler(EVT_REMOTE_SPOT, false);
	event_manager_enable_handler(EVT_REMOTE_MAX, false);
	/*---cliff---*/
	event_manager_register_and_disable_x(EVT_CLIFF_ALL);
	event_manager_register_and_disable_x(EVT_CLIFF_LEFT);
	event_manager_register_and_disable_x(EVT_CLIFF_FRONT);
	event_manager_register_and_disable_x(EVT_CLIFF_RIGHT);
	/*---bumper---*/
	event_manager_register_and_disable_x(EVT_BUMPER_RIGHT);
	event_manager_register_and_disable_x(EVT_BUMPER_LEFT);
}

void go_home_handle_charge_detect(bool state_now, bool state_last)
{
	if(g_go_home_state_now == 4)
	{
		g_charge_detect = 1;
	}
	else
	{
		if(state_now == true && state_last == true)
		{
			g_charge_detect_cnt++;
			if(g_charge_detect_cnt > 2)
			{
				g_charge_detect = 1;
			}
		}
		else
		{
			g_charge_detect_cnt = 0;
		}
	}
}

void go_home_handle_key_clean(bool state_now, bool state_last)
{
	/*---go home main while---*/
	while (get_key_press() & KEY_CLEAN)
	{
		ROS_WARN("%s %d: User hasn't release key or still cliff detected.", __FUNCTION__, __LINE__);
		usleep(20000);
	}
	g_key_clean_pressed = 1;
	reset_touch();
}

void go_home_handle_remote_clean(bool state_now, bool state_last)
{
	g_key_clean_pressed = 1;
	reset_rcon_remote();
}

void go_home_handle_cliff_all(bool state_now, bool state_last)
{
	wav_play(WAV_ERROR_LIFT_UP);
	g_cliff_all_triggered = true;
}

void go_home_handle_cliff(bool state_now, bool state_last)
{
	g_cliff_triggered = true;
}

void go_home_handle_bumper_left(bool state_now, bool state_last)
{
	static int bumper_left_cnt = 0;
	if (state_now == true && state_last == true)
	{
		bumper_left_cnt++;
		if (bumper_left_cnt > 2)
		{
			bumper_left_cnt = 0;
			g_bumper_left = true;
		}
	}
	else
	{
		bumper_left_cnt = 0;
	}
}

void go_home_handle_bumper_right(bool state_now, bool state_last)
{
	static int bumper_right_cnt = 0;
	if (state_now == true && state_last == true)
	{
		bumper_right_cnt++;
		if (bumper_right_cnt > 2)
		{
			bumper_right_cnt = 0;
			g_bumper_right = true;
		}
	}
	else
	{
		bumper_right_cnt = 0;
	}
}

uint8_t turn_connect(void)
{
	// This function is for trying turning left and right to adjust the pose of robot, so that it can charge.
	extern uint8_t g_wheel_left_direction, g_wheel_right_direction;
	int16_t target_angle;
	int8_t speed = 5;
	// Enable the switch for charging.
	set_start_charge();
	// Wait for 200ms for charging activated.
	usleep(200000);
	if(g_charge_detect)
	{
		g_charge_detect = 0;
		ROS_INFO("[movement.cpp] Reach charger without turning.");
		return 1;
	}
	// Start turning right.
	target_angle = Gyro_GetAngle() - 120;
	if (target_angle < 0)
	{
		target_angle = 3600 + target_angle;
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 1;
	set_wheel_speed(speed, speed);
	while (abs(target_angle - Gyro_GetAngle()) > 20)
	{
		if (g_charge_detect)
		{
			g_charge_detect = 0;
			disable_motors();
			// Wait for a while to decide if it is really on the charger stub.
			usleep(500000);
			if (g_charge_detect)
			{
				g_charge_detect = 0;
				ROS_INFO("[movement.cpp] Turn left reach charger.");
				return 1;
			}
			set_wheel_speed(speed, speed);
		}
		if(g_key_clean_pressed || g_cliff_all_triggered)
		{
			set_clean_mode(Clean_Mode_Userinterface);
			disable_motors();
			if (! robot::instance()->isLowBatPaused())
				if (! robot::instance()->isManualPaused())
				{
					g_key_clean_pressed = 0;
					g_cliff_all_triggered = false;
				}
			return 2;
		}
	}
	stop_brifly();
	// Start turning left.
	target_angle = Gyro_GetAngle() + 240;
	if (target_angle > 3600)
	{
		target_angle = target_angle - 3600;
	}
	g_wheel_left_direction = 1;
	g_wheel_right_direction = 0;
	set_wheel_speed(speed, speed);
	while (abs(target_angle - Gyro_GetAngle()) > 20)
	{
		if (g_charge_detect)
		{
			g_charge_detect = 0;
			disable_motors();
			// Wait for a while to decide if it is really on the charger stub.
			usleep(500000);
			if (g_charge_detect)
			{
				g_charge_detect = 0;
				ROS_INFO("[movement.cpp] Turn left reach charger.");
				return 1;
			}
			set_wheel_speed(speed, speed);
		}
		if(g_key_clean_pressed || g_cliff_all_triggered)
		{
			set_clean_mode(Clean_Mode_Userinterface);
			disable_motors();
			if (! robot::instance()->isLowBatPaused())
				if (! robot::instance()->isManualPaused())
				{
					g_key_clean_pressed = 0;
					g_cliff_all_triggered = false;
				}
			return 2;
		}
	}
	stop_brifly();

	return 0;
}


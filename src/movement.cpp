#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <ros/ros.h>
#include "robot.hpp"
#include <time.h>
#include <fcntl.h>
#include <motion_manage.h>

#include "gyro.h"
#include "movement.h"
#include "crc8.h"
#include "serial.h"
#include "robotbase.h"
#include "config.h"
#include "core_move.h"
#include "wall_follow_slam.h"
#include "wall_follow_trapped.h"
#include "wav.h"
#include "slam.h"
#include "event_manager.h"

extern uint8_t g_send_stream[SEND_LEN];

static int16_t g_left_obs_trig_value = 500;
static int16_t g_front_obs_trig_value = 500;
static int16_t g_right_obs_trig_value = 500;
volatile int16_t g_obs_trig_value = 800;
static int16_t g_leftwall_obs_trig_vale = 500;
static uint8_t g_wheel_left_direction = 0;
static uint8_t g_wheel_right_direction = 0;
static uint8_t g_remote_move_flag=0;
static uint8_t g_home_remote_flag = 0;
uint32_t g_rcon_status;
uint32_t g_average_move = 0;
uint32_t g_average_counter =0;
uint32_t g_max_move = 0;
uint32_t g_auto_work_time = 2800;
uint32_t g_room_work_time = 3600;
uint8_t g_room_mode = 0;
uint8_t g_sleep_mode_flag = 0;

static uint32_t g_wall_accelerate =0;
static int16_t g_left_wheel_speed = 0;
static int16_t g_right_wheel_speed = 0;
static uint32_t g_left_wheel_step = 0;
static uint32_t g_right_wheel_step = 0;
static uint32_t g_leftwall_step = 0;
static uint32_t g_rightwall_step = 0;

//Value for saving SideBrush_PWM
static uint16_t g_l_brush_pwm = 0;
static uint16_t g_r_brush_pwm = 0;

static int32_t g_move_step_counter = 0;
static uint32_t g_mobility_step = 0;
static uint8_t g_direction_flag=0;
// Variable for vacuum mode

volatile uint8_t g_vac_mode;
volatile uint8_t g_vac_mode_save;
static uint8_t g_cleaning_mode = 0;
static uint8_t g_sendflag=0;
static time_t g_start_work_time;
ros::Time g_lw_t,g_rw_t; // these variable is used for calculate wheel step

// Flag for homeremote
volatile uint8_t g_r_h_flag=0;

// Counter for bumper error
volatile uint8_t g_bumper_error = 0;

// Value for wall sensor offset.
volatile int16_t g_left_wall_baseline = 50;
volatile int16_t g_right_wall_baseline = 50;

// Variable for key status, key may have many key types.
volatile uint8_t g_key_status = 0;
// Variable for touch status, touch status is just for KEY_CLEAN.
volatile uint8_t g_touch_status = 0;
// Variable for remote status, remote status is just for remote controller.
volatile uint8_t g_remote_status = 0;
// Variable for stop event status.
volatile uint8_t g_stop_event_status = 0;
// Variable for plan status
volatile uint8_t g_plan_status = 0;

// Error code for exception case
volatile uint8_t g_error_code = 0;
/*----------------------- Work Timer functions--------------------------*/
void reset_start_work_time()
{
	g_start_work_time = time(NULL);
}

uint32_t get_work_time()
{
	return (uint32_t)difftime(time(NULL), g_start_work_time);
}

/*----------------------- Set error functions--------------------------*/
void set_error_code(uint8_t code)
{
	g_error_code = code;
}

uint8_t get_error_code()
{
	return g_error_code;
}

void alarm_error(void)
{
	switch (get_error_code())
	{
		case Error_Code_LeftWheel:
		{
			wav_play(WAV_ERROR_LEFT_WHEEL);
			break;
		}
		case Error_Code_RightWheel:
		{
			wav_play(WAV_ERROR_RIGHT_WHEEL);
			break;
		}
		case Error_Code_LeftBrush:
		{
			wav_play(WAV_ERROR_LEFT_BRUSH);
			break;
		}
		case Error_Code_RightBrush:
		{
			wav_play(WAV_ERROR_RIGHT_BRUSH);
			break;
		}
		case Error_Code_MainBrush:
		{
			wav_play(WAV_ERROR_MAIN_BRUSH);
			break;
		}
		case Error_Code_Fan_H:
		{
			wav_play(WAV_ERROR_SUCTION_FAN);
			break;
		}
		case Error_Code_Cliff:
		{
			wav_play(WAV_ERROR_CLIFF);
			break;
		}
		case Error_Code_Bumper:
		{
			wav_play(WAV_ERROR_BUMPER);
			break;
		}
		default:
		{
			break;
		}
	}

}

void set_left_brush_stall(uint8_t L)
{
	// Actually not used in old code
	L = L;
}

uint32_t get_right_wheel_step(void)
{
	double t,step;
	double rwsp;
	if(g_right_wheel_speed<0)
		rwsp = (double)g_right_wheel_speed*-1;
	else
		rwsp = (double)g_right_wheel_speed;
	t = (double)(ros::Time::now()-g_rw_t).toSec();
	step = rwsp*t/0.12;//origin 0.181
	g_right_wheel_step = (uint32_t)step;
	return g_right_wheel_step;
}
uint32_t get_left_wheel_step(void)
{
	double t,step;
	double lwsp;
	if(g_left_wheel_speed<0)
		lwsp = (double)g_left_wheel_speed*-1;
	else
		lwsp = (double)g_left_wheel_speed;
	t=(double)(ros::Time::now()-g_lw_t).toSec();
	step = lwsp*t/0.12;//origin 0.181
	g_left_wheel_step = (uint32_t)step;
	return g_left_wheel_step;
}

void reset_wheel_step(void)
{
	g_lw_t = ros::Time::now();
	g_rw_t = ros::Time::now();
	g_right_wheel_step = 0;
	g_left_wheel_step = 0;
}

void reset_wall_step(void)
{
	g_leftwall_step = 0;
	g_rightwall_step = 0;
}

uint32_t get_left_wall_step(void)
{
	return g_leftwall_step = get_left_wheel_step();
}

uint32_t get_right_wall_step(void)
{
	return g_rightwall_step = get_right_wheel_step();
}

void set_wheel_step(uint32_t Left, uint32_t Right)
{
	g_left_wheel_step = Left;
	g_right_wheel_step = Right;
}

int32_t get_wall_adc(int8_t dir)
{
	if (dir == 0)
	{
		return (int32_t)(int16_t) robot::instance()->getLeftWall();
	}
	else
	{
		return (int32_t)(int16_t) robot::instance()->getRightWall();
	}
}

void set_dir_backward(void)
{
	g_wheel_left_direction = 1;
	g_wheel_right_direction = 1;
}

void set_dir_forward(void)
{
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;
}

uint8_t is_encoder_fail(void)
{
	return 0;
}

void set_right_brush_stall(uint8_t R)
{
	R = R;
}

void wall_dynamic_base(uint32_t Cy)
{
	//ROS_INFO("Run wall_dynamic_base.");
	static int32_t Left_Wall_Sum_Value=0, Right_Wall_Sum_Value=0;
	static int32_t Left_Wall_Everage_Value=0, Right_Wall_Everage_Value=0;
	static int32_t Left_Wall_E_Counter=0, Right_Wall_E_Counter=0;
	static int32_t Left_Temp_Wall_Buffer=0, Right_Temp_Wall_Buffer=0;

	// Dynamic adjust for left wall sensor.
	Left_Temp_Wall_Buffer = get_wall_adc(0);
	Left_Wall_Sum_Value+=Left_Temp_Wall_Buffer;
	Left_Wall_E_Counter++;
	Left_Wall_Everage_Value=Left_Wall_Sum_Value/Left_Wall_E_Counter;

	if(ABS_Minus(Left_Wall_Everage_Value,Left_Temp_Wall_Buffer)>20)
	{
		Left_Wall_Everage_Value=0;
		Left_Wall_E_Counter=0;
		Left_Wall_Sum_Value=0;
		Left_Temp_Wall_Buffer=0;
	}
	if ((uint32_t) Left_Wall_E_Counter > Cy)
	{
		// Get the wall base line for left wall sensor.
		Left_Wall_Everage_Value += get_wall_base(0);
		if(Left_Wall_Everage_Value>300)Left_Wall_Everage_Value=300;//set a limit
		// Adjust the wall base line for left wall sensor.
		set_wall_base(0, Left_Wall_Everage_Value);
		Left_Wall_Everage_Value=0;
		Left_Wall_E_Counter=0;
		Left_Wall_Sum_Value=0;
		Left_Temp_Wall_Buffer=0;
		//ROS_INFO("Set Left Wall base value as: %d.", get_wall_base(0));
	}

	// Dynamic adjust for right wall sensor.
	Right_Temp_Wall_Buffer = get_wall_adc(1);
	Right_Wall_Sum_Value+=Right_Temp_Wall_Buffer;
	Right_Wall_E_Counter++;
	Right_Wall_Everage_Value=Right_Wall_Sum_Value/Right_Wall_E_Counter;

	if(ABS_Minus(Right_Wall_Everage_Value,Right_Temp_Wall_Buffer)>20)
	{
		Right_Wall_Everage_Value=0;
		Right_Wall_E_Counter=0;
		Right_Wall_Sum_Value=0;
		Right_Temp_Wall_Buffer=0;
	}
	if ((uint32_t) Right_Wall_E_Counter > Cy)
	{
		// Get the wall base line for right wall sensor.
		Right_Wall_Everage_Value += get_wall_base(1);
		if(Right_Wall_Everage_Value>300)Right_Wall_Everage_Value=300;//set a limit
		// Adjust the wall base line for right wall sensor.
		set_wall_base(1, Right_Wall_Everage_Value);
		Right_Wall_Everage_Value=0;
		Right_Wall_E_Counter=0;
		Right_Wall_Sum_Value=0;
		Right_Temp_Wall_Buffer=0;
		//ROS_INFO("Set Right Wall base value as: %d.", get_wall_base(0));
	}

}

void set_wall_base(int8_t dir, int32_t data)
{
	if (dir == 0)
	{
		g_left_wall_baseline = data;
	}
	else
	{
		g_right_wall_baseline = data;
	}
}
int32_t get_wall_base(int8_t dir)
{
	if(dir == 0)
	{
		return g_left_wall_baseline;
	}
	else
	{
		return g_right_wall_baseline;
	}
}

void quick_back(uint8_t speed, uint16_t distance)
{
	// The distance is for mm.
	float saved_x, saved_y;
	saved_x = robot::instance()->getOdomPositionX();
	saved_y = robot::instance()->getOdomPositionY();
	// Quickly move back for a distance.
	g_wheel_left_direction = 1;
	g_wheel_right_direction = 1;
	reset_wheel_step();
	Set_Wheel_Speed(speed, speed);
	while (sqrtf(powf(saved_x - robot::instance()->getOdomPositionX(), 2) + powf(saved_y - robot::instance()->getOdomPositionY(), 2)) < (float)distance / 1000)
	{
		ROS_DEBUG("%s %d: saved_x: %f, saved_y: %f current x: %f, current y: %f.", __FUNCTION__, __LINE__, saved_x, saved_y, robot::instance()->getOdomPositionX(), robot::instance()->getOdomPositionY());
		if (g_fatal_quit_event || g_key_clean_pressed || g_charge_detect)
			break;
		usleep(20000);
	}
	ROS_INFO("quick_back finished.");
}

void turn_left_at_init(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle + angle;
	if (target_angle >= 3600) {
		target_angle -= 3600;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Left();

	Set_Wheel_Speed(speed, speed);

	uint8_t oc=0;
	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		if(Is_Turn_Remote())
			break;
//		if(Get_Bumper_Status()){
//			Stop_Brifly();
//			WFM_move_back(120);
//			Stop_Brifly();
//			Set_Dir_Left();
//			ROS_INFO("Bumper triged when turn left, back 20mm.");
//		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d, diff = %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed,target_angle - Gyro_GetAngle());
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void turn_left(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle + angle;
	if (target_angle >= 3600) {
		target_angle -= 3600;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Left();

	Set_Wheel_Speed(speed, speed);

	uint8_t oc=0;
	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		// For GoHome(), if reach the charger stub during turning, should stop immediately.
		if (is_charge_on())
		{
			ROS_DEBUG("Reach charger while turn left.");
			Stop_Brifly();
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		//prompt for useless remote command
		if (Get_Rcon_Remote() > 0) {
			ROS_INFO("%s %d: Rcon", __FUNCTION__, __LINE__);
			if (Get_Rcon_Remote() & (Remote_Clean)) {
			} else {
				beep_for_command(false);
				Reset_Rcon_Remote();
			}
		}
		/* check plan setting*/
		if(Get_Plan_Status() == 1)
		{
			Set_Plan_Status(0);
			beep_for_command(false);
		}
		/*if(Is_Turn_Remote())
			break;*/
		if(Get_Bumper_Status()){
			break;
		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d,diff = %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed,target_angle - Gyro_GetAngle());
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void Turn_Right(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle - angle;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Right();

	Set_Wheel_Speed(speed, speed);
	uint8_t oc=0;

	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		// For GoHome(), if reach the charger stub during turning, should stop immediately.
		if (is_charge_on())
		{
			ROS_DEBUG("Reach charger while turn right.");
			Stop_Brifly();
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		//prompt for useless remote command
		if (Get_Rcon_Remote() > 0) {
			ROS_INFO("%s %d: Rcon", __FUNCTION__, __LINE__);
			if (Get_Rcon_Remote() & (Remote_Clean)) {
			} else {
				beep_for_command(false);
				Reset_Rcon_Remote();
			}
		}
		/* check plan setting*/
		if(Get_Plan_Status() == 1)
		{
			Set_Plan_Status(0);
			beep_for_command(false);
		}
		/*if(Is_Turn_Remote())
			break;*/
		if(Get_Bumper_Status()){
			break;
		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void Round_Turn_Left(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle + angle;
	if (target_angle >= 3600) {
		target_angle -= 3600;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Left();

	Set_Wheel_Speed(speed, speed);

	uint8_t oc=0;
	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		/*if(Is_Turn_Remote())
			break;*/
		if (Get_Rcon_Remote() > 0) {
			ROS_INFO("%s %d: Rcon", __FUNCTION__, __LINE__);
			if (Get_Rcon_Remote() & (Remote_Clean | Remote_Home | Remote_Max)) {
				if (Get_Rcon_Remote() & (Remote_Home | Remote_Clean)) {
					break;
				}
				if (Remote_Key(Remote_Max)) {
					Reset_Rcon_Remote();
					Switch_VacMode(true);
				}
			} else {
				beep_for_command(false);
				Reset_Rcon_Remote();
			}
		}
		/* check plan setting*/
		if(Get_Plan_Status() == 1)
		{
			Set_Plan_Status(0);
			beep_for_command(false);
		}
		if(Get_Bumper_Status()){
			Stop_Brifly();
			WFM_move_back(120);
			Stop_Brifly();
			if(Is_Bumper_Jamed())
			{
				break;
			}
			Set_Dir_Left();
		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d,diff = %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed,target_angle - Gyro_GetAngle());
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void Round_Turn_Right(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle - angle;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Right();

	Set_Wheel_Speed(speed, speed);
	uint8_t oc=0;

	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		/*if(Is_Turn_Remote())
			break;*/
		if (Get_Rcon_Remote() > 0) {
			ROS_INFO("%s %d: Rcon", __FUNCTION__, __LINE__);
			if (Get_Rcon_Remote() & (Remote_Clean | Remote_Home | Remote_Max)) {
				if (Get_Rcon_Remote() & (Remote_Home | Remote_Clean)) {
					break;
				}
				if (Remote_Key(Remote_Max)) {
					Reset_Rcon_Remote();
					Switch_VacMode(true);
				}
			} else {
				beep_for_command(false);
				Reset_Rcon_Remote();
			}
		}
		/* check plan setting*/
		if(Get_Plan_Status() == 1)
		{
			Set_Plan_Status(0);
			beep_for_command(false);
		}
		if(Get_Bumper_Status()){
			Stop_Brifly();
			WFM_move_back(120);
			Stop_Brifly();
			if(Is_Bumper_Jamed())
			{
				break;
			}
			Set_Dir_Right();
		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void WF_Turn_Right(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;
	int32_t i, j;
	float pos_x, pos_y;
	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle - angle;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Right();

	Set_Wheel_Speed(speed, speed);
	uint8_t oc=0;

	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		pos_x = robot::instance()->getPositionX() * 1000 * CELL_COUNT_MUL / CELL_SIZE;
		pos_y = robot::instance()->getPositionY() * 1000 * CELL_COUNT_MUL / CELL_SIZE;
		map_set_position(pos_x, pos_y);

		i = map_get_relative_x(Gyro_GetAngle(), CELL_SIZE_2, 0);
		j = map_get_relative_y(Gyro_GetAngle(), CELL_SIZE_2, 0);
		if (map_get_cell(MAP, count_to_cell(i), count_to_cell(j)) != BLOCKED_BOUNDARY) {
			map_set_cell(MAP, i, j, BLOCKED_OBS);
		}

		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		/*if(Is_Turn_Remote())
			break;
		 */
		
		if (Get_Rcon_Remote() > 0) {
			ROS_INFO("%s %d: Rcon", __FUNCTION__, __LINE__);
			if (Get_Rcon_Remote() & (Remote_Clean | Remote_Home | Remote_Max)) {
				if (Get_Rcon_Remote() & (Remote_Home | Remote_Clean)) {
					break;
				}
				if (Remote_Key(Remote_Max)) {
					Reset_Rcon_Remote();
					Switch_VacMode(true);
				}
			} else {
				beep_for_command(false);
				Reset_Rcon_Remote();
			}
		}

		/* check plan setting*/
		if(Get_Plan_Status() == 1)
		{
			Set_Plan_Status(0);
			beep_for_command(false);
		}
		if(Get_Bumper_Status()){
			break;
		}
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void Jam_Turn_Left(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle + angle;
	if (target_angle >= 3600) {
		target_angle -= 3600;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Left();

	Set_Wheel_Speed(speed, speed);

	uint8_t oc=0;
	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		if(!Get_Bumper_Status())
			break;
		/*if(Is_Turn_Remote())
			break;*/
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d,diff = %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed,target_angle - Gyro_GetAngle());
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}

void Jam_Turn_Right(uint16_t speed, int16_t angle)
{
	int16_t target_angle;
	int16_t gyro_angle;

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle - angle;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}
	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);

	Set_Dir_Right();

	Set_Wheel_Speed(speed, speed);
	uint8_t oc=0;

	uint8_t accurate;
	accurate = 10;
	if(speed > 30) accurate  = 30;
	while (ros::ok()) {
		if (abs(target_angle - Gyro_GetAngle()) < accurate) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 5);
		}
		else if (abs(target_angle - Gyro_GetAngle()) < 200) {
			auto speed_ = std::min((uint16_t)5,speed);
			Set_Wheel_Speed(speed_, speed_);
			//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), 10);
		}
		else {
			Set_Wheel_Speed(speed, speed);
		}
		oc= Check_Motor_Current();
		if(oc == Check_Left_Wheel || oc== Check_Right_Wheel)
			break;
		if(Stop_Event())
			break;
		if(!Get_Bumper_Status())
			break;
		/*if(Is_Turn_Remote())
			break;*/
		usleep(10000);
		//ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(0, 0);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle());
}
int32_t Get_FrontOBS(void)
{
	return (int32_t) robot::instance()->getObsFront();
}

int32_t Get_LeftOBS(void)
{
	return (int32_t) robot::instance()->getObsLeft();
}
int32_t Get_RightOBS(void)
{
	return (int32_t) robot::instance()->getObsRight();
}
uint8_t Get_Bumper_Status(void)
{
	uint8_t Temp_Status = 0;

	if (robot::instance()->getBumperLeft()) {
		Temp_Status |= LeftBumperTrig;
	}
	if (robot::instance()->getBumperRight()) {
		Temp_Status |= RightBumperTrig;
	}
	return Temp_Status;
}

uint8_t Get_Cliff_Trig(void)
{
	uint8_t Cliff_Status = 0x00;
	int16_t cl,cr,cf;
	cl = robot::instance()->getCliffLeft();
	cr = robot::instance()->getCliffRight();
	cf = robot::instance()->getCliffFront();
	if (cl < Cliff_Limit){
		Cliff_Status |= 0x01;
	}
	if (cr< Cliff_Limit){
		Cliff_Status |= 0x02;
	}
	if (cf < Cliff_Limit){
		Cliff_Status |= 0x04;
	}
	/*
	if (Cliff_Status != 0x00){
		ROS_WARN("Return Cliff status:%x.", Cliff_Status);
	}
	*/
	return Cliff_Status;
}

uint8_t Cliff_Escape(void)
{
	uint8_t count=1;
	uint8_t cc;	
	while(ros::ok()){
		cc = Get_Cliff_Trig();
		if(cc){
			if(cc==(Status_Cliff_Left|Status_Cliff_Right|Status_Cliff_Front)){
				return 1;
			}
			switch(count++){
				case 1:
						Cliff_Turn_Right(30,300);
						break;
				case 2:
						Cliff_Turn_Left(30,300);
						break;
				case 3:
						Cliff_Turn_Right(30,300);
						break;
				case 4:
						Cliff_Turn_Left(30,300);
						break;
				case 5:
						Move_Back();
						Cliff_Turn_Left(30,800);
						break;
				default:
					return 1;
			}
			
		}
		else
			return 0;
	}
	return 0;
}

uint8_t Cliff_Event(uint8_t event)
{
	uint16_t temp_adjust=0,random_factor=0;
	uint8_t d_flag = 0;
	// There is 50% chance that the temp_adjust = 450.
	//if(g_left_wheel_step%2)temp_adjust = 450;
	if(((int)ros::Time::now().toSec())%2)temp_adjust = 450;
	else temp_adjust = 0;
	// There is 33% chance that the random_factor = 1.
	//if(g_right_wheel_step%3)random_factor = 1;
	if(((int)ros::Time::now().toSec())%3)random_factor = 1;
	else random_factor = 0;

	switch(event){
		case Status_Cliff_Left:
			Cliff_Turn_Right(Turn_Speed,temp_adjust+900);
			break;
		case Status_Cliff_Right:
			Cliff_Turn_Left(Turn_Speed,temp_adjust+900);
			d_flag = 1;
			break;
		case Status_Cliff_Front:
			if(random_factor)
			{
				Cliff_Turn_Left(Turn_Speed,1200+temp_adjust);
				d_flag = 1;
			}
			else Cliff_Turn_Right(Turn_Speed,1300+temp_adjust);
			break;
		case (Status_Cliff_Left|Status_Cliff_Front):
			Cliff_Turn_Right(Turn_Speed,1650+temp_adjust);
			break;
		case (Status_Cliff_Right|Status_Cliff_Front):
			Cliff_Turn_Left(Turn_Speed,1650+temp_adjust);
			d_flag = 1;
			break;
		case (Status_Cliff_Left|Status_Cliff_Front|Status_Cliff_Right):
			Cliff_Turn_Right(Turn_Speed,1700);
			break;
		case 0:
			break;
		default:
			Cliff_Turn_Left(Turn_Speed,1800);
			d_flag = 1;
			break;
	}
	Move_Forward(30,30);
	if(d_flag)return 1;
	return 0;
}
/*-------------------------------Check if at charger stub------------------------------------*/
bool is_on_charger_stub(void)
{
	// 1: On charger stub and charging.
	// 2: On charger stub but not charging.
	if (robot::instance()->getChargeStatus() == 2 || robot::instance()->getChargeStatus() == 1)
		return true;
	else
		return false;
}

bool is_direct_charge(void)
{
	// 3: Direct connect to charge line but not charging.
	// 4: Direct connect to charge line and charging.
	if (robot::instance()->getChargeStatus() == 3 || robot::instance()->getChargeStatus() == 4)
		return true;
	else
		return false;
}

uint8_t Turn_Connect(void)
{
	// This function is for trying turning left and right to adjust the pose of robot, so that it can charge.

	int16_t target_angle;
	int8_t speed = 5;
	// Enable the switch for charging.
	set_start_charge();
	// Wait for 200ms for charging activated.
	usleep(200000);
	if (is_charge_on())
	{
		ROS_INFO("[movement.cpp] Reach charger without turning.");
		return 1;
	}
	// Start turning right.
	target_angle = Gyro_GetAngle() - 120;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 1;
	Set_Wheel_Speed(speed, speed);
	while(abs(target_angle - Gyro_GetAngle()) > 20)
	{
		if(is_charge_on())
		{
			Disable_Motors();
			// Wait for a while to decide if it is really on the charger stub.
			usleep(500000);
			if(is_charge_on())
			{
				ROS_INFO("[movement.cpp] Turn left reach charger.");
				return 1;
			}
			Set_Wheel_Speed(speed, speed);
		}
		if(Stop_Event())
		{
			ROS_INFO("%s %d: Stop_Event.", __FUNCTION__, __LINE__);
			Disable_Motors();
			return 0;
		}
	}
	Stop_Brifly();
	// Start turning left.
	target_angle = Gyro_GetAngle() + 240;
	if (target_angle > 3600) {
		target_angle = target_angle - 3600;
	}
	g_wheel_left_direction = 1;
	g_wheel_right_direction = 0;
	Set_Wheel_Speed(speed, speed);
	while(abs(target_angle - Gyro_GetAngle()) > 20)
	{
		if(is_charge_on())
		{
			Disable_Motors();
			Stop_Brifly();
			if(is_charge_on())
			{
				ROS_INFO("[movement.cpp] Turn right reach charger.");
				return 1;
			}
			Set_Wheel_Speed(speed, speed);
		}
		if(Stop_Event())
		{
			ROS_INFO("%s %d: Stop_Event.", __FUNCTION__, __LINE__);
			Disable_Motors();
			return 0;
		}
	}
	Stop_Brifly();

	return 0;
}

void SetHomeRemote(void)
{
	g_home_remote_flag = 1;
}

void Reset_HomeRemote(void)
{
	g_home_remote_flag = 0;
}

uint8_t IsHomeRemote(void)
{
	return g_r_h_flag;
}

uint8_t  Is_MoveWithRemote(void){
	return g_remote_move_flag;
}

uint8_t Is_OBS_Near(void)
{
	if(robot::instance()->getObsFront() > (g_front_obs_trig_value-200))return 1;
	if(robot::instance()->getObsRight() > (g_right_obs_trig_value-200))return 1;
	if(robot::instance()->getObsLeft() > (g_left_obs_trig_value-200))return 1;
	return 0;
}

void Reset_TempPWM(void)
{
}

void Set_Wheel_Speed(uint8_t Left, uint8_t Right)
{
	//ROS_INFO("Set wheel speed:%d, %d.", Left, Right);
	Left = Left < RUN_TOP_SPEED ? Left : RUN_TOP_SPEED;
	Right = Right < RUN_TOP_SPEED ? Right : RUN_TOP_SPEED;
	Set_LeftWheel_Speed(Left);
	Set_RightWheel_Speed(Right);

#if GYRO_DYNAMIC_ADJUSTMENT
	// Add handling for gyro dynamic adjustment.
	// If robot going straight, should turn off gyro dynamic adjustment.
	// If robot turning, should turn on gyro dynamic adjustment.
	if (abs(Left - Right) > 1)
	{
		Set_Gyro_Dynamic_On();
	}
	else
	{
		Set_Gyro_Dynamic_Off();
	}
#endif
}

void Set_LeftWheel_Speed(uint8_t speed)
{
	int16_t l_speed;
	speed = speed>RUN_TOP_SPEED?RUN_TOP_SPEED:speed;
	l_speed = (int16_t)(speed*SPEED_ALF);
	if(g_wheel_left_direction == 1)
		l_speed |=0x8000;
	g_left_wheel_speed = l_speed;
	control_set(CTL_WHEEL_LEFT_HIGH, (l_speed >> 8) & 0xff);
	control_set(CTL_WHEEL_LEFT_LOW, l_speed & 0xff);

}

void Set_RightWheel_Speed(uint8_t speed)
{
	int16_t r_speed;
	speed = speed>RUN_TOP_SPEED?RUN_TOP_SPEED:speed;
	r_speed = (int16_t)(speed*SPEED_ALF);
	if(g_wheel_right_direction == 1)
		r_speed |=0x8000;
	g_right_wheel_speed = r_speed;
	control_set(CTL_WHEEL_RIGHT_HIGH, (r_speed >> 8) & 0xff);
	control_set(CTL_WHEEL_RIGHT_LOW, r_speed & 0xff);
}

int16_t Get_LeftWheel_Speed(void)
{
	return g_left_wheel_speed;
}

int16_t Get_RightWheel_Speed(void)
{
	return g_right_wheel_speed;
}

void Work_Motor_Configure(void)
{
	// Set the vacuum to a normal mode
	Set_VacMode(Vac_Save);
	Set_Vac_Speed();

	// Trun on the main brush and side brush
	Set_SideBrush_PWM(50, 50);
	Set_MainBrush_PWM(30);
}

uint8_t Check_Motor_Current(void)
{
	static uint8_t lwheel_oc_count = 0;
	static uint8_t rwheel_oc_count = 0;
	static uint8_t vacuum_oc_count = 0;
	static uint8_t mbrush_oc_count = 0;
	uint8_t sidebrush_oc_status = 0;
	if((uint32_t) robot::instance()->getLwheelCurrent() > Wheel_Stall_Limit){
		lwheel_oc_count++;
		if(lwheel_oc_count >40){
			lwheel_oc_count =0;
			ROS_WARN("%s,%d,left wheel over current,%u mA\n",__FUNCTION__,__LINE__,(uint32_t) robot::instance()->getLwheelCurrent());
			return Check_Left_Wheel;
		}
	}
	else
		lwheel_oc_count = 0;
	if((uint32_t) robot::instance()->getRwheelCurrent() > Wheel_Stall_Limit){
		rwheel_oc_count++;
		if(rwheel_oc_count > 40){
			rwheel_oc_count = 0;
			ROS_WARN("%s,%d,right wheel over current,%u mA",__FUNCTION__,__LINE__,(uint32_t) robot::instance()->getRwheelCurrent());
			return Check_Right_Wheel;
		}
	}
	else
		rwheel_oc_count = 0;
	sidebrush_oc_status = Check_SideBrush_Stall();
	if(sidebrush_oc_status == 2){
		ROS_WARN("%s,%d,right brush over current",__FUNCTION__,__LINE__);
		return Check_Right_Brush;
	}
	if(sidebrush_oc_status == 1){
		ROS_WARN("%s,%d,left brush over current",__FUNCTION__,__LINE__);
		return Check_Left_Brush;
	}
	if(robot::instance()->getMbrushOc()){
		mbrush_oc_count++;
		if(mbrush_oc_count > 40){
			mbrush_oc_count =0;
			ROS_WARN("%s,%d,main brush over current",__FUNCTION__,__LINE__);
			return Check_Main_Brush;
		}
	}
	if(robot::instance()->getVacuumOc()){
		vacuum_oc_count++;
		if(vacuum_oc_count>40){
			vacuum_oc_count = 0;
			ROS_WARN("%s,%d,vacuum over current",__FUNCTION__,__LINE__);
			return Check_Vacuum;	
		}
	}
	return 0;
}

/*-----------------------------------------------------------Self Check-------------------*/
uint8_t Self_Check(uint8_t Check_Code)
{
	static time_t mboctime;
	static time_t vacoctime;
	static uint8_t mbrushchecking = 0;
	uint8_t Time_Out=0;
	int32_t Wheel_Current_Summary=0;
	uint8_t Left_Wheel_Slow=0;
	uint8_t Right_Wheel_Slow=0;

/*
	if(Get_Clean_Mode() == Clean_Mode_Navigation)
		cm_move_back(COR_BACK_20MM);
	else
		quick_back(30,20);
*/
	Disable_Motors();
	usleep(10000);
	/*------------------------------Self Check right wheel -------------------*/
	if(Check_Code==Check_Right_Wheel)
	{
		Right_Wheel_Slow=0;
		if(Get_Direction_Flag() == Direction_Flag_Left)
		{
			Set_Dir_Right();
		}
		else
		{
			Set_Dir_Left();
		}
		Set_Wheel_Speed(30,30);
		usleep(50000);
		Time_Out=50;
		Wheel_Current_Summary=0;
		while(Time_Out--)
		{
			Wheel_Current_Summary += (uint32_t) robot::instance()->getRwheelCurrent();
			usleep(20000);
		}
		Wheel_Current_Summary/=50;
		if(Wheel_Current_Summary>Wheel_Stall_Limit)
		{
			Disable_Motors();
			ROS_WARN("%s,%d right wheel stall maybe, please check!!\n",__FUNCTION__,__LINE__);
			set_error_code(Error_Code_RightWheel);
			alarm_error();
			return 1;

		}
		/*
		if(Right_Wheel_Slow>100)
		{
			Disable_Motors();
			set_error_code(Error_Code_RightWheel);
			return 1;
		}
		*/
		Stop_Brifly();
		//Turn_Right(Turn_Speed,1800);
	}
	/*---------------------------Self Check left wheel -------------------*/
	else if(Check_Code==Check_Left_Wheel)
	{
		Left_Wheel_Slow=0;
		if(Get_Direction_Flag() == Direction_Flag_Right)
		{
			Set_Dir_Left();
		}
		else
		{
			Set_Dir_Right();
		}
		Set_Wheel_Speed(30,30);
		usleep(50000);
		Time_Out=50;
		Wheel_Current_Summary=0;
		while(Time_Out--)
		{
			Wheel_Current_Summary += (uint32_t) robot::instance()->getLwheelCurrent();
			usleep(20000);
		}
		Wheel_Current_Summary/=50;
		if(Wheel_Current_Summary>Wheel_Stall_Limit)
		{
			Disable_Motors();
			ROS_WARN("%s %d,left wheel stall maybe, please check!!", __FUNCTION__, __LINE__);
			set_error_code(Error_Code_LeftWheel);
			alarm_error();
			return 1;
		}
		/*
		if(Left_Wheel_Slow>100)
		{
			Disable_Motors();
			set_error_code(Error_Code_RightWheel);
			return 1;
		}
		*/
		Stop_Brifly();
		//turn_left(Turn_Speed,1800);
	}
	else if(Check_Code==Check_Main_Brush)
	{
		if(!mbrushchecking){
			Set_MainBrush_PWM(0);
			mbrushchecking = 1;
			mboctime = time(NULL);
		}
		else if((uint32_t)difftime(time(NULL),mboctime)>=3){
			mbrushchecking = 0;
			set_error_code(Error_Code_MainBrush);
			Disable_Motors();
			alarm_error();
			return 1;
		}
		return 0;
	}
	else if(Check_Code==Check_Vacuum)
	{
		#ifndef BLDC_INSTALL
		ROS_INFO("%s, %d: Vacuum Over Current!!", __FUNCTION__, __LINE__);
		ROS_INFO("%d", Get_SelfCheck_Vacuum_Status());
		while(Get_SelfCheck_Vacuum_Status() != 0x10)
		{
			/*-----wait until self check begin-----*/
			Start_SelfCheck_Vacuum();
		}
		ROS_INFO("%s, %d: Vacuum Self checking", __FUNCTION__, __LINE__);
		/*-----reset command for start self check-----*/
		Reset_SelfCheck_Vacuum_Controler();
		/*-----wait for the end of self check-----*/
		while(Get_SelfCheck_Vacuum_Status() == 0x10);
		ROS_INFO("%s, %d: end of Self checking", __FUNCTION__, __LINE__);
		if(Get_SelfCheck_Vacuum_Status() == 0x20)
		{
			ROS_INFO("%s, %d: Vacuum error", __FUNCTION__, __LINE__);
			/*-----vacuum error-----*/
			set_error_code(Error_Code_Fan_H);
			Disable_Motors();
			alarm_error();
			Reset_SelfCheck_Vacuum_Controler();
			return 1;
		}
		Reset_SelfCheck_Vacuum_Controler();
		#else
		Disable_Motors();
		//Stop_Brifly();
		Set_Vac_Speed();
		usleep(100000);
		vacoctime = time(NULL);
		uint16_t tmpnoc_n = 0;
		while((uint32_t)difftime(time(NULL),vacoctime)<=3){
			if(!robot::instance()->robot_get_vacuum_oc()){
				tmpnoc_n++;
				if(tmpnoc_n>20){
					Work_Motor_Configure();
					tmpnoc_n = 0;
					return 0;
				}
			}
			usleep(50000);
		}
		set_error_code(Error_Code_Fan_H);
		Disable_Motors();
		Alarm_Error();
		return 1;
		#endif
	}
	else if(Check_Code==Check_Left_Brush)
	{
		set_error_code(Error_Code_LeftBrush);
		Disable_Motors();
		alarm_error();
		return 1;
	}
	else if(Check_Code==Check_Right_Brush)
	{
		set_error_code(Error_Code_RightBrush);
		Disable_Motors();
		alarm_error();
		return 1;
	}
	Stop_Brifly();
	Left_Wheel_Slow=0;
	Right_Wheel_Slow=0;
	Work_Motor_Configure();
	//Move_Forward(5,5);
	return 0;
}

uint8_t Get_SelfCheck_Vacuum_Status(void)
{
	return (uint8_t) robot::instance()->getVacuumSelfCheckStatus();
}
uint8_t Check_Bat_Home(void)
{
	// Check if battary is lower than the low battery go home voltage value.
	if (GetBatteryVoltage() > 0 && GetBatteryVoltage() < LOW_BATTERY_GO_HOME_VOLTAGE){
		return 1;
	}
	return 0;
}

uint8_t Check_Bat_Full(void)
{
	// Check if battary is higher than the battery full voltage value.
	if (GetBatteryVoltage() > BATTERY_FULL_VOLTAGE){
		return 1;
	}
	return 0;
}

uint8_t Check_Bat_Ready_To_Clean(void)
{
	uint16_t battery_limit;
	if (Get_Clean_Mode() == Clean_Mode_Charging)
	{
		battery_limit = BATTERY_READY_TO_CLEAN_VOLTAGE + 60;
	}
	else
	{
		battery_limit = BATTERY_READY_TO_CLEAN_VOLTAGE;
	}
	//ROS_INFO("%s %d: Battery limit is %d.", __FUNCTION__, __LINE__, battery_limit);
	// Check if battary is lower than the low battery go home voltage value.
	if (GetBatteryVoltage() >= battery_limit){
		return 1;
	}
	return 0;
}

uint8_t Get_Clean_Mode(void)
{
	return g_cleaning_mode;
}

void Set_VacMode(uint8_t mode,bool is_save)
{
	// Set the mode for vacuum.
	// The data should be Vac_Speed_Max/Vac_Speed_Normal/Vac_Speed_NormalL.
	g_vac_mode = g_vac_mode_save;
	if(mode!=Vac_Save){
		g_vac_mode = mode;
		if(is_save)
			g_vac_mode_save = g_vac_mode;
	}

	ROS_INFO("%s ,%d g_vac_mode(%d),g_vac_mode_save(%d)",__FUNCTION__,__LINE__,g_vac_mode,g_vac_mode_save);
}

void Set_BLDC_Speed(uint32_t S)
{
	// Set the power of BLDC, S should be in range(0, 100).
	S = S < 100 ? S : 100;
	control_set(CTL_VACCUM_PWR, S & 0xff);
}

void Set_Vac_Speed(void)
{
	// Set the power of BLDC according to different situation
	// Stop the BLDC if rGobot carries the water tank
	if (Is_Water_Tank()){
		Set_BLDC_Speed(0);
	}else{
		// Set the BLDC power to max if robot in max mode
		if (Get_VacMode() == Vac_Max){
			Set_BLDC_Speed(Vac_Speed_Max);
		}else{
			// If work time less than 2 hours, the BLDC should be in normal level, but if more than 2 hours, it should slow down a little bit.
			if (get_work_time() < Two_Hours){
				Set_BLDC_Speed(Vac_Speed_Normal);
			}else{
				//ROS_INFO("%s %d: Work time more than 2 hours.", __FUNCTION__, __LINE__);
				Set_BLDC_Speed(Vac_Speed_NormalL);
			}
		}
	}
}

/*--------------------------------------Obs Dynamic adjust----------------------*/
void OBS_Dynamic_Base(uint16_t Cy)
{
	static int32_t Front_OBS_Buffer = 0, Left_OBS_Buffer = 0, Right_OBS_Buffer = 0;
	static uint32_t FrontOBS_E_Counter = 0,LeftOBS_E_Counter = 0,RightOBS_E_Counter = 0;
	static int32_t FrontOBS_Everage = 0,LeftOBS_Everage = 0, RightOBS_Everage = 0;
	static int32_t FrontOBS_Sum = 0, LeftOBS_Sum = 0, RightOBS_Sum = 0;
	int16_t OBS_Diff = 350, OBS_adjust_limit = 100;

	/*---------------Front-----------------------*/
	Front_OBS_Buffer = Get_FrontOBS();

	FrontOBS_Sum += Front_OBS_Buffer;
	FrontOBS_E_Counter++;
	FrontOBS_Everage = FrontOBS_Sum / FrontOBS_E_Counter;
	if (ABS_Minus(FrontOBS_Everage, Front_OBS_Buffer) > 50) {
		FrontOBS_E_Counter = 0;
		FrontOBS_Sum = 0;
	}
	if (FrontOBS_E_Counter > Cy) {
		FrontOBS_E_Counter = 0;
		Front_OBS_Buffer = g_front_obs_trig_value - OBS_Diff;
		Front_OBS_Buffer = (FrontOBS_Everage + Front_OBS_Buffer) / 2;
		if (Front_OBS_Buffer < OBS_adjust_limit) {
			Front_OBS_Buffer = OBS_adjust_limit;
		}
		g_front_obs_trig_value = Front_OBS_Buffer + OBS_Diff;
		//ROS_INFO("Update g_front_obs_trig_value = %d.", g_front_obs_trig_value);
	}

	/*---------------Left-----------------------*/
	Left_OBS_Buffer = Get_LeftOBS();

	LeftOBS_Sum += Left_OBS_Buffer;
	LeftOBS_E_Counter++;
	LeftOBS_Everage = LeftOBS_Sum / LeftOBS_E_Counter;
	if (ABS_Minus(LeftOBS_Everage, Left_OBS_Buffer) > 50) {
		LeftOBS_E_Counter = 0;
		LeftOBS_Sum = 0;
	}
	if (LeftOBS_E_Counter > Cy) {
		LeftOBS_E_Counter = 0;
		Left_OBS_Buffer = g_left_obs_trig_value - OBS_Diff;
		Left_OBS_Buffer = (LeftOBS_Everage + Left_OBS_Buffer) / 2;
		if (Left_OBS_Buffer < OBS_adjust_limit) {
			Left_OBS_Buffer = OBS_adjust_limit;
		}
		g_left_obs_trig_value = Left_OBS_Buffer + OBS_Diff;
		//ROS_INFO("Update g_left_obs_trig_value = %d.", g_left_obs_trig_value);
	}
	/*---------------Right-----------------------*/
	Right_OBS_Buffer = Get_RightOBS();

	RightOBS_Sum += Right_OBS_Buffer;
	RightOBS_E_Counter++;
	RightOBS_Everage = RightOBS_Sum / RightOBS_E_Counter;
	if (ABS_Minus(RightOBS_Everage, Right_OBS_Buffer) > 50) {
		RightOBS_E_Counter = 0;
		RightOBS_Sum = 0;
	}
	if (RightOBS_E_Counter > Cy) {
		RightOBS_E_Counter = 0;
		Right_OBS_Buffer = g_right_obs_trig_value - OBS_Diff;
		Right_OBS_Buffer = (RightOBS_Everage + Right_OBS_Buffer) / 2;
		if (Right_OBS_Buffer < OBS_adjust_limit) {
			Right_OBS_Buffer = OBS_adjust_limit;
		}
		g_right_obs_trig_value = Right_OBS_Buffer + OBS_Diff;
		//ROS_INFO("Update g_right_obs_trig_value = %d.", g_right_obs_trig_value);
	}
}

int16_t Get_FrontOBST_Value(void)
{
	return g_front_obs_trig_value + 1700;
}

int16_t Get_LeftOBST_Value(void)
{
	return g_left_obs_trig_value + 200;
}

int16_t Get_RightOBST_Value(void)
{
	return g_right_obs_trig_value + 200;
}

uint8_t Is_WallOBS_Near(void)
{
	if (robot::instance()->getObsFront() > (g_front_obs_trig_value + 500)) {
		return 1;
	}
	if (robot::instance()->getObsRight() > (g_right_obs_trig_value + 500)) {
		return 1;
	}
	if (robot::instance()->getObsLeft() > (g_front_obs_trig_value + 1000)) {
		return 1;
	}
	if (robot::instance()->getLeftWall() > (g_leftwall_obs_trig_vale +500)){
		return 1;
	}
	return 0;
}

void Adjust_OBST_Value(void)
{
	if(robot::instance()->getObsFront() > g_front_obs_trig_value )
		g_front_obs_trig_value += 800;
	if(robot::instance()->getObsLeft() > g_left_obs_trig_value)
		g_left_obs_trig_value	+= 800;
	if(robot::instance()->getObsRight() > g_right_obs_trig_value)
		g_right_obs_trig_value += 800;
}

void Reset_OBST_Value(void)
{
	g_left_obs_trig_value = robot::instance()->getObsFront() + 1000;
	g_front_obs_trig_value = robot::instance()->getObsLeft() + 1000;
	g_right_obs_trig_value = robot::instance()->getObsRight() + 1000;
}

uint8_t Spot_OBS_Status(void)
{
	uint8_t status =0;
	if(robot::instance()->getObsLeft() > 1000)status |=Status_Left_OBS;
	if(robot::instance()->getObsRight() > 1000)status |=Status_Right_OBS;
	if(robot::instance()->getObsFront() >1500)status |=Status_Front_OBS;
	return status;
}

uint8_t Get_OBS_Status(void)
{
	uint8_t Status = 0;

	if (robot::instance()->getObsLeft() > g_left_obs_trig_value)
		Status |= Status_Left_OBS;

	if (robot::instance()->getObsFront() > g_front_obs_trig_value)
		Status |= Status_Front_OBS;

	if (robot::instance()->getObsRight() > g_right_obs_trig_value)
		Status |= Status_Right_OBS;

	return Status;
}

void Move_Forward(uint8_t Left_Speed, uint8_t Right_Speed)
{
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 0;

	Set_Wheel_Speed(Left_Speed, Right_Speed);
}

uint8_t Get_VacMode(void)
{
	// Return the vacuum mode
	return g_vac_mode;
}

void Switch_VacMode(bool is_save)
{
	// Switch the vacuum mode between Max and Normal
	if (Get_VacMode() == Vac_Normal){
		Set_VacMode(Vac_Max,is_save);
	}else{
		Set_VacMode(Vac_Normal,is_save);
	}
	// Process the vacuum mode
	Set_Vac_Speed();
}

void Set_Rcon_Status(uint32_t code)
{
	g_rcon_status = code;
}

void Reset_Rcon_Status(void)
{
	g_rcon_status = 0;
}

uint32_t Get_Rcon_Status(){
	//g_rcon_status = robot::instance()->getRcon();
	return g_rcon_status;
}

/*----------------------------------------Remote--------------------------------*/
uint8_t Remote_Key(uint8_t key)
{
	// Debug
	if (g_remote_status > 0)
	{
		ROS_DEBUG("%s, %d g_remote_status = %x",__FUNCTION__,__LINE__, g_remote_status);
	}
	if(g_remote_status & key)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
void Set_Rcon_Remote(uint8_t cmd)
{
	g_remote_status |= cmd;
}
void Reset_Rcon_Remote(void)
{
	g_remote_status = 0;
}

uint8_t Get_Rcon_Remote(void)
{
	return g_remote_status;
}

void Set_MoveWithRemote(void)
{
	g_remote_move_flag = 1;
}

void Reset_MoveWithRemote(void)
{
	g_remote_move_flag = 0;
}

uint8_t Check_Bat_SetMotors(uint32_t Vacuum_Voltage, uint32_t Side_Brush, uint32_t Main_Brush)
{
	static uint8_t low_acc=0;
	if(Check_Bat_Stop()){
		if(low_acc<255)
			low_acc++;
		if(low_acc>50){
			low_acc = 0;
			uint16_t t_vol = GetBatteryVoltage();
			uint8_t v_pwr = Vacuum_Voltage/t_vol;
			uint8_t s_pwr = Side_Brush/t_vol;
			uint8_t m_pwr = Main_Brush/t_vol;

			Set_BLDC_Speed(v_pwr);
			Set_SideBrush_PWM(s_pwr,s_pwr);
			Set_MainBrush_PWM(m_pwr);
			return 1;
			
		}
		else
			return 0;
	}
	else{
		return 0;
	}
}


void Set_Dir_Left(void)
{
	g_wheel_left_direction = 1;
	g_wheel_right_direction = 0;
}

void Set_Dir_Right(void)
{
	g_wheel_left_direction = 0;
	g_wheel_right_direction = 1;
}

void Set_LED(uint16_t G, uint16_t R)
{
	// Set the brightnesss of the LED within range(0, 100).
	G = G < 100 ? G : 100;
	R = R < 100 ? R : 100;
	control_set(CTL_LED_RED, R & 0xff);
	control_set(CTL_LED_GREEN, G & 0xff);
}

void Stop_Brifly(void)
{
	//ROS_INFO("%s %d: stopping robot.", __FUNCTION__, __LINE__);
	do {
		Set_Wheel_Speed(0, 0);
		usleep(15000);
		//ROS_INFO("%s %d: linear speed: (%f, %f, %f)", __FUNCTION__, __LINE__,
		//	robot::instance()->getLinearX(), robot::instance()->getLinearY(), robot::instance()->getLinearZ());
	} while (robot::instance()->isMoving());
	//ROS_INFO("%s %d: robot is stopped.", __FUNCTION__, __LINE__);
}

void Set_MainBrush_PWM(uint16_t PWM)
{
	// Set main brush PWM, the value of PWM should be in range (0, 100).
	PWM = PWM < 100 ? PWM : 100;
	control_set(CTL_BRUSH_MAIN, PWM & 0xff);
}

void Set_SideBrush_PWM(uint16_t L, uint16_t R)
{
	// Set left and right brush PWM, the value of L/R should be in range (0, 100).
	L = L < 100 ? L : 100 ;
	g_l_brush_pwm = L;
	control_set(CTL_BRUSH_LEFT, L & 0xff);
	R = R < 100 ? R : 100 ;
	g_r_brush_pwm = R;
	control_set(CTL_BRUSH_RIGHT, R & 0xff);
}

void Set_LeftBrush_PWM(uint16_t L)
{
	L = L < 100 ? L : 100 ;
	control_set(CTL_BRUSH_LEFT, L & 0xff);
}

void Set_RightBrush_PWM(uint16_t R)
{
	R = R < 100 ? R : 100 ;
	control_set(CTL_BRUSH_RIGHT, R & 0xff);
}

uint8_t Get_LeftBrush_Stall(void)
{
	return 0;
}

uint8_t Get_RightBrush_Stall(void)
{
	return 0;
}




uint8_t Get_Touch_Status(void)
{
	return g_touch_status;
}

void Reset_Touch(void)
{
	g_touch_status = 0;
}

void Set_Touch(void)
{
	g_touch_status = 1;
}

void Reset_Stop_Event_Status(void)
{
	g_stop_event_status = 0;
	// For key release checking.
	Reset_Touch();
}

uint8_t Stop_Event(void)
{
	// If it has already had a g_stop_event_status, then no need to check.
	if (!g_stop_event_status)
	{
		// Get the key value from robot sensor
		if (Get_Touch_Status()){
			ROS_WARN("Touch status == 1");
#if MANUAL_PAUSE_CLEANING
			if (Get_Clean_Mode() == Clean_Mode_Navigation)
				robot::instance()->setManualPause();
#endif
			Reset_Touch();
			g_stop_event_status = 1;
		}
		if (Remote_Key(Remote_Clean)){
			ROS_WARN("Remote_Key clean.");
			Reset_Rcon_Remote();
#if MANUAL_PAUSE_CLEANING
			if (Get_Clean_Mode() == Clean_Mode_Navigation)
				robot::instance()->setManualPause();
#endif
			g_stop_event_status = 2;
		}
		if (Get_Cliff_Trig() == 0x07){
			ROS_WARN("Cliff triggered.");
			g_stop_event_status = 3;
		}

		if (get_error_code())
		{
			ROS_WARN("Detects Error: %x!", get_error_code());
			if (get_error_code() == Error_Code_Slam)
			{
				Stop_Brifly();
				// Check if it is really stopped.
				uint8_t slam_error_count = 0;
				tf::StampedTransform transform;
				for (uint8_t i = 0; i < 3; i++)
				{
					try {
						robot::instance()->robot_tf_->lookupTransform("/map", "base_link", ros::Time(0), transform);
					} catch(tf::TransformException e) {
						ROS_WARN("%s %d: Failed to compute map transform, skipping scan (%s)", __FUNCTION__, __LINE__, e.what());
						slam_error_count++;
					}
					if (slam_error_count > 0)
						break;
					i++;
					usleep(20000);
				}
				if (slam_error_count > 0)
				{
					// Beep for debug
					//Beep(3, 25, 25, 3);
					system("rosnode kill /slam_karto &");
					usleep(3000000);
					system("roslaunch pp karto_slam.launch &");
					robotbase_restore_slam_correction();
					MotionManage::s_slam->isMapReady(false);
					while (!MotionManage::s_slam->isMapReady())
					{
						ROS_WARN("Slam not ready yet.");
						MotionManage::s_slam->enableMapUpdate();
						usleep(500000);
					}
					ROS_WARN("Slam restart successed.");
					// Wait for 0.5s to make sure it has process the first scan.
					usleep(500000);
				}
				set_error_code(Error_Code_None);
			}
			else
			{
				g_stop_event_status = 4;
			}
		}
		if (is_direct_charge())
		{
			ROS_WARN("Detect direct charge!");
			g_stop_event_status = 5;
		}
	}
	return g_stop_event_status;
}

uint8_t Is_Station(void)
{
	if (Get_Rcon_Status() & RconAll_Home_TLR) // It means eight rcon accepters receive any of the charger stub signal.
	{
		return 1;
	}
	return 0;
}

bool is_charge_on(void)
{
	// 1: On charger stub and charging.
	// 4: Direct connect to charge line and charging.
	if (robot::instance()->getChargeStatus() == 1 || robot::instance()->getChargeStatus() == 4)
		return true;
	else
		return false;
}

uint8_t Is_Water_Tank(void)
{
	return 0;
}


void Set_Clean_Mode(uint8_t mode)
{
	g_cleaning_mode = mode;
}

void Beep(uint8_t Sound_Code, int Sound_Time_Count, int Silence_Time_Count, int Total_Time_Count)
{
	// Sound_Code means the interval of the speaker sounding, higher interval makes lower sound.
	robotbase_sound_code = Sound_Code;
	// Total_Time_Count means how many loops of speaker sound loop will it sound.
	robotbase_speaker_sound_loop_count = Total_Time_Count;
	// A speaker sound loop contains one sound time and one silence time
	// Sound_Time_Count means how many loops of g_send_stream loop will it sound in one speaker sound loop
	robotbase_speaker_sound_time_count = Sound_Time_Count;
	// Silence_Time_Count means how many loops of g_send_stream loop will it be silence in one speaker sound loop, -1 means consistently beep.
	robotbase_speaker_silence_time_count = Silence_Time_Count;
	// Trigger the update flag to start the new beep action
	robotbase_beep_update_flag = true;
}

void Initialize_Motor(void)
{
#ifdef BLDC_INSTALL
	Clear_BLDC_Fail();
	BLDC_OFF;
	delay(5000);
	Set_BLDC_TPWM(40);
	Set_Vac_Speed();
#endif
	Set_MainBrush_PWM(50);
	Set_SideBrush_PWM(60,60);
	Set_BLDC_Speed(40);
//	Move_Forward(0,0);
	Stop_Brifly();
	Reset_TempPWM();
//	Left_Wheel_Slow=0;
//	Right_Wheel_Slow=0;
//	Reset_Bumper_Error();
}

void Disable_Motors(void)
{
	// Disable all the motors, including brush, wheels, and vacuum.
	// Stop the wheel
	Set_Wheel_Speed(0, 0);
	// Stop the side brush
	Set_SideBrush_PWM(0, 0);
	// Stop the main brush
	Set_MainBrush_PWM(0);
	// Stop the vacuum, directly stop the BLDC
	Set_BLDC_Speed(0);
}

void set_start_charge(void)
{
	// This function will turn on the charging function.
	control_set(CTL_CHARGER, 0x01);
}

void set_stop_charge(void)
{
	// Set the flag to false so that it can quit charger mode.
	control_set(CTL_CHARGER, 0x00);
}

void Set_Main_PwrByte(uint8_t val)
{
	control_set(CTL_MAIN_PWR, val & 0xff);
}

uint8_t Get_Main_PwrByte(){
	return g_send_stream[CTL_MAIN_PWR];
}

void Set_CleanTool_Power(uint8_t vacuum_val,uint8_t left_brush_val,uint8_t right_brush_val,uint8_t main_brush_val)
{
	int vacuum_pwr = vacuum_val;
	vacuum_pwr = vacuum_pwr > 0 ? vacuum_pwr : 0;
	vacuum_pwr = vacuum_pwr < 100 ? vacuum_pwr : 100;
	control_set(CTL_VACCUM_PWR, vacuum_pwr & 0xff);

	int brush_left = left_brush_val;
	control_set(CTL_BRUSH_LEFT, brush_left & 0xff);

	int brush_right = right_brush_val;
	control_set(CTL_BRUSH_RIGHT, brush_right & 0xff);

	int brush_main = main_brush_val;
	control_set(CTL_BRUSH_MAIN, brush_main & 0xff);
}

void Start_SelfCheck_Vacuum(void)
{
	control_set(CTL_OMNI_RESET, g_send_stream[CTL_OMNI_RESET] | 0x02);
}

void End_SelfCheck_Vacuum(void)
{
	control_set(CTL_OMNI_RESET, g_send_stream[CTL_OMNI_RESET] | 0x04);
}

void Reset_SelfCheck_Vacuum_Controler(void)
{
	control_set(CTL_OMNI_RESET, g_send_stream[CTL_OMNI_RESET] & ~0x06);
}

void control_set(uint8_t type, uint8_t val)
{
	SetSendFlag();
	if (type >= CTL_WHEEL_LEFT_HIGH && type <= CTL_GYRO) {
		g_send_stream[type] = val;
		//g_send_stream[SEND_LEN-3] = calcBufCrc8((char *)g_send_stream, SEND_LEN-3);
		//serial_write(SEND_LEN, g_send_stream);
	}
	ResetSendFlag();
}

void control_append_crc(){
	SetSendFlag();
	g_send_stream[CTL_CRC] = calcBufCrc8((char *)g_send_stream, SEND_LEN-3);
	ResetSendFlag();
}

void control_stop_all(void)
{
	uint8_t i;
	SetSendFlag();
	for(i = 2; i < (SEND_LEN)-2; i++) {
		if (i == CTL_MAIN_PWR)
			g_send_stream[i] = 0x01;
		else
			g_send_stream[i] = 0x00;
	}
	ResetSendFlag();
	//g_send_stream[SEND_LEN-3] = calcBufCrc8((char *)g_send_stream, SEND_LEN-3);
	//serial_write(SEND_LEN, g_send_stream);
}

int control_get_sign(uint8_t* key, uint8_t* sign, uint8_t key_length, int sequence_number)
{
	int		num_send_packets = key_length / KEY_DOWNLINK_LENGTH;
	uint8_t	ptr[RECEI_LEN], buf[SEND_LEN];

	//Set random seed.
	//srand(time(NULL));
	//Send random key to robot.
	for (int i = 0; i < num_send_packets; i++) {
		//Populate dummy.
		for (int j = 0; j < DUMMY_DOWNLINK_LENGTH; j++)
			g_send_stream[j + DUMMY_DOWNLINK_OFFSET] = (uint8_t) (rand() % 256);

		//Populate Sequence number.
		for (int j = 0; j < SEQUENCE_DOWNLINK_LENGTH; j++)
			g_send_stream[j + SEQUENCE_DOWNLINK_OFFSET] = (uint8_t) ((sequence_number >> 8 * j) % 256);

		//Populate key.
#if VERIFY_DEBUG
		printf("appending key: ");
#endif

		for (int k = 0; k < KEY_DOWNLINK_LENGTH; k++) {
			g_send_stream[k + KEY_DOWNLINK_OFFSET] = key[i * KEY_DOWNLINK_LENGTH + k];

#if VERIFY_DEBUG
			printf("%02x ", g_send_stream[k + KEY_DOWNLINK_OFFSET]);
			if (k == KEY_DOWNLINK_LENGTH - 1)
				printf("\n");
#endif

		}

		//Fill command field
		switch (i) {
			case 0:
				g_send_stream[SEND_LEN - 4] = CMD_KEY1;
				break;
			case 1:
				g_send_stream[SEND_LEN - 4] = CMD_KEY2;
				break;
			case 2:
				g_send_stream[SEND_LEN - 4] = CMD_KEY3;
				break;
			default:

#if VERIFY_DEBUG
				printf("control_get_sign : Error! key_length too large.");
#endif

				return -1;
				//break;
		}

		for (int i = 0; i < 40; i++) {	//200ms (round trip takes at leat 15ms)
			int counter = 0, ret;


			memcpy(buf, g_send_stream, sizeof(uint8_t) * SEND_LEN);
			buf[CTL_CRC] = calcBufCrc8((char *)buf, SEND_LEN - 3);
			serial_write(SEND_LEN, buf);

#if VERIFY_DEBUG
			printf("sending data to robot: i: %d\n", i);
			for (int j = 0; j < SEND_LEN; j++) {
				printf("%02x ", buf[j]);
			}
			printf("\n");
#endif

			while (counter < 200) {

				ret = serial_read(1, ptr);
				if (ptr[0] != 0xAA)
					continue;

				ret = serial_read(1, ptr);
				if (ptr[0] != 0x55)
					continue;

				ret = serial_read(RECEI_LEN - 2, ptr);
				if (RECEI_LEN - 2!= ret) {

#if VERIFY_DEBUG
					printf("%s %d: receive count error: %d\n", __FUNCTION__, __LINE__, ret); 
#endif

					usleep(100);
					counter++;
				} else {
					break;
				}
			}
			if (counter < 200) {

#if VERIFY_DEBUG
				printf("%s %d: counter: %d\tdata count: %d\treceive cmd: 0x%02x\n", __FUNCTION__, __LINE__, counter, ret, ptr[CMD_UPLINK_OFFSET]);

				printf("receive from robot: %d\n");
				for (int j = 0; j < RECEI_LEN - 2; j++) {
					printf("%02x ", ptr[j]);
				}
				printf("\n");
#endif

				if(ptr[CMD_UPLINK_OFFSET - 2] ==  CMD_NCK)   //robot received bronen packet
					continue;

				if(ptr[CMD_UPLINK_OFFSET - 2] == CMD_ACK) {	//set finished
					//printf("Downlink command ACKed!!\n");
					g_send_stream[CTL_CMD] = 0;                              //clear command byte
					break;
				}
			} else {

#if VERIFY_DEBUG
				printf("%s %d: max read count reached: %d\n", counter);
#endif

			}
			usleep(500);
		}
	}

	//Block and wait for signature.
	for (int i = 0; i < 400; i++) {                              //200ms (round trip takes at leat 15ms)
		int counter = 0, ret;
		while (counter < 400) {
			ret = serial_read(1, ptr);
			if (ptr[0] != 0xAA)
				continue;

			ret = serial_read(1, ptr);
			if (ptr[0] != 0x55)
					continue;

			ret = serial_read(RECEI_LEN - 2, ptr);

#if VERIFY_DEBUG
			printf("%s %d: %d %d %d\n", __FUNCTION__, __LINE__, ret, RECEI_LEN - 2, counter);
#endif

			if (RECEI_LEN - 2!= ret) { 
				usleep(100);
				counter++;
			} else {
				break;
			}
		}

#if VERIFY_DEBUG
		for (int j = 0; j < RECEI_LEN - 2; j++) {
			printf("%02x ", ptr[j]);
		}
		printf("\n");
#endif

		if (counter < 400 && ptr[CMD_UPLINK_OFFSET - 3] == CMD_ID) {
			//set finished

#if VERIFY_DEBUG
			printf("Signature received!!\n");
#endif

			for (int j = 0; j < KEY_UPLINK_LENGTH; j++)
				sign[j] = ptr[KEY_UPLINK_OFFSET - 2 + j];

			//Send acknowledge back to MCU.
			g_send_stream[CTL_CMD] = CMD_ACK;
			for (int k = 0; k < 20; k++) {
				memcpy(buf, g_send_stream, sizeof(uint8_t) * SEND_LEN);
				buf[CTL_CRC] = calcBufCrc8((char *)buf, SEND_LEN - 3);
				serial_write(SEND_LEN, buf);

				usleep(500);
			}
			g_send_stream[CTL_CMD] = 0;

#if VERIFY_DEBUG
			printf("%s %d: exit\n", __FUNCTION__, __LINE__);
#endif

			return KEY_UPLINK_LENGTH;
		}
		usleep(500);
	}
	g_send_stream[CTL_CMD] = 0;

#if VERIFY_DEBUG
	printf("%s %d: exit\n", __FUNCTION__, __LINE__);
#endif

	return -1;
}

uint8_t IsSendBusy(void)
{
	return g_sendflag;
}

void SetSendFlag(void)
{
	g_sendflag = 1;
}

void ResetSendFlag(void)
{
	g_sendflag = 0;
}
void Random_Back(void)
{
	Stop_Brifly();
	quick_back(12,30);
	
}

void Move_Back(void)
{
	Stop_Brifly();
	quick_back(18,30);
}

void Cliff_Move_Back()
{
	Stop_Brifly();
	quick_back(18,60);
}
void Set_RightWheel_Step(uint32_t step)
{
	g_right_wheel_step = step;
}

void Set_LeftWheel_Step(uint32_t step)
{
	g_left_wheel_step = step;
}
void Reset_RightWheel_Step()
{
	g_rw_t = ros::Time::now();
	g_right_wheel_step = 0;
}

void Reset_LeftWheel_Step()
{
	g_lw_t = ros::Time::now();
	g_left_wheel_step	= 0;
}

uint16_t GetBatteryVoltage()
{
	return robot::instance()->getBatteryVoltage();
}

uint8_t  Check_Bat_Stop()
{
	if(GetBatteryVoltage()<LOW_BATTERY_STOP_VOLTAGE)
		return 1;
	else
		return 0;
}

void Set_Key_Press(uint8_t key)
{
	g_key_status |= key;
}

void Reset_Key_Press(uint8_t key)
{
	g_key_status &= ~key;
}

uint8_t Get_Key_Press(void)
{	
	return g_key_status;
}

uint16_t Get_Key_Time(uint16_t key)
{
	// This time is count for 20ms.
	uint16_t time = 0;
	while(ros::ok()){
		time++;
		if (time == 151)
		{
			beep_for_command(true);
		}
		if(time>1500)break;
		usleep(20000);
		if(Get_Key_Press()!=key)break;
	}
	return time;
}

uint8_t Is_Front_Close(){	
	if(robot::instance()->getObsFront() > g_front_obs_trig_value+1500)
		return 1;
	else
		return 0;
}

uint8_t Is_virtualWall(void){
	return 0;
}

uint8_t Is_Turn_Remote(void)
{
	if (Remote_Key(Remote_Max | Remote_Home | Remote_Spot | Remote_Wall_Follow))
	{
		Reset_Rcon_Remote();
		return 1;
	}
	else
	{
		return 0;
	}
}

uint8_t Get_Direction_Flag(void)
{
	return g_direction_flag;
}

void Set_Direction_Flag(uint8_t flag)
{
	g_direction_flag = flag;
}

uint8_t Is_Direction_Right(void)
{
	if(Get_Direction_Flag() == Direction_Flag_Right)return 1;
	return 0;
}
uint8_t Is_Direction_Left(void)
{
	if(Get_Direction_Flag() == Direction_Flag_Left)return 1;
	return 0;
}
uint8_t Is_LeftWheel_Reach(int32_t step)
{
	if(g_left_wheel_step > (uint32_t)step)
		return 1;
	else
		return 0;
}

uint8_t Is_RightWheel_Reach(int32_t step)
{
	if(g_right_wheel_step > (uint32_t)step)
		return 1;
	else
		return 0;
}

void Wall_Move_Back(void)
{
	uint16_t count=0;
	uint16_t tp = 0;
	uint8_t mc = 0;
	set_dir_backward();
	Set_Wheel_Speed(3,3);
	usleep(20000);
	reset_wheel_step();
	while(((g_left_wheel_step<100)||(g_right_wheel_step<100))&&ros::ok()){
		tp = g_left_wheel_step/3+8;
		if(tp>12)tp=12;	
		Set_Wheel_Speed(tp,tp);
		usleep(1000);
	
		if(Stop_Event())
			return;
		count++;
		if(count>3000);
			return;	
		mc = Check_Motor_Current();
		if(mc == Check_Left_Wheel || mc == Check_Right_Wheel)
			return;
	}
	set_dir_forward();
	Set_Wheel_Speed(0,0);
		
}

void Reset_Move_Distance(){
	g_move_step_counter = 0;
}

uint8_t Is_Move_Finished(int32_t distance)
{
	g_move_step_counter = get_left_wheel_step();
	g_move_step_counter += get_right_wheel_step();
	if((g_move_step_counter/2)>distance)
		return 1;
	else
		return 0;
}
uint32_t Get_Move_Distance(void)
{
	g_move_step_counter = get_left_wheel_step();
	g_move_step_counter += get_right_wheel_step();

	if(g_move_step_counter>0)return (uint32_t)(g_move_step_counter/2);
	return 0;
}

void OBS_Turn_Left(uint16_t speed,uint16_t angle)
{
	uint16_t counter = 0;
	uint8_t oc = 0;
	Set_Dir_Left();
	Reset_Rcon_Remote();
	Set_Wheel_Speed(speed,speed);
	Reset_LeftWheel_Step();
	while(ros::ok()&&g_left_wheel_step <angle){
		counter++;
		if(counter>3000)
			return;
		if(Is_Turn_Remote())
			return;
		oc = Check_Motor_Current();
		if(oc== Check_Left_Wheel || oc==Check_Right_Wheel)
			return;
		usleep(1000);
	}
		
}
void OBS_Turn_Right(uint16_t speed,uint16_t angle)
{
	uint16_t counter = 0;
	uint8_t oc = 0;
	Set_Dir_Right();
	Reset_Rcon_Remote();
	Set_Wheel_Speed(speed,speed);
	Reset_RightWheel_Step();
	while(ros::ok()&&g_right_wheel_step <angle){
		counter++;
		if(counter>3000)
			//return;
			break;
		if(Is_Turn_Remote())
			//return;
			break;
		oc = Check_Motor_Current();
		if(oc== Check_Left_Wheel || oc==Check_Right_Wheel)
			//return;
			break;
		usleep(1000);
	}

}

void Cliff_Turn_Left(uint16_t speed,uint16_t angle)
{
	uint16_t Counter_Watcher=0;
	//Left_Wheel_Step=0;
	Reset_TempPWM();
	Set_Wheel_Speed(speed,speed);
	Counter_Watcher=0;
	Reset_Rcon_Remote();
	int16_t target_angle;
	uint16_t gyro_angle;
	// This decides whether robot should stop when left cliff triggered.
	bool right_cliff_triggered = false;

	if (Get_Cliff_Trig() & Status_Cliff_Right)
	{
		right_cliff_triggered = true;
	}

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle + angle;
	if (target_angle >= 3600) {
		target_angle = target_angle - 3600;
	}

	Set_Dir_Left();
	Set_Wheel_Speed(speed, speed);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	while(ros::ok())
	{
		if (abs(target_angle - Gyro_GetAngle()) < 20) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			Set_Wheel_Speed(speed / 2, speed / 2);
		} else {
			Set_Wheel_Speed(speed, speed);
		}
		//delay(10);
		usleep(1000);
		Counter_Watcher++;
		if(Counter_Watcher>3000)
		{
			if(is_encoder_fail())
			{
				set_error_code(Error_Code_Encoder);
			}
			return;
		}
		if(Is_Turn_Remote())return;
		if(!right_cliff_triggered && (Get_Cliff_Trig() & Status_Cliff_Right))
		{
			Stop_Brifly();
			return;
		}
		if(Stop_Event())
		{
			return;
		}
		if((Check_Motor_Current()==Check_Left_Wheel)||(Check_Motor_Current()==Check_Right_Wheel))return;
	}
}

void Cliff_Turn_Right(uint16_t speed,uint16_t angle)
{
	uint16_t Counter_Watcher=0;
	//Left_Wheel_Step=0;
	Reset_TempPWM();
	Set_Wheel_Speed(speed,speed);
	Counter_Watcher=0;
	Reset_Rcon_Remote();
	int16_t target_angle;
	uint16_t gyro_angle;
	// This decides whether robot should stop when left cliff triggered.
	bool left_cliff_triggered = false;

	if (Get_Cliff_Trig() & Status_Cliff_Left)
	{
		left_cliff_triggered = true;
	}

	gyro_angle = Gyro_GetAngle();

	target_angle = gyro_angle - angle;
	if (target_angle < 0) {
		target_angle = 3600 + target_angle;
	}

	Set_Dir_Right();
	Set_Wheel_Speed(speed, speed);

	ROS_INFO("%s %d: angle: %d(%d)\tcurrent: %d\tspeed: %d\n", __FUNCTION__, __LINE__, angle, target_angle, Gyro_GetAngle(), speed);
	while(ros::ok())
	{
		if (abs(target_angle - Gyro_GetAngle()) < 20) {
			break;
		}
		if (abs(target_angle - Gyro_GetAngle()) < 50) {
			Set_Wheel_Speed(speed / 2, speed / 2);
		} else {
			Set_Wheel_Speed(speed, speed);
		}
		//delay(10);
		usleep(1000);
		Counter_Watcher++;
		if(Counter_Watcher>3000)
		{
			if(is_encoder_fail())
			{
				set_error_code(Error_Code_Encoder);
			}
			return;
		}
		if(Is_Turn_Remote())return;
		if(!left_cliff_triggered && (Get_Cliff_Trig() & Status_Cliff_Left))
		{
			Stop_Brifly();
			return;
		}
		if(Stop_Event())
		{
			return;
		}
		if((Check_Motor_Current()==Check_Left_Wheel)||(Check_Motor_Current()==Check_Right_Wheel))return;
	}
}

uint8_t Get_Random_Factor(void){
	srand(time(0));
	return (uint8_t)rand()%100;
}

uint8_t Is_NearStation(void){
	static uint32_t s_count=0;
	static uint32_t no_s = 0;
	static uint32_t s_rcon =0;
	s_rcon = Get_Rcon_Status();
	if(s_rcon&0x00000f00){
		return 1;
	}
	if(s_rcon&0x0330ff){
		no_s = 0;
		s_count ++;
		Reset_Rcon_Status();	
		if(s_count>3)
		{
			s_count=0;
			return 1;
		}
	}
	else{
		no_s ++;
		if(no_s > 50){
			no_s = 0;
			s_count =0;
		}
	}
	return 0;
}

void Set_Mobility_Step(uint32_t Steps)
{
	g_mobility_step = Steps;
}

void Reset_Mobility_Step()
{
	control_set(CTL_OMNI_RESET, g_send_stream[CTL_OMNI_RESET] | 0x01);
}

void Clear_Reset_Mobility_Step()
{
	control_set(CTL_OMNI_RESET, g_send_stream[CTL_OMNI_RESET] & ~0x01);
}

uint32_t  Get_Mobility_Step()
{
	return g_mobility_step;
}

void Check_Mobility(void)
{
	
}

void Add_Average(uint32_t data)
{
	g_average_move += data;
	g_average_counter++;
	if(data>g_max_move)
		g_max_move = data;
	
}

uint32_t Get_Average_Move(void)
{
	return (g_average_move/g_average_counter);
}

uint32_t Reset_Average_Counter(void)
{
	g_average_move = 0;
	g_average_counter = 0;
}

void Reset_VirtualWall(void)
{

}

uint8_t  VirtualWall_TurnRight()
{
	return 0;
}

uint8_t VirtualWall_TurnLeft()
{
	return 0;
}

uint8_t Is_WorkFinish(uint8_t m)
{
	static uint8_t bat_count = 0;
	uint32_t wt = get_work_time();
	if(m){
		if(wt>g_auto_work_time)return 1;
	}
	else if(wt>Const_Work_Time)return 1;
	if(GetBatteryVoltage()<1420)
	{
		bat_count++;
		if(bat_count>50)return 1;
	}
	else
		bat_count = 0;
	return 0;		
}

uint8_t Get_Room_Mode(){
	return g_room_mode;
}

void Set_Room_Mode(uint8_t m){
	if(m)
		g_room_mode = 1;
	else
		g_room_mode = 0;
}

void Reset_WallAccelerate()
{
		g_wall_accelerate =0;
}

uint32_t Get_WallAccelerate()
{
	
	return g_wall_accelerate= get_right_wheel_step();
}

/*---------------------------------Bumper Error ----------------------------------------*/
uint8_t Is_Bumper_Jamed()
{
	if(Get_Bumper_Status())
	{
		ROS_INFO("JAM1");
		Move_Back();
		if(Stop_Event())
		{
			ROS_INFO("%s, %d: Stop event in JAM1", __FUNCTION__, __LINE__);
			return 0;
		}
		if(Get_Bumper_Status())
		{
			ROS_INFO("JAM2");
			// Quick back will not set speed to 100, it will be limited by the RUN_TOP_SPEED.
			quick_back(100,200);
			if(Stop_Event())
			{
				ROS_INFO("%s, %d: Stop event in JAM2", __FUNCTION__, __LINE__);
				return 0;
			}
			if(Get_Bumper_Status())
			{
				ROS_INFO("JAM3");
				Jam_Turn_Right(60, 900);
				if(Stop_Event())
				{
					ROS_INFO("%s, %d: Stop event in JAM3", __FUNCTION__, __LINE__);
					return 0;
				}
				if(Get_Bumper_Status())
				{
					ROS_INFO("JAM4");
					Jam_Turn_Left(60, 1800);
					if(Get_Bumper_Status())
					{
						ROS_INFO("JAM5");
						Set_Clean_Mode(Clean_Mode_Userinterface);
						set_error_code(Error_Code_Bumper);
						alarm_error();
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

void Reset_Bumper_Error(void)
{
	g_bumper_error=0;
}
uint8_t Is_Bumper_Fail(void)
{
	g_bumper_error++;
	ROS_INFO("Buper_Error = %d", g_bumper_error);
	if(g_bumper_error>3)return 1;
	else return 0;
}

uint8_t Is_VirtualWall()
{
	return 0;
}

int32_t ABS_Minus(int32_t A,int32_t B)
{
	if(A>B)
	{
		return A-B;
	}
	return B-A;
}

#define NORMAL	1
#define STOP	2
#define MAX		3
uint8_t Check_SideBrush_Stall(void)
{
	static uint32_t Time_LBrush = 0, Time_RBrush = 0;
	static uint8_t LBrush_Stall_Counter = 0, RBrush_Stall_Counter = 0, LBrush_Error_Counter = 0, RBrush_Error_Counter = 0;
	static uint8_t LeftBrush_Status = NORMAL, RightBrush_Status = NORMAL;

	/*---------------------------------Left Brush Stall---------------------------------*/
	switch(LeftBrush_Status)
	{
		case NORMAL:
			if(robot::instance()->getLbrushOc())
			{
				if(LBrush_Stall_Counter < 200)
					LBrush_Stall_Counter++;
			}
			else
			{
				LBrush_Stall_Counter = 0;
			}
			if(LBrush_Stall_Counter > 10)
			{
				/*-----Left Brush is stall in normal mode, stop the brush-----*/
				Set_LeftBrush_PWM(0);
				LeftBrush_Status = STOP;
				Time_LBrush = time(NULL);
			}
			break;

		case STOP:
			/*-----brush should stop for 5s-----*/
			if((time(NULL) - Time_LBrush) > 5)
			{
				Set_LeftBrush_PWM(100);
				LeftBrush_Status = MAX;
				Time_LBrush = time(NULL);
			}
			break;

		case MAX:
			if(robot::instance()->getLbrushOc())
			{
				if(LBrush_Stall_Counter < 200)
					LBrush_Stall_Counter++;
			}
			else
			{
				LBrush_Stall_Counter = 0;
			}

			if(LBrush_Stall_Counter > 10)
			{
				/*-----brush is stall in max mode, stop the brush and increase error counter -----*/
				Set_LeftBrush_PWM(0);
				LeftBrush_Status = STOP;
				Time_LBrush = time(NULL);
				LBrush_Error_Counter++;
				if(LBrush_Error_Counter > 2)
				{
					/*-----return error message-----*/
					return 1;
				}
				break;
			}
			else
			{
				if((time(NULL) - Time_LBrush) < 5)
				{
					/*-----brush should works in max mode for 5s-----*/
					LeftBrush_Status = MAX;
				}
				else
				{
					/*-----brush is in max mode more than 5s, turn to normal mode and reset error counter-----*/
					Set_LeftBrush_PWM(g_l_brush_pwm);
					LBrush_Error_Counter = 0;
					LeftBrush_Status = NORMAL;
				}
			}
			break;
	}
	/*-------------------------------Rigth Brush Stall---------------------------------*/
	switch(RightBrush_Status)
	{
		case NORMAL:
			if(robot::instance()->getRbrushOc())
			{
				if(RBrush_Stall_Counter < 200)
					RBrush_Stall_Counter++;
			}
			else
			{
				RBrush_Stall_Counter = 0;
			}
			if(RBrush_Stall_Counter > 10)
			{
				/*-----Right Brush is stall in normal mode, stop the brush-----*/
				Set_RightBrush_PWM(0);
				RightBrush_Status = STOP;
				Time_RBrush = time(NULL);
			}
			break;

		case STOP:
			/*-----brush should stop for 5s-----*/
			if((time(NULL) - Time_RBrush) > 5)
			{
				Set_RightBrush_PWM(100);
				RightBrush_Status = MAX;
				Time_RBrush = time(NULL);
			}
			break;

		case MAX:
			if(robot::instance()->getRbrushOc())
			{
				if(RBrush_Stall_Counter < 200)
					RBrush_Stall_Counter++;
			}
			else
			{
				RBrush_Stall_Counter = 0;
			}

			if(RBrush_Stall_Counter > 10)
			{
				/*-----brush is stall in max mode, stop the brush and increase error counter -----*/
				Set_RightBrush_PWM(0);
				RightBrush_Status = STOP;
				Time_RBrush = time(NULL);
				RBrush_Error_Counter++;
				if(RBrush_Error_Counter > 2)
				{
					/*-----return error message-----*/
					return 2;
				}
				break;
			}
			else
			{
				if((time(NULL) - Time_RBrush) < 5)
				{
					/*-----brush should works in max mode for 5s-----*/
					RightBrush_Status = MAX;
				}
				else
				{
					/*-----brush is in max mode more than 5s, turn to normal mode and reset error counter-----*/
					Set_RightBrush_PWM(g_r_brush_pwm);
					RBrush_Error_Counter = 0;
					RightBrush_Status = NORMAL;
				}
			}
			break;
	}
	return 0;
}

void Set_Plan_Status(uint8_t Status)
{
	g_plan_status = Status;
	if (g_plan_status != 0)
		ROS_DEBUG("Plan status return %d.", g_plan_status);
}

uint8_t Get_Plan_Status()
{
	return g_plan_status;
}
uint8_t GetSleepModeFlag()
{
	return g_sleep_mode_flag;
}
void SetSleepModeFlag()
{
	g_sleep_mode_flag = 1;
}
void ResetSleepModeFlag()
{
	g_sleep_mode_flag = 0;
}

void Clear_Manual_Pause(void)
{
	if (robot::instance()->isManualPaused())
	{
		// These are all the action that ~MotionManage() won't do if isManualPaused() returns true.
		ROS_WARN("Reset manual pause status.");
		wav_play(WAV_CLEANING_FINISHED);
		robot::instance()->resetManualPause();
		robot::instance()->savedOffsetAngle(0);
		if (MotionManage::s_slam != nullptr)
		{
			delete MotionManage::s_slam;
			MotionManage::s_slam = nullptr;
		}
		extern std::list <Point32_t> g_home_point;
		g_home_point.clear();
		cm_reset_go_home();
	}
}

void beep_for_command(bool valid)
{
	if (valid)
		Beep(2, 2, 0, 1);
	else
		Beep(5, 2, 0, 1);
}

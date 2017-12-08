#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ros/ros.h>
#include <battery.h>
#include <remote.hpp>
#include <charger.h>
#include <clean_mode.hpp>
#include "charge.hpp"
#include "core_move.h"
#include "gyro.h"
#include "bumper.h"
#include "led.h"
#include "movement.h"
#include "lidar.hpp"
#include "robot.hpp"
#include "main.h"
#include "serial.h"
#include "robotbase.h"
#include "spot.h"
#include "idle.h"
#include "remote_mode.h"
#include "sleep.h"
#include "wall_follow_trapped.h"
#include "event_manager.h"
#include "go_home.hpp"
#include "speaker.h"
#include "clean_mode.h"
#include "arch.hpp"


#if VERIFY_CPU_ID || VERIFY_KEY
#include "verify.h"
#endif

Mode* getNextMode(int next_mode_i_)
{
	switch (next_mode_i_)
	{
		case Mode::md_charge:
			return new ModeCharge();
		case Mode::md_sleep:
			return new ModeSleep();
//		case Mode::md_go_to_charger:
//			return new ModeGoToCharger();
		case Mode::md_remote:
			return new ModeRemote();

		case Mode::cm_navigation:
			return new CleanModeNav();
//		case Mode::cm_wall_follow:
//			return new CleanModeWallFollow();
//		case Mode::cm_spot:
//			return new CleanModeSpot();
//		case Mode::cm_exploration:
//			return new CleanModeExploration();
		default:
			return new ModeIdle();
	}
}
void *core_move_thread(void *)
{
	pthread_detach(pthread_self());
	//ROS_INFO("Waiting for robot sensor ready.");
	while (!robot::instance()->isSensorReady()) {
		usleep(1000);
	}
	//ROS_INFO("Robot sensor ready.");
	//speaker.play(SPEAKER_WELCOME_ILIFE);
	usleep(200000);

	if (charger.isDirected() || charger.isOnStub())
		cm_set(Clean_Mode_Charging);
	else if (battery.isReadyToClean())
		speaker.play(SPEAKER_PLEASE_START_CLEANING, false);

#if NEW_FRAMEWORK

	boost::shared_ptr<Mode> p_mode = nullptr;
	if (charger.isOnStub() || charger.isDirected())
		p_mode.reset(new ModeCharge());
	else
		p_mode.reset(new ModeIdle());

	while(ros::ok())
	{
		ROS_INFO("%s %d: %d", __FUNCTION__, __LINE__, p_mode);
		p_mode->run();
		auto next_mode = p_mode->getNextMode();
		p_mode.reset();
		ROS_INFO("%s %d: %d", __FUNCTION__, __LINE__, p_mode);
		p_mode.reset(getNextMode(next_mode));
		ROS_INFO("%s %d: %d", __FUNCTION__, __LINE__, p_mode);
	}

#else
	if (charger.isDirected() || charger.isOnStub())
		cm_set(Clean_Mode_Charging);
	else if (battery.isReadyToClean())
		speaker.play(SPEAKER_PLEASE_START_CLEANING);

	while(ros::ok()){
		usleep(20000);
		switch(cm_get()){
			case Clean_Mode_Idle:
				ROS_INFO("\n-------idle mode_------\n");
				serial.setCleanMode(Clean_Mode_Idle);
//				speaker.play(SPEAKER_TEST_MODE);
				idle();
				break;
			case Clean_Mode_WallFollow:
				ROS_INFO("\n-------wall follow mode_------\n");
				serial.setCleanMode(Clean_Mode_WallFollow);
				g_is_low_bat_pause = false;

				cs_paused_setting();

//				wall_follow(Map_Wall_Follow_Escape_Trapped);
				cm_cleaning();
				break;
			case Clean_Mode_Navigation:
				ROS_INFO("\n-------Navigation mode_------\n");
				serial.setCleanMode(Clean_Mode_Navigation);
				cm_cleaning();
				break;
			case Clean_Mode_Charging:
				ROS_INFO("\n-------Charge mode_------\n");
				serial.setCleanMode(Clean_Mode_Charging);
				charge_function();
				break;
			case Clean_Mode_Go_Charger:
				//goto_charger();
				ROS_INFO("\n-------GoHome mode_------\n");
				serial.setCleanMode(Clean_Mode_Go_Charger);
				g_is_low_bat_pause = false;
				cs_paused_setting();
#if GO_HOME_REGULATOR
				cm_cleaning();
#else
				go_home();
#endif
				break;

			case Clean_Mode_Exploration:
				//goto_charger();
				ROS_INFO("\n-------Exploration mode_------\n");
				serial.setCleanMode(Clean_Mode_Exploration);
				g_is_low_bat_pause = false;
				cs_paused_setting();
				cm_cleaning();
				break;

			case Clean_Mode_Test:
				break;

			case Clean_Mode_Remote:
				remote_mode();
				break;

			case Clean_Mode_Spot:
				ROS_INFO("\n-------Spot mode_------\n");
				serial.setCleanMode(Clean_Mode_Spot);
				g_is_low_bat_pause = false;
				cs_paused_setting();
				remote.reset();
				SpotMovement::instance()->setSpotType(NORMAL_SPOT);
				cm_cleaning();
				cs_disable_motors();
				usleep(200000);
				break;

			case Clean_Mode_Sleep:
				ROS_INFO("\n-------Sleep mode_------\n");
				//serial.setStatus(Clean_Mode_Sleep);
				g_is_low_bat_pause = false;
				cs_paused_setting();
				cs_disable_motors();
				sleep_mode();
				break;
			default:
				cm_set(Clean_Mode_Idle);
				break;

		}
	}
#endif

	return NULL;
	//pthread_exit(NULL);	
}


int main(int argc, char **argv)
{
	int		baudrate, ret1, core_move_thread_state;
	bool	verify_ok = true;
	pthread_t	core_move_thread_id, event_manager_thread_id, event_handler_thread_id;
	std::string	serial_port;
	std::string lidar_bumper_dev;

	ros::init(argc, argv, "pp");
	ros::NodeHandle	nh_private("~");

	robot	robot_obj;

	SpotMovement spot_obj(1.0);

	event_manager_init();

	nh_private.param<std::string>("serial_port", serial_port, "/dev/ttyS3");
	nh_private.param<int>("baudrate", baudrate, 57600);
	nh_private.param<std::string>("lidar_bumper_file", lidar_bumper_dev, "/dev/input/event0");
	
	serial.init(serial_port.c_str(), baudrate);
	if(bumper.lidarBumperInit(lidar_bumper_dev.c_str()) == -1){
		ROS_ERROR(" lidar bumper open fail!");
	}
#if VERIFY_CPU_ID
	if (verify_cpu_id() < 0) {
		verify_ok = false;
	}
#endif

#if VERIFY_KEY
	if (verify_ok == true && verify_key() == 0) {
		verify_ok = false;
	}
#endif

	robotbase_reset_send_stream();
	robotbase_init();

	if (verify_ok == true) {
#if 1
		ret1 = pthread_create(&event_manager_thread_id, 0, event_manager_thread, NULL);
		if (ret1 != 0) {
			ROS_ERROR("%s %d: event_manager_thread fails to run!", __FUNCTION__, __LINE__);
		} else {
			ROS_INFO("%s %d: \033[32mevent_manager_thread\033[0m is up!", __FUNCTION__, __LINE__);
		}
#endif


#if 1
		ret1 = pthread_create(&event_handler_thread_id, 0, event_handler_thread, NULL);
		if (ret1 != 0) {
			ROS_ERROR("%s %d: event_handler_thread fails to run!", __FUNCTION__, __LINE__);
		} else {
			ROS_INFO("%s %d: \033[32mevent_handler_thread\033[0m is up!", __FUNCTION__, __LINE__);
		}
#endif

		ret1 = pthread_create(&core_move_thread_id, 0, core_move_thread, NULL);
		if (ret1 != 0) {
			core_move_thread_state = 0;
		} else {
			ROS_INFO("%s %d: \033[32mcore_move_thread\033[0m is up!", __FUNCTION__, __LINE__);
			core_move_thread_state = 1;
		}
		ros::spin();
	} else {
		printf("turn on led\n");
		led.set_mode(LED_STEADY, LED_ORANGE);
		sleep(10);
	}
	bumper.lidarBumperDeinit();
	robotbase_deinit();
	return 0;
}

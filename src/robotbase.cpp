#include <ros/ros.h>
#include <std_msgs/String.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Pose.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <pp/x900sensor.h>
#include <dev.h>

#include <sleep.h>
#include <clean_timer.h>
#include <odom.h>

#define _H_LEN 2
#if __ROBOT_X400
uint8_t g_receive_stream[RECEI_LEN]={		0xaa,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcc,0x33};
uint8_t g_send_stream[SEND_LEN]={0xaa,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0xcc,0x33};

#elif __ROBOT_X900
uint8_t g_receive_stream[RECEI_LEN]={		0xaa,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0xcc,0x33};
uint8_t g_send_stream[SEND_LEN]={0xaa,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x00,0x02,0x00,0x00,0xcc,0x33};
#endif

#define  _RATE 50

bool is_robotbase_init = false;
bool robotbase_thread_stop = false;
bool send_stream_thread = false;

pthread_t robotbaseThread_id;
pthread_t receiPortThread_id;
pthread_t sendPortThread_id;
pthread_mutex_t recev_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  recev_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t serial_data_ready_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t serial_data_ready_cond = PTHREAD_COND_INITIALIZER;

pp::x900sensor	sensor;

bool robotbase_beep_update_flag = false;
int robotbase_speaker_sound_loop_count = 0;
uint8_t robotbase_sound_code = 0;
int robotbase_speaker_sound_time_count = 0;
int temp_speaker_sound_time_count = -1;
int robotbase_speaker_silence_time_count = 0;
int temp_speaker_silence_time_count = 0;

// For led control.
uint8_t robotbase_led_type = LED_STEADY;
bool robotbase_led_update_flag = false;
uint8_t robotbase_led_color = LED_GREEN;
uint16_t robotbase_led_cnt_for_switch = 0;
uint16_t live_led_cnt_for_switch = 0;

// Lock for odom coordinate
boost::mutex odom_mutex;

// Lock for g_send_stream
boost::mutex g_send_stream_mutex;

int robotbase_init(void)
{
	int	serr_ret;
	int base_ret;
	int sers_ret;
	uint8_t buf[SEND_LEN];

	robotbase_thread_stop = false;
	send_stream_thread = true;
	
	if (!is_serial_ready()) {
		ROS_ERROR("serial not ready\n");
		return -1;
	}
	controller.set_status(POWER_ACTIVE);
	g_send_stream_mutex.lock();
	memcpy(buf, g_send_stream, sizeof(uint8_t) * SEND_LEN);
	g_send_stream_mutex.unlock();
	uint8_t crc;
	crc = calc_buf_crc8(buf, SEND_LEN - 3);
	controller.set(SEND_LEN - 3, crc);
	ROS_INFO("waiting robotbase awake ");
	serr_ret = pthread_create(&receiPortThread_id, NULL, serial_receive_routine, NULL);
	base_ret = pthread_create(&robotbaseThread_id, NULL, robotbase_routine, NULL);
	sers_ret = pthread_create(&sendPortThread_id,NULL,serial_send_routine,NULL);
	if (base_ret < 0 || serr_ret < 0 || sers_ret < 0) {
		is_robotbase_init = false;
		robotbase_thread_stop = true;
		send_stream_thread = false;
		if (base_ret < 0) {ROS_INFO("%s,%d, fail to create robotbase thread!! %s ", __FUNCTION__,__LINE__,strerror(base_ret));}
		if (serr_ret < 0) {ROS_INFO("%s,%d, fail to create serial receive thread!! %s ",__FUNCTION__,__LINE__, strerror(serr_ret));}
		if (sers_ret < 0){ROS_INFO("%s,%d, fail to create serial send therad!! %s ",__FUNCTION__,__LINE__,strerror(sers_ret));}
		return -1;
	}
	is_robotbase_init = true;
	return 0;
}

void debug_received_stream()
{
	ROS_INFO("%s %d: Received stream:", __FUNCTION__, __LINE__);
	for (int i = 0; i < RECEI_LEN; i++)
		printf("%02x ", g_receive_stream[i]);
	printf("\n");
}

void debug_send_stream(uint8_t *buf)
{
	ROS_INFO("%s %d: Send stream:", __FUNCTION__, __LINE__);
	for (int i = 0; i < SEND_LEN; i++)
		printf("%02x ", *(buf + i));
	printf("\n");
}

bool is_robotbase_stop(void)
{
	return robotbase_thread_stop ? true : false;
}

void robotbase_deinit(void)
{
	uint8_t buf[2];

	if (is_robotbase_init) {
		is_robotbase_init = false;
		robotbase_thread_stop = true;
		ROS_INFO("%s,%d,shutdown robotbase power",__FUNCTION__,__LINE__);
		led.set_mode(LED_STEADY, LED_OFF);
		controller.set(CTL_BUZZER, 0x00);
		gyro.set_off();
		cs_disable_motors();
		controller.set_status(POWER_DOWN);
		usleep(40000);
		send_stream_thread = false;
		usleep(40000);
		serial_close();
		ROS_INFO("%s,%d, Stop OK",__FUNCTION__,__LINE__);
		int mutex_ret = pthread_mutex_destroy(&recev_lock);
		if(mutex_ret<0)
			ROS_ERROR("_%s,%d, pthread mutex destroy fail",__FUNCTION__,__LINE__);
		int cond_ret = pthread_cond_destroy(&recev_cond);
		if(cond_ret<0)
			ROS_ERROR("%s,%d,pthread cond destroy fail",__FUNCTION__,__LINE__);
	}
}

void robotbase_reset_send_stream(void)
{
	boost::mutex::scoped_lock(g_send_stream_mutex);
	for (int i = 0; i < SEND_LEN; i++) {
		if (i != CTL_LED_GREEN)
			controller.set(i, 0x00);
		else
			controller.set(i, 0x64);
	}
	controller.set(0, 0xaa);
	controller.set(1, 0x55);
	controller.set(SEND_LEN - 2, 0xcc);
	controller.set(SEND_LEN - 1, 0x33);
}

void *serial_receive_routine(void *)
{
	pthread_detach(pthread_self());
	ROS_INFO("robotbase,\033[32m%s\033[0m,%d thread is up",__FUNCTION__,__LINE__);
	int i, j, ret, wh_len, wht_len, whtc_len;

	uint8_t r_crc, c_crc;
	uint8_t h1 = 0xaa, h2 = 0x55, header[2], t1 = 0xcc, t2 = 0x33;
	uint8_t header1 = 0x00;
	uint8_t header2 = 0x00;
	uint8_t tempData[RECEI_LEN], receiData[RECEI_LEN];
	tempData[0] = h1;
	tempData[1] = h2;

	wh_len = RECEI_LEN - 2; //length without header bytes
	wht_len = wh_len - 2; //length without header and tail bytes
	whtc_len = wht_len - 1; //length without header and tail and crc bytes

	while (ros::ok() && (!robotbase_thread_stop)) {
		ret = serial_read(1, &header1);
		if (ret <= 0 ){
			ROS_WARN("%s, %d, serial read return %d ,  requst %d byte",__FUNCTION__,__LINE__,ret,1);
			continue;
		}
		if(header1 != h1)
			continue;
		ret= serial_read(1,&header2);
		if (ret <= 0 ){
			ROS_WARN("%s,%d,serial read return %d , requst %d byte",__FUNCTION__,__LINE__,ret,1);
			continue;
		}
		if(header2 != h2){
			continue;
		}
		ret = serial_read(wh_len, receiData);
		if(ret < wh_len){
			ROS_WARN("%s,%d,serial read %d bytes, requst %d bytes",__FUNCTION__,__LINE__,ret,wh_len);
		}
		if(ret<= 0){
			ROS_WARN("%s,%d,serial read return %d",__FUNCTION__,__LINE__,ret);
			continue;
		}

		for (i = 0; i < whtc_len; i++){
			tempData[i + 2] = receiData[i];
		}

		c_crc = calc_buf_crc8(tempData, wh_len - 1);//calculate crc8 with header bytes
		r_crc = receiData[whtc_len];

		if (r_crc == c_crc){
			if (receiData[wh_len - 1] == t2 && receiData[wh_len - 2] == t1) {
				for (j = 0; j < wht_len; j++) {
					g_receive_stream[j + 2] = receiData[j];
				}
				if(pthread_cond_signal(&recev_cond)<0)//if receive data corret than send signal
					ROS_ERROR(" in serial read, pthread signal fail !");
			} else {
				ROS_WARN(" in serial read ,data tail error\n");
			}
		} else {
			ROS_ERROR("%s,%d,in serial read ,data crc error\n",__FUNCTION__,__LINE__);
		}
	}
	ROS_INFO("\033[32m%s\033[0m,%d,exit!",__FUNCTION__,__LINE__);
}

void *robotbase_routine(void*)
{
	pthread_detach(pthread_self());
	ROS_INFO("robotbase,\033[32m%s\033[0m,%d, is up!",__FUNCTION__,__LINE__);
	uint16_t	lw_speed, rw_speed;

	ros::Rate	r(_RATE);
	ros::Time	cur_time, last_time;

	//Debug
	uint16_t error_count = 0;

	nav_msgs::Odometry			odom_msg;
	tf::TransformBroadcaster	odom_broad;
	geometry_msgs::Quaternion	odom_quat;

	geometry_msgs::TransformStamped	odom_trans;

	ros::Publisher	odom_pub, sensor_pub;
	ros::NodeHandle	robotsensor_node;
	ros::NodeHandle	odom_node;

	sensor_pub = robotsensor_node.advertise<pp::x900sensor>("/robot_sensor",1);
	odom_pub = odom_node.advertise<nav_msgs::Odometry>("/odom",1);

	cur_time = ros::Time::now();
	last_time  = cur_time;
	int16_t last_rcliff = 0, last_fcliff = 0, last_lcliff = 0;
	int16_t last_x_acc = -1000, last_y_acc = -1000, last_z_acc = -1000;
	while (ros::ok() && !robotbase_thread_stop)
	{
		/*--------data extrict from serial com--------*/
		if(pthread_mutex_lock(&recev_lock)!=0)ROS_ERROR("robotbase pthread receive lock fail");
		if(pthread_cond_wait(&recev_cond,&recev_lock)!=0)ROS_ERROR("robotbase pthread receive cond wait fail");
		if(pthread_mutex_unlock(&recev_lock)!=0)ROS_WARN("robotbase pthread receive unlock fail");
		//ros::spinOnce();

		boost::mutex::scoped_lock(odom_mutex);

		lw_speed = (g_receive_stream[REC_LW_S_H] << 8) | g_receive_stream[REC_LW_S_L];
		rw_speed = (g_receive_stream[REC_RW_S_H] << 8) | g_receive_stream[REC_RW_S_L];

		sensor.lw_vel = (lw_speed & 0x8000) ? -((float)((lw_speed & 0x7fff) / 1000.0)) : (float)((lw_speed & 0x7fff) / 1000.0);
		sensor.rw_vel = (rw_speed & 0x8000) ? -((float)((rw_speed & 0x7fff) / 1000.0)) : (float)((rw_speed & 0x7fff) / 1000.0);

		sensor.angle = -(float)((int16_t)((g_receive_stream[REC_ANGLE_H] << 8) | g_receive_stream[REC_ANGLE_L])) / 100.0;//ros angle * -1
		sensor.angle -= robot::instance()->offsetAngle();
		sensor.angle_v = -(float)((int16_t)((g_receive_stream[REC_ANGLE_V_H] << 8) | g_receive_stream[REC_ANGLE_V_L])) / 100.0;//ros angle * -1

		sensor.lw_crt = (((g_receive_stream[REC_LW_C_H] << 8) | g_receive_stream[REC_LW_C_L]) & 0x7fff) * 1.622;
		sensor.rw_crt = (((g_receive_stream[REC_RW_C_H] << 8) | g_receive_stream[REC_RW_C_L]) & 0x7fff) * 1.622;

		sensor.left_wall = ((g_receive_stream[REC_L_WALL_H] << 8)| g_receive_stream[REC_L_WALL_L]);

		sensor.l_obs = ((g_receive_stream[REC_L_OBS_H] << 8) | g_receive_stream[REC_L_OBS_L]) - obs.get_left_baseline();
		sensor.f_obs = ((g_receive_stream[REC_F_OBS_H] << 8) | g_receive_stream[REC_F_OBS_L]) - obs.get_front_baseline();
		sensor.r_obs = ((g_receive_stream[REC_R_OBS_H] << 8) | g_receive_stream[REC_R_OBS_L]) - obs.get_right_baseline();

#if __ROBOT_X900

		sensor.right_wall = ((g_receive_stream[REC_R_WALL_H]<<8)|g_receive_stream[REC_R_WALL_L]);

		sensor.lbumper = (g_receive_stream[REC_BUMPER] & 0xf0) ? true : false;
		sensor.rbumper = (g_receive_stream[REC_BUMPER] & 0x0f) ? true : false;
		auto lidar_bumper_status = bumper_get_lidar_status();
		if (lidar_bumper_status == 1)
			sensor.lidar_bumper = 1;
		else if (lidar_bumper_status == 0)
			sensor.lidar_bumper = 0;
		// else if (lidar_bumper_status == -1)
		// sensor.lidar_bumper = sensor.lidar_bumper;

		sensor.ir_ctrl = g_receive_stream[REC_REMOTE_IR];
		if (sensor.ir_ctrl > 0)
			remote.set(sensor.ir_ctrl);

		sensor.c_stub = (g_receive_stream[REC_CHARGE_STUB_4] << 24 ) | (g_receive_stream[REC_CHARGE_STUB_3] << 16)
			| (g_receive_stream[REC_CHARGE_STUB_2] << 8) | g_receive_stream[REC_CHARGE_STUB_1];
		c_rcon.set_status(c_rcon.get_status() | sensor.c_stub);

		sensor.visual_wall = (g_receive_stream[REC_VISUAL_WALL_H] << 8)| g_receive_stream[REC_VISUAL_WALL_L];

		sensor.key = g_receive_stream[REC_KEY];
		key.eliminate_jitter(sensor.key);

		sensor.c_s = g_receive_stream[REC_CHARGE_STATE];

		sensor.w_tank = (g_receive_stream[REC_WATER_TANK]>0)?true:false;

		sensor.batv = (g_receive_stream[REC_BAT_V]);

		if(((g_receive_stream[REC_L_CLIFF_H] << 8) | g_receive_stream[REC_L_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_lcliff > CLIFF_LIMIT)
				last_lcliff = ((g_receive_stream[REC_L_CLIFF_H] << 8) | g_receive_stream[REC_L_CLIFF_L]);
			else
				sensor.lcliff = last_lcliff = ((g_receive_stream[REC_L_CLIFF_H] << 8) | g_receive_stream[REC_L_CLIFF_L]);
		}
		else
			sensor.lcliff = last_lcliff = ((g_receive_stream[REC_L_CLIFF_H] << 8) | g_receive_stream[REC_L_CLIFF_L]);
		if(((g_receive_stream[REC_F_CLIFF_H] << 8) | g_receive_stream[REC_F_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_fcliff > CLIFF_LIMIT)
				last_fcliff = ((g_receive_stream[REC_F_CLIFF_H] << 8) | g_receive_stream[REC_F_CLIFF_L]);
			else
				sensor.fcliff = last_fcliff = ((g_receive_stream[REC_F_CLIFF_H] << 8) | g_receive_stream[REC_F_CLIFF_L]);
		}
		else
			sensor.fcliff = last_fcliff = ((g_receive_stream[REC_F_CLIFF_H] << 8) | g_receive_stream[REC_F_CLIFF_L]);
		if(((g_receive_stream[REC_R_CLIFF_H] << 8) | g_receive_stream[REC_R_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_rcliff > CLIFF_LIMIT)
				last_rcliff = ((g_receive_stream[REC_R_CLIFF_H] << 8) | g_receive_stream[REC_R_CLIFF_L]);
			else
				sensor.rcliff = last_rcliff = ((g_receive_stream[REC_R_CLIFF_H] << 8) | g_receive_stream[REC_R_CLIFF_L]);
		}
		else
			sensor.rcliff = last_rcliff = ((g_receive_stream[REC_R_CLIFF_H] << 8) | g_receive_stream[REC_R_CLIFF_L]);

		sensor.vacuum_selfcheck_status = (g_receive_stream[REC_CL_OC] & 0x30);
		sensor.lbrush_oc = (g_receive_stream[REC_CL_OC] & 0x08) ? true : false;		// left brush over current
		sensor.mbrush_oc = (g_receive_stream[REC_CL_OC] & 0x04) ? true : false;		// main brush over current
		sensor.rbrush_oc = (g_receive_stream[REC_CL_OC] & 0x02) ? true : false;		// right brush over current
		sensor.vcum_oc = (g_receive_stream[REC_CL_OC] & 0x01) ? true : false;		// vaccum over current

		sensor.gyro_dymc = g_receive_stream[REC_GYRO_DYMC];

		sensor.omni_wheel = (g_receive_stream[REC_OMNI_W_H]<<8)|g_receive_stream[REC_OMNI_W_L];

		if (last_x_acc == -1000)
			sensor.x_acc = (g_receive_stream[REC_XACC_H]<<8)|g_receive_stream[REC_XACC_L];//in mG
		else
			sensor.x_acc = ((g_receive_stream[REC_XACC_H]<<8)|g_receive_stream[REC_XACC_L] + last_x_acc) / 2;//in mG
		if (last_y_acc == -1000)
			sensor.y_acc = (g_receive_stream[REC_YACC_H]<<8)|g_receive_stream[REC_YACC_L];//in mG
		else
			sensor.y_acc = ((g_receive_stream[REC_YACC_H]<<8)|g_receive_stream[REC_YACC_L] + last_y_acc) / 2;//in mG
		if (last_z_acc == -1000)
			sensor.z_acc = (g_receive_stream[REC_ZACC_H]<<8)|g_receive_stream[REC_ZACC_L];//in mG
		else
			sensor.z_acc = ((g_receive_stream[REC_ZACC_H]<<8)|g_receive_stream[REC_ZACC_L] + last_z_acc) / 2;//in mG

		sensor.plan = g_receive_stream[REC_PLAN];
		if(sensor.plan)
			timer.set_status(sensor.plan);

#elif __ROBOT_X400
		sensor.lbumper = (g_receive_stream[22] & 0xf0)?true:false;
		sensor.rbumper = (g_receive_stream[22] & 0x0f)?true:false;
		sensor.c_stub = (g_receive_stream[24] << 16) | (g_receive_stream[25] << 8) | g_receive_stream[26];
		sensor.key = g_receive_stream[27];
		sensor.c_s = g_receive_stream[28];
		sensor.w_tank_ = (g_receive_stream[29] > 0) ? true : false;
		sensor.batv = g_receive_stream[30];

		sensor.lcliff = ((g_receive_stream[31] << 8) | g_receive_stream[32]);
		sensor.fcliff = ((g_receive_stream[33] << 8) | g_receive_stream[34]);
		sensor.rcliff = ((g_receive_stream[35] << 8) | g_receive_stream[36]);

		sensor.lbrush_oc_ = (g_receive_stream[37] & 0x08) ? true : false;		// left brush over current
		sensor.mbrush_oc_ = (g_receive_stream[37] & 0x04) ? true : false;		// main brush over current
		sensor.rbrush_oc_ = (g_receive_stream[37] & 0x02) ? true : false;		// right brush over current
		sensor.vcum_oc = (g_receive_stream[37] & 0x01) ? true : false;		// vaccum over current
		sensor.gyro_dymc_ = g_receive_stream[38];
		sensor.right_wall_ = ((g_receive_stream[39]<<8)|g_receive_stream[40]);
		sensor.x_acc = (g_receive_stream[41]<<8)|g_receive_stream[42]; //in mG
		sensor.y_acc = (g_receive_stream[43]<<8)|g_receive_stream[44];//in mG
		sensor.z_acc = (g_receive_stream[45]<<8)|g_receive_stream[46]; //in mG
#endif
	#if GYRO_DYNAMIC_ADJUSTMENT
	if (wheel.getLeftWheelSpeed() < 0.01 && wheel.getRightWheelSpeed() < 0.01)
	{
		gyro.set_dynamic_on();
	} else
	{
		gyro.set_dynamic_off();
	}
	tilt.check();
	if(omni.isEnable())
		omni.detect();
#endif
		/*---------extrict end-------*/

		pthread_mutex_lock(&serial_data_ready_mtx);
		pthread_cond_broadcast(&serial_data_ready_cond);
		pthread_mutex_unlock(&serial_data_ready_mtx);
		sensor_pub.publish(sensor);

		/*------------setting for odom and publish odom topic --------*/
		odom.setMovingSpeed(static_cast<float>((sensor.lw_vel + sensor.rw_vel) / 2.0));
		odom.setAngle(sensor.angle);
		odom.setAngleSpeed(sensor.angle_v);
		cur_time = ros::Time::now();
		double angle_rad, dt;
		angle_rad = static_cast<float>(odom.getAngle() * 0.01745);	//degree into radian
		dt = (cur_time - last_time).toSec();
		last_time = cur_time;
		odom.setX(static_cast<float>(odom.getX() + (odom.getMovingSpeed() * cos(angle_rad)) * dt));
		odom.setY(static_cast<float>(odom.getY() + (odom.getMovingSpeed() * sin(angle_rad)) * dt));
		odom_quat = tf::createQuaternionMsgFromYaw(angle_rad);
		odom_msg.header.stamp = cur_time;
		odom_msg.header.frame_id = "odom";
		odom_msg.child_frame_id = "base_link";
		odom_msg.pose.pose.position.x = odom.getX();
		odom_msg.pose.pose.position.y = odom.getY();
		odom_msg.pose.pose.position.z = 0.0;
		odom_msg.pose.pose.orientation = odom_quat;
		odom_msg.twist.twist.linear.x = odom.getMovingSpeed();
		odom_msg.twist.twist.linear.y = 0.0;
		odom_msg.twist.twist.angular.z = odom.getAngleSpeed();
		odom_pub.publish(odom_msg);

		odom_trans.header.stamp = cur_time;
		odom_trans.header.frame_id = "odom";
		odom_trans.child_frame_id = "base_link";
		odom_trans.transform.translation.x = odom.getX();
		odom_trans.transform.translation.y = odom.getY();
		odom_trans.transform.translation.z = 0.0;
		odom_trans.transform.rotation = odom_quat;
		odom_broad.sendTransform(odom_trans);
		/*------publish end -----------*/

	}//end while
	ROS_INFO("\033[32m%s\033[0m,%d,robotbase thread exit",__FUNCTION__,__LINE__);
}

void *serial_send_routine(void*)
{
	pthread_detach(pthread_self());
	ROS_INFO("robotbase,\033[32m%s\033[0m,%d is up",__FUNCTION__,__LINE__);
	ros::Rate r(_RATE);
	uint8_t buf[SEND_LEN];
	int sl = SEND_LEN-3;
	reset_send_flag();
	while(send_stream_thread){
		r.sleep();
		if(get_sleep_mode_flag()){
			continue;
		}
		/*-------------------Process for beeper.play and led -----------------------*/
		// Force reset the beeper action when beeper() function is called, especially when last beeper action is not over. It can stop last beeper action and directly start the updated beeper.play action.
		if (robotbase_beep_update_flag){
			temp_speaker_sound_time_count = -1;
			temp_speaker_silence_time_count = 0;
			robotbase_beep_update_flag = false;
		}
		//ROS_INFO("%s %d: tmp_sound_count: %d, tmp_silence_count: %d, sound_loop_count: %d.", __FUNCTION__, __LINE__, temp_speaker_sound_time_count, temp_speaker_silence_time_count, robotbase_speaker_sound_loop_count);
		// If count > 0, it is processing for different alarm.
		if (robotbase_speaker_sound_loop_count != 0){
			process_beep();
		}

		if (robotbase_led_update_flag)
			process_led();

		//if(!is_flag_set()){
			/*---pid for wheels---*/
			extern uint8_t g_wheel_left_direction, g_wheel_right_direction;
		wheel.set_pid();
			if(left_pid.actual_speed < 0)	wheel.set_left_dir(BACKWARD);
			else							wheel.set_left_dir(FORWARD);
			if(right_pid.actual_speed < 0)	wheel.set_right_dir(BACKWARD);
			else							wheel.set_right_dir(FORWARD);

		wheel.set_left_speed((uint8_t) fabsf(left_pid.actual_speed));
		wheel.set_right_speed((uint8_t) fabsf(right_pid.actual_speed));
			g_send_stream_mutex.lock();
			memcpy(buf,g_send_stream,sizeof(uint8_t)*SEND_LEN);
			g_send_stream_mutex.unlock();
			buf[CTL_CRC] = calc_buf_crc8(buf, sl);
			//debug_send_stream(&buf[0]);
			serial_write(SEND_LEN, buf);
		//}
		//reset omni wheel bit
		if(controller.get(CTL_OMNI_RESET) & 0x01)
			omni.clear();
	}
	ROS_INFO("\033[32m%s\033[0m,%d pthread exit",__FUNCTION__,__LINE__);
	//pthread_exit(NULL);
}

void process_beep()
{
	// This routine handles the speaker sounding logic
	// If temp_speaker_silence_time_count == 0, it is the end of loop of silence, so decrease the count and set sound in g_send_stream.
	if (temp_speaker_silence_time_count == 0){
		temp_speaker_silence_time_count--;
		temp_speaker_sound_time_count = robotbase_speaker_sound_time_count;
		controller.set(CTL_BUZZER, robotbase_sound_code & 0xFF);
	}
	// If temp_speaker_sound_time_count == 0, it is the end of loop of sound, so decrease the count and set sound in g_send_stream.
	if (temp_speaker_sound_time_count == 0){
		temp_speaker_sound_time_count--;
		temp_speaker_silence_time_count = robotbase_speaker_silence_time_count;
		controller.set(CTL_BUZZER, 0x00);
		// Decreace the speaker sound loop count because when it turns to silence this sound loop will be over when silence end, so we can decreace the sound loop count here.
		// If it is for constant beeper.play, the loop count will be less than 0, it will not decrease either.
		if (robotbase_speaker_sound_loop_count > 0){
			robotbase_speaker_sound_loop_count--;
		}
	}
	// If temp_speaker_silence_time_count == -1, it is in loop of sound, so decrease the count.
	if (temp_speaker_silence_time_count == -1){
		temp_speaker_sound_time_count--;
	}
	// If temp_speaker_sound_time_count == -1, it is in loop of silence, so decrease the count.
	if (temp_speaker_sound_time_count == -1){
		temp_speaker_silence_time_count--;
	}
}

void process_led()
{
	uint16_t led_brightness = 100;
	switch (robotbase_led_type)
	{
		case LED_STEADY:
		{
			robotbase_led_update_flag = false;
			break;
		}
		case LED_FLASH:
		{
			if (live_led_cnt_for_switch > robotbase_led_cnt_for_switch / 2)
				led_brightness = 0;
			break;
		}
		case LED_BREATH:
		{
			if (live_led_cnt_for_switch > robotbase_led_cnt_for_switch / 2)
				led_brightness = led_brightness * (2 * (float)live_led_cnt_for_switch / (float)robotbase_led_cnt_for_switch - 1.0);
			else
				led_brightness = led_brightness * (1.0 - 2 * (float)live_led_cnt_for_switch / (float)robotbase_led_cnt_for_switch);
			break;
		}
	}

	if (live_led_cnt_for_switch++ > robotbase_led_cnt_for_switch)
		live_led_cnt_for_switch = 0;

	switch (robotbase_led_color)
	{
		case LED_GREEN:
		{
			led.set(led_brightness, 0);
			break;
		}
		case LED_ORANGE:
		{
			led.set(led_brightness, led_brightness);
			break;
		}
		case LED_RED:
		{
			led.set(0, led_brightness);
			break;
		}
		case LED_OFF:
		{
			led.set(0, 0);
			break;
		}
	}
	//ROS_INFO("%s %d: live_led_cnt_for_switch: %d, led_brightness: %d.", __FUNCTION__, __LINE__, live_led_cnt_for_switch, led_brightness);
}

void robotbase_reset_odom_pose(void)
{
	// Reset the odom pose to (0, 0)
	boost::mutex::scoped_lock(pose_mutex);
	odom.setX(0);
	odom.setY(0);
}
/*
void robotbase_restore_slam_correction()
{
	// For restarting slam
	boost::mutex::scoped_lock(odom_mutex);
	pose_x += robot::instance()->getRobotCorrectionX();
	pose_y += robot::instance()->getRobotCorrectionY();
	robot::instance()->offsetAngle(robot::instance()->offsetAngle() + robot::instance()->getRobotCorrectionYaw());
	ROS_INFO("%s %d: Restore slam correction as x: %f, y: %f, angle: %f.", __FUNCTION__, __LINE__, robot::instance()->getRobotCorrectionX(), robot::instance()->getRobotCorrectionY(), robot::instance()->getRobotCorrectionYaw());
}*/

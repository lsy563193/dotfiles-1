#include "pp.h"
#include <std_msgs/String.h>

#define  _RATE 50

#define POWER_ACTIVE 1
#define POWER_DOWN 7
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

int robotbase_init(void)
{
	int	serr_ret;
	int base_ret;
	int sers_ret;
	int speaker_ret;

	robotbase_thread_stop = false;
	send_stream_thread = true;
	
	while (!serial.is_ready()) {
		ROS_ERROR("serial not ready\n");
	}

	robotbase_reset_send_stream();

	ROS_INFO("waiting robotbase awake ");
//	serr_ret = pthread_create(&receiPortThread_id, NULL, serial_receive_routine_cb, NULL);
	auto serial_receive_routine = new boost::thread(serial_receive_routine_cb);
	serial_receive_routine->detach();
	// todo:If do not usleep for 20ms, it will process died, still don't know why. --by Austin Liu
	// todo:Seems it works fine now, still don't know why. --by Austin Liu(171228)
	//usleep(20000);
//	base_ret = pthread_create(&robotbaseThread_id, NULL, robotbase_routine_cb, NULL);
	auto robotbase_routine = new boost::thread(robotbase_routine_cb);
	robotbase_routine->detach();
//	sers_ret = pthread_create(&sendPortThread_id, NULL, serial_send_routine_cb, NULL);
	auto serial_send_routine = new boost::thread(serial_send_routine_cb);
	serial_send_routine->detach();
//	speaker_ret = pthread_create(&sendPortThread_id, NULL, speaker_play_routine_cb, NULL);
	auto speaker_play_routine = new boost::thread(speaker_play_routine_cb);
	speaker_play_routine->detach();
	if (base_ret < 0 || serr_ret < 0 || sers_ret < 0 || speaker_ret < 0) {
		is_robotbase_init = false;
		robotbase_thread_stop = true;
		send_stream_thread = false;
		if (base_ret < 0) {ROS_INFO("%s,%d, fail to create robotbase thread!! %s ", __FUNCTION__,__LINE__,strerror(base_ret));}
		if (serr_ret < 0) {ROS_INFO("%s,%d, fail to create serial receive thread!! %s ",__FUNCTION__,__LINE__, strerror(serr_ret));}
		if (sers_ret < 0){ROS_INFO("%s,%d, fail to create serial send thread!! %s ",__FUNCTION__,__LINE__,strerror(sers_ret));}
		if (speaker_ret < 0){ROS_INFO("%s,%d, fail to create speaker paly thread!! %s ",__FUNCTION__,__LINE__,strerror(speaker_ret));}
		return -1;
	}
	is_robotbase_init = true;
	return 0;
}

void debug_received_stream()
{
	ROS_INFO("%s %d: Received stream:", __FUNCTION__, __LINE__);
	for (int i = 0; i < RECEI_LEN; i++)
		printf("%02x ", serial.receive_stream[i]);
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
		serial.setSendData(CTL_BUZZER, 0x00);
		gyro.setOff();
		wheel.stop();
		brush.stop();
		vacuum.stop();
		serial.setCleanMode(POWER_DOWN);
		usleep(40000);
		send_stream_thread = false;
		usleep(40000);
		serial.close();
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
			serial.setSendData(i, 0x00);
		else
			serial.setSendData(i, 0x64);
	}
	serial.setSendData(0, 0xaa);
	serial.setSendData(1, 0x55);
	serial.setSendData(SEND_LEN - 2, 0xcc);
	serial.setSendData(SEND_LEN - 1, 0x33);

	serial.setCleanMode(POWER_ACTIVE);
	uint8_t buf[SEND_LEN];
	memcpy(buf, serial.send_stream, sizeof(uint8_t) * SEND_LEN);
	uint8_t crc;
	crc = serial.calc_buf_crc8(buf, SEND_LEN - 3);
	serial.setSendData(SEND_LEN - 3, crc);
}

void serial_receive_routine_cb()
{
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
		ret = serial.read(1, &header1);
		if (ret <= 0 ){
			ROS_WARN("%s, %d, serial read return %d ,  requst %d byte",__FUNCTION__,__LINE__,ret,1);
			continue;
		}
		if(header1 != h1)
			continue;
		ret= serial.read(1,&header2);
		if (ret <= 0 ){
			ROS_WARN("%s,%d,serial read return %d , requst %d byte",__FUNCTION__,__LINE__,ret,1);
			continue;
		}
		if(header2 != h2){
			continue;
		}
		ret = serial.read(wh_len, receiData);
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

		c_crc = serial.calc_buf_crc8(tempData, wh_len - 1);//calculate crc8 with header bytes
		r_crc = receiData[whtc_len];

		if (r_crc == c_crc){
			if (receiData[wh_len - 1] == t2 && receiData[wh_len - 2] == t1) {
				for (j = 0; j < wht_len; j++) {
					serial.receive_stream[j + 2] = receiData[j];
				}
				if(pthread_cond_signal(&recev_cond)<0)//if receive data corret than send signal
					ROS_ERROR(" in serial read, pthread signal fail !");
			}
			else {
				ROS_WARN(" in serial read ,data tail error\n");
			}
		}
		else {
			ROS_ERROR("%s,%d,in serial read ,data crc error\n",__FUNCTION__,__LINE__);
		}
	}
	ROS_INFO("\033[32m%s\033[0m,%d,exit!",__FUNCTION__,__LINE__);
}

void robotbase_routine_cb()
{
	ROS_INFO("robotbase,\033[32m%s\033[0m,%d, is up!",__FUNCTION__,__LINE__);

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

		uint16_t	lw_speed, rw_speed;
		lw_speed = (serial.receive_stream[REC_LW_S_H] << 8) | serial.receive_stream[REC_LW_S_L];
		rw_speed = (serial.receive_stream[REC_RW_S_H] << 8) | serial.receive_stream[REC_RW_S_L];

		auto _right_wheel_speed = (lw_speed & 0x8000) ? -(static_cast<float>((lw_speed & 0x7fff) / 1000.0)) : static_cast<float>((lw_speed & 0x7fff) / 1000.0);
		auto _left_wheel_speed = (rw_speed & 0x8000) ? -(static_cast<float>((rw_speed & 0x7fff) / 1000.0)) : static_cast<float>((rw_speed & 0x7fff) / 1000.0);
		sensor.lw_vel = _left_wheel_speed;
		sensor.rw_vel = _right_wheel_speed;
		wheel.setLeftWheelActualSpeed(_left_wheel_speed);
		wheel.setRightWheelActualSpeed(_right_wheel_speed);

		sensor.angle = -(float)((int16_t)((serial.receive_stream[REC_ANGLE_H] << 8) | serial.receive_stream[REC_ANGLE_L])) / 100.0;//ros angle * -1
//		sensor.angle -= robot::instance()->offsetAngle();
		gyro.setAngle(sensor.angle);
		sensor.angle_v = -(float)((int16_t)((serial.receive_stream[REC_ANGLE_V_H] << 8) | serial.receive_stream[REC_ANGLE_V_L])) / 100.0;//ros angle * -1
		gyro.setAngleV(sensor.angle_v);

		auto _left_wheel_current = static_cast<float>((((serial.receive_stream[REC_LW_C_H] << 8) | serial.receive_stream[REC_LW_C_L]) & 0x7fff) * 1.622);
		auto _right_wheel_current = static_cast<float>((((serial.receive_stream[REC_RW_C_H] << 8) | serial.receive_stream[REC_RW_C_L]) & 0x7fff) * 1.622);
		sensor.lw_crt = _left_wheel_current;
		sensor.rw_crt = _right_wheel_current;
		wheel.setLeftWheelCurrent(_left_wheel_current);
		wheel.setRightWheelCurrent(_right_wheel_current);

		sensor.left_wall = ((serial.receive_stream[REC_L_WALL_H] << 8)| serial.receive_stream[REC_L_WALL_L]);

		auto _left_obs_value = ((serial.receive_stream[REC_L_OBS_H] << 8) | serial.receive_stream[REC_L_OBS_L]);
		obs.setLeft(_left_obs_value);
		sensor.l_obs = obs.getLeft();

		auto _front_obs_value = ((serial.receive_stream[REC_F_OBS_H] << 8) | serial.receive_stream[REC_F_OBS_L]);
		obs.setFront(_front_obs_value);
		sensor.f_obs = obs.getFront();

		auto _right_obs_value = ((serial.receive_stream[REC_R_OBS_H] << 8) | serial.receive_stream[REC_R_OBS_L]);
		obs.setRight(_right_obs_value);
		sensor.r_obs = obs.getRight();

		sensor.right_wall = ((serial.receive_stream[REC_R_WALL_H]<<8)|serial.receive_stream[REC_R_WALL_L]);

		sensor.lbumper = (serial.receive_stream[REC_BUMPER] & 0xf0) ? true : false;
		sensor.rbumper = (serial.receive_stream[REC_BUMPER] & 0x0f) ? true : false;
		auto _left_bumper_status = (serial.receive_stream[REC_BUMPER] & 0xf0) ? true : false;
		auto _right_bumper_status = (serial.receive_stream[REC_BUMPER] & 0x0f) ? true : false;
		sensor.lbumper = static_cast<uint8_t>(_left_bumper_status);
		sensor.rbumper = static_cast<uint8_t>(_right_bumper_status);
		bumper.setLeft(_left_bumper_status);
		bumper.setRight(_right_bumper_status);
//		printf("bl(%d),br(%d)\n",bumper.getLeft(), bumper.getRight());

		bumper.setLidarBumperStatus();
		if (bumper.getLidarBumperStatus())
		{
			sensor.lbumper = sensor.rbumper = 1;
			bumper.setLeft(true);
			bumper.setRight(true);
		}

		sensor.ir_ctrl = serial.receive_stream[REC_REMOTE_IR];
		if (sensor.ir_ctrl > 0)
		{
			ROS_DEBUG("%s %d: Remote received:%d", __FUNCTION__, __LINE__, sensor.ir_ctrl);
			remote.set(sensor.ir_ctrl);
		}

		sensor.c_stub = (serial.receive_stream[REC_CHARGE_STUB_4] << 24 ) | (serial.receive_stream[REC_CHARGE_STUB_3] << 16)
			| (serial.receive_stream[REC_CHARGE_STUB_2] << 8) | serial.receive_stream[REC_CHARGE_STUB_1];
		c_rcon.setStatus(sensor.c_stub);

		sensor.visual_wall = (serial.receive_stream[REC_VISUAL_WALL_H] << 8)| serial.receive_stream[REC_VISUAL_WALL_L];

		sensor.key = serial.receive_stream[REC_KEY];
		key.eliminate_jitter(sensor.key);

		sensor.c_s = serial.receive_stream[REC_CHARGE_STATE];
		charger.setChargeStatus(sensor.c_s);

		sensor.w_tank = (serial.receive_stream[REC_WATER_TANK]>0)?true:false;

		sensor.batv = (serial.receive_stream[REC_BAT_V]);
		battery.setVoltage((uint16_t)sensor.batv * 10);

		if(((serial.receive_stream[REC_L_CLIFF_H] << 8) | serial.receive_stream[REC_L_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_lcliff > CLIFF_LIMIT)
				last_lcliff = ((serial.receive_stream[REC_L_CLIFF_H] << 8) | serial.receive_stream[REC_L_CLIFF_L]);
			else
			{
				last_lcliff = ((serial.receive_stream[REC_L_CLIFF_H] << 8) | serial.receive_stream[REC_L_CLIFF_L]);
				sensor.lcliff = last_lcliff;
				cliff.setLeft(last_lcliff);
			}
		}
		else
		{
			last_lcliff = ((serial.receive_stream[REC_L_CLIFF_H] << 8) | serial.receive_stream[REC_L_CLIFF_L]);
			sensor.lcliff = last_lcliff;
			cliff.setLeft(last_lcliff);
		}

		if(((serial.receive_stream[REC_F_CLIFF_H] << 8) | serial.receive_stream[REC_F_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_fcliff > CLIFF_LIMIT)
				last_fcliff = ((serial.receive_stream[REC_F_CLIFF_H] << 8) | serial.receive_stream[REC_F_CLIFF_L]);
			else
			{
				last_fcliff = ((serial.receive_stream[REC_F_CLIFF_H] << 8) | serial.receive_stream[REC_F_CLIFF_L]);
				sensor.fcliff = last_fcliff;
				cliff.setFront(last_fcliff);
			}
		}
		else
		{
			last_fcliff = ((serial.receive_stream[REC_F_CLIFF_H] << 8) | serial.receive_stream[REC_F_CLIFF_L]);
			sensor.fcliff = last_fcliff;
			cliff.setFront(last_fcliff);
		}

		if(((serial.receive_stream[REC_R_CLIFF_H] << 8) | serial.receive_stream[REC_R_CLIFF_L]) < CLIFF_LIMIT)
		{
			if(last_rcliff > CLIFF_LIMIT)
				last_rcliff = ((serial.receive_stream[REC_R_CLIFF_H] << 8) | serial.receive_stream[REC_R_CLIFF_L]);
			else
			{
				last_rcliff = ((serial.receive_stream[REC_R_CLIFF_H] << 8) | serial.receive_stream[REC_R_CLIFF_L]);
				sensor.rcliff = last_rcliff;
				cliff.setRight(last_rcliff);
			}
		}
		else
		{
			last_rcliff = ((serial.receive_stream[REC_R_CLIFF_H] << 8) | serial.receive_stream[REC_R_CLIFF_L]);
			sensor.rcliff = last_rcliff;
			cliff.setRight(last_rcliff);
		}

		sensor.vacuum_selfcheck_status = (serial.receive_stream[REC_CL_OC] & 0x30);
		sensor.lbrush_oc = (serial.receive_stream[REC_CL_OC] & 0x08) ? true : false;		// left brush over current
		brush.setLeftOc(sensor.lbrush_oc);
		sensor.mbrush_oc = (serial.receive_stream[REC_CL_OC] & 0x04) ? true : false;		// main brush over current
		brush.setMainOc(sensor.mbrush_oc);
		sensor.rbrush_oc = (serial.receive_stream[REC_CL_OC] & 0x02) ? true : false;		// right brush over current
		brush.setRightOc(sensor.rbrush_oc);
		sensor.vcum_oc = (serial.receive_stream[REC_CL_OC] & 0x01) ? true : false;		// vaccum over current
		vacuum.setOc(sensor.vcum_oc);

		sensor.gyro_dymc = serial.receive_stream[REC_GYRO_DYMC];

		sensor.omni_wheel = (serial.receive_stream[REC_OMNI_W_H]<<8)|serial.receive_stream[REC_OMNI_W_L];

		if (last_x_acc == -1000)
			sensor.x_acc = (serial.receive_stream[REC_XACC_H]<<8)|serial.receive_stream[REC_XACC_L];//in mG
		else
			sensor.x_acc = ((serial.receive_stream[REC_XACC_H]<<8)|serial.receive_stream[REC_XACC_L] + last_x_acc) / 2;//in mG
		if (last_y_acc == -1000)
			sensor.y_acc = (serial.receive_stream[REC_YACC_H]<<8)|serial.receive_stream[REC_YACC_L];//in mG
		else
			sensor.y_acc = ((serial.receive_stream[REC_YACC_H]<<8)|serial.receive_stream[REC_YACC_L] + last_y_acc) / 2;//in mG
		if (last_z_acc == -1000)
			sensor.z_acc = (serial.receive_stream[REC_ZACC_H]<<8)|serial.receive_stream[REC_ZACC_L];//in mG
		else
			sensor.z_acc = ((serial.receive_stream[REC_ZACC_H]<<8)|serial.receive_stream[REC_ZACC_L] + last_z_acc) / 2;//in mG
		gyro.setXAcc(sensor.x_acc);
		gyro.setYAcc(sensor.y_acc);
		gyro.setZAcc(sensor.z_acc);

		sensor.plan = serial.receive_stream[REC_PLAN];
		if(sensor.plan)
			robot_timer.setPlanStatus(sensor.plan);

#if GYRO_DYNAMIC_ADJUSTMENT
		if (wheel.getLeftWheelActualSpeed() < 0.01 && wheel.getRightWheelActualSpeed() < 0.01)
		{
			gyro.setDynamicOn();
		} else
		{
			gyro.setDynamicOff();
		}
		gyro.checkTilt();
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
		angle_rad = deg_to_rad(odom.getAngle());
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

void serial_send_routine_cb()
{
	ROS_INFO("robotbase,\033[32m%s\033[0m,%d is up",__FUNCTION__,__LINE__);
	ros::Rate r(_RATE);
	uint8_t buf[SEND_LEN];
	int sl = SEND_LEN-3;
	while(send_stream_thread){
		r.sleep();
		if(serial.isSleep()){
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
		wheel.pidAdjustSpeed();
		brush.updatePWM();

		g_send_stream_mutex.lock();
		memcpy(buf,serial.send_stream,sizeof(uint8_t)*SEND_LEN);
		g_send_stream_mutex.unlock();
		buf[CTL_CRC] = serial.calc_buf_crc8(buf, sl);
		//debug_send_stream(&buf[0]);
		serial.write(SEND_LEN, buf);
	}
	ROS_INFO("\033[32m%s\033[0m,%d pthread exit",__FUNCTION__,__LINE__);
	//pthread_exit(NULL);
}

void core_thread_cb()
{
	ROS_INFO("Waiting for robot sensor ready.");
	while (robot::instance() == nullptr || !robot::instance()->isSensorReady()) {
		usleep(1000);
	}
	ROS_INFO("Robot sensor ready.");
//	speaker.play(VOICE_WELCOME_ILIFE);
	usleep(200000);

	boost::shared_ptr<Mode> p_mode = nullptr;
	if (charger.isOnStub() || charger.isDirected())
		p_mode.reset(new ModeCharge());
	else
	{
		speaker.play(VOICE_PLEASE_START_CLEANING, false);
		p_mode.reset(new ModeIdle());
	}

	while(ros::ok())
	{
		ROS_INFO("%s %d: %x", __FUNCTION__, __LINE__, p_mode);
		p_mode->run();
		auto next_mode = p_mode->getNextMode();
		p_mode.reset();
		ROS_INFO("%s %d: %x", __FUNCTION__, __LINE__, p_mode);
		p_mode.reset(getNextMode(next_mode));
		ROS_INFO("%s %d: %x", __FUNCTION__, __LINE__, p_mode);
	}

}

Mode *getNextMode(int next_mode_i_)
{

	ROS_INFO("%s %d: next mode:%d", __FUNCTION__, __LINE__, next_mode_i_);
	switch (next_mode_i_)
	{
		case Mode::md_charge:
			return new ModeCharge();
		case Mode::md_sleep:
			return new ModeSleep();
		case Mode::md_go_to_charger:
			return new ModeGoToCharger();
		case Mode::md_remote:
			return new ModeRemote();

		case Mode::cm_navigation:
			return new CleanModeNav();
		case Mode::cm_wall_follow:
			return new CleanModeFollowWall();
		case Mode::cm_spot:
			return new CleanModeSpot();
		case Mode::cm_test:
			return new CleanModeTest();
		case Mode::cm_exploration:
			return new CleanModeExploration();
//		case Mode::cm_exploration:
//			return new CleanModeExploration();
		default:
		{
			ROS_INFO("%s %d: next mode:%d", __FUNCTION__, __LINE__, next_mode_i_);
			return new ModeIdle();
		}
	}
}


void process_beep()
{
	// This routine handles the speaker sounding logic
	// If temp_speaker_silence_time_count == 0, it is the end of loop of silence, so decrease the count and set sound in g_send_stream.
	if (temp_speaker_silence_time_count == 0){
		temp_speaker_silence_time_count--;
		temp_speaker_sound_time_count = robotbase_speaker_sound_time_count;
		serial.setSendData(CTL_BUZZER, robotbase_sound_code & 0xFF);
	}
	// If temp_speaker_sound_time_count == 0, it is the end of loop of sound, so decrease the count and set sound in g_send_stream.
	if (temp_speaker_sound_time_count == 0){
		temp_speaker_sound_time_count--;
		temp_speaker_silence_time_count = robotbase_speaker_silence_time_count;
		serial.setSendData(CTL_BUZZER, 0x00);
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
	boost::mutex::scoped_lock(odom_mutex);
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
void speaker_play_routine_cb()
{
	speaker.playRoutine();
}


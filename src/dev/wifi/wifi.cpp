#include "wifi/wifi.h"
#include <ros/ros.h>
#include <time.h>
#include "speaker.h"
#include "mode.hpp"
#include "move_type.hpp"
#include "serial.h"
#include "error.h"
#include "battery.h"
#include "event_manager.h"
#include "dev.h"

#define CLOUD_DOMAIN_ID 5479
#define CLOUD_SUBDOMAIN_ID 6369
#define CLOUD_AP "Robot"
#define CLOUD_KEY "BEEE3F8925CC677AC1F3D1D9FEBC8B8632316C4B98431632E11933E1FB3C2167E223EFCC653ED3324E3EA219CCCD3E97D8242465C9327A91578901CA65015DB1C80DD4A0F45C5CC7DF1267A2FD5C00E7BD3175C2BB08BA8CFA886CCED2F70D214FB2CC88ECCA6BB1B5F4E6EE9948D424"

S_Wifi s_wifi;

bool S_Wifi::is_wifi_connected_ = false;
bool S_Wifi::is_cloud_connected_ = false;

S_Wifi::S_Wifi():isStatusRequest_(false),inFactoryTest_(false),isRegDevice_(false)
				 ,is_wifi_active_(false)
{
	init();
	this->sleep();
}
S_Wifi::~S_Wifi()
{
	deinit();
}

bool S_Wifi::deinit()
{
	this->sleep();
	return true;
}

bool S_Wifi::init()
{
	robot_work_mode_ = wifi::WorkMode::SHUTDOWN;
	INFO_BLUE(" wifi register msg... ");
	//-----on reg device ,connect or disconnect-----
	//regestory request
	s_wifi_rx_.regOnNewMsgListener<wifi::RegDeviceRequestRxMsg>(
		[&]( const wifi::RxMsg &a_msg ) 
		{
			wifi::RegDeviceTxMsg p(	wifi::RegDeviceTxMsg::CloudEnv::MAINLAND_DEV,
									{0, 0, 1},
									CLOUD_DOMAIN_ID,
									CLOUD_SUBDOMAIN_ID,
									wifi::RegDeviceTxMsg::toKeyArray(CLOUD_KEY),
									a_msg.seq_num());
			s_wifi_tx_.push(std::move( p )).commit();
			isRegDevice_ = true;
			is_wifi_connected_ = true;
			wifi_led.setMode(LED_FLASH,WifiLed::state::on);
			speaker.play( VOICE_WIFI_CONNECTED,false);
		}
	);
/*
	s_wifi_rx_.regOnNewMsgListener<wifi::wifiConnectedNotifRxMsg>(
			[this]( const wifi::RxMsg &a_msg ) {
				is_wifi_connected_ = true;
				wifi_led.setMode(LED_FLASH, WifiLed::state::on);
				speaker.play( VOICE_WIFI_CONNECTED,false);
			});
*/
	//wifi disconncet
	s_wifi_rx_.regOnNewMsgListener<wifi::wifiDisconnectedNotifRxMsg>(
			[this]( const wifi::RxMsg &a_msg ) {
				is_wifi_connected_ = false;
				wifi_led.setMode(LED_STEADY, WifiLed::state::off);
				speaker.play( VOICE_WIFI_UNCONNECTED,false);
			});
	//cloud connect
	s_wifi_rx_.regOnNewMsgListener<wifi::CloudConnectedNotifRxMsg>(
			[this]( const wifi::RxMsg &a_msg ) {
				is_cloud_connected_ = true;
				wifi_led.setMode(LED_STEADY,WifiLed::state::on);
				speaker.play( VOICE_CLOUD_CONNECTED,false);
				uploadLastCleanData();

				clearRealtimeMap(0x00);
			});
	//cound disconnect
	s_wifi_rx_.regOnNewMsgListener<wifi::CloudDisconnectedNotifRxMsg>(
				[this](const wifi::RxMsg &a_msg){
				is_cloud_connected_ = false;
				wifi_led.setMode(LED_STEADY,WifiLed::state::off);
				speaker.play(VOICE_CLOUD_UNCONNECTED,false);
				});

	//-----app query -----
	//query device status
	s_wifi_rx_.regOnNewMsgListener<wifi::QueryDeviceStatusRxMsg>(
			[&]( const wifi::RxMsg &a_msg ) {
				const wifi::QueryDeviceStatusRxMsg &msg = static_cast<const wifi::QueryDeviceStatusRxMsg&>( a_msg );
				replyRobotStatus( msg.MSG_CODE,msg.seq_num());
			});
	//query schedule
	s_wifi_rx_.regOnNewMsgListener<wifi::QueryScheduleStatusRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::QueryScheduleStatusRxMsg &msg = static_cast<const wifi::QueryScheduleStatusRxMsg&>( a_msg );
				//todo
				std::vector<wifi::ScheduleStatusTxMsg::Schedule> vec_sch;
				for(int i = 0;i<10;i++)
					vec_sch.push_back(wifi::ScheduleStatusTxMsg::Schedule::createDisabled(i));//tmp set to disable
				//ack
				wifi::ScheduleStatusTxMsg p(
							vec_sch,
							msg.seq_num()
							);
				s_wifi_tx_.push(std::move(p)).commit();
			});
	//query consumption
	s_wifi_rx_.regOnNewMsgListener<wifi::QueryConsumableStatusRxMsg>(
			[this]( const wifi::RxMsg &a_msg ) {
				const wifi::QueryConsumableStatusRxMsg &msg = static_cast<const wifi::QueryConsumableStatusRxMsg&>( a_msg );
				uint16_t worktime = (uint16_t)(robot_timer.getWorkTime()/3600);
				//ack
				wifi::ConsumableStatusTxMsg p(
							worktime,
							0x64,0x64,0x64,0x64,0x64,//todo
							msg.seq_num()	
						);
				s_wifi_tx_.push(std::move(p)).commit();

			});

	//-----app control -----
	//set clean mode
	s_wifi_rx_.regOnNewMsgListener<wifi::SetModeRxMsg>(
			[&]( const wifi::RxMsg &a_msg){
				const wifi::SetModeRxMsg &msg = static_cast<const wifi::SetModeRxMsg&>( a_msg );
				//ack
				wifi::Packet p(
							-1,
							msg.seq_num(),
							0,
							msg.msg_code(),
							msg.data()
							);
				s_wifi_tx_.push(std::move(p)).commit();
				//set mode
				setRobotCleanMode(msg.getWorkMode());
			});
	//set room mode
	s_wifi_rx_.regOnNewMsgListener<wifi::SetRoomModeRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::SetRoomModeRxMsg &msg = static_cast<const wifi::SetRoomModeRxMsg &>( a_msg );
				//ack
				wifi::Packet p(
							-1,
							msg.seq_num(),
							0,
							msg.msg_code(),
							msg.data()
							);
				s_wifi_tx_.push(std::move(p)).commit();
			});
	//set max clean power
	s_wifi_rx_.regOnNewMsgListener<wifi::SetMaxCleanPowerRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::SetMaxCleanPowerRxMsg &msg = static_cast<const wifi::SetMaxCleanPowerRxMsg&>( a_msg );
				//ack
				//todo
				wifi::MaxCleanPowerTxMsg p(false,false);
				s_wifi_tx_.push( std::move(p)).commit();
			});
	//remote control
	s_wifi_rx_.regOnNewMsgListener<wifi::RemoteControlRxMsg>(
			[this]( const wifi::RxMsg &a_msg ) {
				const wifi::RemoteControlRxMsg &msg = static_cast<const wifi::RemoteControlRxMsg&>( a_msg );
				appRemoteCtl(msg.getCmd());
				//ack
				wifi::Packet p(
							-1,
							msg.seq_num(),
							0,
							msg.msg_code(),
							msg.data()
							);
				s_wifi_tx_.push(std::move(p)).commit();
			});
	//todo
	//s_wifi_rx.regOnNewMsgListener<wifi::SetScheduleRxMsg>{
	//		[this](const wifi::RxMsg &a_msg){
	//			const wifi::SetScheduleRxMsg &msg = static_cost<const wifi::ScheduleRxMsg&>(a_msg);
	//			//ack
		//		wifi::Packet p(
		//					-1,
		//					msg.seq_num(),
		//					0,
		//					msg.msg_code(),
		//					msg.data()
		//					);
		//		s_wifi_tx_.push(std::move(p)).commit();

	//			//todo
	//			setSchedule(msg.seq_num());
	//		});
	//}
	//reset consumable status
	s_wifi_rx_.regOnNewMsgListener<wifi::ResetConsumableStatusRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::ResetConsumableStatusRxMsg &msg = static_cast<const wifi::ResetConsumableStatusRxMsg&>( a_msg );
				//ack
				wifi::Packet p(
							-1,
							msg.seq_num(),
							0,
							msg.msg_code(),
							msg.data()
							);
				s_wifi_tx_.push(std::move(p)).commit();
				//todo
			});

	//sync clock
	s_wifi_rx_.regOnNewMsgListener<wifi::SyncClockRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::SyncClockRxMsg &msg = static_cast<const wifi::SyncClockRxMsg&>( a_msg );
				syncClock(msg.getYear(),msg.getMonth(),msg.getDay(),msg.getHour(),msg.getMin(),msg.getSec());	
				//ack
				wifi::Packet p(
							-1,
							msg.seq_num(),
							0,
							msg.msg_code(),
							msg.data()
							);
				s_wifi_tx_.push(std::move(p)).commit();
				is_wifi_connected_ = true;
				is_cloud_connected_ = true;
				wifi_led.setMode(LED_STEADY, WifiLed::state::on);
			});
	//set status requset
	s_wifi_rx_.regOnNewMsgListener<wifi::RealtimeStatusRequestRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::RealtimeStatusRequestRxMsg &msg = static_cast<const wifi::RealtimeStatusRequestRxMsg&>( a_msg );
				isStatusRequest_ = msg.isEnable()?true:false;
				//ack
				wifi::Packet p(
						-1,
						msg.seq_num(),
						0,
						msg.msg_code(),
						msg.data()
						);
				s_wifi_tx_.push(std::move(p)).commit();
			});
	//set do not disturb
	s_wifi_rx_.regOnNewMsgListener<wifi::SetDoNotDisturbRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::SetDoNotDisturbRxMsg &msg = static_cast<const wifi::SetDoNotDisturbRxMsg&>( a_msg );
				//ack
				wifi::Packet p(
						-1,
						msg.seq_num(),
						0,
						msg.msg_code(),
						msg.data()
						);
				s_wifi_tx_.push(std::move(p)).commit();
				//todo
			}
	);
	//factory reset
	s_wifi_rx_.regOnNewMsgListener<wifi::FactoryResetRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::FactoryResetRxMsg &msg = static_cast<const wifi::FactoryResetRxMsg&>( a_msg );
				//ack
				wifi::Packet p(
						-1,
						msg.seq_num(),
						0,
						msg.msg_code(),
						msg.data()
						);
				s_wifi_tx_.push(std::move(p)).commit();
				//todo
			}
	);
	//factory test
	s_wifi_rx_.regOnNewMsgListener<wifi::FactoryTestRxMsg>(
			[this](const wifi::RxMsg &a_msg){
				const wifi::FactoryTestRxMsg &msg = static_cast<const wifi::FactoryTestRxMsg&>( a_msg );
				//todo
				inFactoryTest_ = true;
				wifi_led.setMode(LED_FLASH,WifiLed::state::on);
			}
	);

	//-----ack cloud------
	s_wifi_rx_.regOnNewMsgListener<wifi::RealtimeMapUploadAckMsg>(
			[&](const wifi::RxMsg &a_msg){
			}
	);
	s_wifi_rx_.regOnNewMsgListener<wifi::ClearRealtimeMapAckMsg>(
			[&](const wifi::RxMsg &a_msg){
				const wifi::ClearRealtimeMapAckMsg &msg = static_cast<const wifi::ClearRealtimeMapAckMsg&>( a_msg );
			}
	);	
	s_wifi_rx_.regOnNewMsgListener<wifi::wifiResumeAckMsg>(
			[&](const wifi::RxMsg &a_msg){
				const wifi::wifiResumeAckMsg &msg = static_cast<const wifi::wifiResumeAckMsg&>(a_msg);	
					if(is_wifi_active_ == false){
						INFO_BLUE("RESUME ACK");
						is_wifi_active_ = true;
						this->reboot();
					}
				});
	s_wifi_rx_.regOnNewMsgListener<wifi::wifiSuspendAckMsg>(
			[&](const wifi::RxMsg &a_msg){
				const wifi::wifiSuspendAckMsg &msg = static_cast<const wifi::wifiSuspendAckMsg&>(a_msg);
					is_wifi_active_ = false;
					wifi_led.setMode(LED_STEADY, WifiLed::state::off);
				});

	INFO_BLUE("register done ");
	return true;
}

uint8_t S_Wifi::replyRobotStatus(int msg_code,const uint8_t seq_num)
{
	wifi::WorkMode work_mode = robot_work_mode_;
	uint8_t error_code = 0;
	wifi::DeviceStatusBaseTxMsg::CleanMode box;
	box = water_tank.checkEquipment()? wifi::DeviceStatusBaseTxMsg::CleanMode::WATER_TANK: wifi::DeviceStatusBaseTxMsg::CleanMode::DUST;
	if(robot::instance()->p_mode != nullptr)
	{
		int next_mode = (int)robot::instance()->p_mode->getNextMode();
		setWorkMode((int)next_mode);	
		work_mode = getWorkMode();
	}
	switch (error.get())
	{
		case ERROR_CODE_NONE:
			error_code = 0x00;
			break;

		case ERROR_CODE_BUMPER:
			error_code = 0x01;
			break;

		case ERROR_CODE_OBS:
			error_code = 0x11;
			break;

		case ERROR_CODE_LEFTWHEEL:
			error_code = 0x51;
			break;

		case ERROR_CODE_RIGHTWHEEL:
			error_code = 0x52;
			break;

		case ERROR_CODE_LEFTBRUSH:
			error_code = 0x41;
			break;

		case ERROR_CODE_RIGHTBRUSH:
			error_code = 0x42;
			break;

		case ERROR_CODE_CLIFF:
			error_code = 0x21;
			break;

		case ERROR_CODE_STUCK:
			error_code = 0xd1;
			break;

		case ERROR_CODE_MAINBRUSH:
			error_code = 0x61;
			break;

		case ERROR_CODE_VACUUM:
			error_code = 0x71;
			break;

		case ERROR_CODE_WATERTANK:
			error_code = 0x81;
			break;

		case ERROR_CODE_BTA:
			error_code = 0xa1;
			break;

		case ERROR_CODE_DUSTBIN:
			error_code = 0x91;
			break;

		case ERROR_CODE_GYRO:
			error_code = 0xB1;
			break;

		case ERROR_CODE_LIDAR:
			error_code = 0xc1;
			break;

		case ERROR_CODE_AIRPUMP:
			error_code = 0x82;
			break;

		case ERROR_CODE_FILTER:
			error_code = 0x93;
			break;

		case ERROR_CODE_OTHER:
			error_code = 0xe1;
			break;

		default:
			error_code = 0x00;
	}

	if(msg_code == 0xc8)// auto upload
	{
		wifi::DeviceStatusUploadTxMsg p(
				work_mode,
				wifi::DeviceStatusBaseTxMsg::RoomMode::LARGE,//default set large
				box,
				serial.getSendData(CTL_VACCUM_PWR),
				serial.getSendData(CTL_BRUSH_MAIN),
				battery.getPercent(),
				0x01,//notify sound wav
				0x01,//led on/off
				error_code,
				seq_num);

		s_wifi_tx_.push(std::move( p )).commit();
	}
	else if(msg_code == 0x41)//app check upload
	{
		wifi::DeviceStatusReplyTxMsg p(
				work_mode,
				wifi::DeviceStatusBaseTxMsg::RoomMode::LARGE,//default set larger
				box,
				serial.getSendData(CTL_VACCUM_PWR),
				serial.getSendData(CTL_BRUSH_MAIN),
				battery.getPercent(),
				0x01,//notify sound wav
				0x01,//led on/off
				error_code,
				seq_num);
		s_wifi_tx_.push(std::move( p )).commit();
	}
	return 0;
}

uint8_t S_Wifi::replyRealtimeMap()
{
	if(!is_wifi_connected_ && !is_cloud_connected_)
		return 1;
	uint32_t time  = (uint32_t)ros::Time::now().toSec();
	std::vector<uint8_t> map_data;
	std::vector<std::vector<uint8_t>> map_pack;
	int pack_cnt=0;
	int byte_cnt=0;
	ROS_INFO("\033[1;33m realtime map send work mode = %d\033[0m", (int)getWorkMode());
	if( getWorkMode() == wifi::WorkMode::PLAN1)
	//			|| robot_work_mode_ == wifi::WorkMode::WALL_FOLLOW 
	//			||robot_work_mode_ == wifi::WorkMode::SPOT
	//			||robot_work_mode_ == wifi::WorkMode::HOMING
	//			||robot_work_mode_ == wifi::WorkMode::FIND)
	{
		auto mode = boost::dynamic_pointer_cast<ACleanMode>(robot::instance()->p_mode);
		GridMap g_map = mode->clean_map_;
		uint16_t clean_area = (uint16_t)(g_map.getCleanedArea()*CELL_SIZE*CELL_SIZE);
		//push clean area and work time
		map_data.push_back((uint8_t)((clean_area&0xff00)>>8));
		map_data.push_back((uint8_t)clean_area);
		map_data.push_back((uint8_t)((robot_timer.getWorkTime()&0x0000ff00)>>8));
		map_data.push_back((uint8_t)robot_timer.getWorkTime());
		byte_cnt+=4;
		int16_t x_min,x_max,y_min,y_max;
		g_map.getMapRange(CLEAN_MAP,&x_min,&x_max,&y_min,&y_max);
		for(int16_t i=x_min;i<x_max;i++)
		{
			for(int16_t j=y_min;j<y_max;j++)
			{
				if(pack_cnt>=255){
					pack_cnt=255;
					ROS_ERROR("%s,%d,MAP TOO BIG TO SEND",__FUNCTION__,__LINE__);
					break;
				}
				CellState c_state = g_map.getCell(CLEAN_MAP,i,j);
				if(c_state >= CLEANED ){
					map_data.push_back((uint8_t) (i>>8));
					map_data.push_back((uint8_t) (0x00ff&i));
					map_data.push_back((uint8_t) (j>>8));
					map_data.push_back((uint8_t) (0x00ff&j));
					byte_cnt+=4;
				}	
				if(byte_cnt>= 480)
				{
					map_pack.push_back(map_data);
					map_data.clear();
					pack_cnt+=1;
					//push clean area and work time
					map_data.push_back((uint8_t)((clean_area&0xff00)>>8));
					map_data.push_back((uint8_t)clean_area);
					map_data.push_back((uint8_t)((robot_timer.getWorkTime()&0x0000ff00)>>8));
					map_data.push_back((uint8_t)robot_timer.getWorkTime());
					byte_cnt=4;
				}	
			}
		}
		if(byte_cnt < 480)
			map_pack.push_back(map_data);
		ROS_INFO("%s,%d,map_pack size %ld",__FUNCTION__,__LINE__,map_pack.size());
		for(int k=1;k<=map_pack.size();k++){
			wifi::RealtimeMapUploadTxMsg p(
								time,
								(uint8_t)k,
								(uint8_t)map_pack.size(),
								map_pack[k-1]
								);
			s_wifi_tx_.push(std::move(p)).commit();
		}
	}
	/*
	else{
		wifi::RealtimeMapUploadTxMsg p(
								time,
								0x00,
								0x00,
								{0x00,0x00,0x00,0x00,0x00}
								);
		s_wifi_tx_.push(std::move(p)).commit();

	}
	*/
	return 0;
}

uint8_t S_Wifi::setRobotCleanMode(wifi::WorkMode work_mode)
{ 
	static wifi::WorkMode last_mode;
	if(!is_wifi_connected_ && !is_cloud_connected_)
		return 1;
	ROS_INFO("%s,%d,work mode  = %d",__FUNCTION__,__LINE__,(int)work_mode);
	switch(work_mode)
	{
		case wifi::WorkMode::SLEEP:
			ev.key_long_pressed = true;//set sleep mode
			break;
		case wifi::WorkMode::IDLE:
			if(last_mode == wifi::WorkMode::PLAN1 
						|| last_mode == wifi::WorkMode::WALL_FOLLOW
						|| last_mode == wifi::WorkMode::SPOT
						|| last_mode == wifi::WorkMode::HOMING
						|| last_mode == wifi::WorkMode::FIND
						|| last_mode == wifi::WorkMode::RANDOM
						|| last_mode == wifi::WorkMode::REMOTE)//get last mode
			{
				remote.set(REMOTE_CLEAN);
				beeper.beepForCommand(true);
			}
			else{
				beeper.beepForCommand(false);
			}
			INFO_BLUE("receive mode idle");
			break;
		case wifi::WorkMode::RANDOM:
			beeper.beepForCommand(false);
			remote.set(REMOTE_CLEAN);
			INFO_BLUE("receive mode random");
			break;
		case wifi::WorkMode::WALL_FOLLOW:
			if(last_mode == wifi::WorkMode::WALL_FOLLOW)
			{
				beeper.beepForCommand(false);
				remote.set(REMOTE_CLEAN);
			}
			else{
				beeper.beepForCommand(true);
				remote.set(REMOTE_WALL_FOLLOW);//wall follow
			}

			INFO_BLUE("receive mode wall follow");
			break;
		case wifi::WorkMode::SPOT:
			if(last_mode == wifi::WorkMode::SPOT)
			{
				beeper.beepForCommand(false);
				remote.set(REMOTE_CLEAN);
			}
			else{
				beeper.beepForCommand(false);
				remote.set(REMOTE_SPOT);//spot
			}
			INFO_BLUE("receive mode spot");
			break;
		case wifi::WorkMode::PLAN1://plan 1
			beeper.beepForCommand(true);
			remote.set(REMOTE_CLEAN);//clean key
			if(last_mode == wifi::WorkMode::PLAN1)
				clearRealtimeMap(0x00);
			INFO_BLUE("receive mode plan1");
			break;
		case wifi::WorkMode::PLAN2://plan 2
			beeper.beepForCommand(true);
			remote.set(REMOTE_CLEAN);//clean key
			INFO_BLUE("receive mode plan2");
			break;
		case wifi::WorkMode::HOMING:
			if(last_mode == wifi::WorkMode::HOMING)
			{
				remote.set(REMOTE_CLEAN);
				beeper.beepForCommand(false);
			}
			else
			{
				remote.set(REMOTE_HOME);//go home
				beeper.beepForCommand(true);
			}
			INFO_BLUE("receive mode gohome");
			break;
		case wifi::WorkMode::CHARGE:
			INFO_BLUE("remote charger command ");
			beeper.beepForCommand(false);
			break;
		case wifi::WorkMode::REMOTE:
			remote.set(REMOTE_FORWARD);//remote hand mode
			INFO_BLUE("remote hand mode command ");
			break;

		case wifi::WorkMode::FIND:
			if(last_mode == wifi::WorkMode::FIND)
				beeper.beepForCommand(false);
			else{
				remote.set(REMOTE_HOME);//find home point 
				beeper.beepForCommand(true);
			}
			INFO_BLUE("remote app find home mode command ");
			break;

	}
	//replyRobotStatus(0xc8,0x00);
	last_mode = work_mode;
	return 0;
}

uint8_t S_Wifi::clearRealtimeMap(const uint8_t seq_num)
{
	if(!is_wifi_connected_ && !is_cloud_connected_)
		return 1;
	wifi::ClearRealtimeMapTxMsg p(true,seq_num);
	s_wifi_tx_.push(std::move( p )).commit();
	return 0;
}

uint8_t S_Wifi::appRemoteCtl(wifi::RemoteControlRxMsg::Cmd data)
{
	if(!is_wifi_connected_ && !is_cloud_connected_)
		return 1;
	switch(data)
	{
		case wifi::RemoteControlRxMsg::Cmd::FORWARD:
			remote.set(REMOTE_FORWARD);//forward control
			break;
		case wifi::RemoteControlRxMsg::Cmd::BACKWARD://backward control
			beeper.beepForCommand(false);
			remote.reset(); //tmp set stop
			break;
		case wifi::RemoteControlRxMsg::Cmd::LEFT:
			remote.set(REMOTE_LEFT);//left control
			break;
		case wifi::RemoteControlRxMsg::Cmd::RIGHT:
			remote.set(REMOTE_RIGHT);//right control
			break;
		case wifi::RemoteControlRxMsg::Cmd::STOP:
			remote.set(remote.get());
			remote.reset();//stop control
			break;
	}
	return 0;
}

uint8_t S_Wifi::syncClock(int year,int mon,int day,int hour,int minu,int sec)
{
	ROS_INFO("%s,%d     %d,%d,%d,%d,%d,%d",__FUNCTION__,__LINE__,year,mon,day,hour,minu,sec);
	struct tm timeinfo;
	timeinfo.tm_year = year-1900;
	timeinfo.tm_mon = mon-1;
	timeinfo.tm_mday = day;
	timeinfo.tm_hour = hour;
	timeinfo.tm_min = minu;
	timeinfo.tm_sec = sec;
	mktime(&timeinfo);
	struct tm *local_time;
	time_t ltime;
	time(&ltime);
	local_time = localtime(&ltime);
	ROS_INFO("%s,%d,local time %s",__FUNCTION__,__LINE__,asctime(local_time));
	return 0;
}

uint8_t S_Wifi::rebind()
{
	INFO_BLUE("wifi rebind");
	wifi::ForceUnbindTxMsg p(0x00);//no responed
	s_wifi_tx_.push(std::move(p)).commit();
	is_wifi_connected_ = false;
	is_cloud_connected_ = false;
	//speaker.play(VOICE_WIFI_UNBIND,false);
	return 0;
}

uint8_t S_Wifi::smartLink()
{
	INFO_BLUE("SMART LINK");
	wifi::SmartLinkTxMsg p(0x00);//no responed
	s_wifi_tx_.push( std::move(p)).commit();
	speaker.play(VOICE_WIFI_SMART_LINK,false);
	wifi_led.setMode(LED_FLASH,WifiLed::state::on);
	return 0;
}

uint8_t S_Wifi::smartApLink()
{
	INFO_BLUE("AP LINK");
	wifi::SmartApLinkTxMsg p(CLOUD_AP,0x00);
	s_wifi_tx_.push(std::move(p)).commit();
	speaker.play(VOICE_WIFI_CONNECTING,false);
	wifi_led.setMode(LED_FLASH,WifiLed::state::on);
	return 0;
}

uint8_t S_Wifi::uploadLastCleanData()
{
	INFO_BLUE("UPLOAD LAST STATE");
	time_t timer;
	time(&timer);
	std::vector<uint8_t> data;
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	data.push_back(0x00);
	wifi::Packet p(-1,0x01,0,0xc9,data);
	s_wifi_tx_.push(std::move(p)).commit();
	return 0;
}

bool S_Wifi::factoryTest()
{
	this->resume();
	usleep(20000);
	int waitResp = 0;
	isRegDevice_ = false;
	wifi::FactoryTestTxMsg p(0x01);
	s_wifi_tx_.push(std::move(p)).commit();
	while(!inFactoryTest_ ){
		usleep(20000);
		waitResp++;
		if(waitResp >= 100)//wait 2 second
		{
			ROS_INFO("%s,%d,FACTORY TEST FAIL!!",__FUNCTION__,__LINE__);
			return false;
		}
	}
	waitResp = 0;
	ROS_INFO("INTO FACTORY TEST!!");
	while(isRegDevice_ == false){
		usleep(20000);
		if(waitResp >= 10000){
			ROS_INFO("%s,%d,FACTORY TEST FAIL!!",__FUNCTION__,__LINE__);
			return false;
		}
		waitResp++;
	}
	ROS_INFO("FACTORY TEST SUCCESS!!");
	return true;
}

uint8_t S_Wifi::reboot()
{
	ROS_INFO("SERIAL WIFI REBOOT!!");
	wifi::Packet p(
			-1,
			0x00,
			0x00,
			0x0B,
			{0xb7,0x7b}
			);
	s_wifi_tx_.push(std::move(p)).commit();
	return 0;
}

uint8_t S_Wifi::resume()
{
	ROS_INFO("SERIAL WIFI RESUME!!");
	wifi::ResumeTxMsg p(0x00);
	s_wifi_tx_.push(std::move(p)).commit();
	return 0;
}

uint8_t S_Wifi::sleep()
{
	ROS_INFO("SERIAL WIFI SLEEP!!");
	wifi::SuspendTxMsg p(0x00);
	s_wifi_tx_.push(std::move(p)).commit();
	is_wifi_active_ = false;
	return 0;
}

bool S_Wifi::setWorkMode(int mode)
{
	switch (mode)
	{
		case Mode::md_idle:
			robot_work_mode_ = wifi::WorkMode::IDLE;
			break;

		case Mode::md_charge:
			robot_work_mode_ = wifi::WorkMode::CHARGE;
			break;

		case Mode::md_sleep:
			robot_work_mode_ = wifi::WorkMode::SLEEP;
			break;

		case Mode::md_remote:
			robot_work_mode_ = wifi::WorkMode::REMOTE;
			break;

		case Mode::cm_navigation:
			robot_work_mode_ = wifi::WorkMode::PLAN1;
			break;

		case Mode::cm_wall_follow:
			robot_work_mode_ = wifi::WorkMode::WALL_FOLLOW;
			break;

		case Mode::cm_spot:
			robot_work_mode_ = wifi::WorkMode::SPOT;
			break;

		case Mode::cm_exploration:
			robot_work_mode_ = wifi::WorkMode::FIND;
			break;

		default:
			robot_work_mode_ = wifi::WorkMode::SHUTDOWN;
			break;
	}
	ROS_INFO("\033[1;35m %s,%d,set work mode %d\033[0m",__FUNCTION__,__LINE__,(int)robot_work_mode_);
	return true;
}


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
#include "remote_mode.h"
#include <ros/ros.h>

extern volatile uint32_t Left_Wheel_Step,Right_Wheel_Step;


void Remote_Mode(void)
{
	uint32_t Moving_Speed=0;
	uint16_t No_Command_Counter=0;
	uint8_t Forward_Flag=0;
	uint8_t Dec_Counter=0;
	uint32_t OBS_Stop=0;
	uint8_t mccount = 0;

  //Display_Clean_Status(Display_Remote);

	if (!Is_Gyro_On()){
		Set_LED(100,0);
//		Beep(3,25,25,2);
		Set_Gyro_On();
//		Set_Gyro_Status();
		//printf("Gyro_Status%d\n", Is_Gyro_On());
	}

	Reset_Wheel_Step();
	Reset_Touch();
	Set_BLDC_Speed(40);
	Set_Vac_Speed();
	while(ros::ok())
	{
		usleep(20000);

		if(Remote_Key(Remote_Forward))
		{
			Forward_Flag=1-Forward_Flag;
			Reset_Rcon_Remote();
			No_Command_Counter=0;
		}

		if(Forward_Flag)
		{
		
			if(Get_OBS_Status())
			{
				Dec_Counter++;
				if(Moving_Speed>10)Moving_Speed--;
				Move_Forward(Moving_Speed,Moving_Speed);
				OBS_Stop++;
				if(OBS_Stop>15)Forward_Flag=0;
			}
			else
			{
				Moving_Speed=(Get_RightWheel_Step()/80)+25;
				if(Moving_Speed<25)Moving_Speed=25;
				if(Moving_Speed>42)Moving_Speed=42;
				Move_Forward(Moving_Speed,Moving_Speed);
				Work_Motor_Configure();
				OBS_Stop=0;
			}
			No_Command_Counter=0;
		}
		else
		{
			Stop_Brifly();
			Work_Motor_Configure();
		}



		if(Remote_Key(Remote_Left))
		{

			Deceleration();
			Work_Motor_Configure();
			Reset_Rcon_Remote();
			Turn_Left(Turn_Speed,320);
			Set_SideBrush_PWM(30,30);
			Set_MainBrush_PWM(30);
			Set_BLDC_Speed(30);
			No_Command_Counter=0;
			Reset_TempPWM();
			Reset_Wheel_Step();
			Forward_Flag=0;
		}
		if(Remote_Key(Remote_Right))
		{
	
			Deceleration();
			Work_Motor_Configure();
			Reset_Rcon_Remote();
			Turn_Right(Turn_Speed,320);
			Set_SideBrush_PWM(30,30);
			Set_MainBrush_PWM(30);
			Set_BLDC_Speed(30);
			No_Command_Counter=0;
			Reset_Wheel_Step();
			Forward_Flag=0;
		}
		if(Remote_Key(Remote_Max))
		{

//			Switch_VacMode();
			Deceleration();
			Reset_Rcon_Remote();
			Work_Motor_Configure();
			Turn_Right(Turn_Speed,1800);
			Set_SideBrush_PWM(30,30);
			Set_MainBrush_PWM(30);
			Set_BLDC_Speed(30);
			Reset_TempPWM();
			No_Command_Counter=0;
			Forward_Flag=0;
			Reset_Rcon_Remote();
			Reset_Wheel_Step();
		}

		No_Command_Counter++;
		if(No_Command_Counter>100)
		{
			No_Command_Counter=0;
			Set_Clean_Mode(Clean_Mode_Userinterface);
			break;
		}

		if(Remote_Key(Remote_Spot))
		{
			Disable_Motors();
			Set_Clean_Mode(Clean_Mode_Spot);
//			Initialize_Motor();
			Set_MoveWithRemote();
			Reset_Rcon_Remote();
			return;
		}

		if(Remote_Key(Remote_Clean))
		{
			Set_Clean_Mode(Clean_Mode_Userinterface);
			Reset_Rcon_Remote();
			return;
		}

		if(Remote_Key(Remote_Wall_Follow))
		{
			Disable_Motors();
			Set_Clean_Mode(Clean_Mode_WallFollow);
			Reset_Rcon_Remote();
			return;
		}
		/*
		if(Remote_Key(Remote_Random))
		{
			Disable_Motors();
			Set_Clean_Mode(Clean_Mode_RandomMode);
//			Initialize_Motor();
//			Set_MoveWithRemote();
			Reset_Rcon_Remote();
			return;
		}
		*/
		if(Remote_Key(Remote_Home))
		{
			Set_Clean_Mode(Clean_Mode_GoHome);
			SetHomeRemote();
			Reset_Rcon_Remote();
			return;
		}

	  /*------------------------------------------------------Touch and Remote event-----------------------*/
		if(Touch_Detect())
		{
			Beep(5, 20, 0, 1);
			// Key release detection, if user has not release the key, don't do anything.
			while (Get_Key_Press() & KEY_CLEAN)
			{
				ROS_INFO("%s %d: User hasn't release key or still cliff detected.", __FUNCTION__, __LINE__);
				usleep(20000);
			}
			// Key relaesed, then the touch status should be cleared.
		  Reset_Touch();
		  Set_Clean_Mode(Clean_Mode_Userinterface);
			break;
		}

		/*------------------------------------------------------Check Battery-----------------------*/
		if(Check_Battery()==0)
		{
			Set_Clean_Mode(Clean_Mode_Userinterface);
			break;
		}
		/*------------------------------------------------------Bumper Event-----------------------*/
		if(Get_Bumper_Status()||Get_Cliff_Trig())
		{
			Move_Back();
			Set_Clean_Mode(Clean_Mode_Userinterface);
			break;
		}
		/*------------------------------------------------check motor over current event ---------*/
		uint8_t octype =0;
		octype = Check_Motor_Current();
		if(octype){
			mccount=mccount+1;
			if(mccount >= 10){
				Self_Check(octype);
				Set_Clean_Mode(Clean_Mode_Userinterface);
				mccount = 0;
				break;
			}
		}
	}
	Disable_Motors();
}

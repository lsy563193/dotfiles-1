//
// Created by lsy563193 on 6/28/17.
//

#ifndef PP_REGULATOR_BASE_H
#define PP_REGULATOR_BASE_H

#include <move_type.h>

class RegulatorBase {
public:

	bool isExit();

	bool isPause(){
		return RegulatorBase::isStop() || isStop();
	};

	virtual bool isSwitch() = 0;
	virtual void adjustSpeed(int32_t&, int32_t&)=0;
	virtual bool isStop()=0;
	virtual bool isReach() = 0;

public:
	static Point32_t s_target;
	static Point32_t s_origin;
};

class BackRegulator: public RegulatorBase{
public:
	BackRegulator();
	void adjustSpeed(int32_t&, int32_t&);
	bool isSwitch();
	bool isStop();
	void setOrigin(){
		auto pos_x = robot::instance()->getOdomPositionX();
		auto pos_y = robot::instance()->getOdomPositionY();
		pos_x_ = pos_x;
		pos_y_ = pos_y;
	}

protected:
	bool isReach();

private:
	float pos_x_,pos_y_;
	int counter_;
	int32_t speed_;
};

class TurnRegulator: public RegulatorBase{
public:

	TurnRegulator();
	void adjustSpeed(int32_t&, int32_t&);
	bool isSwitch();
	bool isStop();
	void setTarget(){
		if (LASER_FOLLOW_WALL)
			laser_turn_angle();

		target_angle_ = calc_target(g_turn_angle);
	};

protected:
	bool isReach();

private:
	int16_t target_angle_;
	uint16_t accurate_;
	uint16_t speed_max_;
};

class TurnSpeedRegulator{
public:
	TurnSpeedRegulator(uint8_t speed_max,uint8_t speed_min, uint8_t speed_start):
					speed_max_(speed_max),speed_min_(speed_min), speed_(speed_start),tick_(0){};
	~TurnSpeedRegulator() {
		set_wheel_speed(0, 0);
	}
	bool adjustSpeed(int16_t diff, uint8_t& speed_up);

private:
	uint8_t speed_max_;
	uint8_t speed_min_;
	uint8_t speed_;
	uint32_t tick_;
};

class SelfCheckRegulator{
public:
	SelfCheckRegulator(){
		ROS_WARN("%s, %d: ", __FUNCTION__, __LINE__);
	};
	~SelfCheckRegulator(){
		set_wheel_speed(0, 0);
	};
	void adjustSpeed(uint8_t bumper_jam_state);
};

class FollowWallRegulator:public RegulatorBase{

public:
	FollowWallRegulator();
	~FollowWallRegulator(){ set_wheel_speed(0,0); };
	void adjustSpeed(int32_t &left_speed, int32_t &right_speed);
	bool isSwitch();
	bool isStop();
//	void setTarget(Point32_t target){s_target = target;};
//	void setOrigin(Point32_t origin){s_origin = origin;};
protected:
	bool isReach();

private:
	int32_t	 previous_;
	int jam_;
};

class LinearRegulator: public RegulatorBase{
public:
	LinearRegulator();
	~LinearRegulator(){ };
	bool isStop();
	bool isSwitch();
	void adjustSpeed(int32_t&, int32_t&);

protected:
	bool isReach();

private:
	int32_t speed_max_;
	int32_t integrated_;
	int32_t base_speed_;
	uint8_t integration_cycle_;
	uint32_t tick_;
	uint8_t turn_speed_;
};

class RegulatorProxy:public RegulatorBase{
public:
	RegulatorProxy(Point32_t origin, Point32_t target);
	~RegulatorProxy();
	void switchToNext();
	void adjustSpeed(int32_t &left_speed, int32_t &right_speed);
	bool isSwitch();
	bool isStop();
	bool isReach();
	RegulatorBase* getType(){ return p_reg_; };
private:
	RegulatorBase* p_reg_;
	RegulatorBase* mt_reg_;
	TurnRegulator* turn_reg_;
	BackRegulator* back_reg_;

};

#endif //PP_REGULATOR_BASE_H
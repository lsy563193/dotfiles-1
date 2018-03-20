//
// Created by austin on 17-11-24.
//

#ifndef PP_ODOM_H
#define PP_ODOM_H

#include "pose.h"

class Odom
{
public:
	Odom();
	~Odom();

	void setX(float x);
	float getX();
	void setY(float y);
	float getY();
//	void setZ(float z);
//	float getZ();
	void setOriginRadian(double radian);
	double getOriginRadian();
	double getRadian();
	void setRadianOffset(double radian);
	void setXOffset(double radian);
	void setYOffset(double radian);
	double getRadianOffset();

	void setMovingSpeed(float speed);
	float getMovingSpeed(void);
	void setAngleSpeed(float speed);
	float getAngleSpeed(void);

private:

//	Pose pose;

	double rad_origin_;
	double radian_offset_;

	float x_;
	float y_;
	double x_offset_{};
	double y_offset_{};

	// In meters.
	float moving_speed_;
	// In degrees.
	float angle_speed_;
};

extern Odom odom;
#endif //PP_ODOM_H

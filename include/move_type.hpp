//
// Created by root on 12/5/17.
//

#ifndef PP_MOVE_TYPE_HPP
#define PP_MOVE_TYPE_HPP
#define TRAP_IN_SMALL_AREA_COUNT 20

#include "action.hpp"
#include "movement.hpp"
#include "boost/shared_ptr.hpp"
#include "rcon.h"
//#include "mode.hpp"

class Mode;
class ACleanMode;
class IMoveType:public IAction
{
public:
	IMoveType();
	bool shouldMoveBack();
	bool shouldTurn();
	bool RconTrigger();
//	~IMoveType() = default;

	bool isBlockCleared(GridMap &map, Points &passed_path);
	Point_t last_{};
//	bool closed_count_{};
	void setMode(Mode* cm)
	{sp_mode_ = cm;}
	Mode* getMode()
	{return sp_mode_;}

	virtual bool isFinish();
//	void updatePath();
	void run() override;

	int8_t rcon_cnt[Rcon::enum_end + 1]{};
	uint32_t countRconTriggered(uint32_t rcon_value, int max_cnt);
	bool isRconStop();
	bool isOBSStop();
	bool isLidarStop();

	static boost::shared_ptr<IMovement> sp_movement_;
	static Mode *sp_mode_;
	static int movement_i_;
	void resetTriggeredValue();
	Point_t start_point_;
	bool state_turn{};
//	Point_t target_point_;
	int dir_;
	Points remain_path_{};
protected:
	double turn_target_radian_{};

	float back_distance_;
	enum{//movement
		mm_null,
		mm_back,
		mm_turn,
		mm_stay,
		mm_rcon,
		mm_forward,
		mm_straight,
		mm_dynamic,
	};

};

class MoveTypeLinear:public IMoveType
{
public:
	MoveTypeLinear(Points remain_path);
	~MoveTypeLinear() override;
	bool isFinish() override;
//	IAction* setNextAction();

	bool isPassTargetStop(Dir_t &dir);
	bool isCellReach();
	bool isPoseReach();

	bool isLinearForward();

private:
	bool handleMoveBackEvent(ACleanMode* p_clean_mode);
	void switchLinearTarget(ACleanMode * p_clean_mode);
};

class MoveTypeFollowWall:public IMoveType
{
public:
	MoveTypeFollowWall() = delete;
	~MoveTypeFollowWall() override;

	MoveTypeFollowWall(Points remain_path, bool is_left);

	bool isFinish() override;

	bool isNewLineReach(GridMap &map);
	bool isOverOriginLine(GridMap &map);

private:
	bool handleMoveBackEvent(ACleanMode* p_clean_mode);
	bool handleMoveBackEventRealTime(ACleanMode* p_clean_mode);
	bool is_left_{};
	int16_t bumperTurnAngle();
	int16_t cliffTurnAngle();
	int16_t tiltTurnAngle();
	int16_t obsTurnAngle();
	bool _lidarTurnRadian(bool is_left, double &turn_radian, double lidar_min, double lidar_max, double radian_min,
						  double radian_max, bool is_moving,
						  double dis_limit = 0.3);
	bool lidarTurnRadian(double &turn_radian);
	double getTurnRadianByEvent();
	double getTurnRadian(bool);
	double robot_to_wall_distance = 0.8;
	float g_back_distance = 0.01;
	struct lidar_angle_param{
		double lidar_min;
		double lidar_max;
		double radian_min;
		double radian_max;
	};
};

class MoveTypeGoToCharger:public IMoveType
{
public:
	MoveTypeGoToCharger();

	bool isFinish() override ;

	void run() override ;
protected:

	boost::shared_ptr<MovementGoToCharger> p_gtc_movement_;
	boost::shared_ptr<IMovement> p_turn_movement_;
	boost::shared_ptr<IMovement> p_back_movement_;
};

class MoveTypeBumperHitTest: public IMoveType
{
public:
	MoveTypeBumperHitTest();
	~MoveTypeBumperHitTest() = default;

	bool isFinish() override;

	void run() override ;

private:
	bool turn_left_{true};
	int16_t turn_target_angle_{0};
	double turn_time_stamp_;
	boost::shared_ptr<IMovement> p_direct_go_movement_;
	boost::shared_ptr<IMovement> p_turn_movement_;
	boost::shared_ptr<IMovement> p_back_movement_;
};

class MoveTypeRemote: public IMoveType
{
public:
	MoveTypeRemote();
	~MoveTypeRemote() override;

	bool isFinish() override;

	void run() override ;

private:
	boost::shared_ptr<IMovement> p_movement_;
};

class MoveTypeDeskTest: public IMoveType
{
public:
	MoveTypeDeskTest();
	~MoveTypeDeskTest() override;

	void deskTestRoutineThread();
	bool isFinish() override;

	void run() override;

	bool dataExtract(const uint8_t *buf);


private:
	boost::shared_ptr<IAction> p_movement_;

	uint16_t error_code_{0};

	int current_work_mode_{0};

	/*
	 * Test stage: 1 ~ 7.
	 * Stage 1: Check for base value for currents.
	 * Stage 2: Check lidar, then turn 180 degrees, check lidar again, turn 180 degrees, and get the baselines
	 *          of OBS and cliff and check the rcon receivers.
	 * Stage 3: Check for bumpers and OBS value.
	 * Stage 4: Follow wall to cliff position and check for cliff value.
	 * Stage 5: Move for a distance and check the max currents.
	 * Stage 6: Turn for 360 degrees to check for rcon.
	 */
	int test_stage{0};

	// Each stage has several steps.
	int test_step_{0};

	// For stage 1.
	int sum_cnt_{0};
	int32_t left_brush_current_baseline_sum_{};
	int32_t right_brush_current_baseline_sum_{};
	int32_t main_brush_current_baseline_sum_{};
	int32_t left_wheel_current_baseline_sum_{};
	int32_t right_wheel_current_baseline_sum_{};
	int32_t vacuum_current_baseline_sum_{};
	int32_t water_tank_current_baseline_sum_{};

	int16_t left_brush_current_baseline_;
	int16_t right_brush_current_baseline_;
	int16_t main_brush_current_baseline_;
	int16_t left_wheel_current_baseline_;
	int16_t right_wheel_current_baseline_;
	int16_t vacuum_current_baseline_;
	int16_t water_tank_current_baseline_;

	bool check_stage_1_finish();

	// For stage 2.
	uint8_t lidar_check_cnt_{0};
	double lidar_check_seq_{0};

	long left_obs_sum_{0};
	long front_obs_sum_{0};
	long right_obs_sum_{0};

	int16_t left_obs_baseline_{0};
	int16_t front_obs_baseline_{0};
	int16_t right_obs_baseline_{0};

	bool check_stage_2_finish();

	// For stage 3.
	int16_t obs_ref_{2000}; //todo
	int16_t left_obs_max_{0};
	int16_t front_obs_max_{0};
	int16_t right_obs_max_{0};
	bool check_stage_3_finish();

	// For stage 4.
	int16_t cliff_min_ref_{80}; //todo
	int16_t cliff_max_ref_{200}; //todo

	int16_t left_cliff_max_{0};
	int16_t front_cliff_max_{0};
	int16_t right_cliff_max_{0};

	long left_cliff_sum_{0};
	long front_cliff_sum_{0};
	long right_cliff_sum_{0};

	int16_t left_cliff_baseline_{0};
	int16_t front_cliff_baseline_{0};
	int16_t right_cliff_baseline_{0};

	bool check_stage_4_finish();

	// For stage 5.

	uint16_t left_brush_current_max_{0};
	uint16_t right_brush_current_max_{0};
	uint16_t main_brush_current_max_{0};
	uint16_t left_wheel_forward_current_max_{0};
	uint16_t right_wheel_forward_current_max_{0};
	uint16_t left_wheel_backward_current_max_{0};
	uint16_t right_wheel_backward_current_max_{0};
	uint16_t vacuum_current_max_{0};
	uint16_t water_tank_current_max_{0};

	uint16_t side_brush_current_ref_{0}; // todo:
	uint16_t main_brush_current_ref_{0};
	uint16_t wheel_current_ref_{0};
	uint16_t vacuum_current_ref_{0};
	uint16_t water_tank_current_ref_{0};

	bool check_stage_5_finish();

	// For stage 6.
	bool check_stage_6_finish();

	// For stage 7.
	bool check_stage_7_finish();
};
#endif //PP_MOVE_TYPE_HPP

/*
 ******************************************************************************
 * @file	AI Cleaning Robot
 * @author	ILife Team Alvin Xie
 * @version V1.0
 * @date	Feb-2017
 * @brief	The logic and algorithm wall follow when the robot using laser slam
 ******************************************************************************
 * <h2><center>&copy; COPYRIGHT 2011 ILife CO.LTD</center></h2>
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "robot.hpp"
#include "movement.h"
#include "core_move.h"
#include "gyro.h"
#include "map.h"
#include "mathematics.h"
#include "path_planning.h"
#include "wall_follow_slam.h"
#include "wall_follow_trapped.h"
#include <ros/ros.h>
#include "debug.h"
#include "rounding.h"
#include <vector>
#include <shortest_path.h>
#include "charger.hpp"
#include "wav.h"
#include "robotbase.h"

#include "motion_manage.h"

#include "regulator.h"
//Turn speed
#ifdef Turn_Speed
#undef Turn_Speed
#define Turn_Speed  18
#endif

std::vector<Pose16_t> g_wf_cell;
int g_isolate_count = 0;
int g_trapped_count = 0;
const int TRAPPED_COUNT_LIMIT = 2;
bool g_isolate_triggered = false;
// This list is for storing the position that robot sees the charger stub.
extern bool g_from_station;
extern int16_t g_x_min, g_x_max, g_y_min, g_y_max;

extern uint8_t g_trapped_cell_size;
extern Cell_t g_cell_history[5];
extern uint8_t g_wheel_left_direction;
extern uint8_t g_wheel_right_direction;
bool	wf_time_out = false;
bool	g_is_left_start;

//Timer
uint32_t g_wall_follow_timer;
int32_t g_reach_count = 0;
const static int32_t REACH_COUNT_LIMIT = 10;//10 represent the wall follow will end after overlap 10 cells
const int16_t DIFF_LIMIT = 200;//1500 means 150 degrees, it is used by angle check.
int32_t g_same_cell_count = 0;
//MFW setting
static const MapWallFollowSetting MFW_SETTING[6] = {{1200, 250, 150},
																										{1200, 250, 150},
																										{1200, 250, 150},
																										{1200, 250, 70},
																										{1200, 250, 150},
																										{1200, 250, 150},};
//------------static
static bool wf_is_reach_new_cell(Pose16_t &pose)
{
	if (g_wf_cell.empty() || g_wf_cell.back() != pose)
	{
		g_same_cell_count = 0;
		g_wf_cell.push_back(pose);
		return true;
	} else if (! g_wf_cell.empty())
		g_same_cell_count++;//for checking if the robot is traped

	return false;
}

static bool is_reach(void)
{
	/*check if spot turn*/
	if  (get_sp_turn_count() > 400) {
		reset_sp_turn_count();
		ROS_WARN("%s,%d:sp_turn over 400",__FUNCTION__,__LINE__);
		return true;
	}
	extern int g_trapped_mode;
	if (g_trapped_mode != 1) {
		if (wf_is_reach_start()) {
			ROS_WARN("%s,%d:reach the start point!",__FUNCTION__,__LINE__);
			return true;
		}
	}
#if 0
	if ( g_reach_count < REACH_COUNT_LIMIT)
		return false;
#endif
	//ROS_INFO("reach_count>10(%d)",g_reach_count);
	g_reach_count = 0;
	int16_t th_diff;
	int8_t pass_count = 0;
	int8_t sum = REACH_COUNT_LIMIT;
	bool fail_flag = 0;

	std::vector<Cell_t> reach_point;
	reach_point.clear();
	Cell_t new_point;
/*old wall follow ending condition, overlap 10 cells*/
#if 0
	try
	{
		for (int i = 1; i <= REACH_COUNT_LIMIT; i++)
		{
			Pose16_t cell = g_wf_cell[g_wf_cell.size() - i];
			fail_flag = 0;
			for (std::vector<Pose16_t>::reverse_iterator r_iter = (g_wf_cell.rbegin() + i); r_iter != g_wf_cell.rend(); ++r_iter)
			{
				if (*r_iter == cell)
				{
					bool is_found = false;
					new_point.X = r_iter->X;
					new_point.Y = r_iter->Y;
					for (std::vector<Cell_t>::iterator c_it = reach_point.begin(); is_found == false && c_it != reach_point.end(); ++c_it) {
						if (new_point.X == c_it->X && new_point.Y == c_it-> Y) {
							is_found = true;
						}
					}
					if (is_found == false) {
						ROS_INFO("p_new_point.X = %d, p_new_point.Y = %d",new_point.X, new_point.Y);
						reach_point.push_back(new_point);
					}
					th_diff = (abs(r_iter->TH - cell.TH));
					ROS_INFO("r_iter->X = %d, r_iter->Y = %d, r_iter->TH = %d", r_iter->X, r_iter->Y, r_iter->TH);
					if (th_diff > 1800)
						th_diff = 3600 - th_diff;

					if (th_diff <= DIFF_LIMIT)
					{
						pass_count++;
						ROS_WARN("th_diff = %d <= %d, pass angle check!", th_diff, DIFF_LIMIT);
						break;
					} else{
						ROS_WARN("th_diff = %d > %d, fail angle check!", th_diff, DIFF_LIMIT);
						fail_flag = 1;
					}
				}
				/*in case of the g_wf_cell no second same point, the g_reach_count++ caused by cleanning the block obstacle which
				 cost is 2 or 3, it will be set 1(CLEANED), at this situation the sum should sum--, it will happen when the robot
				 get close to the left wall but there is no wall exist, then the robot can judge it is in the isolate island.*/
				if ((r_iter == (g_wf_cell.rend() - 1)) && (fail_flag == 0)){
					sum--;
					ROS_WARN("Can't find second same pose in WF_Point! sum--");
				}
			}
		}
		ROS_ERROR("reach_point.size() = %lu",reach_point.size());
		if (reach_point.size() < 4) {
			ROS_ERROR("reach_point.size() = %lu < 3",reach_point.size());
			return 0;
		}
		if (sum < REACH_COUNT_LIMIT) return 0;

		if (pass_count < REACH_COUNT_LIMIT) return 0; else return 1;

	}
	catch (const std::out_of_range &oor)
	{
		std::cerr << "Out of range error:" << oor.what() << '\n';
	}
#endif
	if (g_wf_cell.size() > REACH_COUNT_LIMIT) {
		try
		{
			int i = REACH_COUNT_LIMIT;
			Pose16_t cell = g_wf_cell[g_wf_cell.size() - 1];

			for (std::vector<Pose16_t>::reverse_iterator r_iter = (g_wf_cell.rbegin() + i); r_iter != g_wf_cell.rend(); ++r_iter)
			{
				if (r_iter->X == cell.X && r_iter->Y == cell.Y)
				{
					new_point.X = r_iter->X;
					new_point.Y = r_iter->Y;

					th_diff = (abs(r_iter->TH - cell.TH));
					ROS_INFO("r_iter->X = %d, r_iter->Y = %d, r_iter->TH = %d", r_iter->X, r_iter->Y, r_iter->TH);
					if (th_diff > 1800)
						th_diff = 3600 - th_diff;
					if (th_diff <= DIFF_LIMIT)
					{
						ROS_WARN("th_diff = %d <= %d, pass angle check!", th_diff, DIFF_LIMIT);
						return true;
					} else{
						ROS_WARN("th_diff = %d > %d, fail angle check!", th_diff, DIFF_LIMIT);
					}
				}
			}
		}
		catch (const std::out_of_range &oor)
		{
			std::cerr << "Out of range error:" << oor.what() << '\n';
		}
	}
	return 0;
}

static bool is_trap(void)
{
	if (g_same_cell_count >= 1000)
	{
		ROS_WARN("Maybe the robot is trapped! Checking!");
		g_same_cell_count = 0;
/*		if (is_isolate())
		{
			ROS_WARN("Not trapped!");
		} else
		{
			ROS_WARN("Trapped!");
			set_clean_mode(Clean_Mode_Userinterface);
			return 0;
		}*/
		return 0;
	}
	return false;
}

static bool is_time_out(void)
{
	if (get_work_time() > WALL_FOLLOW_TIME) {
		ROS_INFO("Wall Follow time longer than 60 minutes");
		ROS_INFO("time now : %d", get_work_time());
		wf_time_out = true;
		return true;
	} else {
		return false;
	}
}


static void wf_update_cleaned()
{

	int16_t x, y;
	auto heading = gyro_get_angle();
	if (x != map_get_x_cell() || y != map_get_y_cell())
	{
		for (auto c = 1; c >= -1; --c)
		{
			for (auto d = 1; d >= -1; --d)
			{
				auto i = map_get_relative_x(heading, CELL_SIZE * c, CELL_SIZE * d, true);
				auto j = map_get_relative_y(heading, CELL_SIZE * c, CELL_SIZE * d, true);
				auto e = map_get_cell(MAP, count_to_cell(i), count_to_cell(j));

				if (e == BLOCKED_OBS || e == BLOCKED_BUMPER || e == BLOCKED_BOUNDARY)
				{
					map_set_cell(MAP, i, j, CLEANED);
				}
			}
		}
	}
}

static bool wf_is_reach_cleaned(void)
{
	if (! g_wf_cell.empty())
		if (map_get_cell(MAP, g_wf_cell.back().X, g_wf_cell.back().Y) == CLEANED)
			return true;

	return false;
}

/*
static void wf_mark_home_point(void)
{
	//path_planning_initialize(&, &g_homes.front().Y);
	int32_t x, y;
	int i, j;
	std::list<Cell_t> WF_Home_Point = g_homes;

	while (!WF_Home_Point.empty())
	{
		x = WF_Home_Point.front().X;
		y = WF_Home_Point.front().Y;
		ROS_INFO("%s %d: WF_Home_Point.front().X = %d, WF_Home_Point.front().Y = %d, WF_Home_Point.size() = %d",
						 __FUNCTION__, __LINE__, x, y, (uint) WF_Home_Point.size());
		ROS_INFO("%s %d: g_x_min = %d, g_x_max = %d", __FUNCTION__, __LINE__, g_x_min, g_x_max);
		WF_Home_Point.pop_front();

		for (i = -2; i <= 2; i++)
		{
			for (j = -2; j <= 2; j++)
			{
				map_set_cell(MAP, x + i, y + j, CLEANED);//0, -1
				//ROS_INFO("%s %d: x + i = %d, y + j = %d", __FUNCTION__, __LINE__, x + i, y + j);
			}
		}
	}
}
*/
static bool is_isolate() {
	path_update_cell_history();
	int16_t	val = 0;
	uint16_t i = 0;
	int16_t x_min, x_max, y_min, y_max;
	Point32_t	Remote_Point;
	Remote_Point.X = 0;
	Remote_Point.Y = 0;
	path_get_range(MAP, &x_min, &x_max, &y_min, &y_max);
	Cell_t out_cell {int16_t(x_max + 1),int16_t(y_max + 1)};
//	pnt16ArTmp[0] = out_cell;

//	path_escape_set_trapped_cell(pnt16ArTmp, 1);
//	cm_update_map();
	cm_update_position();
	map_mark_robot(MAP);//note: To clear the obstacle when check isolated, please don't remove it!
#if DEBUG_MAP
	debug_map(MAP, 0, 0);
#endif

	Cell_t remote{0,0};
	if ( out_cell != remote){
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, out_cell.X, out_cell.Y, 0);
			val = (val < 0 || val == SCHAR_MAX) ? 0 : 1;
	} else {
		if (is_block_accessible(0, 0) == 1) {
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);
			if (val < 0 || val == SCHAR_MAX) {
				/* Robot start position is blocked. */
				val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);

				if (val < 0 || val == SCHAR_MAX) {
					val = 0;
				} else {
					val = 1;
				};
			} else {
				val = 1;
			}
		} else {
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);
			if (val < 0 || val == SCHAR_MAX)
				val = 0;
			else
				val = 1;
		}
	}
	return val != 0;
}

static bool trapped_is_isolate() {
	path_update_cell_history();
	int16_t	val = 0;
	uint16_t i = 0;
	int16_t x_min, x_max, y_min, y_max;
	Point32_t	Remote_Point;
	Remote_Point.X = 0;
	Remote_Point.Y = 0;
	path_get_range(WFMAP, &x_min, &x_max, &y_min, &y_max);
	Cell_t out_cell {int16_t(x_max + 1),int16_t(y_max + 1)};
//	pnt16ArTmp[0] = out_cell;

//	path_escape_set_trapped_cell(pnt16ArTmp, 1);
//	cm_update_map();
	cm_update_position();
	map_mark_robot(WFMAP);//note: To clear the obstacle when check isolated, please don't remove it!
#if DEBUG_WF_MAP
	debug_map(WFMAP, 0, 0);
#endif

	Cell_t remote{0,0};
	if ( out_cell != remote){
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, out_cell.X, out_cell.Y, 0);
			val = (val < 0 || val == SCHAR_MAX) ? 0 : 1;
	} else {
		if (is_block_accessible(0, 0) == 1) {
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);
			if (val < 0 || val == SCHAR_MAX) {
				/* Robot start position is blocked. */
				val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);

				if (val < 0 || val == SCHAR_MAX) {
					val = 0;
				} else {
					val = 1;
				};
			} else {
				val = 1;
			}
		} else {
			val = wf_path_find_shortest_path(g_cell_history[0].X, g_cell_history[0].Y, 0, 0, 0);
			if (val < 0 || val == SCHAR_MAX)
				val = 0;
			else
				val = 1;
		}
	}
	return val != 0;
}

/*------------------------------------------------------------------ Wall Follow Mode--------------------------*/
uint8_t wf_break_wall_follow(void)
{
//	ROS_INFO("%s %d: /*****************************************Release Memory************************************/", __FUNCTION__, __LINE__);
	g_wf_cell.clear();
	map_reset(WFMAP);
	std::vector<Pose16_t>(g_wf_cell).swap(g_wf_cell);
	return 0;
}

uint8_t wf_clear(void)
{
	ROS_INFO("%s %d: /*****************************************Release Memory************************************/", __FUNCTION__, __LINE__);
	g_wf_cell.clear();
	std::vector<Pose16_t>(g_wf_cell).swap(g_wf_cell);
	g_isolate_count = 0;
	g_trapped_count = 0;
	g_isolate_triggered = false;
	wf_time_out = false;
	return 0;
}

void wf_update_map(uint8_t id)
{
	auto cell = cm_update_position();

	Pose16_t curr_cell{cell.X, cell.Y, (int16_t) gyro_get_angle()};
	if (wf_is_reach_new_cell(curr_cell))
	{
		g_reach_count = wf_is_reach_cleaned() ? g_reach_count + 1 : 0;
		ROS_INFO("reach_count(%d)",g_reach_count);
		int size = (g_wf_cell.size() - 2);
		if (size >= 0)
			map_set_cell(id, cell_to_count(g_wf_cell[size].X), cell_to_count(g_wf_cell[size].Y), CLEANED);

		std::list<Cell_t> empty_path;
		empty_path.clear();
		MotionManage::pubCleanMapMarkers(WFMAP, g_next_cell, g_target_cell, empty_path);
	}

	int32_t x,y;
	cm_world_to_point(gyro_get_angle(), CELL_SIZE_2, 0, &x, &y);
	if (map_get_cell(id, count_to_cell(x), count_to_cell(y)) != BLOCKED_BOUNDARY)
		map_set_cell(id, x, y, BLOCKED_OBS);
}

bool wf_is_reach_isolate()
{
	if(is_reach()){
		ROS_INFO("is_reach()");
		//g_isolate_count = is_isolate() ? g_isolate_count+1 : 4;
		//if (is_isolate()) {
		if (trapped_is_isolate()) {
			g_isolate_count++;
			g_isolate_triggered = true;
			//map_reset(MAP);
			wf_break_wall_follow();
			ROS_WARN("is_isolate");
		} else {
			g_isolate_count = 4;
			ROS_WARN("is not isolate");
		}
		ROS_INFO("isolate_count(%d)", g_isolate_count);
		return true;
	}
	return false;
}

bool wf_is_end()
{
	if (wf_is_reach_isolate() || is_time_out() || is_trap())
		return true;
	return false;
}

bool trapped_is_end()
{
	/*
	auto cell = cm_update_position();
	Pose16_t curr_cell{cell.X, cell.Y, (int16_t) gyro_get_angle()};
	wf_is_reach_new_cell(curr_cell);
	ROS_INFO("cell.X = %d, cell.Y = %d", cell.X, cell.Y);

	if (is_reach())
		return true;
	return false;
	*/
	if (wf_is_reach_isolate() || is_trap()) {
		g_trapped_count++;
		wf_break_wall_follow();
		ROS_WARN("g_trapped_count = %d", g_trapped_count);
		if (g_trapped_count >= TRAPPED_COUNT_LIMIT) {
			ROS_WARN("g_trapped_count >= TRAPPED_COUNT_LIMIT");
			ROS_WARN("g_trapped_count = %d", g_trapped_count);
			g_trapped_count = 0;
			return true;
		}
	}
	return false;
}

bool wf_is_go_home()
{
	if (g_isolate_count > 3 || wf_time_out) {
		return true;
	} else {
		return false;
	}
};

bool wf_is_first()
{
	return g_isolate_count == 0;
}

bool wf_is_reach_start()
{
	int32_t x_s = count_to_cell(RegulatorBase::s_origin.X);
	int32_t y_s = count_to_cell(RegulatorBase::s_origin.Y);
	int16_t origin_angle = RegulatorBase::s_origin_angle;
	uint32_t dis = two_points_distance(x_s, y_s, g_wf_cell.back().X, g_wf_cell.back().Y);
	int16_t th_diff;
	//ROS_WARN("%s,%d:dis = %d",__FUNCTION__,__LINE__,dis);
	if (g_is_left_start) {
		if (dis <= 1) {
					th_diff = (abs(g_wf_cell.back().TH - origin_angle));
					if (th_diff > 1800)
						th_diff = 3600 - th_diff;
					if (th_diff <= DIFF_LIMIT)
					{
						ROS_WARN("th_diff = %d <= %d, pass angle check!", th_diff, DIFF_LIMIT);
					} else{
						ROS_WARN("th_diff = %d > %d, fail angle check!", th_diff, DIFF_LIMIT);
						return false;
					}
			return true;
		}
		else {
			return false;
		}
	} else {
		if (dis > 3)
			g_is_left_start = true;
	}

}

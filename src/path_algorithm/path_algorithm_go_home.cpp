//
// Created by lsy563193 on 12/13/17.
//

#include <map.h>
#include <mode.hpp>
#include "robot.hpp"
#include "ros/ros.h"
#include "path_algorithm.h"

bool GoHomePathAlgorithm::generatePath(GridMap &map, const Point_t &curr, const Dir_t &last_dir, Points &plan_path)
{
	bool generate_finish = false;
	plan_path.clear();
	while (!generate_finish && ros::ok())
	{
		switch (home_way_index_)
		{
			case SLAM_MAP_CLEAR_BLOCKS:
				generate_finish = generatePathWithSlamMapClearBlocks(map, curr, last_dir, plan_path);
				break;
			case THROUGH_SLAM_MAP_REACHABLE_AREA:
				generate_finish = generatePathThroughSlamMapReachableArea(map, curr, last_dir, plan_path);
				break;
			case THROUGH_UNKNOWN_AREA:
				generate_finish = generatePathThroughUnknownArea(map, curr, last_dir, plan_path);
				break;
			default: //case THROUGH_CLEANED_AREA:
				generate_finish = generatePathThroughCleanedArea(map, curr, last_dir, plan_path);
				break;
		}
	}

	return !plan_path.empty();

}

bool GoHomePathAlgorithm::generatePathThroughCleanedArea(GridMap &map, const Point_t &curr, const Dir_t &last_dir,
														 Points &plan_path)
{
	Cells plan_path_cells{};
	auto &home_point_index = home_point_index_[home_way_index_];

	if (!home_points_.empty())
	{
		if (home_point_index >= home_points_.size())
		{
			home_way_index_ = SLAM_MAP_CLEAR_BLOCKS;
			ROS_INFO("%s %d: Clear c_blocks with slam map.", __FUNCTION__, __LINE__);
			slam_grid_map.print(curr.toCell(), CLEAN_MAP, Cells{{0,0}});
			map.merge(slam_grid_map, false, false, false, false, false, true);
			map.print(curr.toCell(),CLEAN_MAP, Cells{curr.toCell()});
			return false;
		}

		current_home_point_ = home_points_[home_point_index];

		if (map.isBlockAccessible(current_home_point_.toCell().x, current_home_point_.toCell().y))
		{
			Cell_t min_corner, max_corner;
			plan_path_cells = findShortestPath(map, curr.toCell(), current_home_point_.toCell(),
											   last_dir, false, false, min_corner, max_corner);
		}

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);
			map.print(curr.toCell(),CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, this home point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);

			home_point_index++;
			return false;
		}
	}
	else // For going back to start point.
	{
		Cell_t min_corner, max_corner;
		plan_path_cells = findShortestPath(map, curr.toCell(), start_point_.toCell(),
										   last_dir, false, false, min_corner, max_corner);

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, start point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			home_way_index_ = SLAM_MAP_CLEAR_BLOCKS;
			ROS_INFO("%s %d: Clear c_blocks with slam map.", __FUNCTION__, __LINE__);
			slam_grid_map.print(curr.toCell(), CLEAN_MAP, Cells{{0, 0}});
			map.merge(slam_grid_map, false, false, false, false, false, true);
			map.print(curr.toCell(), CLEAN_MAP, Cells{curr.toCell()});
			return false;
		}
	}
}

bool GoHomePathAlgorithm::generatePathWithSlamMapClearBlocks(GridMap &map, const Point_t &curr, const Dir_t &last_dir,
														Points &plan_path)
{
	Cells plan_path_cells{};
	auto &home_point_index = home_point_index_[home_way_index_];

	if (!home_points_.empty())
	{
		if (home_point_index >= home_points_.size())
		{
			ROS_INFO("%s %d: Use slam map reachable area.", __FUNCTION__, __LINE__);
			home_way_index_ = THROUGH_SLAM_MAP_REACHABLE_AREA;
			return false;
		}

		current_home_point_ = home_points_[home_point_index];

		if (map.isBlockAccessible(current_home_point_.toCell().x, current_home_point_.toCell().y))
		{
			Cell_t min_corner, max_corner;
			plan_path_cells = findShortestPath(map, curr.toCell(), current_home_point_.toCell(),
											   last_dir, false, false, min_corner, max_corner);
		}

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, this home point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);

			home_point_index++;
			return false;
		}
	}
	else // For going back to start point.
	{
		Cell_t min_corner, max_corner;
		plan_path_cells = findShortestPath(map, curr.toCell(), start_point_.toCell(),
										   last_dir, false, false, min_corner, max_corner);

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, start point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			ROS_INFO("%s %d: Use slam map reachable area.", __FUNCTION__, __LINE__);
			home_way_index_ = THROUGH_SLAM_MAP_REACHABLE_AREA;
			return false;
		}
	}
}

bool GoHomePathAlgorithm::generatePathThroughSlamMapReachableArea(GridMap &map, const Point_t &curr,
																  const Dir_t &last_dir, Points &plan_path)
{
	Cells plan_path_cells{};
	auto &home_point_index = home_point_index_[home_way_index_];

	if (!home_points_.empty())
	{
		if (home_point_index >= home_points_.size())
		{
			ROS_INFO("%s %d: Use unknown area.", __FUNCTION__, __LINE__);
			home_way_index_ = THROUGH_UNKNOWN_AREA;
			return false;
		}

		current_home_point_ = home_points_[home_point_index];

		if (map.isBlockAccessible(current_home_point_.toCell().x, current_home_point_.toCell().y))
		{
			Cell_t min_corner, max_corner;
			GridMap temp_map;
			temp_map.copy(map);
			temp_map.merge(slam_grid_map, false, false, true, false, false, false);
			temp_map.print(curr.toCell(), CLEAN_MAP, Cells{{0, 0}});
			plan_path_cells = findShortestPath(temp_map, curr.toCell(), current_home_point_.toCell(),
											   last_dir, false, false, min_corner, max_corner);
		}

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, this home point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);

			home_point_index++;
			return false;
		}
	}
	else // For going back to start point.
	{
		Cell_t min_corner, max_corner;
		GridMap temp_map;
		temp_map.copy(map);
		temp_map.merge(slam_grid_map, false, false, true, false, false, false);
		temp_map.print(curr.toCell(), CLEAN_MAP, Cells{{0, 0}});
		plan_path_cells = findShortestPath(temp_map, curr.toCell(), start_point_.toCell(),
										   last_dir, false, false, min_corner, max_corner);

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, start point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			ROS_INFO("%s %d: Use unknown area.", __FUNCTION__, __LINE__);
			home_way_index_ = THROUGH_UNKNOWN_AREA;
			return false;
		}
	}
}

bool GoHomePathAlgorithm::generatePathThroughUnknownArea(GridMap &map, const Point_t &curr, const Dir_t &last_dir,
														 Points &plan_path)
{
	Cells plan_path_cells{};
	auto &home_point_index = home_point_index_[home_way_index_];

	if (!home_points_.empty())
	{
		current_home_point_ = home_points_[home_point_index];

		if (map.isBlockAccessible(current_home_point_.toCell().x, current_home_point_.toCell().y))
		{
			Cell_t min_corner, max_corner;
			plan_path_cells = findShortestPath(map, curr.toCell(), current_home_point_.toCell(),
											   last_dir, true, false, min_corner, max_corner);
		}

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, this home point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: Index(%d), current_home_point_(%d, %d) NOT reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, home_point_index,
					 current_home_point_.toCell().x, current_home_point_.toCell().y);

			// Drop this unreachable home point.
			eraseHomePoint(current_home_point_);
			home_point_index = 0;

			if (home_points_.empty()) // No more home points, go back to start point.
			{
				current_home_point_ = {CELL_SIZE * (MAP_SIZE + 1), CELL_SIZE * (MAP_SIZE + 1), 0};
				home_way_index_ = THROUGH_CLEANED_AREA;
				ROS_INFO("%s %d: No more reachable home points, just go back to start point.", __FUNCTION__, __LINE__);
			}

			return false;
		}
	}
	else // For going back to start point.
	{
		Cell_t min_corner, max_corner;
		plan_path_cells = findShortestPath(map, curr.toCell(), start_point_.toCell(),
										   last_dir, true, false, min_corner, max_corner);

		if (!plan_path_cells.empty())
		{
			plan_path = *cells_generate_points(make_unique<Cells>(plan_path_cells));
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) reachable in this way." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			map.print(curr.toCell(), CLEAN_MAP, plan_path_cells);
			return true;
		}
		else
		{
			// In this way, start point is not reachable.
			ROS_INFO("\033[1;46;37m" "%s,%d: start_point_(%d, %d) NOT reachable in this way, exit state go home point." "\033[0m",
					 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			return true;
		}
	}
}

bool GoHomePathAlgorithm::reachTarget(bool &should_go_to_charger)
{
	bool ret = false;

	if (!home_points_.empty())
	{
//		ROS_ERROR("!!!!!!!!!!!!!home_points_(%d)", home_points_.size());
		if (getPosition().toCell() == getCurrentHomePoint().toCell())
		{
			ROS_INFO("%s %d: Reach home point(%d, %d).",
					 __FUNCTION__, __LINE__, getCurrentHomePoint().toCell().x, getCurrentHomePoint().toCell().y);
			// Erase home point but do NOT change the index.
			eraseHomePoint(getCurrentHomePoint());
			// Reset current home point.
			current_home_point_ = {CELL_SIZE * (MAP_SIZE + 1), CELL_SIZE * (MAP_SIZE + 1), 0};

			std::string msg = "Home_points_: ";
			for (auto it : home_points_)
				msg += "(" + std::to_string(it.toCell().x) + ", " + std::to_string(it.toCell().y) + "),";
			ROS_INFO("%s %d: %s", __FUNCTION__, __LINE__, msg.c_str());

			ret = true;
			should_go_to_charger = true;
		}
	}
	else {
		ROS_INFO("!!home_points_ empty curr (%d,%d,%f), start(%d,%d,%f)",
							getPosition().toCell().x,
							getPosition().toCell().y,
							radian_to_degree(getPosition().th),
							start_point_.toCell().x,
							start_point_.toCell().y,
							radian_to_degree(start_point_.th));
		if (getPosition().isCellAndAngleEqual(start_point_)) {
			ROS_INFO("%s %d: Reach start point(%d, %d).",
							 __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
			ret = true;
			should_go_to_charger = false;
		}
	}

	return ret;
}

Point_t GoHomePathAlgorithm::getCurrentHomePoint()
{
	return current_home_point_;
}

Points GoHomePathAlgorithm::getRestHomePoints()
{
	return home_points_;
}

bool GoHomePathAlgorithm::eraseHomePoint(Point_t target_home_point)
{
	bool ret = false;
	Points::iterator home_point_it = home_points_.begin();
	for (;home_point_it != home_points_.end(); home_point_it++)
	{
		if (home_point_it->toCell() == target_home_point.toCell())
		{
			ROS_INFO("%s %d: Erase this home point(%d, %d).",
					 __FUNCTION__, __LINE__, home_point_it->toCell().x, home_point_it->toCell().y);
			home_points_.erase(home_point_it);
			ret = true;
			break;
		}
	}

	// Reset the index.
	for (int &i : home_point_index_)
		i = 0;

	return ret;
}

void GoHomePathAlgorithm::setHomePoint(Point_t current_point)
{
	// Set home cell.
	Points::iterator home_point_it = home_points_.begin();
	for (;home_point_it != home_points_.end(); home_point_it++)
	{
		if (home_point_it->toCell() == current_point.toCell())
		{
			ROS_INFO("%s %d: Home point(%d, %d) exists.",
					 __FUNCTION__, __LINE__, home_point_it->toCell().x, home_point_it->toCell().y);

			return;
		}
	}

	while(ros::ok() && home_points_.size() >= (uint32_t)HOME_POINTS_SIZE && (home_points_.size() >= 1))
		// Drop the oldest home point to keep the home_points_.size() is within HOME_POINTS_SIZE.
		home_points_.pop_back();

	home_points_.push_front(current_point);
	std::string msg = "Update Home_points_: ";
	for (auto it : home_points_)
		msg += "(" + std::to_string(it.toCell().x) + ", " + std::to_string(it.toCell().y) + "),";
	ROS_INFO("%s %d: %s", __FUNCTION__, __LINE__, msg.c_str());
}

void GoHomePathAlgorithm::updateStartPointRadian(double radian)
{
	start_point_.th = radian;
	ROS_INFO("%s %d: Start point radian update to %f(%f in degree).", __FUNCTION__, __LINE__, start_point_.th,
			 radian_to_degree(start_point_.th));
}

void GoHomePathAlgorithm::initForGoHomePoint(GridMap &map)
{
	if (home_points_.empty())
		ROS_INFO("%s %d: No home points, just go to start point.", __FUNCTION__, __LINE__);
	else
	{
		std::string msg = "Home_points_: ";
		for (auto it : home_points_)
		{
			msg += "(" + std::to_string(it.toCell().x) + ", " + std::to_string(it.toCell().y) + "),";
			// Clear 8 cells around home points.
			map.setArea(it.toCell(), CLEANED, 1, 1);
		}
		ROS_INFO("%s %d: %s", __FUNCTION__, __LINE__, msg.c_str());
	}

	ROS_INFO("%s %d: Start point(%d, %d).", __FUNCTION__, __LINE__, start_point_.toCell().x, start_point_.toCell().y);
	// Clear 8 cells around start point.
	map.setArea(start_point_.toCell(), CLEANED, 1, 1);

	// set the rcon c_blocks to cleaned
	auto map_tmp = map.generateBound();
	for (const auto &cell : map_tmp) {
		if(map.getCell(CLEAN_MAP,cell.x,cell.y) == BLOCKED_TMP_RCON
					|| map.getCell(CLEAN_MAP, cell.x, cell.y) == BLOCKED_RCON)
			map.setCell(CLEAN_MAP,cell.x,cell.y, UNCLEAN);
	}

	// For debug.
	std::string msg = "home point index: ";
	for (auto it : home_point_index_)
		msg += "(" + std::to_string(it) + "),";
	ROS_INFO("%s %d: %s", __FUNCTION__, __LINE__, msg.c_str());
}

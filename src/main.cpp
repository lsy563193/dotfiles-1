#include <serial.h>
#include <bumper.h>
#include <signal.h>
#include <path_algorithm.h>
#include "robot.hpp"
#include "speaker.h"

#if R16_BOARD_TEST
#include "r16_board_test.hpp"
#endif

#if VERIFY_CPU_ID || VERIFY_KEY
#include "verify.h"
#endif

robot* robot_instance = nullptr;

void signal_catch(int sig)
{
	switch(sig){
		case SIGSEGV:
		{
			ROS_ERROR("Oops!!! pp receive SIGSEGV signal,segment fault!");
			if(robot_instance != nullptr){
				speaker.play(VOICE_CLEANING_STOP,false);
				delete robot_instance;
			}
			break;
		}
		case SIGINT:
		{
			ROS_ERROR("Oops!!! pp receive SIGINT signal,ctrl+c press");
			if(robot_instance != nullptr){
				speaker.play(VOICE_CLEANING_STOP,false);
				delete robot_instance;
			}
			break;
		}
		case SIGTERM:
		{
			ROS_ERROR("Ouch!!! pp receive SIGTERM signal,being kill!");
			if(robot_instance != nullptr){
				speaker.play(VOICE_CLEANING_STOP,false);
				delete robot_instance;
			}
			break;
		}
		default:
			ROS_ERROR("Oops!! pp receive %d signal",sig);
	}
	robot_instance = nullptr;
	ros::shutdown();
}
void case_2(GridMap &map) {
	map.setBlockWithBound({-15,-15}, {15,15}, CLEANED, 1);
	map.setBlockWithBound({-5,-5},{5,5},CLEANED,1);
	map.setBlockWithBound({-10,-5},{-7,5},UNCLEAN,1);
	map.setBlockWithBound({-11,-6},{6,-6},CLEANED,0);
//	map.setBlockWithBound({-5,-6},{6,-6},CLEANED,0);

//	map_set_block({-2,0},{3,0},);
//	map_set_block({-2,0},{3,0},UNCLEAN);
}
int main(int argc, char **argv)
{
	ros::init(argc, argv, "pp");
	ros::NodeHandle	nh_dev("~");

	struct sigaction act;
	act.sa_handler = signal_catch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESETHAND;
	sigaction(SIGTERM,&act,NULL);
	sigaction(SIGSEGV,&act,NULL);
	sigaction(SIGINT,&act,NULL);
	ROS_INFO("set signal action done!");

	robot_instance = new robot();
/*//test
	ROS_INFO("set signal action done!");
	GridMap map;
	case_2(map);
	map.print(CLEAN_MAP, 0, 0);

	boost::shared_ptr<APathAlgorithm> clean_path_algorithm_{};
	Dir_t old_dir_=MAP_POS_X;
	Points plan_path_{};
	clean_path_algorithm_.reset(new NavCleanPathAlgorithm);
	if (clean_path_algorithm_->generatePath(map, getPosition(), old_dir_, plan_path_)) {

	}*/
	ros::spin();
	return 0;
}

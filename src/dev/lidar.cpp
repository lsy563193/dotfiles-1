#include "pp.h"
#include <std_srvs/Empty.h>
#include <std_srvs/SetBool.h>
#include <pp/SetLidar.h>
#include <pp.h>

boost::mutex scanLinear_mutex_;
boost::mutex scanOriginal_mutex_;
boost::mutex scanCompensate_mutex_;
//float* Lidar::last_ranges_ = NULL;

sensor_msgs::LaserScan Lidar::lidarScanData_original_;

Lidar lidar;

Lidar::Lidar():angle_n_(0)
{
	//todo: lidar should add a status for setting ScanReady in case lidar still get a scan after shutting down.
//	setScanLinearReady(0);
//	setScanOriginalReady(0);
//	scanLinear_update_time = ros::Time::now().toSec();
//	scanOriginal_update_time = ros::Time::now().toSec();
	//last_ranges_ = new float[360];
	//memset(last_ranges_,0.0,360*sizeof(float));
}

Lidar::~Lidar()
{
//	motorCtrl(OFF);
//	setScanLinearReady(0);
//	setScanOriginalReady(0);
//	//delete []last_ranges_;
//	ROS_INFO("\033[35m" "%s %d: Lidar stopped." "\033[0m", __FUNCTION__, __LINE__);
}

void Lidar::scanLinearCb(const sensor_msgs::LaserScan::ConstPtr &scan)
{
	if(1){
		scanLinear_mutex_.lock();
		lidarScanData_linear_ = *scan;
		scanLinear_mutex_.unlock();
		angle_n_ = (int)((scan->angle_max - scan->angle_min) / scan->angle_increment);
		setScanLinearReady(1);
	}
	scanLinear_update_time = ros::Time::now().toSec();

}

#define WF_SCAN_TYPE						(2)
void Lidar::scanOriginalCb(const sensor_msgs::LaserScan::ConstPtr &scan)
{
	uint8_t scan2_valid_cnt = 0;

	for (uint16_t i = 0; i < 360; i++)
	{
		if (scan->ranges[i] != std::numeric_limits<float>::infinity())
		{
			if (++scan2_valid_cnt > 30)
				break;
		}
	}
	if (scan2_valid_cnt > 30) {
		// lidar not been covered.
		setLidarScanDataOriginal(scan);
		scanOriginal_update_time = ros::Time::now().toSec();
		setScanOriginalReady(1);
	}
}

void Lidar::scanCompensateCb(const sensor_msgs::LaserScan::ConstPtr &scan){
	scanCompensate_mutex_.lock();
	lidarScanData_compensate_ = *scan;
	scanCompensate_mutex_.unlock();
	setScanCompensateReady(1);
}

void Lidar::lidarPointCb(const visualization_msgs::Marker &point_marker) {
	scanCompensate_mutex_.lock();
	lidarXY_points = point_marker.points;
	scanCompensate_mutex_.unlock();
}

bool Lidar::lidarObstcalDetected(double distance, int angle, double range)
{
	int		i, count;
	bool	found = false;
	double	angle_min, angle_max, tmp, range_tmp;

	if (range < 0.0) {
		range_tmp = 0.155;
	} else {
		range_tmp = range;
	}
	angle_min = deg_to_rad((double) (angle % 360), 1) - atan(range_tmp / (distance + 0.155));
	angle_max = deg_to_rad((double) (angle % 360), 1) + atan(range_tmp / (distance + 0.155));

	scanLinear_mutex_.lock();
	auto tmp_scan_data = lidarScanData_linear_;
	scanLinear_mutex_.unlock();
	count = (int)((tmp_scan_data.angle_max - tmp_scan_data.angle_min) / tmp_scan_data.angle_increment);
	//ROS_INFO("%s %d %f %f %f %f", __FUNCTION__, __LINE__, range_tmp, distance + 0.155, range_tmp / (distance + 0.155), atan(range_tmp / (distance + 0.155)));
	//ROS_INFO("%s %d: angle min: %f max: %f\tcount: %d\tdtor: %f\ttan: %f", __FUNCTION__, __LINE__, angle_min, angle_max, count, deg_to_rad((double) (angle % 360), 1),  atan(range_tmp / (distance + 0.155)));
	for (i = 0; found == false && i < count; i++) {
		tmp = tmp_scan_data.angle_min + i * tmp_scan_data.angle_increment;
		if (tmp > angle_min && tmp < angle_max && tmp_scan_data.ranges[i] < distance + 0.155) {
			//ROS_INFO("%s %d: i: %d\ttmp: %f(%f, %f)\tdist: %f(%f)", __FUNCTION__, __LINE__, i, tmp, angle_min, angle_max, tmp_scan_data.ranges[i], distance + 0.155);
			found = true;
		}
	}

	return found;
}

int8_t Lidar::isScanOriginalReady()
{
	return is_scanOriginal_ready_;
}

int8_t Lidar::isScanLinearReady()
{
	return is_scanLinear_ready_;
}

int8_t Lidar::isScanCompensateReady()
{
	return is_scanCompensate_ready_;
}

void Lidar::setScanLinearReady(uint8_t val)
{
	is_scanLinear_ready_ = val;
}

void Lidar::setScanOriginalReady(uint8_t val)
{
	is_scanOriginal_ready_ = val;
}

void Lidar::setScanCompensateReady(uint8_t val)
{
	is_scanCompensate_ready_ = val;
}

void Lidar::motorCtrl(bool switch_)
{
	ROS_INFO("%s %d: Operate on lidar.", __FUNCTION__, __LINE__);
	if(switch_){
		scanLinear_update_time = ros::Time::now().toSec();
		scanOriginal_update_time = ros::Time::now().toSec();
		open_command_time_stamp_ = time(NULL);
	}

	if (!robot::instance()->lidarMotorCtrl(switch_))
		ROS_ERROR("%s %d: Lidar service not received!",__FUNCTION__,__LINE__);

	if (!switch_)
	{
		setScanLinearReady(0);
		setScanOriginalReady(0);
		//delete []last_ranges_;
		ROS_INFO("\033[35m" "%s %d: Lidar stopped." "\033[0m", __FUNCTION__, __LINE__);
	}
/*	time_t start_time = time(NULL);
	bool eh_status_now = false, eh_status_last = false;
	bool request_sent = false;
	while(ros::ok())
	{
		if (event_manager_check_event(&eh_status_now, &eh_status_last) == 1) {
			continue;
		}

		if (switch_ && (ev.fatal_quit || ev.key_clean_pressed || ev.cliff_all_triggered)) // Interrupt only happens during starting lidar.
		{
			if (!ev.fatal_quit)
			{
				setScanLinearReady(0);
				ROS_WARN("\033[34m" "%s %d: Lidar starting interrupted, status: %d" "\033[0m", __FUNCTION__, __LINE__, isScanLinearReady());
			}
			else
				setScanLinearReady(-1);//open lidar fail ,stop process
			break;
		}

		if (!request_sent)
		{
			request_sent = true;
			ROS_INFO("\033[35m" "%s %d: Send command %s!" "\033[0m", __FUNCTION__, __LINE__, switch_ ? "ON":"OFF");
			if (robot::instance()->lidarMotorCtrl(switch_)){
				start_time = time(NULL);
				if (!switch_)
					// For stop command.
					break;
			}
			else{
				ROS_ERROR("\033[35m" "%s %d: Service not received!" "\033[0m",__FUNCTION__,__LINE__);
				request_sent = false;
				setScanLinearReady(0);
			}
		}

		if (switch_ && isScanLinearReady())
		{
			ROS_INFO("\033[32m" "%s %d: Scan topic received, start lidar successed." "\033[0m", __FUNCTION__, __LINE__);
			break;
		}

		if (switch_ && (time(NULL) - start_time > 8)){ // Time out
			//ROS_ERROR("lidar.cpp, %s,%d,start lidar motor timeout",__FUNCTION__,__LINE__);
			ev.fatal_quit = true;
			continue;
		}
	}*/
}

/*
 * @author mengshige1988@qq.com
 * @brief for combine each lines in lines
 * @param1 lines before
 * @param2 lines after
 * @return false if no more lines for combine ,else return true.
 * */
static bool lineCombine(std::vector<LineABC> *lines_before,std::vector<LineABC> *lines_after)
{
	bool ret = false;
	const float MIN_COMBINE_ANGLE=10.0;//in degrees
	const float MIN_COMBINE_DIST=0.6;//in meters
	LineABC line_tmp;
	std::vector<LineABC>::iterator it;
	for(it = lines_before->begin(); it != lines_before->end(); it++){
		if( (it+1)!=lines_before->end() ){
			if(abs(it->K - (it+1)->K) <= MIN_COMBINE_ANGLE){
				if(two_points_distance_double(it->x2,it->y2,(it+1)->x1,(it+1)->y1) < MIN_COMBINE_DIST){
					line_tmp.x1 = it->x1;
					line_tmp.y1 = it->y1;
					line_tmp.x2 = (it+1)->x2;
					line_tmp.y2 = (it+1)->y2;
					line_tmp.A = line_tmp.y2 - line_tmp.y1;
					line_tmp.B = line_tmp.x1 - line_tmp.x2;
					line_tmp.C = line_tmp.x2 * line_tmp.y1 - line_tmp.x1 * line_tmp.y2;
					line_tmp.len = two_points_distance_double(line_tmp.x1,line_tmp.y1,line_tmp.x2,line_tmp.y2);
					line_tmp.K = atan(-1*(line_tmp.A / line_tmp.B))*180/PI;
					lines_after->push_back(line_tmp);
					it++;
					ret = true;
				}
			}
			else
				lines_after->push_back(*it);
		}
	}
	return ret;
}

/*
 * @author mengshige1988@qq.com
 * @brief find lines from scan data
 * @param1 lines vector
 * @return ture if found lines, alse return false
 * */
bool Lidar::findLines(std::vector<LineABC> *lines,bool combine)
{
	if(!isScanLinearReady())
		return false;
	const float MAX_LIDAR_DIST = 4.0;
	const float ACCR_PERSET = 0.05;//%5

	sensor_msgs::LaserScan scan_data;
	//scanLinear_mutex_.lock();
	if(lidarCheckFresh(0.210,1)){
		scan_data = lidarScanData_linear_;
	}
	else
		return false;
	//scanLinear_mutex_.unlock();

	/*---------translate lidar distance to point set----------*/
	int n_angle = angle_n_;
	int i=0;
	Vector2<double> lidar_point_pos;
	std::vector<Vector2<double>> point_set;
	for(i=0;i<n_angle;i++){
		if(scan_data.ranges[i] <= MAX_LIDAR_DIST){
			double cor_yaw =(double) robot::instance()->getWorldPoseAngle()/10.0;
			double cor_p_x =(double) robot::instance()->getWorldPoseX();
			double cor_p_y =(double) robot::instance()->getWorldPoseY();

			double ranges = scan_data.ranges[i];
			lidar_point_pos.X = cos(( i + cor_yaw + 180.0)*PI/180.0 ) *ranges + cor_p_x;//in meters
			lidar_point_pos.Y = sin(( i + cor_yaw + 180.0)*PI/180.0 ) *ranges + cor_p_y;//in meters
			coordinate_transform(&lidar_point_pos.X, &lidar_point_pos.Y, LIDAR_THETA, LIDAR_OFFSET_X, LIDAR_OFFSET_Y);
			point_set.push_back(lidar_point_pos);
			lidar_point_pos.X = 0.0;
			lidar_point_pos.Y = 0.0;
		}
	}
	//pubPointMarkers(&point_set);

	if(point_set.size() <2)
		return false;// not enough point to make lines , at least got 2 points

	/*--------ready to find lines in point set-----------*/
	LineABC line;
	lines->clear();
	const float LINE_n_P_DIST_MAX = 0.05;//max line and point distance
	const float POINT_n_P_DIST_MAX = 0.3;//max point and point distance
	int line_count = 0;
	int point_count = 0;
	bool find_next_line = true;
	i = 0;
	while(i < (point_set.size() -1)){
		if(find_next_line){
			double x1 = point_set.at(i).X;
			double y1 = point_set.at(i).Y;
			i++;
			double x2 = point_set.at(i).X;
			double y2 = point_set.at(i).Y;
			double dist = two_points_distance_double(x1,y1,x2,y2);
			if(dist > POINT_n_P_DIST_MAX)//find next line
				continue;
			find_next_line = false;
			line.len = dist;
			line.A = y2-y1;
			line.B = x1-x2;
			line.C = x2*y1-x1*y2;
			line.K = atan((-1)*line.A/line.B)*180/PI;
			line.x1 = x1;
			line.y1 = y1;
			line.x2 = x2;
			line.y2 = y2;
			lines->push_back(line);
			line_count++;
			point_count+=2;
		}
		else{
			double xn = point_set.at(i).X;
			double yn = point_set.at(i).Y;
			double dist_p2l = fabs(line.A*xn+line.B*yn+line.C)/sqrt(line.A*line.A+line.B*line.B);//point to line distance
			double dist_p2p = two_points_distance_double(point_set.at(i-1).X ,point_set.at(i-1).Y ,xn ,yn);//current point to last point distance
			point_count++;
			if(dist_p2l> LINE_n_P_DIST_MAX || dist_p2p > POINT_n_P_DIST_MAX){ //find next line
				find_next_line = true;
				point_count = 0;
				continue;
			}
			if(point_count>3){
				double xn_2 = (point_set.at(i-1).X - point_set.at(i-2).X) /2.0 + point_set.at(i-2).X;
				double yn_2 = (point_set.at(i-1).Y - point_set.at(i-2).Y) /2.0 + point_set.at(i-2).Y;
				/*---update A,B,C,x2,y2,K---*/
				LineABC *last_line = &lines->back();
				last_line->A = yn_2 - line.y1;
				last_line->B = line.x1 - xn_2;
				last_line->C = xn_2*line.y1-line.x1*yn_2;
				last_line->K= atan(-1*(last_line->A / last_line->B))*180/PI;
				last_line->x2 = xn_2;
				last_line->y2 = yn_2;
				last_line->len = two_points_distance_double(line.x1,line.y1,xn_2,yn_2);
			}
		}
		i++;
	}

	/*------combine short lines to long lines------*/
	if(combine){
		std::vector<LineABC> lines_after;
		while(lineCombine(lines,&lines_after)){
			lines->clear();
			*lines = lines_after;
			lines_after.clear();
		}
	}
	/*------print line data------*/
	/*
	std::string msg("");
	std::vector<LineABC>::iterator it;
	for(it = lines->begin();it!= lines->end();it++){
		msg+= "\n[A:"+ std::to_string(it->A) +",B:" + std::to_string(it->B) +",C:" +
			std::to_string(it->C)+",len = "+std::to_string(it->len)+",K = " +
			std::to_string(it->K)+",("+std::to_string(it->x1)+","+
			std::to_string(it->y1)+"),("+std::to_string(it->x2)+","+std::to_string(it->y2)+")]";
	}
	ROS_INFO("%s,%d,pub line markers,lines numbers \033[35m%u\033[0m, lines:\033[32m %s \033[0m",__FUNCTION__,__LINE__,lines->size(),msg.c_str());
	*/
	robot::instance()->pubLineMarker(lines);
	return true;
}

void Lidar::startAlign()
{
	start_align_time_stamp_ = time(NULL);
	align_finish_ = false;
}

bool Lidar::alignTimeOut()
{
	auto time_diff = time(NULL) - start_align_time_stamp_;
	//ROS_INFO("%s %d: Time diff:%d", __FUNCTION__, __LINE__, time_diff);
	if (time_diff > 2)
	{
		ROS_WARN("%s %d: Align time out.", __FUNCTION__, __LINE__);
		return true;
	}

	return false;
}

bool Lidar::alignFinish()
{
	return align_finish_;
}

/*
 * @auther mengshige1988@qq.com
 * @breif get ac_align angle
 * @param1 liens vector
 * @param2 ac_align angle
 * @retrun true if found ac_align angle ,alse return false
 * */
bool Lidar::getAlignAngle(const std::vector<LineABC> *lines ,float *align_angle)
{
	if(lines->empty())
		return false;
	const float DIF_ANG_RANGE = 10.0;//different angle ranges
	const float LONG_ENOUGTH = 1.0;//in meters
	std::vector<float> same_angle_count;
	std::vector<LineABC>::const_iterator it1,it2;
	float len = 0.0;
	int i=0;
	int pos = 0;
	/*----find the longest one in lines---*/
	for(it1 = lines->cbegin(); it1!= lines->cend(); it1++){
		if(it1->len >= len){
			len = it1->len;
			pos = i;
		}
		i++;
	}
	if(lines->at(pos).len >= LONG_ENOUGTH){
		*align_angle = lines->at(pos).K;
		ROS_INFO("%s,%d: find long line %f ,align_angle %f",__FUNCTION__,__LINE__,lines->at(pos).len,*align_angle);
		align_finish_ = true;
		return true;
	}

	/*---find those most populars in lines ---*/
	float sum = 0.0;
	float avg = 0.0;
	i=0;
	if(lines->size() >1){
		for(it2 = lines->cbegin();it2!=lines->cend();it2++){
			if(i > lines->size()/2)
				return false;
			i++;
			sum = 0.0;
			same_angle_count.clear();
			for(it1 = it2;it1!= lines->cend(); it1++){
				if((it1+1) != lines->cend()){
					if(fabs((it1+1)->K - it1->K) < DIF_ANG_RANGE){
						sum += it1->K;
						same_angle_count.push_back(it1->K);
						//printf("similar angle %f\n",it1->K);
						if(same_angle_count.size() > lines->size()/2 ){//if number count more than 1/2 of total ,return
							avg = sum / (1.0*same_angle_count.size());
							*align_angle = avg;
							align_finish_ = true;
							ROS_INFO("%s,%d,get most popular line angle %f",__FUNCTION__,__LINE__,avg);
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}


bool Lidar::lidarGetFitLine(int begin, int end, double range, double dis_lim, double *line_angle, double *distance,bool is_left,bool is_align)
{
	if(isScanLinearReady() == 0)
		return false;
	bool isReverse = false;
	float consec_lim = 0.10;
	int points_count_lim = 10;
	float t_lim_split = 0.10;
	float t_lim_merge = 0.10;
	double th;
	Vector2<double>	New_Lidar_Point;
	Lidar_Point.clear();

	if(is_align){
		consec_lim = 0.06;
		points_count_lim = 10;
		t_lim_split = 0.06;
		t_lim_merge = 0.06;
	}
	ROS_DEBUG("lidarGetFitLine");
	scanLinear_mutex_.lock();
	auto tmp_scan_data = lidarScanData_linear_;
	scanLinear_mutex_.unlock();
	begin = static_cast<int>(atan2(ROBOT_RADIUS * sin(begin * M_PI / 180),
																	 LIDAR_OFFSET_X + ROBOT_RADIUS * cos(begin * M_PI / 180)) * 180 / M_PI);
	end = static_cast<int>(atan2(ROBOT_RADIUS * sin(end * M_PI / 180),
																 LIDAR_OFFSET_X + ROBOT_RADIUS * cos(end * M_PI / 180)) * 180 / M_PI);
	begin -= LIDAR_THETA / 10;
	end -= LIDAR_THETA / 10;
	if(begin < 0)
		begin += 360;
	if(end < 0)
		end += 360;
	if(begin >= end)
		isReverse = true;
	for (int i = begin; (i < end || isReverse);) {
		if(i > 359)
			i = i - 360;
//		ROS_INFO("i = %d", i);
		if (tmp_scan_data.ranges[i] < 4) {
			th = i * 1.0;
			th = th + 180.0;
			New_Lidar_Point.X = cos(th * PI / 180.0) * tmp_scan_data.ranges[i];
			New_Lidar_Point.Y = sin(th * PI / 180.0) * tmp_scan_data.ranges[i];
			coordinate_transform(&New_Lidar_Point.X, &New_Lidar_Point.Y, LIDAR_THETA, LIDAR_OFFSET_X, LIDAR_OFFSET_Y);
			Lidar_Point.push_back(New_Lidar_Point);
		}
		i++;
		if(i == end)
			isReverse = false;
	}

	splitLine(Lidar_Point, consec_lim,points_count_lim);
	splitLine2nd(&Lidar_Group, t_lim_split,points_count_lim);
	mergeLine(&Lidar_Group, t_lim_merge);
	fitLineGroup(&Lidar_Group,dis_lim,is_align);
	//*line_angle = atan2(-a, b) * 180 / PI;
	Lidar_Group.clear();
	if (!fit_line.empty()) {
		//todo mt
		if (is_left) {
			*line_angle = atan2(0 - fit_line.begin()->A, fit_line.begin()->B) * 180 / PI;
			*distance = fabs(fit_line.begin()->C / (sqrt(fit_line.begin()->A * fit_line.begin()->A + fit_line.begin()->B * fit_line.begin()->B)));
		} else {
			*line_angle = atan2(0 - fit_line.back().A, fit_line.back().B) * 180 / PI;
			*distance = fabs(fit_line.back().C / (sqrt(fit_line.back().A * fit_line.back().A + fit_line.back().B * fit_line.back().B)));
		}
		//ROS_INFO("a = %lf, b = %lf, c = %lf", a, b, c);
//		ROS_ERROR("line_angle = %lf", *line_angle);
		align_finish_ = true;
		return true;
	} else {
		ROS_DEBUG("no line to fit!");
		return false;
	}
}

bool Lidar::lineFit(const std::vector<Vector2<double>> &points, double &a, double &b, double &c)
{
	int size = points.size();
	if(size < 2) {
		a = 0;
		b = 0;
		c = 0;
		return false;
	}
	double x_mean = 0;
	double y_mean = 0;

	for(int i = 0; i < size; i++) {
		x_mean += points[i].X;
		y_mean += points[i].Y;
	}
	x_mean /= size;
	y_mean /= size;
	double Dxx = 0, Dxy = 0, Dyy = 0;

	for(int i = 0; i < size; i++) {
		Dxx += (points[i].X - x_mean) * (points[i].X - x_mean);
		Dxy += (points[i].X - x_mean) * (points[i].Y - y_mean);
		Dyy += (points[i].Y - y_mean) * (points[i].Y - y_mean);
	}

	double lambda = ( (Dxx + Dyy) - sqrt( (Dxx - Dyy) * (Dxx - Dyy) + 4 * Dxy * Dxy) ) / 2.0;
	double den = sqrt( Dxy * Dxy + (lambda - Dxx) * (lambda - Dxx) );

	//ROS_INFO("cells.size = %d, den = %lf", size, den);
	if(fabs(den) < 1e-5) {
		if( fabs(Dxx / Dyy - 1) < 1e-5) {
			ROS_INFO("line fit failed!");
			return false;
		}
		else {
			a = 1;
			b = 0;
			c = - x_mean;
		}
	}
	else {
		a = Dxy / den;
		b = (lambda - Dxx) / den;
		c = - a * x_mean - b * y_mean;
	}
	return true;
}

bool Lidar::splitLine(const std::vector<Vector2<double>> &points, double consec_lim, int points_count_lim) {
	int points_size = points.size();
	double distance;
	std::vector<Vector2<double>> new_line;
	new_line.push_back(points[0]);
	for(int i = 1; i < (points_size - 1); i++) {
		distance = sqrt((points[i].X - points[i-1].X) * (points[i].X - points[i-1].X) + (points[i].Y - points[i-1].Y) * (points[i].Y - points[i-1].Y));
		if (distance <= consec_lim) {
			new_line.push_back(points[i]);
			if (i == (points_size - 1)) {
				Lidar_Group.push_back(new_line);
				new_line.clear();
			}
		} else {
			Lidar_Group.push_back(new_line);
			new_line.clear();
			new_line.push_back(points[i]);
		}
	}
	for (std::vector<std::vector<Vector2<double>> >::iterator iter = Lidar_Group.begin(); iter != Lidar_Group.end();){
		if (iter->size() < points_count_lim) {
			iter = Lidar_Group.erase(iter);
		} else {
			++iter;
		}
	}
	ROS_DEBUG("splitLine : Lidar_Group.size = %lu", Lidar_Group.size());
//	robot::instance()->pubLineMarker(&Lidar_Group,"splitLine");
	return true;
}

bool Lidar::splitLine2nd(std::vector<std::vector<Vector2<double>> > *groups, double t_lim, int points_count_lim) {
	int points_size;
	int erased_size;
	double a, b, c;
	double x1, y1, x2, y2;
	double t, t_max;
	int	points_index_max;
	bool end_iterate_flag = true;
	std::vector<int> groups_erased_index;
	std::vector<Vector2<double>> new_line;
	std::vector<std::vector<Vector2<double>> > new_group;

	ROS_DEBUG("Lidar::splitLine2nd");
	groups_erased_index.clear();
	new_line.clear();
	new_group.clear();

	/*loop for lines in groups*/
	for (std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin(); iter != (*groups).end(); ++iter) {
		x1 = iter->begin()->X;
		y1 = iter->begin()->Y;
		x2 = (iter->end() - 1)->X;
		y2 = (iter->end() - 1)->Y;
		points_size = static_cast<int>(iter->size());
		end_iterate_flag = true;
		if (x1 != x2 && y1 != y2) {
			a = y2 - y1;
			b = x1 - x2;
			c= x2 * y1 - x1 * y2;
			t = 0;
			t_max = 0;
			for (int j = 0; j < points_size; j++) {
				t = fabs((a * (iter->begin() + j)->X + b * (iter->begin() +j)->Y + c) / sqrt(a * a + b * b));
				if (t >= t_max) {
					t_max = t;
					points_index_max = j;
				}
			}
			if (t_max > t_lim) {
				end_iterate_flag &= false;
				int index = static_cast<int>(std::distance((*groups).begin(), iter));
				groups_erased_index.push_back(index);
				for (int j = 0; j < points_size; j++) {
					new_line.push_back(*(iter->begin() + j));
					if (j == points_index_max) {
						if (j == (points_size - 1)) {
							new_line.pop_back();
							new_group.push_back(new_line);
							new_line.clear();
						} else {
							new_group.push_back(new_line);
							new_line.clear();
						}
					} else if ((j > points_index_max) && (j == (points_size - 1))) {
						new_group.push_back(new_line);
						new_line.clear();
					}
				}
			} else {//else push_back the line into new_group
				end_iterate_flag &= true;
			}
		}
	}
	/*loop for erasing the old lines which are unsplit and push_back lines in new_group into groups*/
	ROS_DEBUG("loop for erasing the old lines which are unsplit");
	erased_size = static_cast<int>(groups_erased_index.size());
	if (!groups_erased_index.empty()) {
		for (int i = 0; i < erased_size; i++) {
			std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin() + (groups_erased_index[i] + i);
			(*groups).erase(iter);
			for (int j = 0; j < 2; j++) {
				(*groups).insert((*groups).begin() + (groups_erased_index[i] + j), *(new_group.begin() + j));
			}
		}
	}
	groups_erased_index.clear();
	new_group.clear();

	/*loop for erasing the line which size is less than points_count_lim*/
	ROS_DEBUG("loop for erasing the line which size is less than points_count_lim");
	for (std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin(); iter != (*groups).end();) {
		if (iter->size() < points_count_lim) {
			iter = (*groups).erase(iter);
		} else {
			++iter;
		}
	}

	/*if the lines still can be splited, iterate to split it*/
	if (end_iterate_flag == false) {
		//ROS_INFO("iterate!");
		splitLine2nd(&Lidar_Group, 0.01,10);
	}
//	robot::instance()->pubLineMarker(&Lidar_Group,"splitLine2nd");
	return true;
}

bool Lidar::mergeLine(std::vector<std::vector<Vector2<double>> > *groups, double t_lim) {
	double a, b, c;
	double x1, y1, x2, y2;
	int points_size, points_size_2nd;
	double t, t_max = 0;
	std::vector<int> merge_index;
	std::vector<Vector2<double>> new_line;
	std::vector<std::vector<Vector2<double>> > new_group;
	new_line.clear();
	new_group.clear();
	if (!(*groups).empty()) {
		for (std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin(); iter != (*groups).end() - 1; ++iter) {
			x1 = iter->begin()->X;
			y1 = iter->begin()->Y;
			x2 = ((iter + 1)->end() - 1)->X;
			y2 = ((iter + 1)->end() - 1)->Y;
			if (x1 != x2 && y1 != y2) {
				a = y2 - y1;
				b = x1 - x2;
				c= x2 * y1 - x1 * y2;
				points_size = static_cast<int>(iter->size());
				points_size_2nd = static_cast<int>((iter + 1)->size());
				/*loop for checking the first line*/
				for (int j = 0; j < points_size; j++) {
					t = fabs((a * (iter->begin() + j)->X + b * (iter->begin() +j)->Y + c) / sqrt(a * a + b * b));
					if (t >= t_max) {
						t_max = t;
					}
				}
				/*loop for checking the second line*/
				for (int j = 0; j < points_size_2nd; j++) {
					t = fabs((a * ((iter + 1)->begin() + j)->X + b * ((iter + 1)->begin() +j)->Y + c) / sqrt(a * a + b * b));
					if (t >= t_max) {
						t_max = t;
					}
				}
				if (t_max < t_lim) {
					int index = static_cast<int>(std::distance((*groups).begin(), iter));
					merge_index.push_back(index);
				}
			}
		}
	}

	/*do mergeFromSlamGridMap*/
	if (!merge_index.empty()) {
		int loop_count = 0;
		for (std::vector<int>::iterator m_iter = merge_index.begin(); m_iter != merge_index.end(); ++m_iter) {
			std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin() + (*m_iter);
			points_size = static_cast<int>(iter->size());
			points_size_2nd = static_cast<int>((iter + 1)->size());
			for (int j = 0; j < points_size; j++) {
				new_line.push_back(*(iter->begin() + j));
			}
			for (int j = 0; j < points_size_2nd; j++) {
				new_line.push_back(*((iter + 1)->begin() + j));
			}
			(*groups).erase(iter - loop_count);
			(*groups).erase(iter + 1 - loop_count);
			(*groups).insert((*groups).begin() + (*m_iter) - loop_count, new_line);
			loop_count++;
		}
	}
	ROS_DEBUG("pub line marker");
	robot::instance()->pubLineMarker(&Lidar_Group,"merge");
	return true;
}

bool Lidar::fitLineGroup(std::vector<std::vector<Vector2<double>> > *groups, double dis_lim, bool is_align) {
	double 	a, b, c;
	double	x_0;
	int	loop_count = 0;
	LineABC	new_fit_line;
	fit_line.clear();
	if (!(*groups).empty()) {
		for (std::vector<std::vector<Vector2<double>> >::iterator iter = (*groups).begin(); iter != (*groups).end(); ++iter) {
			lineFit((*iter), a, b, c);
			new_fit_line.A = a;
			new_fit_line.B = b;
			new_fit_line.C = c;
			new_fit_line.len = static_cast<int>(iter->size());
			x_0 = 0 - c / a;
			ROS_DEBUG("a = %lf, b = %lf, c = %lf", a, b, c);
			ROS_DEBUG("x_0 = %lf", x_0);
			/*erase the lines which are far away from the robot*/
			double dis = fabs(c / (sqrt(a * a + b * b)));
			new_fit_line.dis = dis;
			//todo x_0
			if (dis > dis_lim || dis < ROBOT_RADIUS || (is_align ? 0 : x_0 < 0)) {
				ROS_DEBUG("the line is too far away from robot. dis = %lf,x_0:%lf,dis_lim:%lf", dis,x_0,dis_lim);
				continue;
			}
			fit_line.push_back(new_fit_line);
			pubFitLineMarker(a, b, c, iter->begin()->Y, (iter->end() - 1)->Y);
			ROS_DEBUG("%s %d: line_angle%d = %lf", __FUNCTION__, __LINE__, loop_count, line_angle);
			loop_count++;
		}
	} else {
		fit_line_marker.points.clear();
		robot::instance()->pubFitLineMarker(fit_line_marker);
	}
	fit_line_marker.points.clear();
	if(is_align){
		std::sort(fit_line.begin(),fit_line.end(),[](LineABC a,LineABC b){
			return a.len > b.len;
		});
	}
	return true;
}

void Lidar::pubFitLineMarker(double a, double b, double c, double y1, double y2) {
	double x1, x2;
	fit_line_marker.ns = "fit_line_marker";
	fit_line_marker.id = 0;
	//fit_line_marker.type = visualization_msgs::Marker::SPHERE_LIST;
	fit_line_marker.type = 5;
	fit_line_marker.action= 0;//add
	fit_line_marker.lifetime=ros::Duration(0);
	fit_line_marker.scale.x = 0.03;
	fit_line_marker.scale.y = 0.03;
	fit_line_marker.scale.z = 0.03;
	fit_line_marker.color.r = 0.0;
	fit_line_marker.color.g = 1.0;
	fit_line_marker.color.b = 0.0;
	fit_line_marker.color.a = 1.0;
	fit_line_marker.header.frame_id = "/base_link";
	fit_line_marker.header.stamp = ros::Time::now();
	lidar_points_.x = 0.0;
	lidar_points_.y = 0.0;
	lidar_points_.z = 0.0;

	x1 = (0 - b * y1 - c) / a;
	x2 = (0 - b * y2 - c) / a;

	lidar_points_.x = x1;
	lidar_points_.y = y1;
	fit_line_marker.points.push_back(lidar_points_);
	lidar_points_.x = x2;
	lidar_points_.y = y2;
	fit_line_marker.points.push_back(lidar_points_);
	robot::instance()->pubFitLineMarker(fit_line_marker);
}

/*
 * @author mengshige1988@qq.com
 * @brief according giveing angle return lidar range data
 * @angle angle from 0 to 359
 * @return distance value (meter)
 * */
double Lidar::getLidarDistance(uint16_t angle){
	if(angle >359 || angle < 0){
		ROS_WARN("%s,%d,angle should be in range 0 to 359,input angle = %u",__FUNCTION__,__LINE__,angle);
		return 0;
	}
	else{
		scanLinear_mutex_.lock();
		auto tmp_scan_data = lidarScanData_linear_;
		scanLinear_mutex_.unlock();
		return tmp_scan_data.ranges[angle];
	}
}

/*----set lidar marker according to direction-----*/
static uint8_t setLidarMarkerAcr2Dir(double X_MIN,double X_MAX,int angle_from,int angle_to,int dx,int dy,const sensor_msgs::LaserScan *scan_range,uint8_t *lidar_status,uint8_t obs_status)
{
	double x,y,th;
	const	double Y_MIN = 0.140;//ROBOT_RADIUS
	const	double Y_MAX = 0.237;//0.279
	int count = 0;
	uint8_t ret = 0;
	int i =angle_from;
	for (int j = angle_from; j < angle_to; j++) {
		i = j>359? j-360:j;
		if (scan_range->ranges[i] < 4) {
			th = i*1.0 + 180.0;
			if(j >= 314 && j < 404){//314~44
				x = -cos(th * PI / 180.0) * scan_range->ranges[i];
				//y = -sin(th * PI / 180.0) * scan_range->ranges[i];
				if (x > X_MIN && x < X_MAX ) {
					count++;
				}
			}
			else if( j >= 44 && j < 134){
				//x = cos(th * PI / 180.0) * scan_range->ranges[i];
				y = -sin(th * PI / 180.0) * scan_range->ranges[i];
				if (y > Y_MIN && y < Y_MAX ) {
					count++;
				}
			}
			else if( j >= 134 && j < 224){
				x = cos(th * PI / 180.0) * scan_range->ranges[i];
				//y = sin(th * PI / 180.0) * scan_range->ranges[i];
				if (x > X_MIN && x < X_MAX ) {
					count++;
				}
			}
			else if( j >= 224 && j < 314){
				//x = cos(th * PI / 180.0) * scan_range->ranges[i];
				y = sin(th * PI / 180.0) * scan_range->ranges[i];
				if (y > Y_MIN && y < Y_MAX ) {
					count++;
				}
			}
		}
	}
	if (count > 10) {
		int16_t x_tmp,y_tmp;
		nav_map.robotToCell(getPosition(), CELL_SIZE * dy, CELL_SIZE * dx, x_tmp, y_tmp);
		if (nav_map.getCell(CLEAN_MAP,x_tmp,y_tmp) != BLOCKED_BUMPER)
		{
			ROS_INFO("\033[36mlidar marker : (%d,%d)\033[0m",x_tmp,y_tmp);
			nav_map.setCell(CLEAN_MAP, x_tmp, y_tmp, BLOCKED_LIDAR);
		}
		ret = 1;
		*lidar_status |= obs_status;
	}
	return ret;

}

uint8_t Lidar::lidarMarker(double X_MAX)
{
	scanCompensate_mutex_.lock();
	if(!lidarCheckFresh(0.6,1)){
		scanCompensate_mutex_.unlock();
		return false;
	}
	double x, y;
	int dx, dy;
//	const double X_MIN = 0.140;//ROBOT_RADIUS
//	const	double Y_MIN = ROBOT_RADIUS;//ROBOT_RADIUS
	const	double Y_MAX = 0.20;//0.279
	int	count_array[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	for(auto point:lidarXY_points){
		x = point.x;
		y = point.y;
		//front
		if (x > ROBOT_RADIUS && x < X_MAX) {
			//middle
			if (y > -0.056 && y < 0.056) {
				count_array[0]++;
			}
		}
		if (x > 0.056 && x < X_MAX) {
			//left
			if (y > 0.056 && y < 0.168) {
				count_array[1]++;
			}
			//right
			if (y > -0.168 && y < -0.056) {
				count_array[2]++;
			}
		}
		//back
		if (x < -ROBOT_RADIUS && x > (-X_MAX)) {
			//middle
			if (y > -0.056 && y < 0.056) {
				count_array[3]++;
			}
		}
		if (x < -0.056 && x > (-X_MAX)) {
			//left
			if (y > 0.056 && y < 0.168) {
				count_array[4]++;
			}
			//right
			if (y > -0.168 && y < -0.056) {
				count_array[5]++;
			}
		}
		//left
		if (y > ROBOT_RADIUS && y < Y_MAX) {
			//middle
			if (x > -0.056 && x < 0.056) {
				count_array[6]++;
			}
			//front
			if (x > 0.056 && x < 0.168) {
				count_array[7]++;
			}
			//back
			if (x > -0.168 && x < -0.056) {
				count_array[8]++;
				}
		}
			//right
		if (y < (0 - ROBOT_RADIUS) && y >  (0 - Y_MAX)) {
			//middle
			if (x > -0.056 && x < 0.056) {
				count_array[9]++;
			}
			//front
			if (x > 0.056 && x < 0.168) {
				count_array[10]++;
			}
			//back
			if (x > -0.168 && x < -0.056) {
				count_array[11]++;
			}
		}
	}

	std::string msg = "";
	std::string direction_msg = "";
	uint8_t block_status = 0;
	for (int i = 0; i < 12; i++) {
		if (count_array[i] > 10) {
			int16_t x_tmp,y_tmp;
			switch(i) {
				case 0 : {
					dx = 2;
					dy = 0;
					direction_msg = "front middle";
					block_status |= BLOCK_FRONT;
					break;
				}
				case 1 : {
					dx = 2;
					dy = 1;
					direction_msg = "front left";
					block_status |= BLOCK_LEFT;
					break;
				}
				case 2 : {
					dx = 2;
					dy = -1;
					direction_msg = "front right";
					block_status |= BLOCK_RIGHT;
					break;
				}
				case 3 : {
					dx = -2;
					dy = 0;
					direction_msg = "back middle";
					break;
				}
				case 4 : {
					dx = -2;
					dy = 1;
					direction_msg = "back left";
					break;
				}
				case 5 : {
					dx = -2;
					dy = -1;
					direction_msg = "back right";
					break;
				}
				case 6 : {
					dx = 0;
					dy = 2;
					direction_msg = "left middle";
					break;
				}
				case 7 : {
					dx = 1;
					dy = 2;
					direction_msg = "left front";
					break;
				}
				case 8 : {
					dx = -1;
					dy = 2;
					direction_msg = "left back";
					break;
				}
				case 9 : {
					dx = 0;
					dy = -2;
					direction_msg = "right middle";
					break;
				}
				case 10 : {
					dx = 1;
					dy = -2;
					direction_msg = "right front";
					break;
				}
				case 11 : {
					dx = -1;
					dy = -2;
					direction_msg = "right back";
					break;
				}
			}

			nav_map.robotToCell(getPosition(), CELL_SIZE * dy, CELL_SIZE * dx, x_tmp, y_tmp);
			auto cell_status = nav_map.getCell(CLEAN_MAP, x_tmp, y_tmp);
			if (cell_status != BLOCKED_BUMPER && cell_status != BLOCKED_OBS)
			{
				//ROS_INFO("    \033[36mlidar marker : (%d,%d), i = %d, dx = %d, dy = %d.\033[0m",count_to_cell(x_tmp),count_to_cell(y_tmp), i, dx, dy);
				msg += direction_msg + "(" + std::to_string(x_tmp) + ", " + std::to_string(y_tmp) + ")";
				nav_map.setCell(CLEAN_MAP,x_tmp,y_tmp, BLOCKED_LIDAR); //BLOCKED_OBS);
			}
		}
	}
//	if (!msg.empty())
//		ROS_INFO("%s %d: \033[36mlidar marker: %s.\033[0m", __FUNCTION__, __LINE__, msg.c_str());
	scanCompensate_mutex_.unlock();
	return block_status;
}

uint8_t Lidar::isRobotSlip()
{
	static int16_t slip_count= 0;
	static uint16_t seq_count = 0;
	static uint32_t seq=0;
	static uint8_t last_ranges_init = 0;
	static std::vector<float> last_ranges ;
	const float PERCENT = 0.85;//85%

	const float acur1 = 0.07;//accuracy 1 ,in meters
	const float acur2 = 0.05;//accuracy 2 ,in meters
	const float acur3 = 0.03;//accuracy 3 ,in meters
	const float acur4 = 0.01;//accuracy 4 ,in meters

	const float dist1 = 3.5;//range distance 1 ,in meters
	const float dist2 = 2.5;//range distance 2
	const float dist3 = 1.5;//range distance 3
	const float dist4 = 0.5;//range distance 4

	const int COUNT = 6;//count number before trigger stuck
	const float WSL = 0.04; //wheel speed limit ,in m/s
	uint16_t same_count = 0;
	uint8_t ret = 0;
	uint16_t tol_count = 0;
	auto tmp_scan_data = getLidarScanDataOriginal();
	if(g_robot_slip_enable && seq != tmp_scan_data.header.seq && isScanOriginalReady() && ( std::abs(
			wheel.getLeftWheelActualSpeed()) >= WSL || std::abs(wheel.getRightWheelActualSpeed()) >= WSL ) )
	{
		seq = tmp_scan_data.header.seq;
		if(last_ranges_init == 0){//for the first time
			last_ranges_init = 1;
			last_ranges = tmp_scan_data.ranges;
			return ret;
		}

		for(int i =0;i<=359;i=i+2){
			if(tmp_scan_data.ranges[i] < dist1){
				tol_count++;
				if(tmp_scan_data.ranges[i] >dist2 && tmp_scan_data.ranges[i] < dist1){//
					if(std::abs( tmp_scan_data.ranges[i] - last_ranges[i] ) <= acur1 ){
						same_count++;
					}
				} 
				else if(tmp_scan_data.ranges[i] >dist3 && tmp_scan_data.ranges[i] < dist2){//
					if(std::abs( tmp_scan_data.ranges[i] - last_ranges[i] ) <= acur2 ){
						same_count++;
					}
				}
				else if(tmp_scan_data.ranges[i] >dist4 && tmp_scan_data.ranges[i] < dist3){//
					if(std::abs( tmp_scan_data.ranges[i] - last_ranges[i] ) <= acur3 ){
						same_count++;
					}
				}
				else if(tmp_scan_data.ranges[i] <= dist4){
					if(std::abs( tmp_scan_data.ranges[i] - last_ranges[i] ) <= acur4 ){
						same_count++;
					}
				}
			}
		}
		if(++seq_count >2){//store last ranges after 2 sequance
			seq_count=0;
			last_ranges = tmp_scan_data.ranges;
		}
		//ROS_INFO("\033[1;45;37msame_count %d,tol_count %d,\033[0m",same_count, tol_count );
		if((same_count*1.0)/(tol_count*1.0) >= PERCENT){
			if(++slip_count>= COUNT){
				slip_count = 0;
				ret = 1;
			}
		}
		else{
			//slip_count= (slip_count>0)? slip_count - 1: 0;
			slip_count = 0;
		}
	}
	else if(!g_robot_slip_enable)
	{
		seq = 0;
		seq_count = 0;
		slip_count= 0;
		last_ranges_init = 0;
	}
	return ret;
}

/*
 * @author Alvin Xie
 * @brief make use of lidar to judge x+ or x- is more closer to the wall
 * @return the closer direction of x to the wall
 * if x+ is closer, return 1, else return 0
 * */
int Lidar::compLaneDistance()
{
	int ret = 0;
	static  uint32_t seq = 0;
	double x,y,th,x1,y1;
	int angle_from, angle_to;
	double x_front_min = 4;
	double x_back_min = 4;

	scanLinear_mutex_.lock();
	auto tmp_scan_data = lidarScanData_linear_;
	scanLinear_mutex_.unlock();

	if (tmp_scan_data.header.seq == seq) {
		//ROS_WARN("lidar seq_ still same, quit!seq_ = %d", tmp_scan_data.header.seq_);
		return 0;
	}
	ROS_INFO("compLaneDistance");
	seq = tmp_scan_data.header.seq;
	int cur_angle = robot::instance()->getWorldPoseAngle() / 10;
#if 0
	angle_from = 149 - cur_angle;
	angle_to = 210 - cur_angle;
	ROS_INFO("cur_angle = %d", cur_angle);
	for (int j = angle_from; j < angle_to; j++) {
		int i = j>359? j-360:j;
		if (tmp_scan_data.ranges[i] < 4) {
			th = i*1.0 + 180.0;
			x = cos(th * PI / 180.0) * tmp_scan_data.ranges[i];
			y = sin(th * PI / 180.0) * tmp_scan_data.ranges[i];
			x1 = x * cos(0 - cur_angle * PI / 180.0) + y * sin(0 - cur_angle * PI / 180.0);
			//ROS_INFO("x = %lf, y = %lf, x1 = %lf, y1 = %lf", x, y, x1, y1);
			if (fabs(y1) < ROBOT_RADIUS) {
				if (fabs(x1) <= x_front_min) {
					x_front_min = fabs(x1);
					//ROS_WARN("x_front_min = %lf", x_front_min);
				}
			}
		}
	}

	angle_from = 329 - cur_angle;
	angle_to = 400 - cur_angle;
	for (int j = angle_from; j < angle_to; j++) {
		int i = j>359? j-360:j;
		if (tmp_scan_data.ranges[i] < 4) {
			th = i*1.0 + 180.0;
			x = cos(th * PI / 180.0) * tmp_scan_data.ranges[i];
			y = sin(th * PI / 180.0) * tmp_scan_data.ranges[i];
			x1 = x * cos(0 - cur_angle * PI / 180.0) + y * sin(0 - cur_angle * PI / 180.0);
			y1 = y * cos(0 - cur_angle * PI / 180.0) - x * sin(0 - cur_angle * PI / 180.0);
			//ROS_INFO("x = %lf, y = %lf, x1 = %lf, y1 = %lf", x, y, x1, y1);
			if (fabs(y1) < ROBOT_RADIUS) {
				if (fabs(x1) <= x_back_min) {
					x_back_min = fabs(x1);
					//ROS_WARN("x_back_min = %lf", x_back_min);
				}
			}
		}
	}
	ROS_INFO("x_front_min = %lf, x_back_min = %lf", x_front_min, x_back_min);
#endif
//	ret = (x_front_min < x_back_min) ? 1 : 0;
	for (int i = 0; i < 360; i++) {
		if (tmp_scan_data.ranges[i] < 4) {
			th = i*1.0 + 180.0;
			x = cos(th * PI / 180.0) * tmp_scan_data.ranges[i];
			y = sin(th * PI / 180.0) * tmp_scan_data.ranges[i];
			coordinate_transform(&x, &y, LIDAR_THETA, LIDAR_OFFSET_X, LIDAR_OFFSET_Y);
			coordinate_transform(&x, &y, cur_angle * 10, 0, 0);
			//ROS_INFO("x = %lf, y = %lf", x, y);
			if (fabs(y) < ROBOT_RADIUS) {
				if (x >= 0){
					if (fabs(x) <= x_front_min) {
						x_front_min = fabs(x);
						ROS_WARN("x_front_min = %lf",x_front_min);
					}
				} else {
					if (fabs(x) <= x_back_min) {
						x_back_min = fabs(x);
						ROS_ERROR("x_back_min = %lf", x_back_min);
					}
				}
			}
		}
	}
	if(x_front_min < x_back_min)
		ret = 1;
	if(x_front_min > x_back_min)
		ret = -1;
	if(x_front_min == x_back_min)
		ret = 0;
	return ret;
}

/*
 * @author Alvin Xie
 * @brief make use of lidar to get the obstacle distance
 * @param dir:0-front 1-back 2-left 3-right
 *        range: dectect range
 * @return the distance to the obstacle
 * */
double Lidar::getObstacleDistance(uint8_t dir, double range)
{
	scanCompensate_mutex_.lock();
	if(!lidarCheckFresh(0.6,1)){
		scanCompensate_mutex_.unlock();
		return DBL_MAX;
	}
	double x,y;
	double x_to_robot,y_to_robot;
	double min_dis = DBL_MAX;

	if(range < 0.056)
	{
		ROS_ERROR("range should be higher than 0.056");
		return 0;
	}

	for(auto point:lidarXY_points){
		x = point.x;
		y = point.y;
		x_to_robot = fabs(x) - ROBOT_RADIUS * sin(acos(fabs(y) / ROBOT_RADIUS));
		y_to_robot = fabs(y) - ROBOT_RADIUS * sin(acos(fabs(x) / ROBOT_RADIUS));
		//ROS_INFO("x = %lf, y = %lf", x, y);
		if (dir == 0) {
			if(fabs(y) < range){
				if(x > 0){
					if (x_to_robot < min_dis) {
						min_dis = x_to_robot;
						//ROS_WARN("back = %lf", back);
						}
					}
			}
		} else if (dir == 1) {
				if (fabs(y) < range) {
					if (x < 0){
						if (x_to_robot < min_dis) {
							min_dis = x_to_robot;
							//ROS_WARN("back = %lf", back);
						}
					}
				}
		} else if (dir == 2) {
				if (fabs(x) < range) {
					if (y >= 0 && x > 0){
						if (y_to_robot < min_dis) {
							min_dis = y_to_robot;
							//ROS_WARN("left = %lf",left);
						}
					}
				}
		} else if (dir == 3) {
				if (fabs(x) < range) {
					if (y < 0 && x > 0){
						if (y_to_robot < min_dis) {
							min_dis = y_to_robot;
							//ROS_WARN("right = %lf",right);
						}
					}
				}
		}
	}
	scanCompensate_mutex_.unlock();
	return min_dis;
}

bool Lidar::lidarCheckFresh(float duration, uint8_t type)
{
	double time_gap;
	if (type == 1 && time_gap < duration)
		time_gap = ros::Time::now().toSec() - scanLinear_update_time;
	if (type == 2)
		time_gap = ros::Time::now().toSec() - scanOriginal_update_time;

	if (time_gap < duration)
	{
		//ROS_INFO("%s %d: type:%d, time_gap(%lf) < duration(%f).", __FUNCTION__, __LINE__, type, time_gap, duration);
		return true;
	}

	//ROS_INFO("%s %d: type:%d, time_gap(%lf), duration(%f).", __FUNCTION__, __LINE__, type, time_gap, duration);
	return false;
}

bool Lidar::openTimeOut()
{
	auto time_diff = time(NULL) - open_command_time_stamp_;
	//ROS_INFO("%s %d: Time diff:%d", __FUNCTION__, __LINE__, time_diff);
	if (time_diff > 8)
	{
		motorCtrl(OFF);
		ROS_ERROR("%s %d: Lidar Open time out.", __FUNCTION__, __LINE__);
		ev.fatal_quit = true;
		error.set(ERROR_CODE_LIDAR);
		return true;
	}

	return false;
}

//
//void Lidar::pubPointMarker(std::vector<Vector2<double>> *point)
//{
//	int points_size;
//	visualization_msgs::Marker point_marker;
//	point_marker.ns = "point_marker";
//	point_marker.id = 0;
//	point_marker.type = visualization_msgs::Marker::SPHERE_LIST;
//	point_marker.action= 0;//add
//	point_marker.lifetime=ros::Duration(0);
//	point_marker.scale.x = 0.05;
//	point_marker.scale.y = 0.05;
//	point_marker.scale.z = 0.05;
//	point_marker.color.r = 0.0;
//	point_marker.color.g = 1.0;
//	point_marker.color.b = 0.0;
//	point_marker.color.a = 1.0;
//	point_marker.header.frame_id = "/base_link";
//	point_marker.header.stamp = ros::Time::now();
//	laser_points_.x = 0.0;
//	laser_points_.y = 0.0;
//	laser_points_.z = 0.0;
//
//	if (!(*point).empty()) {
//		for (std::vector<Vector2<double>>::iterator iter = (*point).begin(); iter != (*point).end(); ++iter) {
//			laser_points_.x = iter->x;
//			laser_points_.y = iter->y;
//			point_marker.points.push_back(laser_points_);
//		}
//		point_marker_pub.publish(point_marker);
//		point_marker.points.clear();
//	} else {
//		point_marker.points.clear();
//		point_marker_pub.publish(point_marker);
//	}
//}

void Lidar::setLidarScanDataOriginal(const sensor_msgs::LaserScan::ConstPtr &scan) {
	boost::mutex::scoped_lock(scan2_mutex_);
	lidarScanData_original_ = *scan;
}

sensor_msgs::LaserScan Lidar::getLidarScanDataOriginal() {
	boost::mutex::scoped_lock(scan2_mutex_);
	return lidarScanData_original_;
}

bool lidar_is_stuck()
{
	if (lidar.isScanOriginalReady() && !lidar.lidarCheckFresh(4, 2))
		return true;
	return false;
}

uint8_t lidar_get_status()
{
	if (lidar.isScanOriginalReady())
		return lidar.lidarMarker(0.20);

	return 0;
}

uint8_t lidar_is_robot_slip()
{
	uint8_t ret = 0;
	if(lidar.isScanOriginalReady() && lidar.isRobotSlip()){
		ROS_INFO("\033[35m""%s,%d,robot slip!!""\033[0m",__FUNCTION__,__LINE__);
		ret = 1;
	}
	return ret;
}


#include <termios.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <ros/ros.h>

#include "serial.h"

#ifndef CR_PORT
#define CR_PORT			"/dev/ttyS3"
#endif

#define	TAG			"Ser. (%d):\t"

static int	crport_fd = -1;
//static int	msg_id = 0;
static bool serial_init_done = false;

static struct termios	orgopt, curopt;

static int _bardrate;
extern double rowCount, columnCount;

void serial_init(const char* port,int baudrate) {
	char buf[1024];

	if( crport_fd != -1 )
		return;	
	_bardrate = baudrate;
	sprintf(buf, "%s", port);
	crport_fd = open(buf, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (crport_fd == -1)
		return;

	fcntl(crport_fd, F_SETFL, FNDELAY);

	tcgetattr(crport_fd, &orgopt);
	tcgetattr(crport_fd, &curopt);
	speed_t CR_BAUDRATE = 115200;
	switch(baudrate){
		case 9600:
			CR_BAUDRATE = B9600;
			break;
		case 19200:
			CR_BAUDRATE = B19200;
			break;
		case 38400:
			CR_BAUDRATE = B38400;
			break;
		case 57600:
			CR_BAUDRATE = B57600;
			break;
		case 115200:
			CR_BAUDRATE = B115200;	
			break;
		case 230400:
			CR_BAUDRATE = B230400;
			break;
		default:
			break;
			
	}
	cfsetispeed(&curopt, CR_BAUDRATE);
	cfsetospeed(&curopt, CR_BAUDRATE);	
	/* Mostly 8N1 */
	curopt.c_cflag &= ~PARENB;
	curopt.c_cflag &= ~CSTOPB;
	curopt.c_cflag &= ~CSIZE;
	curopt.c_cflag |= CS8;
	curopt.c_cflag |= CREAD;
	curopt.c_cflag |= CLOCAL;	//disable modem statuc check

	cfmakeraw(&curopt);		//make raw mode

	if (tcsetattr(crport_fd, TCSANOW, &curopt) == 0){
		serial_init_done = true;
		ROS_INFO("\033[32mserial.cpp\033[0m init done...\n");
	}
}

bool is_serial_ready(){
	return serial_init_done;
}		
int serial_flush(){
	return tcflush(crport_fd,TCIFLUSH);
}
int serial_close()
{
	char buf[128];
	int reval;
	if (crport_fd == -1)
		return -1;

	read(crport_fd, buf, 128);
	tcsetattr(crport_fd, TCSANOW, &orgopt);
	reval = close(crport_fd);
	if(reval==0){
		crport_fd = -1;
		serial_init_done = false;
	}
	return reval;
}

int serial_write(uint8_t len, uint8_t *buf)
{
	int	retval;
	retval = write(crport_fd, buf, len);
	return retval;
}

int serial_read(int len,uint8_t *buf)
{
	int r_ret=0,s_ret=0;
	uint8_t *t_buf;
	t_buf = (uint8_t*)calloc(len,sizeof(uint8_t));
	//memset(t_buf,0,len);
	fd_set read_fd_set;
	struct timeval timeout;
	timeout.tv_sec = 4;
	timeout.tv_usec = 0;// ms
	size_t length = 0;
	if(is_serial_ready()){
		if(ioctl(crport_fd,FIONREAD,&length)==-1)
		{
			ROS_WARN("%s,%d,ioctl return -1",__FUNCTION__,__LINE__);
			free(t_buf);
			return -1;
		}
		if( length >= (size_t)len){
			r_ret = read(crport_fd,t_buf,len);
			if(r_ret == len)
				for(int i =0;i<len;i++){
					buf[i] = t_buf[i];
				}
				//memcpy(buf,t_buf,len);
			free(t_buf);
			return r_ret;
		}
	}
	while (is_serial_ready()){
		FD_ZERO(&read_fd_set);
		FD_SET(crport_fd,&read_fd_set);

		s_ret = select(FD_SETSIZE,&read_fd_set,NULL,NULL,&timeout);
		if (s_ret <0){
			ROS_ERROR("%s %d: -------select error------------", __FUNCTION__, __LINE__);
			free(t_buf);
			FD_CLR(crport_fd,&read_fd_set);
			return -1;
		}
		else if(s_ret ==0){
			ROS_INFO("%s %d: select function \033[33mtimeout!\033[0m", __FUNCTION__, __LINE__);
			free(t_buf);
			FD_CLR(crport_fd,&read_fd_set);
			return 0;
		}
		else if(s_ret >0){
			if(FD_ISSET(crport_fd,&read_fd_set)){
				if(ioctl(crport_fd,FIONREAD,&length)==-1)
				{
					ROS_WARN("%s,%d,ioctl return -1",__FUNCTION__,__LINE__);
					free(t_buf);
					FD_CLR(crport_fd,&read_fd_set);
					return -1;
				}
				if(length>= (size_t)len){
					r_ret = read(crport_fd,t_buf,len);
					if(r_ret == len)
						for(int i =0;i<len;i++){
							buf[i] = t_buf[i];
						}
						//memcpy(buf,t_buf,len);
					free(t_buf);
					FD_CLR(crport_fd,&read_fd_set);
					return r_ret;
				}
				else{
					int time_remain = timeout.tv_sec*1000000 + timeout.tv_usec;
					int time_expect = (len - length)*1000000*8/_bardrate;
					if(time_remain > time_expect)
						usleep(time_expect);
				}
			}
		}
	}
	free(t_buf);
	return s_ret;
}

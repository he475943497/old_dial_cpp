#include "Dial.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <signal.h>  
#include "Agent.h"
#include "Dial_server.h"
#include "Dial_hash.h"
#include "Dial_mode.h"
#include "Dial_thread_pool.h"
#include "Dial_queue.h"
#include "Dial_icmp.h"
#include "Dial_common.h"
#include "Dial_dns.h"
#include "Dial_ssl.h"
#include "version.h"
#include <clib/daemon.h>

int main(int argc, char **argv) 
{
	extern bool s_debug_switch;
	extern bool thread_exit_flag;
	extern threadpool_t tp;
	extern dial_cfg_t g_cfg;

	int rtn = NO_ERROR;
	pthread_t td1,td2,td3;
	pthread_t td_register;
	bool need_daemon = false;

	if(argc >= 2)
	{
		for(int i = 1; i < argc; i++)
		{
			if (strcmp("start", argv[i]) == 0) 
			{
				need_daemon = true;
				continue;
			}
			else if (strcmp("stop", argv[i]) == 0) 
			{
				daemon_stop();
				exit(0);
			}
			else if (strcmp("restart", argv[i]) == 0) 
			{
				daemon_stop();
				need_daemon = true;
				continue;
			}
			else if(strcmp("-v", argv[i]) == 0)
			{
				printf("%s",DIAL_VERSION);
				return -1;
			} 
			else 
			{
				printf("unkown param:%s\n",argv[i]);
				return -1;
			}
		}
	}
	else
	{
		printf("usage:dial start|stop|restart\n");
		return -1;
	}

	rtn = sys_init();
	if(rtn != NO_ERROR) 
	{
		printf("process start failed!\n");
		return ERROR;
	}

	if(need_daemon)
	{
		daemon_start(1);
	}

	s_debug_switch = true;
	thread_exit_flag = true;

	rtn = signal_init();
	if(rtn != NO_ERROR) 
	{
		cfg_debug_printf(LOG_LEVEL_ERROR,"sys_init: failed!!\n");
		return ERROR;
	}

	rtn = sys_log_timer_init();
	if(rtn != NO_ERROR) 
	{
		cfg_debug_printf(LOG_LEVEL_ERROR,"sys_log_timer_init: failed!!\n");
		return ERROR;
	}

	set_core_file();

	rtn = threadpool_init(&tp,THREAD_POOL_NUM);
	if(0 != rtn) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"sys_init:threadpool_init  failed!!\n");
		return ERROR;
	}

	if ((rtn = pthread_create(&td1,NULL,dial_monitor_thread,NULL))!=0) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"main:pthread_create failed!dial_monitor_thread,rtn = %d\n",rtn);
		return ERROR;
	}

	if ((rtn = pthread_create(&td2,NULL,dial_monitor_thread2,NULL))!=0) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"main:pthread_create failed!dial_monitor_thread2,rtn = %d\n",rtn);
		return ERROR;
	}

	if ((rtn = pthread_create(&td3,NULL,dial_monitor_thread3,NULL))!=0) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"main:pthread_create failed!dial_monitor_thread3,rtn = %d\n",rtn);
		return ERROR;
	}

	if ((rtn = pthread_create(&td3,NULL,dial_monitor_queue_thread,NULL))!=0) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"main:pthread_create failed!dial_monitor_queue_thread,rtn = %d\n",rtn);
		return ERROR;
	}	


	if ((rtn = pthread_create(&td_register,NULL,start_register_thread,NULL))!=0) 
	{
		cfg_debug_printf(LOG_LEVEL_BASIC,"main:pthread_create failed!start_register_thread,rtn = %d\n",rtn);
		return ERROR;
	}			

	cfg_debug_printf(LOG_LEVEL_BASIC,"*****************dial module start ***********\n");

	thrift_init(g_cfg.dial_port);

	cfg_debug_printf(LOG_LEVEL_BASIC,"*****************dial module over,thrift died! ***********\n");

	thread_exit_flag = false;
	sleep(5);
	sys_free();
	pthread_join(td1,NULL);
	pthread_join(td2,NULL);
	pthread_join(td3,NULL);
	pthread_join(td_register,NULL);

	return NO_ERROR;
}



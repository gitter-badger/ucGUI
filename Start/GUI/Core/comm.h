#ifndef __COMM_H__
#define __COMM_H__


#define		DEBUG_ON		1

extern		int logx (const char *fmt,...) ;

#if			DEBUG_ON
	#define		XDEBUG	logx
#else
	#define		XDEBUG	
#endif

#endif
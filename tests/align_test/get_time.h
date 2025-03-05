#ifndef __GET_TIME__
#define __GET_TIME__

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include "debug_print.h"

#ifdef WIN32

#define TINIT \
	LARGE_INTEGER nFreq, t1, t2; \
	double time;   

#define TIC \
    QueryPerformanceFrequency(&nFreq); \
    QueryPerformanceCounter(&t1);   

#define TOC(x) \
    QueryPerformanceCounter(&t2); \
    time = (double)(t2.QuadPart - t1.QuadPart)* 1000 / (double)nFreq.QuadPart; \
    Debug_info("%s costs %f ms\n", x, time);

#else

#define TINIT \
    struct timeval start, stop; \
    double time;

#define TIC \
    gettimeofday(&start, NULL); 

#define TOC(x) \
    gettimeofday(&stop, NULL); \
    time = (stop.tv_sec - start.tv_sec) * 1000. + \
    (stop.tv_usec - start.tv_usec) / 1000.; \
    Debug_info("%s costs %f ms\r\n", x, time);

#endif


#endif
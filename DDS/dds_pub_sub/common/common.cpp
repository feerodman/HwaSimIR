#include "common.h"
#include <sys/types.h>
#include <stdio.h>
#include <sys/vfs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <stdlib.h>

/*
获取文件大小
*/
long long get_fileSize( const char *filename )
{
    struct stat64 statbuf;
    int           ret = stat64( filename, &statbuf );
    if ( ret )
        return -1;

    return statbuf.st_size;
}

unsigned long long get_time_us()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long long usec = (long long)ts.tv_sec*1000000+ts.tv_nsec/1000;
	return usec;
}
unsigned long long get_time_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long long msec = (long long)ts.tv_sec*1000+ts.tv_nsec/1000000;
	return msec;
}

int diskFreeK( const char *mnt, unsigned int *blocksK, unsigned int *availK )
{
    struct statfs sfs;
    statfs( mnt, &sfs );
    int percent = ( sfs.f_bavail ) * 100 / ( sfs.f_blocks );

    if(blocksK) *blocksK    = sfs.f_blocks * 4;
    if(availK)  *availK     = sfs.f_bavail * 4;
    // printf("/            %ld    %ld  %ld   %d%% /home\n",
    //         4*sfs.f_blocks,
    //         4*(sfs.f_blocks - sfs.f_bfree),
    //         4*sfs.f_bavail, percent);
    // system("df /mnt");
    // printf("\n%d\n",percent);

    return percent;
}
/*
获取时间戳字符串
*/
string getTimeStr()
{
    struct timeval  tv;
    struct timezone tz;
    struct tm      *t;

    gettimeofday( &tv, &tz );
    t = localtime( &tv.tv_sec );

    string s = string_format( "%04d%02d%02d_%02d%02d%02d",
                              1900 + t->tm_year,
                              1 + t->tm_mon,
                              t->tm_mday,
                              t->tm_hour,
                              t->tm_min,
                              t->tm_sec);

//    string s = string_format( "%04d%02d%02d_%02d%02d%02d_%03d",
//                              1900 + t->tm_year,
//                              1 + t->tm_mon,
//                              t->tm_mday,
//                              t->tm_hour,
//                              t->tm_min,
//                              t->tm_sec,
//                              tv.tv_usec / 1000 );

    return s;
}

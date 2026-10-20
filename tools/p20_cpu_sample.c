/* 只读短时CPU采样器：不修改调度/信号/目标代码，不依赖缺失的perf_5.10。 */
#define _GNU_SOURCE
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char** argv){
 if(argc!=3)return 2;int pid=atoi(argv[1]),seconds=atoi(argv[2]);if(pid<=0||seconds<1||seconds>40)return 2;
 struct perf_event_attr a;memset(&a,0,sizeof(a));a.size=sizeof(a);a.type=PERF_TYPE_SOFTWARE;a.config=PERF_COUNT_SW_CPU_CLOCK;a.freq=1;a.sample_freq=199;a.sample_type=PERF_SAMPLE_IP;a.disabled=1;a.exclude_kernel=1;a.exclude_hv=1;a.wakeup_events=1;
 int fd=syscall(__NR_perf_event_open,&a,pid,-1,-1,0);if(fd<0){perror("perf_event_open");return 3;}
 size_t page=sysconf(_SC_PAGESIZE),size=page*8;void* mapped=mmap(NULL,page+size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(mapped==MAP_FAILED){perror("mmap");close(fd);return 4;}
 struct perf_event_mmap_page* meta=mapped;unsigned char* data=(unsigned char*)mapped+page;uint64_t tail=0;double deadline=now()+seconds;
 ioctl(fd,PERF_EVENT_IOC_RESET,0);ioctl(fd,PERF_EVENT_IOC_ENABLE,0);
 while(now()<deadline){
  uint64_t head=__atomic_load_n(&meta->data_head,__ATOMIC_ACQUIRE);
  while(tail<head){struct perf_event_header h;unsigned char buf[256];for(size_t i=0;i<sizeof(h);++i)((unsigned char*)&h)[i]=data[(tail+i)%size];if(h.size<sizeof(h)||h.size>sizeof(buf)){fprintf(stderr,"invalid event size %u\n",h.size);return 5;}
   for(size_t i=0;i<h.size;++i)buf[i]=data[(tail+i)%size];if(h.type==PERF_RECORD_SAMPLE){uint64_t ip;memcpy(&ip,buf+sizeof(h),sizeof(ip));printf("%llx\n",(unsigned long long)ip);}else if(h.type==PERF_RECORD_LOST)fprintf(stderr,"sample lost\n");tail+=h.size;
  }
  __atomic_store_n(&meta->data_tail,tail,__ATOMIC_RELEASE);usleep(10000);
 }
 ioctl(fd,PERF_EVENT_IOC_DISABLE,0);munmap(mapped,page+size);close(fd);return 0;
}

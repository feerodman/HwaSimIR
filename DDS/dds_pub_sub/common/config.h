#ifndef CONFIG_h
#define CONFIG_h

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <ctype.h>

#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include <pthread.h>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string>

#include "common.h"

using namespace std;

#include "log.h"

#define STR1( R ) #R
#define STR( R )  STR1( R )

#define VER ( ( VerHi << 8 ) | VerLow )


#define BufSizeMax  (2560*2048)
#define UdpPackSizeMax  65535

#define ConfigJsonFile  "../settings.json"


#endif  // CONFIG_h

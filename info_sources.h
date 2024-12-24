#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>

#define CPU_COUNT 8

int get_shell(char* cmd, char* output, int len);
int get_time(char* unused, char* output, int len);
int get_cpu_util(char* unused, char* output, int len);
int get_cpu_util_bars(char* unused, char* output, int len);
int get_mem(char *unused, char *output, int len);
int get_disk(char *unused, char *output, int len);

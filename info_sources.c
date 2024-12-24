#include "info_sources.h"

int get_shell(char* cmd, char* output, int len)
{
    FILE *pipe = popen(cmd, "r");
    fgets(output, len, pipe);
    pclose(pipe);
    return 1;
}

int get_time(char* unused, char* output, int len)
{
    time_t t = time(NULL);
    strftime(output, len, "%F %T\n", localtime(&t));
    return 1;
};


int get_cpu_util(char* unused, char* output, int len)
{
	static long double a[7];
	long double b[7], sum;

	memcpy(b, a, sizeof(b));

	FILE *fp;
	const char *filename = "/proc/stat";

	if (!(fp = fopen(filename, "r")))
	{
		printf("Error from fopen '%s':", filename);
		return -1;
	}

	/* cpu user nice system idle iowait irq softirq */
	if (fscanf(fp, "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf",
	           &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6])
	    != 7)
	{
		snprintf(output, len, "n/a");
		fclose(fp);
		return 0;
	}

	fclose(fp);

	if (b[0] == 0)
	{
		snprintf(output, len, "n/a");
		return 1;
	}

	sum = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6]) -
	      (a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6]);

	if (sum == 0)
	{
		snprintf(output, len, "n/a");
		return 1;
	}

	int util = (int)(100 * ((b[0] + b[1] + b[2] + b[5] + b[6]) -
	                (a[0] + a[1] + a[2] + a[5] + a[6])) / sum);

	for (int i = 0; i < 8; i++)
	{
	    int bar = (util * 8 / 100) > i;
	    output[i] = bar ? '\xDB' : '\xA5';
	}   

	snprintf(&output[8], len, " %d%%\n", util);
	return 1;
}

int get_cpu_util_bars(char* unused, char* output, int len)
{
	static long double as[CPU_COUNT+1][7];
	long double bs[CPU_COUNT+1][7], sum[CPU_COUNT+1];

	memcpy(bs, as, sizeof(bs));

	FILE *fp;

	const char *filename = "/proc/stat";

	if (!(fp = fopen(filename, "r")))
	{
		printf("Error from fopen '%s':", filename);
		return -1;
	}

	for (int i = 0; i < CPU_COUNT+1; i++)
	{
	    int r = fscanf(fp, "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf %*d %*d %*d ",
		    &as[i][0], &as[i][1], &as[i][2], &as[i][3], 
		    &as[i][4], &as[i][5], &as[i][6]);
	    if (r != 7)
	    {
		snprintf(output, len, "n/a");
		fclose(fp);
		return 0;
	    }

	}

	fclose(fp);

	if (bs[0][0] == 0)
	{
		snprintf(output, len, "n/a");
		return 1;
	}

	for (int i = 0; i < CPU_COUNT+1; i++)
	{
	    sum[i] = (bs[i][0] + bs[i][1] + bs[i][2] + bs[i][3] 
	    	+ bs[i][4] + bs[i][5] + bs[i][6]) -
	          (as[i][0] + as[i][1] + as[i][2] + as[i][3] 
	           + as[i][4] + as[i][5] + as[i][6]);
	}

	if (sum[0] == 0)
	{
		snprintf(output, len, "n/a");
		return 1;
	}
	
	int n = 0;

	const char bars[4] = "_\x09\x0B\xDB";
//	const char bars[7] = "0123456";

	for (int i = 1; i < CPU_COUNT+1; i++) {
	    int util = (int)(3 *
	               ((bs[i][0] + bs[i][1] + bs[i][2] + bs[i][5] + bs[i][6]) -
	                (as[i][0] + as[i][1] + as[i][2] + as[i][5] + as[i][6])) / sum[i]);
	    snprintf(output+n++, len, "%c", bars[util]);
	}
	int total_util = (int)(100 * ((bs[0][0] + bs[0][1] + bs[0][2] + bs[0][5] + bs[0][6]) -
	                (as[0][0] + as[0][1] + as[0][2] + as[0][5] + as[0][6])) / sum[0]);

	snprintf(output + n, len, " %d%%\n", total_util);
	return 1;
}


int get_mem(char *unused, char *output, int len)
    
{
	unsigned int total, free, buffers, cached, used;

	FILE *fp;
	const char *filename = "/proc/meminfo";

	if (!(fp = fopen(filename, "r")))
	{
		printf("Error from fopen '%s':", filename);
		return -1;
	}

	int r = fscanf(fp,
	           "MemTotal: %u kB\n"
	           "MemFree: %u kB\n"
	           "MemAvailable: %u kB\n"
	           "Buffers: %u kB\n"
	           "Cached: %u kB\n",
	           &total, &free, &buffers, &buffers, &cached);

	fclose(fp);

	if (r != 5)
	{ 
	    snprintf(output, len, "n/a");
	    return 0;
	}

	used = (total - free - buffers - cached);
	int util = 100 * used / total;

	for (int i = 0; i < 8; i++)
	{
	    int bar = (util * 8 / 100) > i;
	    output[i] = bar ? '\xDB' : '\xA5';
	}   

	snprintf(&output[8], len, " %d%%\n", util);
	return 1;
}

int get_disk(char *unused, char *output, int len)
{
    struct statvfs fs;
    if (statvfs("/home", &fs) < 0)
    {
	snprintf(output, len, "n/a");
	return 0;
    }

    int util = 100 - 100 * fs.f_bavail / fs.f_blocks;

    for (int i = 0; i < 8; i++)
    {
        int bar = (util * 8 / 100) > i;
        output[i] = bar ? '\xDB' : '\xA5';
    }   

    snprintf(&output[8], len, " %d%%\n", util);
    return 1;
}

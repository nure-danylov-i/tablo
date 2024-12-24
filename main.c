#include <wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>

#include "info_sources.h"

#define CONFIG_LABEL_LENGTH 20
#define CONFIG_CMD_LENGTH 256
#define CONFIG_LINE_LENGTH CONFIG_LABEL_LENGTH+CONFIG_CMD_LENGTH+10
#define CONFIG_MAX_INTERVAL 86400

#define DEVICE_BUTTON_COUNT 4
#define DEVICE_LINE_COUNT 4
#define BUFFER_LENGTH 64

struct Line 
{
    int (*func)(char*, char*, int);
    char label[CONFIG_LABEL_LENGTH];
    char cmd[CONFIG_CMD_LENGTH];
    int interval;
};

struct Line lines[DEVICE_LINE_COUNT] = { 0 };

char buttons[DEVICE_BUTTON_COUNT][CONFIG_CMD_LENGTH] = { 0 };

volatile sig_atomic_t g_done = 0;

void sigchld_handler();
void sigterm_handler();
void sigusr1_handler();
int get_config_line(char *line, FILE *fp);
int parse_config(const char *path);
void print_config();
void filter_utf8(char *text);
void pull_sources(struct Line* line, char *output, int len);
int handle_button(unsigned char c);
void *recieve_loop(void* serialPort);
void transmit_loop(int serialPort);
int init_serial();

void sigchld_handler()
{
    wait(NULL);
}

void sigterm_handler()
{
    printf("\nGot SIGTERM! Terminating...\n");
    g_done = 1;
}

void sigusr1_handler()
{
    printf("\nGot SIGUSR1! Reloading configuration...\n");
    if (parse_config("config.txt"))
	print_config();
}

int get_config_line(char *line, FILE *fp)
{
    do
    {
	if (!fgets(line, CONFIG_LINE_LENGTH, fp))
    	{
    	    return 0;
    	}
	if (feof(fp))
	{
	    printf("Parsing error: unexpected end of file");
	}
    } while (line[0] == '\n' || line[0] == '\r' || line[0] == '#');

    return 1;
}

int parse_config(const char *path)
{
    int intervals[DEVICE_LINE_COUNT] = {0};
    char label[DEVICE_LINE_COUNT][CONFIG_LABEL_LENGTH] = {0};
    char cmd[DEVICE_LINE_COUNT][CONFIG_CMD_LENGTH] = {0};
    int (*func[DEVICE_LINE_COUNT])(char*, char*, int);

    FILE *fp;

    if (!(fp = fopen(path, "r")))
    {
    	printf("Error from fopen '%s':", path);
    	return 0;
    }

    for (int i = 0; i < DEVICE_LINE_COUNT; i++)
    {
	char config_line[CONFIG_LINE_LENGTH];

	if (!get_config_line(config_line, fp))
	{
	    printf("Parsing error: could not get line #%d\n", i+1);
	    fclose(fp);
	    return 0;
	}

	int n = 0;
	if (config_line[0] == '"' && config_line[1] == '"')
	{
	    n = 1 + sscanf(config_line, "%*2c %d %[^\n]", &intervals[i], cmd[i]);
	}
	else
	{
	    n = sscanf(config_line, "\"%[^\"] %*1c %d %[^\n]", label[i], &intervals[i], cmd[i]);
	}

    	if (n != 3)
    	{
	    char* error = "";
	    switch (n) {
		case 0:
		    error = "Invalid label"; break;
		case 1:
		    error = "No interval specified"; break;
		case 2:
		    error = "Invalid command"; break;
		default:
		    error = "Unknown error"; break;
	    }
    	    printf("Parsing error: %s at line #%d\n", error, i+1);
    	    fclose(fp);
    	    return 0;
    	}

	if (intervals[i] < 1 || intervals[i] > CONFIG_MAX_INTERVAL)
    	{
    	    printf("Config error: interval at line #%d out of range: %d\n", i+1, intervals[i]);
    	    fclose(fp);
    	    return 0;
    	}

	if (cmd[i][0] == '%')
	{
	    if (strcmp(cmd[i], "%time") == 0)
    	       func[i] = get_time;
	    else if (strcmp(cmd[i], "%cpu_cores") == 0)
    	       func[i] = get_cpu_util_bars;
	    else if (strcmp(cmd[i], "%cpu") == 0)
    	       func[i] = get_cpu_util;
	    else if (strcmp(cmd[i], "%mem") == 0)
    	       func[i] = get_mem;
	    else if (strcmp(cmd[i], "%disk") == 0)
    	       func[i] = get_disk;
	    else
	    {
		printf("Config error: unknown preset source '%s' at line #%d\n", cmd[i], i+1);
    	    	fclose(fp);
    	    	return 0;
	    }
	}
	else
	    func[i] = get_shell;
    }

    char button_funcs[DEVICE_BUTTON_COUNT][CONFIG_CMD_LENGTH] = { 0 };

    for (int i = 0; i < DEVICE_BUTTON_COUNT; i++)
    {
	char config_line[CONFIG_LINE_LENGTH];
	if (!get_config_line(config_line, fp))
	{
	    printf("Parsing error: could not get funcion of button #%d\n", i+1);
	    fclose(fp);
	    return 0;
	}
	sscanf(config_line, "%[^\n]\n", button_funcs[i]);
    }

    fclose(fp);

    // copy config over

    for (int i = 0; i < DEVICE_LINE_COUNT; i++)
    {
	lines[i].func = func[i];
	lines[i].interval = intervals[i];
	strcpy(lines[i].label, label[i]);
	strcpy(lines[i].cmd, cmd[i]);
    }

    for (int i = 0; i < DEVICE_BUTTON_COUNT; i++)
    {
	strcpy(buttons[i], button_funcs[i]);
    }
 
    return 1;
}

void print_config()
{
    printf("\nPrinting configuration\nLines:\n");
    for (int i = 0; i < DEVICE_LINE_COUNT; i++)
    {
	printf("- Line #%d: '%s' %s (every %ds)\n", i+1, lines[i].label, lines[i].cmd, lines[i].interval);
    }

    printf("\nButtons:\n");
    for (int i = 0; i < DEVICE_BUTTON_COUNT; i++)
    {
	printf("- Button #%d: '%s'\n", i+1, buttons[i]);
    }
    printf("\n");
}

void test(const char *text)
{
    for (int i = 0; i < BUFFER_LENGTH; i++)
    {
	if (text[i] == '\0')
	    printf("@");
	else if (text[i] == '\n')
	    printf("~");
	else
	    printf("%c", text[i]);
    }
    printf("\n");
}

void filter_utf8(char *text)
{
    int len = strlen(text);
    for (int i = 0; i < len; i++)
    {
	if (text[i] == (char)0xC2)
	    if (text[i+1] == (char)0xB0) {
		text[i] = ' ';
		text[i+1] = (char)0xDF;
	    }
    }
}

void pull_sources(struct Line* line, char *output, int len)
{
    int len_label = strlen(line->label);
    if (len_label > 0) strcpy(output+1, line->label);

    line->func(line->cmd, output+1+len_label, BUFFER_LENGTH-1-len_label);

    int len_out = strlen(output);
    if (output[len_out - 1] != '\n')
    {
	if (len_out == BUFFER_LENGTH - 1) {
	    output[len_out - 1] = '\n';
	    output[len_out] = '\0';
	}
	else {
	    output[len_out] = '\n';
	    output[len_out+1] = '\0';
	}
    }
}

int handle_button(unsigned char c)
{
    c = c - '1';
    if (c < 0 || c >= DEVICE_BUTTON_COUNT)
    {
	return 0;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
	system(buttons[c]);
	exit(0);
    }

    return 0;
}

void *recieve_loop(void* serialPort)
{
    printf("Recieving thread started\n");
    while (1)
    {
	unsigned char readbuf[BUFFER_LENGTH];
	memset(&readbuf, '\0', sizeof(readbuf));
	int nb = read(*(int *)serialPort, &readbuf, sizeof(readbuf));

	if (nb < 0)
	    printf("Error reading from serial\n");

	handle_button(readbuf[0]);
    }

    return 0;
}

void transmit_loop(int serialPort)
{
    int count = 0;
    while (!g_done) {

	for (int i = 0; i < 4; i++)
	{   
	    if (lines[i].interval == 0 || count % lines[i].interval != 0)
		continue;

	    char buffer[BUFFER_LENGTH] = { 0 };
	    buffer[0] = i+48;
	    pull_sources(&lines[i], buffer, BUFFER_LENGTH);
	    filter_utf8(buffer);
	    write(serialPort, buffer, strlen(buffer));
	}

	count++;
	sleep(1);
    }
}

int init_serial()
{
    int serialPort = open("/dev/ttyUSB0", O_RDWR);
    if (serialPort < 0)
    {
	printf("Error %i from open: %s\n", errno, strerror(errno));
	return -1;
    }

    struct termios tty;

    if(tcgetattr(serialPort, &tty))
    {
	printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
	return -1;
    }

    tty.c_cflag &= ~PARENB; // Вимкнути біт парності
    tty.c_cflag &= ~CSTOPB; // Використовувати один біт зупинки
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; // Встановити 8 бітів на байт
    tty.c_cflag &= ~CRTSCTS; // Вимкнути апаратний контроль передачі
    tty.c_cflag |= CREAD | CLOCAL; // Увімкнути READ та ігнорувати рядки контролю
    
    tty.c_lflag &= ~ICANON; // Вимкнути канонічний режим
    // Вимкнути відлуння (ECHO)
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    // Вимкнути сигнальні символи
    tty.c_lflag &= ~ISIG;

    // Вимкнути апаратний контроль передачі
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    // Вимкнути спеціальну обробку даних при прийманні
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    // Вимкнути спеціальну обробку даних при передачі
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // При зчитуванні, read() має очікувати 1 байт даних
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 1;

    // Встановити швидкість обміну 9600 бод
    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);

    if (tcsetattr(serialPort, TCSANOW, &tty))
    {
	printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
	return -1;
    }

    return serialPort;
}

int main(int argc, char *argv[])
{
    if (!parse_config("config.txt"))
    {
        printf("Failed to parse config\n");
        return 1;
    }
    print_config();

    for (int i = 0; i < argc; i++)
    {
    	if (!strcmp("-d",argv[i]))
	{
	    daemon(0, 0);
	    continue;
	}
    }

    int serialPort = init_serial();

    if (serialPort < 0)
	return 1;

    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGUSR1, sigusr1_handler);

    pthread_t threadRecieve;

    pthread_create(&threadRecieve, NULL, recieve_loop, &serialPort);

    transmit_loop(serialPort);

    pthread_cancel(threadRecieve);

    close(serialPort);

    printf("Goodbye\n");
    return 0;
}

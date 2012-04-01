/* sunnylogpub.c */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>

#include "libyasdimaster.h"
#include "udpmonitor_client.h"

extern SHARED_FUNCTION BOOL yasdiSetDriverOnline(DWORD);
extern SHARED_FUNCTION void yasdiSetDriverOffline(DWORD);

static int exit_commanded = 0;
static int daemonised = 0;
static FILE *logfh;
static int eTotalIndex = -1;
static char todaysDate[40];

static void fatal(char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    if (!daemonised)
    {
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
    else
    {
        vsyslog(LOG_WARNING, fmt, ap); 
    }
    va_end(ap);
    sleep(1);
    exit(EXIT_FAILURE);
}

static char *mktimestamp(void)
{
    static char stampbuff[100];
    time_t tnow;
    struct tm *now;
    
    tnow = time(0);
    now = localtime(&tnow);
    strftime(stampbuff, sizeof stampbuff, "%Y%m%d%H%M%S", now);
    return stampbuff;
}

void signal_handler(int sig)
{
    switch(sig) {
    case SIGHUP:
        syslog(LOG_INFO, "hangup signal caught");
        break;
    case SIGTERM:
        exit_commanded = 1;
        syslog(LOG_WARNING, "terminate signal caught");
        exit(EXIT_SUCCESS);
        break;
    }
}

static void daemonise(void)
{
    int i, lfp;
    char str[10];

    if (getppid() == 1)
        return; /* already a daemon */
    i = fork();
    if (i < 0)
       fatal("fork error");
    if (i > 0)
        exit(EXIT_SUCCESS); /* parent exits */
    /* child (daemon) continues */
    if (setsid() < 0) /* obtain a new process group */
        fatal("setsid error");
    daemonised = 1;
    for (i = getdtablesize(); i >= 0; --i)
        close(i); /* close all descriptors */
    i = open("/dev/null", O_RDWR);
    dup(i); dup(i); /* handle standard I/O */
    umask(022); /* set newly created file permissions */
    chdir("/home/SunnyData"); /* change running directory */
    lfp = open("/var/run/sunnylog.pid", O_RDWR|O_CREAT, 0640);
    if (lfp < 0)
       fatal("can not open lockfile");
    if (lockf(lfp,F_TLOCK,0)<0)
        fatal("can not lock");
    /* first instance continues */
    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); /* record pid to lockfile */
    signal(SIGCHLD, SIG_IGN); /* ignore child */
    signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler); /* catch hangup signal */
    signal(SIGTERM, signal_handler); /* catch kill signal */
}

static void appendbuff(char *buff[], size_t buffsiz,  char **bufp, char *fmt, ...)
{
    va_list ap;
    int written;
    
    va_start(ap, fmt);
    written = vsnprintf(*bufp, buffsiz - (*bufp - *buff), fmt, ap);
    va_end(ap);
    *bufp += written;
}

/* Maintain a file with the energy total */
static void updateDailyTotalFile(float value, char *timestamp)
{
    FILE *fh;
    char namebuff[80];
    
    snprintf(namebuff, sizeof namebuff, "dailyenergy-%s", timestamp);
    if ((fh = fopen(namebuff, "w")) == NULL)
    {
        syslog(LOG_WARNING, "Cannot update daily total, '%s'", strerror(errno));
        return;
    }
    fprintf(fh, "%g\n", value);
    fclose(fh);
}

static void acq_loop(int handlecount, DWORD DeviceHandle, DWORD *ChannelHandles)
{
    char *dp;
    char namebuff[100];
    char databuff[1024];
    char *dbp = databuff;
    char *timestamp;
    int i;
    int res;
    double value;

    for (;;)
    {
        dp = databuff;
        timestamp = mktimestamp();
        if (strncmp(todaysDate, timestamp, 6) != 0)
        {
            syslog(LOG_INFO, "End of day, exiting");
            return;
        }
        appendbuff(&dbp, sizeof databuff, &dp, "%s,", timestamp);
        for (i=0; i < handlecount; ++i)
        {
            if (exit_commanded)
                return;
            if ((res = GetChannelValue(ChannelHandles[i], DeviceHandle, 
                  &value, namebuff, sizeof namebuff, 0)) < 0)
            {
                syslog(LOG_INFO, "Channel %d returned %d", i, res);
                break;
            }
            if (i == eTotalIndex)
                updateDailyTotalFile(value, timestamp);
            appendbuff(&dbp, sizeof databuff, &dp, "%g,%s,", value, namebuff);
        }
        appendbuff(&dbp, sizeof databuff, &dp, "\n");
        if (strlen(databuff) < 30)
            continue; //No data received
        fputs(databuff, logfh);
        fflush(logfh);
        send_udpmonitor("SunnyPV", databuff);
        sleep(30);
    }
}

int main(int argc, char **argv)
{
    struct tm *now;
    time_t tnow;
    int i;
    int res;
    DWORD dDriverNum = 0;          /* BusDriverCount             */
    #define MAXDRIVERS 10
    DWORD Driver[MAXDRIVERS];
    int driverOnline = 0;
    DWORD DeviceHandle[10]; /* place for 10 device ID's   */
    int handlecount;
    DWORD ChannelHandles[100];
    char namebuff[100];
    char databuff[1024];

    if (argc < 2)
        fatal("No logfile specified");
    if (strstr(argv[0], "sunnylogpubd"))
        daemonise();
    tnow = time(0);
    now = localtime(&tnow);
    strftime(todaysDate, sizeof todaysDate, "%Y%m%d", now);
    snprintf(databuff, sizeof databuff, "%s-%s", argv[1], todaysDate);
    if ((logfh = fopen(databuff, "a")) == NULL)
        fatal("Could not append to '%s'", databuff);
    #define INI_FILE "/etc/yasdi.ini"
    if (yasdiMasterInitialize(INI_FILE, &dDriverNum) != 0)
        fatal("INI file '%s' not found", INI_FILE);
    if (0 == dDriverNum)
        fatal("No drivers");
    openlog("sunnylog", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "daemonised");
    dDriverNum = yasdiMasterGetDriver(Driver, MAXDRIVERS);
    for (i=0; i < dDriverNum; i++)
        if (!yasdiSetDriverOnline(Driver[i]))
            syslog(LOG_WARNING, "Could not set interface %d online\n", i);
        else
            ++driverOnline;
    if (!driverOnline)
        fatal("No drivers online");
    res = DoStartDeviceDetection(1, TRUE);
    switch(res)
    {
        case YE_OK:
            break;
        case YE_DEV_DETECT_IN_PROGRESS:
            fatal("already running device detection");
            break;
        case YE_NOT_ALL_DEVS_FOUND:
            fatal("not all devices were found.");
            break;
        default:
            fatal("unexpected return %d from DoStartDeviceDetection", res);
            break;
    }
    if ((handlecount = GetDeviceHandles(DeviceHandle, 10)) < 1)
        fatal("Device handle not found");
    handlecount = GetChannelHandlesEx(
        DeviceHandle[0], ChannelHandles, sizeof ChannelHandles, ALLCHANNELS);
    if (ftell(logfh) == 0)
    {
        for (i=0; i < handlecount; ++i)
        {
            if (GetChannelName(ChannelHandles[i], namebuff, sizeof namebuff) 
                    != 0)
                syslog(LOG_WARNING, "Bad channel %d\n", ChannelHandles[i]);
            else
                fprintf(logfh, "%s,", namebuff);
            if (strcmp(namebuff, "E-Total") == 0)
                eTotalIndex = i;
        }
        fprintf(logfh, "\n");
    }
    syslog(LOG_INFO, "Starting monitoring");
    acq_loop(handlecount, DeviceHandle[0], ChannelHandles);
    fclose(logfh);
    for(i=0; i<dDriverNum; i++)
    {
        yasdiSetDriverOffline(Driver[i]);
    }
    yasdiMasterShutdown();
    return EXIT_SUCCESS;
}

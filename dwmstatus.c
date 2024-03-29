/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <alsa/asoundlib.h>

#include <X11/Xlib.h>

char *tzberkeley = "America/Los_Angeles";
char *tznewyork = "America/New_York";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
get_loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
get_vol(void)
{
    FILE *fp;
    char path[5];
    /* Open the command for reading. */
    fp = popen("/usr/bin/amixer get Master | awk -F'[][]' 'END{ print $2 }'", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }
    fgets(path, sizeof(path), fp);
	/* close */
    pclose(fp);

	return smprintf("%s", path);
}

char *
get_mem(void)
{
    FILE *fp;
    char path[6];
    /* Open the command for reading. */
    fp = popen("/usr/bin/free -h | awk '(NR==2){ print $3 }'", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }
    fgets(path, sizeof(path), fp);
	/* close */
    pclose(fp);

	return smprintf("%s", path);
}
char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return smprintf("%s", line);
}
char *
get_battery(char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char *
get_temperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
get_brightness(char *base, char *current)
{
	char *cu;

	cu = readfile(base, current);
	if (cu == NULL)
		return smprintf("");
	
	return smprintf("%02.0f%%", atof(cu) / 960);
}

char *
execscript(char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	memset(retval, 0, sizeof(retval));

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");
	retval[strlen(retval)-1] = '\0';

	return smprintf("%s", retval);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *bat;
	char *tny;
	char *tber;
	char *t0;
	char *bright;
     	char *vol;
	char *mem;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		avgs = get_loadavg();
		bat = get_battery("/sys/class/power_supply/BAT0");
		tny = mktimes("%H:%M", tznewyork);
		tber = mktimes("%b %d, %Y %H:%M:%S", tzberkeley);
		t0 = get_temperature("/sys/class/thermal/thermal_zone6", "temp");
		vol = get_vol();
		mem = get_mem();
		bright = get_brightness("/sys/class/backlight/intel_backlight", "brightness");
		status = smprintf("  %s %s |  %s |  %s |  %s |  %s | NY: %s | B: %s",
				t0, avgs, mem, bat, vol, bright, tny, tber);
		setstatus(status);

		free(t0);
		free(avgs);
		free(bat);
		free(tny);
		free(tber);
		free(vol);
		free(mem);
		free(status);
		free(bright);
	}

	XCloseDisplay(dpy);

	return 0;
}

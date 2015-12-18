/* The Intelligent Memory Management System (IMMS)
 * Copyright (C) 2015 Onur Ülgen
 *
 * This file is part of IMMS.
 *
 * IMMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * IMMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with IMMS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "perf.h"

#define	LOG_ERROR_PATH		IMMS_PATH "error-logs/"
#define	SPD					(24 * 60 * 60)

const char *imms_malloc_lib_names[] = {
    "System",
    "Hoard",
    "TCMalloc",
    "jemalloc"
};

/* malloc-less time functions imported from diet libc <http://www.fefe.de/dietlibc/> */
/* days per month -- nonleap! */
static int imms_isleap(int year)
{
	/* every fourth year is a leap year except for century years that are
	 * not divisible by 400. */
	return (!(year % 4) && ((year % 100) || !(year % 400)));
}

static struct tm* imms_gmtime_r(const time_t *timep, struct tm *r)
{
    const short spm[13] = {
        0,
        (31),
        (31 + 28),
        (31 + 28 + 31),
        (31 + 28 + 31 + 30),
        (31 + 28 + 31 + 30 + 31),
        (31 + 28 + 31 + 30 + 31 + 30),
        (31 + 28 + 31 + 30 + 31 + 30 + 31),
        (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31),
        (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
        (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
        (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
        (31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31),
    };
	time_t i;
	register time_t work = *timep % SPD;

	r->tm_sec = work % 60;
	work /= 60;
	r->tm_min = work % 60;
	r->tm_hour = work / 60;
	work = *timep / SPD;
	r->tm_wday = (4 + work) % 7;
	for (i = 1970; ; ++i) {
		register time_t k = imms_isleap(i) ? 366 : 365;
		if (work >= k)
			work -= k;
		else
			break;
	}
	r->tm_year = i - 1900;
	r->tm_yday = work;
	r->tm_mday = 1;
	if (imms_isleap(i) && (work > 58)) {
		if (59 == work)
			r->tm_mday = 2;  /* 29.2. */
		work -= 1;
	}
	for (i = 11; i && (spm[i] > work); --i);
	r->tm_mon = i;
	r->tm_mday += work - spm[i];

	return r;
}

static struct tm* imms_localtime_r(const time_t *t, struct tm *r)
{
	time_t tmp;
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv, &tz);
	tmp = *t - tz.tz_minuteswest * 60L;

	return imms_gmtime_r(&tmp, r);
}

char* imms_process_filename()
{
	static char procfilename[NAME_MAX + 1] = {0};
	static char initialised = 0;

    if (initialised != 3) {
		if (!__sync_fetch_and_or(&initialised, 1)) {
		    int fd, i;
            char c;

            fd = open("/proc/self/stat", O_RDONLY);
            if (-1 == fd)
                goto errret;
            do {
                if (read(fd, &c, 1) != 1) {
                    close(fd);
                    goto errret;
                }
            } while (c != '(');
            i = 0;
            do {
                if (read(fd, &c, 1) == 1) {
                    procfilename[i++] = c;
                } else {
                    close(fd);
                    goto errret;
                }
            } while ((c != ')') && (i != sizeof(procfilename)));
            procfilename[i - 1] = 0;
            close(fd);
			__sync_fetch_and_or(&initialised, 2);
		} else {
			while (1 == initialised)
				usleep(1000);
		}
	}

	return *procfilename ? procfilename : NULL;

errret:
    *procfilename = 0;
    initialised = 0;
    return NULL;
}

char* imms_process_filepath()
{
	static char procfilepath[PATH_MAX + 1] = {0};
	static char initialised = 0;
	ssize_t size;

    if (initialised != 3) {
		if (!__sync_fetch_and_or(&initialised, 1)) {
            if ((size = readlink("/proc/self/exe", procfilepath, sizeof(procfilepath) - 1)) == -1)
                *procfilepath = 0;
            else
                procfilepath[size] = 0;
			__sync_fetch_and_or(&initialised, 2);
		} else {
			while (1 == initialised)
				usleep(1000);
		}
	}

    return *procfilepath ? procfilepath : NULL;
}

bool imms_open_perf_log_file(char *szfile, const size_t len, const char *szdir)
{
    char *sz, filepath[PATH_MAX + 1], filename[NAME_MAX + 1], c;
    int fd, i = 0, j;

    sz = strrchr(szfile, '/');
    if (!sz++)
        return false;
    strcpy(filename, sz);
    do {
        snprintf(filepath, sizeof(filepath), "%s%s-%i", szdir, filename, i++);
        fd = open(filepath, O_RDONLY);
        if (-1 == fd)
            return false;
        j = 0;
        do {
            if (read(fd, &c, 1) == 1)
                filepath[j++] = c;
            else {
                j++;
                break;
            }
        } while ((c != '\n') && (j != sizeof(filepath)));
        close(fd);
        filepath[j ? j - 1 : 0] = 0;
    } while (strcmp(filepath, szfile));
    snprintf(szfile, len, "%s%s-%i", szdir, filename, i - 1);

    return true;
}

bool imms_make_log_file(const char *szdir, char *szfile, const size_t len, const bool bperflog)
{
    char *procfilepath = NULL;
	int fd;

	if (!szdir || !szfile)
		return false;
    if (!bperflog) {
        char *procfilename;
        char sztime[32];
        time_t t;
        struct tm dt;

        if (!(procfilename = imms_process_filename()) ||
            !(procfilepath = imms_process_filepath()))
            return false;
        time(&t);
        imms_localtime_r(&t, &dt);
        strftime(sztime, sizeof(sztime), "%Y.%m.%d-%H:%M:%S", &dt);
        snprintf(szfile, len, "%s%s-%s-%d", szdir, procfilename, sztime, getpid());
    } else {
        char *filename, filepath[PATH_MAX + 1];
        int i = 0;

        strcpy(filepath, szfile);
        procfilepath = filepath;
        filename = strrchr(filepath, '/');
        if (!filename++)
            return false;
        do {
            snprintf(szfile, len, "%s%s-%d", szdir, filename, i++);
        } while (!access(szfile, F_OK));
    }
	fd = creat(szfile, 0644);
	if (-1 == fd) {
	    *szfile = 0;
		return false;
	}
	if (procfilepath) {
        size_t len = strlen(procfilepath);
		if (write(fd, procfilepath, len) != len ||
            write(fd, "\n", 1) != 1) {
            close(fd);
            unlink(szfile);
            *szfile = 0;
            return false;
        }
	}
	close(fd);

	return true;
}

void imms_log_error(const char *sz)
{
	static char filename[PATH_MAX + 1] = {0};
	static sem_t mutex;
	static char initialised = 0;
	int fd;
	char sztime[32];
	time_t t;
	struct tm dt;

	if (!sz)
		return;
	if (initialised != 3) {
		if (!__sync_fetch_and_or(&initialised, 1))
			__sync_fetch_and_or(&initialised, sem_init(&mutex, 0, 1) == -1 ? 4 : 2);
		else
			while (1 == initialised)
				usleep(1000);
	}
	if (5 == initialised)
		return;
	sem_wait(&mutex);
	if (!filename[0])
		imms_make_log_file(LOG_ERROR_PATH, filename, sizeof(filename), false);
	if (!filename[0] || (-1 == (fd = open(filename, O_APPEND | O_WRONLY)))) {
		sem_post(&mutex);
		return;
	}
	time(&t);
	imms_localtime_r(&t, &dt);
	strftime(sztime, sizeof(sztime), "%d.%m.%Y %H:%M:%S", &dt);
	write(fd, sztime, strlen(sztime));
	write(fd, " - ", 3);
	write(fd, sz, strlen(sz));
	write(fd, "\n", 1);
	close(fd);
	sem_post(&mutex);
}

static void strrev(char *sz)
{
	int i, j, k, t;
	char c;

    if (!sz)
        return;
    i = strlen(sz);
    t = !(i % 2) ? 1 : 0;
    for (j = i - 1, k = 0; j > (i / 2 - t); j--) {
        c = sz[j];
        sz[j] = sz[k];
        sz[k++] = c;
    }
}

char* imms_itoa(long value, char *result, int base)
{
	char *sz = result;
	long quotient = value;

	/* check that the base if valid */
	if (base < 2 || base > 16) {
		*result = 0;
		return result;
	}
	do {
		*sz++ = "0123456789abcdef"[abs(quotient % base)];
		quotient /= base;
	} while (quotient);
	if (base == 16) {
		*sz++ = 'x';
		*sz++ = '0';
	}
	/* Only apply negative sign for base 10 */
	if (value < 0 && base == 10)
		*sz++ = '-';
	*sz = 0;
	strrev(result);

	return result;
}

void imms_init_daemon(char *dname)
{
    char filename[PATH_MAX + 1];
    pid_t pid;
    int fd;

    if (setuid(0) == -1) {
        printf("This program must be run as root!\n");
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid)
        exit(EXIT_SUCCESS);
    umask(0);
    if (setsid() == -1) {
        imms_log_error("imms_init_daemon setsid error!");
        exit(EXIT_FAILURE);
    }
    if (chdir("/") == -1) {
        imms_log_error("imms_init_daemon chdir error!");
        exit(EXIT_FAILURE);
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    strcpy(filename, IMMS_LOCK_PATH);
    strcat(filename, dname);
    fd = open(filename, O_CREAT | O_RDWR, 0644);
    if (-1 == fd) {
        imms_log_error("imms_init_daemon open error!");
        exit(EXIT_FAILURE);
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == -1)
        exit(EXIT_SUCCESS);
}

bool imms_is_process_excluded()
{
    char *procfilepath;
    bool is_excluded = false;
    int fd;

    fd = open(IMMS_PATH "excluded-bins", O_RDONLY);
    if (-1 == fd) {
        imms_log_error("imms_is_process_excluded open error!");
        return false;
    }
    if (!(procfilepath = imms_process_filepath()))
        goto ret;
    for (;;) {
        char c; int i;
        for (i = 0, c = is_excluded = 1; c; i++) {
            switch (read(fd, &c, 1)) {
            case -1:
                is_excluded = false;
                goto ret;
            case 0:
                if (!i) {
                    is_excluded = false;
                    goto ret;
                }
                c = 0;
            }
            if (c == '\n' || c == '\r')
                c = 0;
            if (is_excluded == 1) {
                if (c == '*')
                    is_excluded = 2;
                else if (procfilepath[i] != c)
                    is_excluded = false;
            }
        }
        if (is_excluded)
            goto ret;
    }

ret:
    close(fd);
    return is_excluded;
}

/* Overflow-less average function with increment value */
/* Long double is used to handle 64-bit memory addresses */
long double imms_average_winc(long double avg, long double add, long double count, long double inc)
{
    return (avg * (count / (count + inc))) + (add / (count + inc));
}

long double imms_average(long double avg, long double add, long double count)
{
    return imms_average_winc(avg, add, count, 1);
}

void imms_share_info(imms_library_t lib, bool test_mode)
{
    key_t key = 'i' << 24 | 'm' << 16 | 'm' << 8 | 's';
    imms_shared_info_t *pshared_info;
    int segid;

    key += getpid();
    segid = shmget(key, sizeof(*pshared_info), IPC_CREAT | IPC_EXCL | 0644);
    if (-1 == segid)
        return;
    pshared_info = shmat(segid, NULL, 0);
    if (pshared_info == (void*)-1)
        return;
    pshared_info->lib = lib;
    pshared_info->test_mode = test_mode;
    shmdt(pshared_info);
}

imms_shared_info_t imms_read_shared_info(pid_t pid)
{
    key_t key = 'i' << 24 | 'm' << 16 | 'm' << 8 | 's';
    imms_shared_info_t shared_info = {0}, *pshared_info;
    int segid;

    key += pid;
    segid = shmget(key, sizeof(*pshared_info), S_IRUSR);
    if (-1 == segid)
        return shared_info;
    pshared_info = shmat(segid, NULL, SHM_RDONLY);
    if (pshared_info == (void*)-1)
        return shared_info;
    shared_info = *pshared_info;
    shmdt(pshared_info);

    return shared_info;
}

size_t imms_get_mem_usage(pid_t pid, bool self)
{
    int fd;
    char buf[PATH_MAX + 128];
    char *offset, *length, *perm, *name;
    unsigned int i;
    size_t mem = 0;

    if (self) {
        fd = open("/proc/self/maps", O_RDONLY);
    } else {
        snprintf(buf, sizeof(buf), "/proc/%u/maps", pid);
        fd = open(buf, O_RDONLY);
    }
    if (-1 == fd)
        return 0;
    for (;;) {
        i = 0;
        do {
            if (read(fd, &buf[i], 1) != 1)
                goto ret;
        } while ((buf[i] != '\n') && (buf[i] != '\r') && (++i < (sizeof(buf) - 1)));
        buf[i] = 0;
        offset = buf;
        length = strchr(buf, '-') + 1;
        perm = strchr(length, ' ') + 1;
        if (!strncmp(perm, "rw-p", 4)) {
            name = strrchr(buf, ' ') + 1;
            if (!*name || !strncmp(name, "[heap]", 6))
                mem += strtoull(length, NULL, 16) - strtoull(offset, NULL, 16);
        }
    }

ret:
    close(fd);
    return mem;
}

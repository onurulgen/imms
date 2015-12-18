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

#define	PERF_LOG_PATH		IMMS_PATH "perf-logs/"
#define SECTONANO           (1000000000)
#define PERF_STAT_TIME      5         /* Write perf log every 5 seconds */

bool imms_perf_test_mode;
static imms_perf_t perf[IMMS_PERF_ARRAY_SIZE];
static size_t malloc_mem;
static pthread_t imms_perf_stat_tid;

void imms_perf_process(struct timespec *start, struct timespec *end, unsigned char type, size_t allocated_size[])
{
    struct timespec ts;
    typeof(ts.tv_sec) sec;

    __sync_add_and_fetch(&malloc_mem, allocated_size[1] - allocated_size[0]);
	ts.tv_sec = end->tv_sec - start->tv_sec;
	ts.tv_nsec = end->tv_nsec - start->tv_nsec;
	if (ts.tv_nsec < 0) {
		ts.tv_sec--;
		ts.tv_nsec += SECTONANO;
	}
	sec = perf[type].time.tv_nsec / SECTONANO;
	ts.tv_sec += sec;
	ts.tv_nsec -= sec * SECTONANO;
	__sync_add_and_fetch(&perf[type].time.tv_sec, ts.tv_sec);
	__sync_add_and_fetch(&perf[type].time.tv_nsec, ts.tv_nsec);
	__sync_add_and_fetch(&perf[type].count, 1);
}

static void* imms_perf_stat(void *pdata)
{
    imms_avg_perf_t perf_avg[IMMS_PERF_ARRAY_SIZE];
    imms_perf_t p;
	unsigned int i;
	off_t pos;
	size_t real_mem;
	int fd = -1;
	char filepath[PATH_MAX + 1];

    memset(perf_avg, 0, sizeof(perf_avg));
start:
    sleep(PERF_STAT_TIME);
    if (!imms_make_log_file(IMMS_PERF_LOGS_PATH, filepath, sizeof(filepath), false)) {
        imms_log_error("imms_perf_stat imms_make_log_file error!");
        return NULL;
    }
    if ((fd = open(filepath, O_RDWR)) == -1 ||
        flock(fd, LOCK_EX | LOCK_NB) == -1 ||
        (pos = lseek(fd, 0, SEEK_END)) == -1 ||
        write(fd, &imms_loaded_malloc_lib, sizeof(imms_loaded_malloc_lib)) != sizeof(imms_loaded_malloc_lib))
        goto error;
    pos += sizeof(imms_loaded_malloc_lib);
	for (;;) {
        if (lseek(fd, pos, SEEK_SET) == -1)
            goto error;
        for (i = 0; i < IMMS_PERF_ARRAY_SIZE; i++) {
            p.time.tv_sec = __sync_lock_test_and_set(&perf[i].time.tv_sec, 0);
            p.time.tv_nsec = __sync_lock_test_and_set(&perf[i].time.tv_nsec, 0);
            p.count = __sync_lock_test_and_set(&perf[i].count, 0);
            /* To prevent division by zero */
            if (perf_avg[i].count || p.count) {
                perf_avg[i].sec = imms_average_winc(perf_avg[i].sec, p.time.tv_sec + ((long double)p.time.tv_nsec / SECTONANO), perf_avg[i].count, p.count);
                perf_avg[i].count += p.count;
            }
        }
        if (write(fd, perf_avg, sizeof(perf_avg)) != sizeof(perf_avg))
            goto error;
        if ((real_mem = imms_get_mem_usage(0, true)) && (lseek(fd, 0, SEEK_END) != -1)) {
            if (write(fd, &malloc_mem, sizeof(malloc_mem)) != sizeof(malloc_mem) ||
                write(fd, &real_mem, sizeof(real_mem)) != sizeof(real_mem))
                goto error;
        }
        sleep(PERF_STAT_TIME);
	}

error:
    if (fd != -1)
        close(fd);
    unlink(filepath);
    goto start;
}

void imms_perf_init()
{
    if (!imms_pthread_create) {
        imms_log_error("imms_perf_init imms_pthread_create is NULL!");
        return;
    }
    malloc_mem = 0;
    memset(perf, 0, sizeof(perf));
    if (imms_pthread_create(&imms_perf_stat_tid, NULL, imms_perf_stat, NULL))
		imms_log_error("imms_perf_init imms_pthread_create error!");
}

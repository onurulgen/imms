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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <sys/sysinfo.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "malloc_libs.h"

#define IMMS_PATH                       "/imms/"
#define IMMS_PERF_LOGS_PATH             IMMS_PATH "perf-logs/"
#define IMMS_ANALYSED_PERF_LOGS_PATH    IMMS_PERF_LOGS_PATH "analysed/"
#define IMMS_FAILED_PERF_LOGS_PATH      IMMS_PERF_LOGS_PATH "failed/"
#define IMMS_PERF_RES_PATH              IMMS_PATH "perf-res/"
#define IMMS_LOCK_PATH                  IMMS_PATH "lock/"
#define IMMS_MALLOC_LIB_PATH            IMMS_PATH "memallocs/"
#define	itoa        imms_itoa
//#define	IMMS_VERBOSE
#define IMMS_LOGGING

#ifdef	IMMS_VERBOSE
	#define	IMMS_VERBOSE_MSG(msg)       puts(msg)
	#define	IMMS_VERBOSE_MSGWPTR(msg, ptr)	{ \
			char sz[50];	\
			itoa((long)ptr, sz, 16);	\
			fputs(msg " ", stdout);	\
			puts(sz);	\
		}
    #define	IMMS_VERBOSE_STD(caller, ptr)   IMMS_VERBOSE_MSGWPTR(caller " called", ptr)
#else
	#define	IMMS_VERBOSE_MSG(msg)
	#define	IMMS_VERBOSE_MSGWPTR(msg, ptr)
	#define	IMMS_VERBOSE_STD(caller, ptr)
#endif

#ifdef  IMMS_LOGGING
	void imms_log_error(const char*);
#else
	#define imms_log_error(sz)
#endif

#ifndef __cplusplus
	#define	true	1
	#define	false	0
	typedef char bool;
#endif

typedef unsigned char imms_library_t;

typedef struct {
    imms_library_t lib;
    bool test_mode;
} imms_shared_info_t;

char* imms_itoa(long value, char *result, int base);
bool imms_init(void **imms_func);
bool imms_open_perf_log_file(char *szfile, const size_t len, const char *szdir);
bool imms_make_log_file(const char *szdir, char *szfile, const size_t len, const bool bperflog);
void imms_init_daemon(char *dname);
char* imms_process_filename();
char* imms_process_filepath();
bool imms_is_process_excluded();
long double imms_average(long double avg, long double add, long double count);
long double imms_average_winc(long double avg, long double add, long double count, long double inc);
void imms_share_info(imms_library_t lib, bool test_mode);
imms_shared_info_t imms_read_shared_info(pid_t pid);
size_t imms_get_mem_usage(pid_t pid, bool self);

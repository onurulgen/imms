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

#include "imms.h"

#define	IMMS_PERF_MALLOC		0
#define	IMMS_PERF_REALLOC		1
#define	IMMS_PERF_MEMALIGN		2
#define	IMMS_PERF_FREE			3
#define IMMS_PERF_ARRAY_SIZE    4

#define	IMMS_PERF_INIT(t)		struct timespec start, end; size_t allocated_size[2] = {0, 0}; const char type = t;
#define	IMMS_PERF_BEGIN(ptr)	if (imms_perf_test_mode) { \
                                    IMMS_VERBOSE_STD("IMMS_PERF_BEGIN", ptr); \
                                    if (ptr) { \
                                        allocated_size[0] = imms_malloc_usable_size(ptr); \
                                    } \
                                    clock_gettime(CLOCK_REALTIME, &start); \
                                }
#define	IMMS_PERF_END(ptr)		if (imms_perf_test_mode) { \
                                    clock_gettime(CLOCK_REALTIME, &end); \
                                    IMMS_VERBOSE_STD("IMMS_PERF_END", ptr); \
                                    if (!ptr && (IMMS_PERF_REALLOC == type)) allocated_size[0] = 0; \
                                    if (ptr) { \
                                        allocated_size[1] = imms_malloc_usable_size(ptr); \
                                    } \
                                    IMMS_VERBOSE_MSGWPTR("IMMS_PERF_END allocated_size[0]", allocated_size[0]); \
                                    IMMS_VERBOSE_MSGWPTR("IMMS_PERF_END allocated_size[1]", allocated_size[1]); \
                                    imms_perf_process(&start, &end, type, allocated_size); \
                                }

typedef struct {
    struct timespec time;
    unsigned int count;
} imms_perf_t;

typedef struct {
    double sec;
    unsigned int count;
} imms_avg_perf_t;

typedef struct {
    struct {
        double sec;
        double memfrag;
        size_t avgmem;
        size_t count;
        time_t time;
    } smr[IMMS_MALLOC_LIB_END + 1];     /* Performance summary */
    imms_library_t result[3], nextlib;
    bool test_mode;
} imms_perf_result_t;

extern bool imms_perf_test_mode;

void imms_perf_init();
void imms_perf_process(struct timespec*, struct timespec*, unsigned char, size_t[]);
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

bool imms_init(void **imms_func)
{
	static char initialised = 0;
	static pid_t tid = 0;

    if (initialised != 3) {
        if (__sync_fetch_and_or(&initialised, 1)) {
            if (syscall(SYS_gettid) == tid) {
                IMMS_VERBOSE_MSGWPTR("imms_init gettid() == tid; tid =", tid);
            } else {
                IMMS_VERBOSE_MSGWPTR("imms_init thread is waiting; tid =", syscall(SYS_gettid));
                while (1 == initialised)
                    usleep(1000);
            }
        } else {
            IMMS_VERBOSE_MSG("imms is initilising...");
            IMMS_VERBOSE_MSG(imms_process_filepath());
            tid = syscall(SYS_gettid);
            IMMS_VERBOSE_MSGWPTR("imms_init tid =", tid);
            imms_load_malloc_lib();
            __sync_fetch_and_or(&initialised, 2);
            IMMS_VERBOSE_MSG("imms initialised");
        }
	}

	return *imms_func ? true : false;
}
/*
__attribute__((constructor))
static void init()
{
    void *p;

    imms_init(&p);
}
*/

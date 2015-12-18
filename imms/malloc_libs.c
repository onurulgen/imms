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

#define DL_FLAGS    RTLD_NOW | RTLD_NODELETE

void* (*imms_malloc)(size_t);
void* (*imms_realloc)(void*, size_t);
void (*imms_free)(void*);
void* (*imms_memalign)(size_t, size_t);
int (*imms_mallopt)(int, int);
size_t (*imms_malloc_usable_size)(void*);
int (*imms_pthread_create)(pthread_t*, const pthread_attr_t*, void *(*)(void*), void*);
void (*imms_pthread_exit)(void*);
imms_library_t imms_loaded_malloc_lib;


/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* System Malloc */

/****************************************************************************************/

static bool load_system()
{
	imms_malloc = dlsym(RTLD_NEXT, "malloc");
    imms_realloc = dlsym(RTLD_NEXT, "realloc");
    imms_free = dlsym(RTLD_NEXT, "free");
    imms_memalign = dlsym(RTLD_NEXT, "memalign");
    imms_mallopt = dlsym(RTLD_NEXT, "mallopt");
    imms_malloc_usable_size = dlsym(RTLD_NEXT, "malloc_usable_size");
	imms_pthread_create = NULL;
	imms_pthread_exit = NULL;

	return true;
}


/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* Hoard */

/****************************************************************************************/

static bool load_hoard()
{
    void *handle;

    handle = dlopen(IMMS_MALLOC_LIB_PATH "libhoard.so", DL_FLAGS);
	imms_malloc = dlsym(handle, "hoard_malloc");
	imms_realloc = dlsym(handle, "hoard_realloc");
	imms_free = dlsym(handle, "hoard_free");
	imms_memalign = dlsym(handle, "hoard_memalign");
	imms_mallopt = dlsym(handle, "hoard_mallopt");
	imms_malloc_usable_size = dlsym(handle, "hoard_malloc_usable_size");
	imms_pthread_create = dlsym(handle, "hoard_pthread_create");
	imms_pthread_exit = dlsym(handle, "hoard_pthread_exit");
	if (!imms_malloc || !imms_realloc || !imms_free || !imms_memalign ||
        !imms_mallopt || !imms_malloc_usable_size || !imms_pthread_create || !imms_pthread_exit) {
        imms_log_error("load_hoard error!");
        return false;
    }

    return true;
}


/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* TCMalloc */

/****************************************************************************************/

static bool load_tcmalloc()
{
    void *handle;

    handle = dlopen(IMMS_MALLOC_LIB_PATH "libtcmalloc_minimal.so", DL_FLAGS);
	imms_malloc = dlsym(handle, "tc_malloc");
	imms_realloc = dlsym(handle, "tc_realloc");
	imms_free = dlsym(handle, "tc_free");
	imms_memalign = dlsym(handle, "tc_memalign");
	imms_mallopt = dlsym(handle, "tc_mallopt");
	imms_malloc_usable_size = dlsym(handle, "tc_malloc_size");
	imms_pthread_create = NULL;
	imms_pthread_exit = NULL;
	if (!imms_malloc || !imms_realloc || !imms_free || !imms_memalign ||
        !imms_mallopt || !imms_malloc_usable_size) {
        imms_log_error("load_tcmalloc error!");
        return false;
    }

    return true;
}


/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* JEMalloc */

/****************************************************************************************/

static bool load_jemalloc()
{
    void *handle;

    handle = dlopen(IMMS_MALLOC_LIB_PATH "libjemalloc.so", DL_FLAGS);
	imms_malloc = dlsym(handle, "je_malloc");
	imms_realloc = dlsym(handle, "je_realloc");
	imms_free = dlsym(handle, "je_free");
	imms_memalign = dlsym(handle, "je_memalign");
	imms_mallopt = NULL;
	imms_malloc_usable_size = dlsym(handle, "je_malloc_usable_size");
	imms_pthread_create = NULL;
	imms_pthread_exit = NULL;
	if (!imms_malloc || !imms_realloc || !imms_free || !imms_memalign ||
        !imms_malloc_usable_size) {
        imms_log_error("load_jemalloc error!");
        return false;
    }

	return true;
}


/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* IMMS Memory Allocator Library Functions */

/****************************************************************************************/

/* if you change this array, accordingly change imms_malloc_lib_names array in util.c */
static bool (*load_malloc[])() = {
    load_system,
    load_hoard,
    load_tcmalloc,
    load_jemalloc
};

void imms_load_malloc_lib()
{
    imms_perf_result_t perfres;
    char filepath[PATH_MAX + 1], *procfilepath;
    int fd;
    imms_library_t lib = 0;
    bool perf_test_mode = false;

    imms_perf_test_mode = false;
    if (!(procfilepath = imms_process_filepath())) {
        IMMS_VERBOSE_MSG("imms_load_malloc_lib imms_process_filepath error!");
        goto errret;
    }
    if (imms_is_process_excluded())
        goto errret;
    perf_test_mode = true;
    strcpy(filepath, procfilepath);
    if (!imms_open_perf_log_file(filepath, sizeof(filepath), IMMS_PERF_RES_PATH)) {
        IMMS_VERBOSE_MSG("imms_load_malloc_lib imms_open_perf_log_file error!");
        goto errret;
    }
    fd = open(filepath, O_RDONLY);
    if (-1 == fd) {
        imms_log_error("imms_load_malloc_lib open error!");
        imms_log_error(filepath);
        goto errret;
    }
    lseek(fd, strlen(procfilepath) + 1, SEEK_SET);
    if (read(fd, &perfres, sizeof(perfres)) != sizeof(perfres)) {
        imms_log_error("imms_load_malloc_lib read error!");
    } else {
        perf_test_mode = perfres.test_mode;
        if (perf_test_mode) {
            lib = perfres.nextlib;
        } else if (perfres.result[0] == perfres.result[1] && perfres.result[1] == perfres.result[2]) {
            lib = perfres.result[0];
        } else {
            struct sysinfo info;
            double cpuload;
            size_t freeram;

            if (!sysinfo(&info)) {
                cpuload = info.loads[1] / 65536.0;      /* Inspect 5-minute CPU load */
                freeram = info.freeram * info.mem_unit;
                if (cpuload >= 0.75 && freeram >= perfres.smr[perfres.result[0]].avgmem)
                    lib = perfres.result[0];
                else if (freeram < perfres.smr[perfres.result[1]].avgmem && cpuload < 0.75)
                    lib = perfres.result[1];
                else
                    lib = perfres.result[2];
            } else {
                imms_log_error("imms_load_malloc_lib sysinfo error!");
            }
        }
    }
    close(fd);

errret:
    if (lib > IMMS_MALLOC_LIB_END)
        lib = 0;
    //lib = 1;      /* For testing */
    if (!load_malloc[lib]()) {
        load_system();
        lib = 0;
        perf_test_mode = false;
    }
    if (!imms_pthread_create)
        imms_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
    if (!imms_pthread_exit)
        imms_pthread_exit = dlsym(RTLD_NEXT, "pthread_exit");
	imms_loaded_malloc_lib = lib;
	imms_share_info(imms_loaded_malloc_lib, perf_test_mode);
	if (perf_test_mode) {
        imms_perf_init();
        imms_perf_test_mode = true;
	}
	IMMS_VERBOSE_MSGWPTR("imms_loaded_malloc_lib =", imms_loaded_malloc_lib);
	IMMS_VERBOSE_MSGWPTR("imms_malloc =", imms_malloc);
    IMMS_VERBOSE_MSGWPTR("imms_realloc =", imms_realloc);
    IMMS_VERBOSE_MSGWPTR("imms_free =", imms_free);
    IMMS_VERBOSE_MSGWPTR("imms_memalign =", imms_memalign);
    IMMS_VERBOSE_MSGWPTR("imms_mallopt =", imms_mallopt);
    IMMS_VERBOSE_MSGWPTR("imms_malloc_usable_size =", imms_malloc_usable_size);
    IMMS_VERBOSE_MSGWPTR("imms_pthread_create =", imms_pthread_create);
    IMMS_VERBOSE_MSGWPTR("imms_pthread_exit =", imms_pthread_exit);
}

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

/*
 *  Simple memory allocator to prevent segmentation fault
 *  caused by malloc_hooks and dlopen. There is no free function,
 *  since it won't be freed (I hope so).
 */
static void* alloc(size_t size)
{
    void *ptr;

    ptr = sbrk(size);

    return ptr == (void*)-1 ? NULL : ptr;
}

void* malloc(size_t size)
{
	void *p;

	IMMS_PERF_INIT(IMMS_PERF_MALLOC);
	if (!imms_init((void**)&imms_malloc))
		return alloc(size);
	IMMS_PERF_BEGIN(NULL);
	p = imms_malloc(size);
	IMMS_PERF_END(p);
	IMMS_VERBOSE_STD("malloc", p);

    return p;
}

void* realloc(void *ptr, size_t size)
{
	void *p;

	IMMS_PERF_INIT(IMMS_PERF_REALLOC);
	if (!imms_init((void**)&imms_realloc))
		return NULL;
	IMMS_PERF_BEGIN(ptr);
	p = imms_realloc(ptr, size);
	IMMS_PERF_END(p);
	IMMS_VERBOSE_STD("realloc", p);

    return p;
}

void* memalign(size_t alignment, size_t size)
{
	void *p;

	IMMS_PERF_INIT(IMMS_PERF_MEMALIGN);
	if (!imms_init((void**)&imms_memalign))
		return NULL;
	IMMS_PERF_BEGIN(NULL);
	p = imms_memalign(alignment, size);
	IMMS_PERF_END(p);
	IMMS_VERBOSE_STD("memalign", p);

    return p;
}

void free(void *ptr)
{
	IMMS_PERF_INIT(IMMS_PERF_FREE);
	IMMS_VERBOSE_STD("free", ptr);
	if (!ptr || !imms_init((void**)&imms_free))
		return;
	IMMS_PERF_BEGIN(ptr);
    imms_free(ptr);
	IMMS_PERF_END(NULL);
}

void* calloc(size_t numelm, size_t elmsize)
{
	void *p;
    size_t size;

	size = numelm * elmsize;
	p = malloc(size);
	if (p && size)
        memset(p, 0, size);

    return p;
}

int posix_memalign(void **ptr, size_t alignment, size_t size)
{
	if (!alignment || (alignment & (alignment - 1)))
		return EINVAL;
	*ptr = memalign(alignment, size);

	return *ptr ? 0 : ENOMEM;
}

void* aligned_alloc(size_t alignment, size_t size)
{
    void *ptr;
    int err;

    err = posix_memalign(&ptr, alignment, size);
    if (err)
        errno = err;

    return ptr;
}

void* valloc(size_t size)
{
	static long pagesize = 0;
	static char initialised = 0;

	if (initialised != 3) {
		if (!__sync_fetch_and_or(&initialised, 1)) {
			pagesize = sysconf(_SC_PAGESIZE);
			if (pagesize < 1)
				pagesize = 4096;
			__sync_fetch_and_or(&initialised, 2);
		} else
			while (1 == initialised)
				usleep(1000);
	}

	return memalign(pagesize, size);
}

void* pvalloc(size_t size)
{
	static long pagemask = 0;
	static char initialised = 0;

	if (initialised != 3) {
		if (!__sync_fetch_and_or(&initialised, 1)) {
			pagemask = sysconf(_SC_PAGESIZE);
			if (pagemask < 1)
				pagemask = 4096;
            pagemask--;
			__sync_fetch_and_or(&initialised, 2);
		} else
			while (1 == initialised)
				usleep(1000);
	}

	return valloc((size + pagemask) & ~pagemask);
}

int mallopt(int param, int value)
{
	if (!imms_init((void**)&imms_mallopt))
		return 0;
	return imms_mallopt(param, value);
}

size_t malloc_usable_size(void *ptr)
{
	if (!imms_init((void**)&imms_malloc_usable_size))
		return 0;
    IMMS_VERBOSE_STD("malloc_usable_size", ptr);

    return imms_malloc_usable_size(ptr);
}

void cfree(void *ptr)
{
	free(ptr);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void* (*start_routine)(void*), void *arg)
{
    IMMS_VERBOSE_MSG("pthread_create called");
	if (!imms_init((void**)&imms_pthread_create))
		return EAGAIN;

	return imms_pthread_create(thread, attr, start_routine, arg);
}

void pthread_exit(void *retval)
{
    IMMS_VERBOSE_MSG("pthread_exit called");
	if (!imms_init((void**)&imms_pthread_exit))
		return;
	imms_pthread_exit(retval);
}


/**************************************************
 * glibc malloc hooks
 **************************************************/
/* causing bus error on system and jemalloc libs
static void* malloc_hook(size_t size, const void *caller)
{
    return malloc(size);
}

static void* realloc_hook(void *ptr, size_t size, const void *caller)
{
    return realloc(ptr, size);
}

static void* memalign_hook(size_t alignment, size_t size, const void *caller)
{
    return memalign(alignment, size);
}

static void free_hook(void *ptr, const void *caller)
{
    free(ptr);
}

#include <malloc.h>

void imms_init_malloc_hooks()
{
    __malloc_hook = malloc_hook;
    __realloc_hook = realloc_hook;
    __memalign_hook = memalign_hook;
    __free_hook = free_hook;
}

void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) = imms_init_malloc_hooks;
*/

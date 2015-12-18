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

#define IMMS_MALLOC_SYSTEM      0
#define IMMS_MALLOC_HOARD       1
#define IMMS_MALLOC_TC          2
#define IMMS_MALLOC_JE          3
#define IMMS_MALLOC_LIB_END     3

extern void* (*imms_malloc)(size_t);
extern void* (*imms_realloc)(void*, size_t);
extern void (*imms_free)(void*);
extern void* (*imms_memalign)(size_t, size_t);
extern int (*imms_mallopt)(int, int);
extern size_t (*imms_malloc_usable_size)(void*);
extern int (*imms_pthread_create)(pthread_t*, const pthread_attr_t*, void *(*)(void*), void*);
extern void (*imms_pthread_exit)(void*);
extern unsigned char imms_loaded_malloc_lib;

extern const char *imms_malloc_lib_names[];

void imms_load_malloc_lib();

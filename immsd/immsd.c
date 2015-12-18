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

#include "../imms/perf.h"

#define SLEEP_TIME                  5       /* Process files in every SLEEP_TIME seconds */
#define MIN_TIME_TO_REPER           (1 * 60 * 60)      /* 1 hour in seconds */
#define MAX_TEST_AMOUNT             5      /* Maximum test amount per memory allocator */

/**********************************************************************
 * At return:
 * perfres->result[0] stores the fastest library
 * perfres->result[1] stores the most memory efficient library
 * perfres->result[2] stores the balanced library for CPU and memory
 **********************************************************************/
static void immsd_analyse(imms_perf_result_t *perfres)
{
    imms_library_t sorted_libs[2][IMMS_MALLOC_LIB_END + 1];
    imms_library_t ranked_libs[2][IMMS_MALLOC_LIB_END + 1];
    imms_library_t i, j, k, tmp;

    /* first index of the sorted libs holds the performance and fragmentation, respectively */
    /* library values are assigned to the array; later, they will be sorted by rank which is held in second index */
    for (i = 0; i < 2; i++) {
        for (j = 0; j <= IMMS_MALLOC_LIB_END; j++)
            sorted_libs[i][j] = j;
    }
    /* sorting operation by rank (j) */
    for (i = 0; i < IMMS_MALLOC_LIB_END; i++) {
        for (j = i + 1; j <= IMMS_MALLOC_LIB_END; j++) {
            if (perfres->smr[sorted_libs[0][i]].sec < perfres->smr[sorted_libs[0][j]].sec) {
                tmp = sorted_libs[0][i];
                sorted_libs[0][i] = sorted_libs[0][j];
                sorted_libs[0][j] = tmp;
            }
            if (perfres->smr[sorted_libs[1][i]].memfrag < perfres->smr[sorted_libs[1][j]].memfrag) {
                tmp = sorted_libs[1][i];
                sorted_libs[1][i] = sorted_libs[1][j];
                sorted_libs[1][j] = tmp;
            }
        }
    }
    perfres->result[0] = sorted_libs[0][IMMS_MALLOC_LIB_END];
    perfres->result[1] = sorted_libs[1][IMMS_MALLOC_LIB_END];
    /* Rank libraries and select balanced library */
    perfres->result[2] = 0;
    for (i = 0; i <= IMMS_MALLOC_LIB_END; i++) {
        for (j = 0; j <= IMMS_MALLOC_LIB_END; j++) {
            for (k = 0; k < 2; k++) {
                if (i == sorted_libs[k][j])
                    ranked_libs[k][i] = j + 1;    /* j + 1 is rank of the library */
            }
        }
        j = ranked_libs[0][perfres->result[2]] + ranked_libs[1][perfres->result[2]];
        k = ranked_libs[0][i] + ranked_libs[1][i];
        if (k > j) {
            perfres->result[2] = i;
        } else if (j == k) {
            /* if two libraries have same ranking then select more memory efficient one */
            if (ranked_libs[1][i] > ranked_libs[1][perfres->result[2]])
                perfres->result[2] = i;
        }
    }
}

static void immsd_process_perf_log(const char *path)
{
    imms_perf_result_t perfres;
    imms_avg_perf_t perf_avg, perf[IMMS_PERF_ARRAY_SIZE];
    long double malloc_mem, real_mem;
    size_t i;
    char c, *sz, perflogpath[PATH_MAX + 1];
    imms_library_t lib;
    time_t t;
    int fd;
    bool opened;

    fd = open(path, O_RDONLY);
    if (-1 == fd) {
        imms_log_error("immsd_process_perf_log open error! File name:");
        imms_log_error(path);
        return;
    }
    i = 0;
    do {
        if (read(fd, &c, 1) <= 0) {
            imms_log_error("immsd_process_perf_log read error! File name:");
            imms_log_error(path);
            goto errret;
        }
        perflogpath[i++] = c;
    } while ((c != '\n') && (c != '\r'));
    perflogpath[i - 1] = 0;
    if (read(fd, &lib, sizeof(lib)) != sizeof(lib)) {
        imms_log_error("immsd_process_perf_log read error on lib! File name:");
        imms_log_error(path);
        goto errret;
    }
    if (lib > IMMS_MALLOC_LIB_END) {
        imms_log_error("immsd_process_perf_log lib exceeded limit! File name:");
        imms_log_error(path);
        goto errret;
    }
    if (read(fd, perf, sizeof(perf)) != sizeof(perf)) {
        imms_log_error("immsd_process_perf_log read error on perf! File name:");
        imms_log_error(path);
        goto errret;
    }
    for (i = 0, perf_avg.sec = 0; i < IMMS_PERF_ARRAY_SIZE; i++)
        perf_avg.sec = imms_average(perf_avg.sec, perf[i].sec, i);
    for (malloc_mem = real_mem = i = 0;;) {
        size_t mem[2];

        if (read(fd, &mem[0], sizeof(mem[0])) == sizeof(mem[0]) &&
            read(fd, &mem[1], sizeof(mem[1])) == sizeof(mem[1])) {
            /* malloc_mem can't be bigger than real_mem; however, OS don't allocate page for
               untouched memory areas. Therefore, malloc_mem can be bigger temporarily. */
            if (mem[0] >= mem[1]) {
                imms_log_error("immsd_process_perf_log (malloc_mem >= real_mem) warning! File name:");
                imms_log_error(path);
            } else {
                malloc_mem = imms_average(malloc_mem, mem[0], i);
                real_mem = imms_average(real_mem, mem[1], i++);
            }
        } else {
            break;
        }
    }
    if (!i) {
        imms_log_error("immsd_process_perf_log malloc_mem or real_mem count is ZERO! File name:");
        imms_log_error(path);
        goto errret;
    }
    close(fd);
    if ((t = time(NULL)) == -1) {
        imms_log_error("immsd_process_perf_log time error!");
        return;
    }

makeperfres:
    if (!(opened = imms_open_perf_log_file(perflogpath, sizeof(perflogpath), IMMS_PERF_RES_PATH)) &&
        !imms_make_log_file(IMMS_PERF_RES_PATH, perflogpath, sizeof(perflogpath), true)) {
        imms_log_error("immsd_process_perf_log perf-res log file error! File name:");
        imms_log_error(path);
        return;
    }
    fd = open(perflogpath, O_RDWR);
    if (-1 == fd) {
        imms_log_error("immsd_process_perf_log open procfile error! File name:");
        imms_log_error(perflogpath);
        return;
    }
    if (opened) {
        ssize_t readbytes;

        do {
            if (!(readbytes = read(fd, &c, 1)))
                goto perfreserr;
            if (readbytes != 1) {
                imms_log_error("immsd_process_perf_log read procfile error! File name:");
                imms_log_error(perflogpath);
                goto cleanup;
            }
        } while ((c != '\n') && (c != '\r'));
        if (!(readbytes = read(fd, &perfres, sizeof(perfres))))
            goto perfreserr;
        if (readbytes != sizeof(perfres)) {
            imms_log_error("immsd_process_perf_log read error on perfres! File name:");
            imms_log_error(perflogpath);
            goto cleanup;
        }
        if (lseek(fd, -sizeof(perfres), SEEK_CUR) == -1) {
            imms_log_error("immsd_process_perf_log (opened) lseek error! File name:");
            imms_log_error(perflogpath);
            goto cleanup;
        }
    } else {
        if (lseek(fd, 0, SEEK_END) == -1) {
            imms_log_error("immsd_process_perf_log (!opened) lseek error! File name:");
            imms_log_error(perflogpath);
            goto cleanup;
        }
        memset(perfres.smr, 0, sizeof(perfres.smr));
    }
    if (perfres.smr[lib].count < MAX_TEST_AMOUNT && difftime(t, perfres.smr[lib].time) >= MIN_TIME_TO_REPERF) {
        perfres.smr[lib].sec = imms_average(perfres.smr[lib].sec, perf_avg.sec, perfres.smr[lib].count);
        if (real_mem < malloc_mem) {
            imms_log_error("immsd_process_perf_log (real_mem < malloc_mem) error! File name:");
            imms_log_error(path);
            goto errret;
        }
        perfres.smr[lib].memfrag = imms_average(perfres.smr[lib].memfrag, (real_mem - malloc_mem) / real_mem, perfres.smr[lib].count);
        perfres.smr[lib].avgmem = imms_average(perfres.smr[lib].avgmem, real_mem, perfres.smr[lib].count);
        perfres.smr[lib].time = t;
        perfres.smr[lib].count++;
        immsd_analyse(&perfres);
    }
    perfres.test_mode = false;
    for (i = 0, lib++; i <= IMMS_MALLOC_LIB_END; i++, lib++) {
        lib %= IMMS_MALLOC_LIB_END + 1;
        if (perfres.smr[lib].count < MAX_TEST_AMOUNT && difftime(t, perfres.smr[lib].time) >= MIN_TIME_TO_REPERF) {
            perfres.nextlib = lib;
            perfres.test_mode = true;
            break;
        }
    }
    if (write(fd, &perfres, sizeof(perfres)) != sizeof(perfres)) {
        imms_log_error("immsd_process_perf_log write error on perfres! File name:");
        imms_log_error(perflogpath);
        close(fd);
        unlink(perflogpath);
        return;
    }
    if (sz = strrchr(path, '/')) {
        strcpy(perflogpath, IMMS_ANALYSED_PERF_LOGS_PATH);
        strcat(perflogpath, ++sz);
        if (rename(path, perflogpath)) {
            imms_log_error("immsd_process_perf_log move perf log file error! File name:");
            imms_log_error(path);
            unlink(path);
        }
    }

cleanup:
    close(fd);
    return;

perfreserr:
    close(fd);
    unlink(perflogpath);
    goto makeperfres;

errret:
    close(fd);
    if (sz = strrchr(path, '/')) {
        strcpy(perflogpath, IMMS_FAILED_PERF_LOGS_PATH);
        strcat(perflogpath, ++sz);
        if (rename(path, perflogpath)) {
            imms_log_error("immsd_process_perf_log move failed perf log file error! File name:");
            imms_log_error(path);
            unlink(path);
        }
    }
}

int main()
{
    DIR *dir;
    struct dirent *de;
    char path[PATH_MAX + 1];
    int fd;

    imms_init_daemon("immsd");
    if (chdir(IMMS_PERF_LOGS_PATH)) {
        imms_log_error("main chdir error!");
        return -1;
    }
    dir = opendir(IMMS_PERF_LOGS_PATH);
    for (;;) {
        sleep(SLEEP_TIME);
        rewinddir(dir);
        while (de = readdir(dir)) {
            if (!strrchr(de->d_name, '-'))
                continue;
            if ((fd = open(de->d_name, O_RDONLY)) != -1) {
                if (flock(fd, LOCK_EX | LOCK_NB) != -1) {
                    close(fd);
                    strcpy(path, IMMS_PERF_LOGS_PATH);
                    strcat(path, de->d_name);
                    immsd_process_perf_log(path);
                } else {
                    close(fd);
                }
            }
        }
    }

    return 0;
}

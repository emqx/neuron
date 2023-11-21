/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/log.h"
#include "utils/time.h"

#include "daemon.h"

#define STOP_TIMEOUT 10000
#define STOP_TICK 2000

void daemonize()
{
    int              nfile;
    pid_t            pid;
    struct rlimit    rl;
    struct sigaction sa;
    int              ret = -1;

    // clear file creation mask
    umask(0);

    // get maximum number of file descriptors
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        exit(1);
    }
    nfile = (RLIM_INFINITY == rl.rlim_max) ? 1024 : rl.rlim_max;

    // become a session leader to lose controlling tty
    if ((pid = fork()) < 0) {
        exit(1);
    } else if (0 != pid) {
        exit(0);
    }
    setsid();

    // ensure future opens won't allocate controlling ttys
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        exit(1);
    }
    if ((pid = fork()) < 0) {
        exit(1);
    } else if (0 != pid) {
        exit(0);
    }

    // close all open file descriptors
    for (int i = 0; i < nfile; ++i) {
        close(i);
    }

    // attach file descriptors 0, 1 and 2 to /dev/null
    open("/dev/null", O_RDWR);
    ret = dup(0);
    assert(ret != -1);
    ret = dup(0);
    assert(ret != -1);
}

static inline int lock_file(int fd)
{
    struct flock fl;
    fl.l_type   = F_WRLCK;
    fl.l_start  = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len    = 0;
    return fcntl(fd, F_SETLK, &fl);
}

static int neuron_wait_exit(uint32_t ms)
{
    uint32_t tick = 0;
    int      ret  = -1;
    int      fd   = -1;

    fd = open(NEURON_DAEMON_LOCK_FNAME, O_WRONLY);
    if (fd < 0) {
        nlog_error("cannot open %s reason: %s\n", NEURON_DAEMON_LOCK_FNAME,
                   strerror(errno));
        exit(1);
    }

    do {
        // try to lock file
        if (lock_file(fd) < 0) {
            if (EACCES == errno || EAGAIN == errno) {
                // a neuron process is running
                printf("neuron is stopping.\n");
            } else {
                close(fd);
                nlog_error("cannot lock %s reason: %s\n",
                           NEURON_DAEMON_LOCK_FNAME, strerror(errno));
                exit(1);
            }
        } else {
            printf("neuron had stopped.\n");
            ret = 0;
            break;
        }

        neu_msleep(STOP_TICK);
        tick += STOP_TICK;

    } while (tick < ms);
    close(fd);
    return ret;
}

int neuron_already_running()
{
    int     fd      = -1;
    char    buf[16] = { 0 };
    int     ret     = -1;
    ssize_t size    = -1;

    umask(0);
    fd = open(NEURON_DAEMON_LOCK_FNAME, O_RDWR | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0) {
        // work around fs.protected_regular
        fd = open(NEURON_DAEMON_LOCK_FNAME, O_RDWR,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd < 0) {
            nlog_error("cannot open %s reason: %s\n", NEURON_DAEMON_LOCK_FNAME,
                       strerror(errno));
            exit(1);
        }
    }

    // try to lock file
    if (lock_file(fd) < 0) {
        if (EACCES == errno || EAGAIN == errno) {
            // a neuron process already running
            close(fd);
            return 1;
        }
        nlog_error("cannot lock %s reason: %s\n", NEURON_DAEMON_LOCK_FNAME,
                   strerror(errno));
        exit(1);
    }

    // write process id to file
    ret = ftruncate(fd, 0);
    assert(ret != -1);

    snprintf(buf, sizeof(buf), "%ld", (long) getpid());
    size = write(fd, buf, strlen(buf) + 1);
    nlog_warn("write %s, error %s(%d)", NEURON_DAEMON_LOCK_FNAME,
              strerror(errno), errno);
    assert(size != -1);

    return 0;
}

int neuron_stop()
{
    int ret = -1;

    FILE *fp = fopen(NEURON_DAEMON_LOCK_FNAME, "r");
    if (NULL == fp) {
        nlog_error("cannot open %s reason: %s\n", NEURON_DAEMON_LOCK_FNAME,
                   strerror(errno));
        return ret;
    }

    long pid = -1;
    if (1 != fscanf(fp, "%ld", &pid)) {
        nlog_error("cannot scan pid from: %s\n", NEURON_DAEMON_LOCK_FNAME);
        goto end;
    }

    pid_t gid = getpgid((pid_t) pid);
    if (-1 == gid) {
        nlog_error("cannot get gpid reason: %s\n", strerror(errno));
        goto end;
    }

    if (0 == kill((pid_t)(-gid), SIGINT)) {
        if (neuron_wait_exit(STOP_TIMEOUT) == 0) {
            ret = 0;
        } else {
            if (0 == kill((pid_t)(-gid), SIGKILL)) {
                ret = 0;
            }
        }
    } else {
        nlog_error("cannot kill gpid:%ld reason: %s\n", (long) gid,
                   strerror(errno));
    }

end:
    fclose(fp);
    return ret;
}
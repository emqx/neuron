/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

/**
 * Common Signals Handler
 * ----------------------
 * This routine sets the action from the signals that could lead to an aboration
 * of the program to instead call a common routine that call the external
 * routine stopHandler() and then exit.
 *
 * NOTE! The external routine stopHandler() should not exit, it should just make
 *a return.
 **/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <functions.h>
#include <types.h>

static neu_boolean_t *running;
static void (*stophandler)(void);
static void sigcatch(int sig);

void neu_setSignalHandler(neu_signalStopHandler stopHandler,
                          neu_boolean_t *       Running)
{
    running     = Running;
    stophandler = stopHandler;
    signal(SIGHUP, sigcatch);
    signal(SIGINT, sigcatch);
    signal(SIGQUIT, sigcatch);
    signal(SIGILL, sigcatch);
    signal(SIGIOT, sigcatch);
    signal(SIGABRT, sigcatch);
    signal(SIGFPE, sigcatch);
    signal(SIGBUS, sigcatch);
    signal(SIGSEGV, sigcatch);
    signal(SIGSYS, sigcatch);
    signal(SIGPIPE, sigcatch);
    signal(SIGALRM, sigcatch);
    signal(SIGTERM, sigcatch);
}

void sigcatch(int signal)
{
    neu_setSignalIgnore(NEU_ON);
    if (stophandler)
        stophandler();
    if (running)
        *running = false;
}

/**
 * SET SIGNAL IGNORE ON/OFF
 * ------------------------
 * This routine sets the action from the signals that could lead to an abortion
 * of the program (except failures) to instead be ignored or to take the
 * previous value.
 *
 * The routine take actions depending on the value of 'status'. If status is
 * NH_ON = 1 (or non-zero) the signals are ignored. If the status is NH_OFF = 0
 * the signals will take the previous action.
 *
 * The routine can be called several times with IGNON and afterwards be called
 * the same amount of times with NH_OFF. Signals are ignored only when the
 * ignore counter goes from 0 to 1, and they receive their previous values only
 * when the ignore counter goes from 1 to 0. */

void neu_setSignalIgnore(neu_word_t status)
{
    static int sigigndone = 0;
    static void (*savesighup)();
    static void (*savesigint)();
    static void (*savesigquit)();
    static void (*savesigalrm)();
    static void (*savesigterm)();

    if (status) {
        if (!sigigndone++) {
            savesighup  = signal(SIGHUP, SIG_IGN);
            savesigint  = signal(SIGINT, SIG_IGN);
            savesigquit = signal(SIGQUIT, SIG_IGN);
            savesigalrm = signal(SIGALRM, SIG_IGN);
            savesigterm = signal(SIGTERM, SIG_IGN);
        }
    } else if (!--sigigndone) {
        signal(SIGHUP, savesighup);
        signal(SIGINT, savesigint);
        signal(SIGQUIT, savesigquit);
        signal(SIGALRM, savesigalrm);
        signal(SIGTERM, savesigterm);
    } else if (sigigndone < 0)
        sigigndone = 0;
}

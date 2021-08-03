/*
 * Neuron - IIoT agent for Industry 4.0
 *
 * Copyright (C) 2017-2020 Joey Cheung <joey@somastech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *        M O D B U S   T C P   P R O T O C O L
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "driver.h"
#include "neuron.h"
#include "plcaddress.h"
#include "tcponet.h"

int  MBSTCP; /* only a symbol for linker */
char drvname[] = "mbstcp";

int maxrwords = 125;
int maxwwords = 100;

#define MAXBYTES (6 + 1 + 1 + 4 + 1 + 255 + 2)

// variables
static int naptimeread  = 40; // nap time before each read
static int naptimewrite = 20; // nap time before each write

static int    stn, area, word, bitop;
static u_char rbuf[MAXBYTES], wbuf[MAXBYTES];

#define gethbyt(w) ((u_char) ((w) >> 8 & 0xff))
#define getlbyt(w) ((u_char) ((w) &0xff))

static int fixaddress(int wreq, char *addr, int woffs, int nword);
static int response(int stn, int req, int nbyt);
static int s_cswrite(u_char *ps, unsigned len);

/*
 *        INIT
 *
 *    This routine initial stuff in the driver. This call is only
 *    done at first start of the driver, before calling any other
 *    routines in this file.
 */

void csinit(void)
{
    getchannelint(DRVCHNIX, NAPTIMEREAD, &naptimeread);
    getchannelint(DRVCHNIX, NAPTIMEWRITE, &naptimewrite);
}

int cssubscribe(char **addr, u_int *dtype, int naddr)
{
    return 0;
}

int csunsubscribe(char **addr, u_int *dtype, int naddr)
{
    return 0;
}

void csmain(void) { }

/*
 *        READ WORD
 *
 *    This routine reads a number of words in the device. Upon
 *    success the return value is 0 else the return value is -1.
 */

int csrdword(char **addr, u_int *dtype, int naddr, int woffs, int nword,
             long *data)
{
    int     i, j, reqin;
    short   val;
    u_char *pbuf;

    /* fix address, this also set the pointer plcs to correct cs */

    if (fixaddress(0, *addr, woffs, nword) < 0)
        return (-1);

    /* check if only address syntax check */

    if (!nword)
        return (0);

    /* check if any short nap before the request, don't download the
     * network */

    if (naptimeread)
        naptime(naptimeread);

    /* check if any socket yet */

    if (tcp_o_getsock() < 0) {
        log_event("create socket ...");
        // create socket, it has its own errcode generating
        if (tcp_o_createsock() < 0) {
            errcodeset(OECOMMDWN);
            errcodes[1] = 1;
            errcodes[2] = errno;
            return (-1);
        }
    }

    /* fix string to send */

    log_protocol("Read %s+%d,%d ...", *addr, woffs, nword);

    pbuf    = wbuf;
    *pbuf++ = 0;
    *pbuf++ = 0;
    *pbuf++ = 0;
    *pbuf++ = 0;
    *pbuf++ = gethbyt(6);
    *pbuf++ = getlbyt(6);
    *pbuf++ = getlbyt(stn);
    *pbuf++ = (u_char) area;
    *pbuf++ = gethbyt(word);
    *pbuf++ = getlbyt(word);
    *pbuf++ = gethbyt(nword);
    *pbuf++ = getlbyt(nword);

    // send on socket, it has its own errcode generating
    if (s_cswrite(wbuf, (unsigned) (pbuf - wbuf)) < 0) {
        if (tcp_o_getsock() >= 0) {
            log_event("close socket ...");
        }
        tcp_o_closesock();
        errcodeset(OECOMMDWN);
        errcodes[1] = 2;
        errcodes[2] = errno;
        return (-1);
    }

    /* wait for reply */

    // get reply, it has its own errcode generating
    if ((reqin = response(stn, area, 0)) < 0) {
        if (tcp_o_getsock() >= 0) {
            log_event("close socket ...");
        }
        tcp_o_closesock();
        return (-1);
    }

    if (reqin > 0x7f) {
        errcodeset(OECOMMDWN);
        errcodes[1] = 40;
        errcodes[2] = rbuf[0];
        return (-1);
    }

    /* fix word in answer */

    for (i = j = 0, pbuf = rbuf + 1; i < nword; i++) {
        if (area == 1 || area == 2) {
            val = (*pbuf >> j++) & 0x1;
            if (j > 8 - 1) {
                pbuf++;
                j = 0;
            }
        } else {
            val = (short) (*pbuf++ << 8);
            val |= *pbuf++;
        }
        *data++ = val;
    }

    return (0);
}

/*
 *        WRITE WORD
 *
 *    This routine writes a number of words to the device. Upon
 *    success the return value is 0 else the return value is -1.
 */

int cswrword(char **addr, u_int *dtype, int naddr, int woffs, int nword,
             long *data)
{
    int     i, reqin, val, bit;
    u_char *pbuf;

    /* fix address, this also set the pointer plcs to correct cs */

    if (fixaddress(1, *addr, woffs, nword) < 0)
        return (-1);

    /* check if only address syntax check */

    if (!nword)
        return (0);

    /* check if any short nap before the request, don't download the
     * network */

    if (naptimewrite)
        naptime(naptimewrite);

    /* check if any socket yet */

    if (tcp_o_getsock() < 0) {
        log_event("create socket ...");
        // create socket, it has its own errcode generating
        if (tcp_o_createsock() < 0) {
            errcodeset(OECOMMDWN);
            errcodes[1] = 3;
            errcodes[2] = errno;
            return (-1);
        }
    }

    /* fix string to send */

    log_protocol("Write %s+%d,%d ...", *addr, woffs, nword);

    pbuf    = wbuf;
    *pbuf++ = 0;
    *pbuf++ = 0;
    *pbuf++ = 0;
    *pbuf++ = 0;
    if (area == 5 || area == 6) {
        *pbuf++ = gethbyt(6);
        *pbuf++ = getlbyt(6);
        *pbuf++ = getlbyt(stn);
        *pbuf++ = (u_char) area;
        *pbuf++ = gethbyt(word);
        *pbuf++ = getlbyt(word);
        if (bitop) {
            if (*data) {
                *pbuf++ = gethbyt(0xff00);
                *pbuf++ = getlbyt(0xff00);
            } else {
                *pbuf++ = gethbyt(0x0000);
                *pbuf++ = getlbyt(0x0000);
            }
        } else {
            *pbuf++ = gethbyt(*data);
            *pbuf++ = getlbyt(*data);
        }
    } else {
        if (bitop) {
            *pbuf++ = gethbyt(7 + (nword - 1) / 8 + 1);
            *pbuf++ = getlbyt(7 + (nword - 1) / 8 + 1);
        } else {
            *pbuf++ = gethbyt(7 + nword * 2);
            *pbuf++ = getlbyt(7 + nword * 2);
        }
        *pbuf++ = getlbyt(stn);
        *pbuf++ = (u_char) area;
        *pbuf++ = gethbyt(word);
        *pbuf++ = getlbyt(word);
        *pbuf++ = gethbyt(nword);
        *pbuf++ = getlbyt(nword);
        if (bitop) {
            *pbuf++ = getlbyt((nword - 1) / 8 + 1);
            for (i = bit = 0; i < nword; i++, data++) {
                if (*data)
                    bit |= 1 << (i % 8);
                if (i % 8 == 0 && i != 0) {
                    *pbuf++ = bit;
                    bit     = 0;
                }
            }
            *pbuf++ = bit;
        } else {
            *pbuf++ = getlbyt(nword * 2);
            for (i = 0; i < nword; i++, data++) {
                *pbuf++ = gethbyt(*data);
                *pbuf++ = getlbyt(*data);
            }
        }
    }
    // send on socket, it has its own errcode generating
    if (s_cswrite(wbuf, (unsigned) (pbuf - wbuf)) < 0) {
        if (tcp_o_getsock() >= 0) {
            log_event("close socket ...");
        }
        tcp_o_closesock();
        errcodeset(OECOMMDWN);
        errcodes[1] = 4;
        errcodes[2] = errno;
        return (-1);
    }

    /* wait for reply */

    // get reply, it has its own errcode generating
    if ((reqin = response(stn, area, 4)) < 0) {
        if (tcp_o_getsock() >= 0) {
            log_event("close socket ...");
        }
        tcp_o_closesock();
        return (-1);
    }

    if (reqin > 0x7f) {
        errcodeset(OECOMMDWN);
        errcodes[1] = 40;
        errcodes[2] = rbuf[0];
        return (-1);
    }

    i   = rbuf[0] << 8 | rbuf[1];
    val = rbuf[2] << 8 | rbuf[3];
    if (bitop) {
        if (word != i) {
            errcodeset(OECOMMDWN);
            errcodes[1] = 42;
            errcodes[2] = i;
            errcodes[3] = val;
            return (-1);
        }
    } else {
        if (word != i || nword != val) {
            errcodeset(OECOMMDWN);
            errcodes[1] = 42;
            errcodes[2] = i;
            errcodes[3] = val;
            return (-1);
        }
    }

    return (0);
}

/*
 *        CLEANUP
 *
 *    This routine cleanup stuff in the driver. This call is only
 *    done at stop of the driver.
 */

void cscleanup(void)
{
    /* close all opened sockets */
    log_event("cleanup, try to close down everything");

    // check if socket created
    if (tcp_o_getsock() >= 0) {
        log_event("close socket ...");
        tcp_o_closesock();
    }
}

/*
 *        FIX ADDRESS
 *
 *    This routine fix the word address. Upon success the return value is 0
 *      else the return    value is -1.
 */

static int fixaddress(int wreq, char *addr, int woffs, int nword)
{
    int                   i;
    struct addresselement ele;

    if (validateaddress(&ele, drvname, ADDNAMEDEL, wreq, addr, woffs, nword) <
        0) {
        errcodeset(OECSBIGADDR);
        for (i = 0; i < NERRCODESZ; i++)
            errcodes[i] = ele.errcodes[i];
        return -1;
    }

    stn   = ele.stn;
    area  = ele.flag;
    word  = ele.word;
    bitop = ele.bitop;

    /* check limits */

    if ((!wreq && nword > maxrwords) || (wreq && nword > maxwwords)) {
        errcodeset(OECSBIGADDR);
        errcodes[1] = 4;
        return (-1);
    }

    return (0);
}

/*
 *        RESPONSE
 *
 *    This routine waits for a response from the device. If 'nbyt' is equal
 *    to 0, the first byte in <data> will indicate how many bytes that
 *    follows in the <data> area, otherwise 'nbyt' indicates the total
 *    number of bytes in <data> area. Upon success the routine returns
 *    function code, else -1.
 */

#define readbyt(ch)                                        \
    do {                                                   \
        /* read byte, it has its own errcode generating */ \
        ch = tcp_o_read();                                 \
        if (ch < 0) {                                      \
            errcodeset(OECOMMDWN);                         \
            errcodes[1] = 70;                              \
            log_protocol(in);                              \
            return (-1);                                   \
        }                                                  \
        if (get_remprotlog()) {                            \
            sprintf(pin, "%02X", ch);                      \
            pin += 2;                                      \
        }                                                  \
    } while (0)

#define retlog()          \
    do {                  \
        log_protocol(in); \
        return (-1);      \
    } while (0)

static int response(int stn, int req, int nbyt)
{
    int     i, c, reqin;
    char    in[MAXBYTES * 2 + 100], *pin;
    u_char *pbuf = rbuf;

    /* init */

    if (get_remprotlog()) {
        strcpy(in, "<<");
        pin = in + strlen(in);
    }

    /* start to receive */

    for (i = 0; i < 6; i++) { /* misc. redundance */
        readbyt(c);
    }

    readbyt(c); /* slave no. */
    if (c != stn) {
        while (tcp_o_read() >= 0)
            ;
        errcodeset(OECOMMDWN);
        errcodes[1] = 10;
        errcodes[2] = c;
        retlog();
    }
    readbyt(c); /* request */
    if ((reqin = c) > 0x7f) {
        c &= 0x7f;
        nbyt = 1;
    }
    if (c != req) {
        while (tcp_o_read() >= 0)
            ;
        errcodeset(OECOMMDWN);
        errcodes[1] = 31;
        errcodes[2] = reqin;
        retlog();
    }
    if (!nbyt) { /* byte count */
        readbyt(c);
        nbyt    = c;
        *pbuf++ = (u_char) c;
    }
    for (; nbyt; nbyt--) { /* remainign data */
        readbyt(c);
        *pbuf++ = (u_char) c;
    }

    log_protocol(in);

    return (reqin);
}

/*
 *        WRITE STRING TO SOCKET
 *
 *    This routine sends a string to the socket. The routine
 *    doesn't modify the string.
 */

static int s_cswrite(u_char *ps, unsigned len)
{
    int  i;
    char s[MAXBYTES * 2 + 100], *pss;

    /* write the string */

    if (get_remprotlog()) {
        strcpy(s, ">>");
        pss = s + strlen(s);
        for (i = 0; i < (int) len; i++) {
            sprintf(pss, "%02X", ps[i]);
            pss += 2;
        }
        log_protocol(s);
    }

    return (tcp_o_write(ps, len));
}

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
 *		M O D B U S    R T U    P R O T O C O L
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "neuron.h"
#include "plcaddress.h"
#include "driver.h"
#include "ttyocom.h"

int  MBSRTU; /* only a symbol for linker */
char drvname[] = "mbsrtu";

int maxrwords = 120;
int maxwwords = 120;

#define MAXBYTES 1 + 1 + 4 + 1 + 255 + 2

#define INITCRC 0xffff
#define CRCPOLY 0xa001

static int     ttyconnect = 0;
static int     stn, area, word, bitop;
static u_short crc;
static u_char  rbuf[MAXBYTES], wbuf[MAXBYTES];

#define gethbyt(w) ((u_char)((w) >> 8 & 0xff))
#define getlbyt(w) ((u_char)((w) &0xff))
#define initcrc()  (crc = (u_short) INITCRC)

static int  fixaddress(int wreq, char *addr, int woffs, int nword);
static int  response(int slave, int req, int nbyt);
static void calccrc(u_char byt);

/*
 *		INIT
 *
 *	This routine initial stuff in the driver. This call is only
 *	done at first start of the driver, before calling any other
 *	routines in this file.
 */

void
csinit(void)
{
}

int
cssubscribe(char **addr, u_int *dtype, int naddr)
{
	return 0;
}

int
csunsubscribe(char **addr, u_int *dtype, int naddr)
{
	return 0;
}

void
csmain(void)
{
}

/*
 *		READ WORD
 *
 *	This routine reads a number of words in the device. Upon
 *	success the return value is 0 else the return value is -1.
 */

int
csrdword(
    char **addr, u_int *dtype, int naddr, int woffs, int nword, long *data)
{
	int     i, j, reqin;
	short   val;
	u_char *pbuf, *pbuf2, *ps;
	char    s[MAXBYTES * 2 + 100], *pss;

	/* fix address */

	if (fixaddress(0, *addr, woffs, nword) < 0)
		return (-1);

	/* check if only address syntax check */

	if (!nword)
		return (0);

	/* open device */

	if (!ttyconnect) {
		log_event("open device file ...");
		if (cs_o_open() < 0) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 50;
			return (-1);
		}
		ttyconnect++;
	}

	/* fix string to send */

	log_protocol("Read %s+%d,%d ...", *addr, woffs, nword);

	ps = pbuf = wbuf;
	*pbuf++   = getlbyt(stn);
	*pbuf++   = (u_char) area;
	*pbuf++   = gethbyt(word);
	*pbuf++   = getlbyt(word);
	*pbuf++   = gethbyt(nword);
	*pbuf++   = getlbyt(nword);
	initcrc();
	for (pbuf2 = wbuf; pbuf2 < pbuf; pbuf2++)
		calccrc(*pbuf2);
	*pbuf++ = getlbyt(crc);
	*pbuf++ = gethbyt(crc);

	if (get_remprotlog()) {
		strcpy(s, ">>");
		pss = s + strlen(s);
		for (i = 0; i < (int) (pbuf - wbuf); i++) {
			sprintf(pss, "%02X", ps[i]);
			pss += 2;
		}
		log_protocol(s);
	}
	if (cs_o_write(wbuf, (unsigned) (pbuf - wbuf)) < 0) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 51;
		ttyconnect  = 0;
		return (-1);
	}

	/* wait for reply */

	if ((reqin = response(stn, area, 0)) < 0)
		return (-1);

	if (reqin > 0x7f) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 23;
		errcodes[2] = rbuf[0];
		return (-1);
	}

	//	if ((rbuf[0] & 0x1) || rbuf[0] / 2 != nword) {
	//		errcodeset(OECOMMDWN);
	//		errcodes[1] = 21;
	//		errcodes[2] = rbuf[0];
	//		return (-1);
	//	}

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
 *		WRITE WORD
 *
 *	This routine writes a number of words to the device. Upon
 *	success the return value is 0 else the return value is -1.
 */

int
cswrword(
    char **addr, u_int *dtype, int naddr, int woffs, int nword, long *data)
{
	int     i, reqin, val, bit;
	u_char *pbuf, *pbuf2, *ps;
	char    s[MAXBYTES * 2 + 100], *pss;

	/* fix address */

	if (fixaddress(1, *addr, woffs, nword) < 0)
		return (-1);

	/* check if only address syntax check */

	if (!nword)
		return (0);

	if (!ttyconnect) {
		log_event("open device file ...");
		if (cs_o_open() < 0) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 52;
			return (-1);
		}
		ttyconnect++;
	}

	/* fix string to send */

	log_protocol("Write %s+%d,%d ...", *addr, woffs, nword);

	ps = pbuf = wbuf;
	*pbuf++   = getlbyt(stn);
	*pbuf++   = (u_char) area;
	*pbuf++   = gethbyt(word);
	*pbuf++   = getlbyt(word);
	if (area == 5 || area == 6) {
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
	initcrc();
	for (pbuf2 = wbuf; pbuf2 < pbuf; pbuf2++)
		calccrc(*pbuf2);
	*pbuf++ = getlbyt(crc);
	*pbuf++ = gethbyt(crc);

	if (get_remprotlog()) {
		strcpy(s, ">>");
		pss = s + strlen(s);
		for (i = 0; i < (int) (pbuf - wbuf); i++) {
			sprintf(pss, "%02X", ps[i]);
			pss += 2;
		}
		log_protocol(s);
	}
	if (cs_o_write(wbuf, (unsigned) (pbuf - wbuf)) < 0) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 53;
		ttyconnect  = 0;
		return (-1);
	}

	/* wait for reply */

	if ((reqin = response(stn, area, 4)) < 0)
		return (-1);

	if (reqin > 0x7f) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 27;
		errcodes[2] = rbuf[0];
		return (-1);
	}

	i   = rbuf[0] << 8 | rbuf[1];
	val = rbuf[2] << 8 | rbuf[3];
	if (bitop) {
		if (word != i) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 22;
			errcodes[2] = i;
			errcodes[3] = val;
			return (-1);
		}
	} else {
		if (word != i || nword != val) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 22;
			errcodes[2] = i;
			errcodes[3] = val;
			return (-1);
		}
	}

	return (0);
}

/*
 *		CLEANUP
 *
 *	This routine cleanup stuff in the driver. This call is only
 *	done at stop of the driver.
 */

void
cscleanup(void)
{
}

/*
 *		FIX ADDRESS
 *
 *	This routine fix the word address. Upon success the return value is 0
 *      else the return	value is -1.
 */

static int
fixaddress(int wreq, char *addr, int woffs, int nword)
{
	int                   i;
	struct addresselement ele;

	if (validateaddress(
	        &ele, drvname, ADDNAMEDEL, wreq, addr, woffs, nword) < 0) {
		errcodeset(OECSBIGADDR);
		for (i = 0; i < NERRCODESZ; i++)
			errcodes[i] = ele.errcodes[i];
		return (-1);
	}

	stn   = ele.stn;
	area  = ele.flag;
	word  = ele.word;
	bitop = ele.bitop;

	if ((!wreq && nword > maxrwords) || (wreq && nword > maxwwords)) {
		errcodeset(OECSBIGADDR);
		errcodes[1] = 4;
		return (-1);
	}

	return (0);
}

/*
 *		RESPONSE
 *
 *	This routine waits for a response from the device. If 'nbyt' is equal
 *	to 0, the first byte in <data> will indicate how many bytes that
 *	follows in the <data> area, otherwise 'nbyt' indicates the total
 *	number of bytes in <data> area. Upon success the routine returns
 *	function code, else -1.
 */

#define addlog()                                 \
	do {                                     \
		if (get_remprotlog()) {          \
			sprintf(pin, "%02X", c); \
			pin += 2;                \
		}                                \
	} while (0)

#define retlog()                  \
	do {                      \
		log_protocol(in); \
		return (-1);      \
	} while (0)

static int
response(int slave, int req, int nbyt)
{
	int     c, reqin, crcin;
	char    in[MAXBYTES * 2 + 100], *pin;
	u_char *pbuf = rbuf;

	if (get_remprotlog()) {
		strcpy(in, "<<");
		pin = in + strlen(in);
	}

	initcrc();

	calccrc((u_char)(c = cs_o_read())); /* slave no. */
	if (c < 0) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 1;
		retlog();
	}
	addlog();
	if (c != slave) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 10;
		errcodes[2] = c;
		while (cs_o_read() >= 0)
			;
		retlog();
	}
	calccrc((u_char)(c = cs_o_read())); /* request */
	if (c < 0) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 2;
		retlog();
	}
	addlog();
	if ((reqin = c) > 0x7f) {
		c &= 0x7f;
		nbyt = 1;
	}
	if (c != req) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 11;
		errcodes[2] = reqin;
		while (cs_o_read() >= 0)
			;
		retlog();
	}
	if (!nbyt) { /* byte count */
		calccrc((u_char)(c = cs_o_read()));
		if (c < 0) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 2;
			retlog();
		}
		addlog();
		nbyt    = c;
		*pbuf++ = (u_char) c;
	}
	for (; nbyt; nbyt--) { /* remaining data */
		calccrc((u_char)(c = cs_o_read()));
		if (c < 0) {
			errcodeset(OECOMMDWN);
			errcodes[1] = 2;
			retlog();
		}
		addlog();
		*pbuf++ = (u_char) c;
	}
	if ((crcin = cs_o_read()) < 0) { /* crc-16 */
		errcodeset(OECOMMDWN);
		errcodes[1] = 2;
		retlog();
	}
	addlog();
	if ((c = cs_o_read()) < 0) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 2;
		retlog();
	}
	addlog();
	if ((crcin |= c << 8) != (int) crc) {
		errcodeset(OECOMMDWN);
		errcodes[1] = 12;
		errcodes[2] = crcin;
		while (cs_o_read() >= 0)
			;
		retlog();
	}

	log_protocol(in);

	return (reqin);
}

/*
 *		CALCULATE CRC-16
 *
 *	This routine calculates the crc-16 value.
 */

static void
calccrc(u_char byt)
{
	int i, usepoly;

	crc ^= byt;
	for (i = 0; i < 8; i++) {
		usepoly = crc & 0x1;
		crc >>= 1;
		if (usepoly)
			crc ^= CRCPOLY;
	}
}

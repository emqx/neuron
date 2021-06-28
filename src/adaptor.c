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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <time.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>


char progname[] = "DRVR";

int errcodes[NERRCODESZ];

static void ccs_checkreq(void);
// static void ccs_checksub(void);
static void rdwordcs(struct omsg_hdr *phdr, struct mc_rdwordcs *pcbuf);
static void wrwordcs(struct omsg_hdr *phdr, struct mc_wrwordcs *pcbuf);

static void log_errcode(char *error);

static int gdb_driverqid;

/*
 *		COMMUNICATION CONTROL SYSTEM
 *
 *	This program handles the communication with the control system.
 */

int
main(int argc, char *argv[])
{
	void *ptr;
	int   gdb_mainqid, gdb_returnqid;

	/* initial the instance number and path */

	if (argc == 2 && isstringdigit(argv[1]))
		setinstanceno(atoi(argv[1]));
	chdir(basepath(argv[0]));

	/* get ini settings */

	get_ini_loglevel();

	/* setup all ipc resources */

	if (createsems() < 0) {
		log_err("create semaphores error");
		exit(1);
	}

	if ((gdb_mainqid = getmainqid()) < 0) {
		log_err("open main message queue error");
		exit(1);
	}

	if ((gdb_returnqid = getreturnqid()) < 0) {
		log_err("open return message queue error");
		exit(1);
	}
	initdbvar(gdb_mainqid, gdb_returnqid);

	if ((gdb_driverqid = getdriverqid()) < 0) {
		log_err("open driver message queue error");
		removereturnqid();
		exit(1);
	}

	if (attachshms(&ptr) < 0) {
		log_err("attach share memory error");
		removedriverqid();
		removereturnqid();
		exit(1);
	}
	initshmpnt(ptr);

	/* start  */

	setsighandler();

	/* set to "en_US.UTF-8" locale LANG */

	setlocale(LC_ALL, "en_US.UTF-8");

	sleep(1);

	/* driver initialization */
	csinit();
	log_info("driver parameters setup done");

	/* driver loop */
	while (1) {
		//        ccs_checksub();
		ccs_checkreq();
		//        csmain();
	}

	return 0;
}

// void ccs_checksub(void)
// {
//     u_int i = 0;
//     int j = 0, cnt = 0, ucnt = 0;
//     struct tagfield *pfld;
//     char **csaddr = NULL;
//     u_int *dtype = NULL;

//     if (gettagfield(0, &i, &pfld) < 0)
//         return;
//     while (1) {
//         if (pfld->tagattr[0] == 'S' && !(pfld->status & TAG_SUBSCRIBED))
//             cnt++;
//         if (pfld->status & TAG_LOCKUP & !(pfld->status & TAG_DISMISSED))
//             ucnt++;
//         if (getnexttagfield(&pfld) < 0)
//             break;
//     }
//     if (cnt)
//     {
//         if ((csaddr = (char **) malloc(cnt * sizeof(char *))) == NULL) {
//             log_err("csaddr malloc error");
//             cleanup();
//             exit(1);
//         }
//         if ((dtype = (u_int *) malloc(cnt * sizeof(u_int))) == NULL) {
//             log_err("dtype malloc error");
//             cleanup();
//             exit(1);
//         }
//         if (gettagfield(0, &i, &pfld) < 0)
//             return;
//         j = 0;
//         while (1) {
//             if (pfld->tagattr[0] == 'S' && !(pfld->status & TAG_SUBSCRIBED))
//             {
//                 csaddr[j] = pfld->tagaddr;
//                 dtype[j] = pfld->dtype;
//                 pfld->status |= TAG_SUBSCRIBED;
//                 if (++j > cnt - 1)
//                     break;
//             }
//             if (getnexttagfield(&pfld) < 0)
//                 break;
//         }
//         if (cssubscribe(csaddr, dtype, cnt) < 0)
//             log_err("subscription error");
//         free(csaddr);
//         free(dtype);
//     }

//     if (ucnt)
//     {
//         if ((csaddr = (char **) malloc(ucnt * sizeof(char *))) == NULL) {
//             log_err("csaddr malloc error");
//             cleanup();
//             exit(1);
//         }
//         if ((dtype = (u_int *) malloc(ucnt * sizeof(u_int))) == NULL) {
//             log_err("dtype malloc error");
//             cleanup();
//             exit(1);
//         }
//         if (gettagfield(0, &i, &pfld) < 0)
//             return;
//         j = 0;
//         while (1) {
//             if (pfld->status & TAG_LOCKUP & !(pfld->status & TAG_DISMISSED))
//             {
//                 csaddr[j] = pfld->tagaddr;
//                 dtype[j] = pfld->dtype;
//                 pfld->status |= TAG_DISMISSED;
//                 if (++j > ucnt - 1)
//                     break;
//             }
//             if (getnexttagfield(&pfld) < 0)
//                 break;
//         }
//         if (csunsubscribe(csaddr, dtype, ucnt) < 0)
//             log_err("subscription error");
//         free(csaddr);
//         free(dtype);
//     }

// }

int
csupdreg(char **addr, int naddr, int nword, long *data)
{
	u_int            i     = 0;
	int              found = 0, nelem;
	u_long           status;
	struct tagfield *pfld;

	if (gettagfield(0, &i, &pfld) < 0)
		return -1;
	while (1) {
		if (pfld->tagaddr == *addr) {
			found++;
			break;
		}
		if (getnexttagfield(&pfld) < 0)
			break;
	}
	if (found) {
		nelem  = nword;
		status = 0;
		putldbvar(pfld->tagname, 0, &nelem, &status, data);
		return 0;
	}
	return -1;
}

/*
 *		CHECK REQUEST FROM OTHERS
 *
 *	This program handles the communication with the control system,
 *	and check for request in from other programs.
 */

void
ccs_checkreq(void)
{
	int             nbytes;
	u_long          cmd;
	struct m_std    mbuf, *prm;
	struct omsg_hdr mhdr;

	/* wait for a message */
	prm = o_recmsg(0, gdb_driverqid, 0, 0, 0, &cmd, &nbytes, "wait msg");
	memcpy(&mbuf, prm, cmd & OM_ERR ? (int) sizeof mbuf : nbytes);
	mhdr = precmsg->hdr;

	/* reply received */
	if (cmd & OM_RSTAT) {
		log_err("driver unexpected reply req=%d from qid=%d", mbuf.req,
		    mhdr.returnqid);

		/* command received */
	} else {
		if (mbuf.req == OMR_RDWORDCS) {
			//            log_debug("into read word");
			rdwordcs(&mhdr, (struct mc_rdwordcs *) &mbuf);
		} else if (mbuf.req == OMR_WRWORDCS) {
			//            log_debug("into write word");
			wrwordcs(&mhdr, (struct mc_wrwordcs *) &mbuf);
		} else {
			log_err("driver unknowed command req=%d from qid=%d",
			    mbuf.req, mhdr.returnqid);
		}
	} /* command received */
}

/*
 *		READ WORD
 *
 *	This routine read words.
 */

static void
rdwordcs(struct omsg_hdr *phdr, struct mc_rdwordcs *pcbuf)
{
	int    i, naddr, nbytes, ret, aofs, wofs, remelem, allelem, nelem;
	u_long cmd;
	u_int *dtype = NULL, *pdtype = NULL;
	struct mr_rdwordcs *prbuf;
	struct tagfield *   pfld, *ppfld;
	long *              wcs    = NULL, *pwcs;
	char **             csaddr = NULL, **pcsaddr = NULL, *ppcsaddr = NULL;

	cmd    = phdr->cmd;
	nbytes = phdr->nbytes;
	prbuf  = (struct mr_rdwordcs *) pcbuf;

	wofs    = pcbuf->woffset;
	remelem = 0;

	if (pcbuf->status & OMS_RDCSSTRUCT) {
		//        log_debug("OMS read structure received csaddr:%s",
		//        pcbuf->csaddr);
		if (gettagfield(pcbuf->csaddr, 0, &pfld) < 0) {
			log_err("get rtag field error");
			if (cmd & OM_REPLY) {
				cmd |= OM_ERR;
				o_sendmsg(0, phdr->returnqid, STDMTYPE,
				    phdr->returnqid, phdr->msgno, &cmd,
				    &nbytes, pcbuf, 0, "error get read addr");
			}
			return;
		}
		//        log_debug("read structure received size:%d",
		//        pfld->tagsz);
		ppfld = pfld;
		i     = 0;
		while (1) {
			if (getnexttagfield(&ppfld) < 0) {
				log_debug("read next tag structure error");
				return;
			}
			//            log_debug("read structure received member
			//            size:%d", ppfld->tagsz);
			remelem += ppfld->tagsz;
			if (++i >= pfld->tagsz)
				break;
		}
		//        log_debug("read total no. of elements %d", remelem);
		if (pfld->tagname[1] == 'V') {
			if ((csaddr = (char **) malloc(
			         pfld->tagsz * sizeof(char *))) == NULL) {
				log_err("csaddr malloc error");
				cleanup();
				exit(1);
			}
			if ((dtype = (u_int *) malloc(
			         pfld->tagsz * sizeof(u_int))) == NULL) {
				log_err("dtype malloc error");
				cleanup();
				exit(1);
			}
			pdtype = dtype;
			ppfld  = pfld;
			i      = 0;
			while (1) {
				if (getnexttagfield(&ppfld) < 0) {
					log_debug(
					    "read next tag structure error");
					return;
				}
				dtype[i]      = ppfld->dtype;
				*(csaddr + i) = &ppfld->tagaddr[1];
				if (++i >= pfld->tagsz)
					break;
			}
			pcsaddr = csaddr;
		} else {
			ppfld = pfld;
			if (getnexttagfield(&ppfld) < 0) {
				log_debug("read next tag structure error");
				return;
			}
			pcsaddr  = &ppcsaddr;
			ppcsaddr = &ppfld->tagaddr[1];
			pdtype   = &pcbuf->dtype;
		}
		//    } else if (pcbuf->status & OMS_RDCSARRAY) {
		//        ;
	} else {
		log_debug("OMS read single element received");
		remelem  = pcbuf->nelem;
		pcsaddr  = &ppcsaddr;
		ppcsaddr = pcbuf->csaddr;
		pdtype   = &pcbuf->dtype;
	}
	//    log_debug("wcs malloc memeory");
	if ((wcs = (long *) malloc(remelem * sizeof(long))) == NULL) {
		log_err("read wcs memory allocation error");
		cleanup();
		exit(1);
	}
	pwcs    = wcs;
	allelem = remelem;

	ppfld = pfld;
	if (getnexttagfield(&ppfld) < 0) {
		log_debug("read next tag structure error");
		return;
	}
	i = aofs = 0;
	while (remelem) { /* read all words */
		if (pcbuf->status & OMS_RDCSSTRUCT) {
			//            log_debug("get into structure tag
			//            process");
			naddr = 0;
			nelem = 0;
			while (1) {
				if (nelem + ppfld->tagsz > maxrwords)
					break;
				naddr++;
				nelem += ppfld->tagsz;
				if (getnexttagfield(&ppfld) < 0) {
					log_debug(
					    "read next tag structure error");
					break;
				}
				if (++i >= pfld->tagsz)
					break;
			}
			if (pfld->tagname[1] == 'R') {
				aofs  = 0;
				naddr = 1;
			} else {
				pcsaddr = csaddr + aofs;
				pdtype  = dtype + aofs;
			}
		} else {
			//            log_debug("get into single tag process");
			aofs  = 0;
			naddr = 1;
			nelem = remelem > maxrwords ? maxrwords : remelem;
		}
		//        log_debug("read %s(ofs:%+d,nelem:%d,remelem:%d),
		//        aofs:%d/naddr:%d",
		//                                *pcsaddr, wofs, nelem,
		//                                remelem, aofs, naddr);
		ret = csrdword(pcsaddr, pdtype, naddr, wofs, nelem, pwcs);
		if (ret < 0) {
			log_protocol("Read %s(%+d,%d,%d), %d/%d", *pcsaddr,
			    wofs, nelem, remelem, aofs, naddr);
			log_errcode("prev");
			if (cmd & OM_REPLY) {
				cmd |= OM_ERR;
				o_sendmsg(0, phdr->returnqid, STDMTYPE,
				    phdr->returnqid, phdr->msgno, &cmd,
				    &nbytes, pcbuf, errcodes[0],
				    "error read word");
			}
			if (wcs) {
				free(wcs);
				wcs = NULL;
			}
			if (dtype) {
				free(dtype);
				dtype = NULL;
			}
			if (csaddr) {
				free(csaddr);
				csaddr = NULL;
			}
			return;
		}
		remelem -= nelem;
		pwcs += nelem;
		wofs += nelem;
		aofs += naddr;
	} /* read all words */

	if (pcbuf->status & OMS_UNSIGNED)
		for (i = 0; i < pcbuf->nelem; i++)
			wcs[i] &= 0xffff;

	if (cmd & OM_REPLY) { /* reply */
		remelem = allelem;
		pwcs    = wcs;
		cmd |= OM_ACK;
		while (remelem) {
			nelem = remelem > NCSREADELEM ? NCSREADELEM : remelem;
			memcpy(prbuf->data, pwcs, nelem * sizeof(long));
			pwcs += nelem;
			remelem -= nelem;
			if (remelem)
				cmd |= OM_NOTCOMPL;
			else
				cmd &= ~OM_NOTCOMPL;
			nbytes = sizeof(struct mr_rdwordcsnoval) +
			    nelem * sizeof(long);
			//            log_debug("reply");
			o_sendmsg(0, phdr->returnqid, STDMTYPE,
			    phdr->returnqid, phdr->msgno, &cmd, &nbytes, prbuf,
			    0, "reply read word");
		}
	} /* reply */
	if (wcs) {
		free(wcs);
		wcs = NULL;
	}
	if (dtype) {
		free(dtype);
		dtype = NULL;
	}
	if (csaddr) {
		free(csaddr);
		csaddr = NULL;
	}
	return;
}

/*
 *		WRITE WORD
 *
 *	This routine write words.
 */

static void
wrwordcs(struct omsg_hdr *phdr, struct mc_wrwordcs *pcbuf)
{
	int    i, naddr, nbytes, aofs, wofs, remelem, nelem, firstrply;
	u_long cmd;
	u_int *dtype               = NULL, *pdtype;
	struct omsg_hdr *   phdr2  = phdr;
	struct mc_wrwordcs *pcbuf2 = pcbuf;
	struct mr_wrwordcs *prbuf;
	struct tagfield *   pfld, *ppfld;
	long *              wcs    = NULL, *pwcs;
	char **             csaddr = NULL, **pcsaddr = NULL, *ppcsaddr = NULL;

	cmd    = phdr->cmd;
	nbytes = phdr->nbytes;
	prbuf  = (struct mr_wrwordcs *) pcbuf;

	wofs    = pcbuf->woffset;
	remelem = 0;

	if (pcbuf->status & OMS_WRCSSTRUCT) {
		log_err("OMS read structure received");
		if (gettagfield(pcbuf->csaddr, 0, &pfld) < 0) {
			log_err("get wtag field error");
			if (cmd & OM_REPLY) {
				cmd |= OM_ERR;
				o_sendmsg(0, phdr->returnqid, STDMTYPE,
				    phdr->returnqid, phdr->msgno, &cmd,
				    &nbytes, pcbuf, 0, "error get write addr");
			}
			return;
		}
		log_debug("write structure received size:%d", pfld->tagsz);
		ppfld = pfld;
		i     = 0;
		while (1) {
			if (getnexttagfield(&ppfld) < 0) {
				log_debug("write next tag structure error");
				return;
			}
			log_debug("write structure received member size:%d",
			    ppfld->tagsz);
			remelem += ppfld->tagsz;
			if (++i >= pfld->tagsz)
				break;
		}
		log_debug("write total no. of elements %d", remelem);
		if (pfld->tagname[1] == 'V') {
			if ((csaddr = (char **) malloc(
			         pfld->tagsz * sizeof(char *))) == NULL) {
				log_err("csaddr malloc error");
				cleanup();
				exit(1);
			}
			if ((dtype = (u_int *) malloc(
			         pfld->tagsz * sizeof(u_int))) == NULL) {
				log_err("dtype malloc error");
				cleanup();
				exit(1);
			}
			pdtype = dtype;
			ppfld  = pfld;
			i      = 0;
			while (1) {
				if (getnexttagfield(&ppfld) < 0) {
					log_debug(
					    "write next tag structure error");
					return;
				}
				dtype[i]      = ppfld->dtype;
				*(csaddr + i) = &ppfld->tagaddr[1];
				if (++i >= pfld->tagsz)
					break;
			}
			pcsaddr = csaddr;
		} else {
			ppfld = pfld;
			if (getnexttagfield(&ppfld) < 0) {
				log_debug("write next tag structure error");
				return;
			}
			pcsaddr  = &ppcsaddr;
			ppcsaddr = &ppfld->tagaddr[1];
			pdtype   = &pcbuf->dtype;
		}
	} else {
		log_debug("OMS write single element received");
		remelem  = pcbuf->nelem;
		pcsaddr  = &ppcsaddr;
		ppcsaddr = pcbuf->csaddr;
		pdtype   = &pcbuf->dtype;
	}
	if ((wcs = (long *) malloc(remelem * sizeof(long))) == NULL) {
		log_err("write wcs memory allocation error");
		cleanup();
		exit(1);
	}
	pwcs = wcs;

	firstrply = 0;
	while (1) { /* get all words */
		if (!firstrply && cmd & OM_NOTCOMPL) {
			if (cmd & OM_REPLY) {
				cmd |= OM_ACK;
				i = sizeof(struct mr_wrwordcs);
				o_sendmsg(0, phdr->returnqid, STDMTYPE,
				    phdr->returnqid, phdr->msgno, &cmd, &i,
				    prbuf, 0, "first reply write word");
			}
			firstrply++;
		}
		i = nbytes - sizeof(struct mc_wrwordcsnoval);
		memcpy(pwcs, pcbuf2->data, i);
		pwcs += i / sizeof(long);
		if (!(cmd & OM_NOTCOMPL))
			break;
		pcbuf2 = o_recmsg(RM_COMMAND | RM_EQUAL, gdb_driverqid, 0,
		    phdr2->returnqid, phdr2->msgno, &cmd, &nbytes,
		    "wait next msg");
		phdr2  = &precmsg->hdr;
	} /* get all words */

	pwcs  = wcs;
	ppfld = pfld;
	if (getnexttagfield(&ppfld) < 0) {
		log_debug("write next tag structure error");
		return;
	}
	i = aofs = 0;
	while (remelem) { /* write all words */
		if (pcbuf->status & OMS_WRCSSTRUCT) {
			log_debug("get into structure tag process");
			naddr = 0;
			nelem = 0;
			while (1) {
				if (nelem + ppfld->tagsz > maxwwords)
					break;
				naddr++;
				nelem += ppfld->tagsz;
				if (getnexttagfield(&ppfld) < 0) {
					log_debug(
					    "write next tag structure error");
					break;
				}
				if (++i >= pfld->tagsz)
					break;
			}
			if (pfld->tagname[1] == 'R') {
				aofs  = 0;
				naddr = 1;
			} else {
				pcsaddr = csaddr + aofs;
				pdtype  = dtype + aofs;
			}
			//        } else if (pcbuf->status & OMS_WRCSARRAY) {
			//            ;
		} else {
			aofs = 0;

			naddr = 1;
			nelem = remelem > maxwwords ? maxwwords : remelem;
		}
		log_debug(
		    "write %s(ofs:%+d,nelem:%d,remelem:%d), aofs:%d/naddr:%d",
		    *pcsaddr, wofs, nelem, remelem, aofs, naddr);
		if (cswrword(pcsaddr, pdtype, naddr, wofs, nelem, pwcs) < 0) {
			log_protocol("Write %s(%+d,%d,%d), %d/%d", *pcsaddr,
			    wofs, nelem, remelem, aofs, naddr);
			log_errcode("prev");
			if (cmd & OM_REPLY) {
				cmd |= OM_ERR;
				i = phdr->nbytes;
				o_sendmsg(0, phdr->returnqid, STDMTYPE,
				    phdr->returnqid, phdr->msgno, &cmd, &i,
				    pcbuf, errcodes[0], "error write word");
			}
			if (wcs) {
				free(wcs);
				wcs = NULL;
			}
			if (dtype) {
				free(dtype);
				dtype = NULL;
			}
			if (csaddr) {
				free(csaddr);
				csaddr = NULL;
			}
			return;
		}
		remelem -= nelem;
		pwcs += nelem;
		wofs += nelem;
		aofs += naddr;
	} /* write all words */

	if (cmd & OM_REPLY) {
		cmd |= OM_ACK;
		nbytes = sizeof(struct mr_wrwordcs);
		o_sendmsg(0, phdr->returnqid, STDMTYPE, phdr->returnqid,
		    phdr->msgno, &cmd, &nbytes, prbuf, 0, "reply write word");
	}
	if (wcs) {
		free(wcs);
		wcs = NULL;
	}
	if (dtype) {
		free(dtype);
		dtype = NULL;
	}
	if (csaddr) {
		free(csaddr);
		csaddr = NULL;
	}
	return;
}

/*
 *		SLEEP SHORT TIME
 *
 *	This routine sleeps for a time.
 *
 *	The routine sleeps for msec millie-seconds and then return.
 */

void
naptime(int msec)
{
	struct timespec req;
	struct timespec rem;

	// set the requested time
	req.tv_sec  = msec / 1000;
	req.tv_nsec = (msec % 1000) * 1000 * 1000;

	// loop until time is out
	while (1) {
		if (nanosleep(&req, &rem) >= 0)
			return;
		req = rem;
	}
}

static FILE *fpclog     = NULL;
static int   remprotlog = 0;

/*
 *		GET REMAIN PROTOCOL LOG NUMBER
 */

int
get_remprotlog(void)
{
	return remprotlog;
}

/*
 *		LOG PROTOCOL
 */

void
log_protocol(char *prot, ...)
{
	char    tstr[TIMESTRSZ + 1] = "", msg[LOGMSGSZ + 1] = "";
	time_t  date;
	va_list args;

	checkdriverloglines(&remprotlog);
	if (!remprotlog) {
		return;
	}
	if (!fpclog) {
		if ((fpclog = fopen(getvarpath(LOGPATH, DRVFILE), "a")) ==
		    NULL) {
			remprotlog = 0;
			return;
		}
		date = time((long *) 0);
		strftime(tstr, (size_t) TIMESTRSZ, "%x %X", localtime(&date));
		tstr[TIMESTRSZ] = '\0';
		fprintf(fpclog, "\n*** %s %s: Driver log started\n\n", tstr,
		    progname);
		fflush(fpclog);
	}

	va_start(args, prot);
	vsnprintf(msg, LOGMSGSZ, prot, args);
	msg[LOGMSGSZ] = '\0';
	date          = time((long *) 0);
	strftime(tstr, (size_t) TIMESTRSZ, "%X", localtime(&date));
	tstr[TIMESTRSZ] = '\0';
	fprintf(fpclog, "%s %s\n", tstr, msg);
	fflush(fpclog);
	va_end(args);

	if (!--remprotlog) {
		if (fpclog)
			fclose(fpclog);
		fpclog = NULL;
	}
}

/*
 *		LOG ERROR
 */

static void
log_errcode(char *error)
{
	int    i;
	char   tstr[TIMESTRSZ + 1] = "";
	time_t date;

	if (!remprotlog)
		return;
	if (!fpclog)
		return;
	date = time((long *) 0);
	strftime(tstr, (size_t) TIMESTRSZ, "%X", localtime(&date));
	tstr[TIMESTRSZ] = '\0';
	fprintf(fpclog, "*** %s Error %s req., error ", tstr, error);
	for (i = 0; i < NERRCODESZ; i++)
		fprintf(fpclog, "%d%s", errcodes[i],
		    i == NERRCODESZ - 1 ? "\n" : ", ");
	fflush(fpclog);

	if (!--remprotlog) {
		fclose(fpclog);
		fpclog = NULL;
	}
}

/*
 *		DEBUG LOG
 *
 *	This routine log debug messages.
 */

int
getldbvar(char *pdbvar, int ix, int *pnelem, u_long *pstatus, dbvar_t *pvar)
{
	return getdbvar(pdbvar, ix, pnelem, pstatus, pvar);
}

int
putldbvar(char *pdbvar, int ix, int *pnelem, u_long *pstatus, dbvar_t *pvar)
{
	return putdbvar(pdbvar, ix, pnelem, pstatus, pvar);
}

/*
 *		CLEANUP
 *
 *	Cleanup routine after receiving a signal.
 */

void
cleanup(void)
{
	setsigignore(IGNON);
	cscleanup();
	detachshms();
	removedriverqid();
	removereturnqid();
	setsigignore(IGNOFF);
}

/* vinuminterrupt.c: bottom half of the driver */

/*-
 * Copyright (c) 1997, 1998, 1999
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinuminterrupt.c,v 1.12 1999/08/07 08:06:30 grog Exp $
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>
#include <sys/resourcevar.h>

void complete_raid5_write(struct rqelement *);
void freerq(struct request *rq);
void free_rqg(struct rqgroup *rqg);
void complete_rqe(struct buf *bp);
void sdio_done(struct buf *bp);

/*
 * Take a completed buffer, transfer the data back if
 * it's a read, and complete the high-level request
 * if this is the last subrequest.
 *
 * The bp parameter is in fact a struct rqelement, which
 * includes a couple of extras at the end.
 */
void 
complete_rqe(struct buf *bp)
{
    struct rqelement *rqe;
    struct request *rq;
    struct rqgroup *rqg;
    struct buf *ubp;					    /* user buffer */

    rqe = (struct rqelement *) bp;			    /* point to the element element that completed */
    rqg = rqe->rqg;					    /* and the request group */
    rq = rqg->rq;					    /* and the complete request */
    ubp = rq->bp;					    /* user buffer */

#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_iodone, (union rqinfou) rqe, ubp);
#endif
    if ((bp->b_flags & B_ERROR) != 0) {			    /* transfer in error */
	if (bp->b_error != 0)				    /* did it return a number? */
	    rq->error = bp->b_error;			    /* yes, put it in. */
	else if (rq->error == 0)			    /* no: do we have one already? */
	    rq->error = EIO;				    /* no: catchall "I/O error" */
	SD[rqe->sdno].lasterror = rq->error;
	if (bp->b_flags & B_READ) {
	    log(LOG_ERR, "%s: fatal read I/O error\n", SD[rqe->sdno].name);
	    set_sd_state(rqe->sdno, sd_crashed, setstate_force); /* subdisk is crashed */
	} else {					    /* write operation */
	    log(LOG_ERR, "%s: fatal write I/O error\n", SD[rqe->sdno].name);
	    set_sd_state(rqe->sdno, sd_stale, setstate_force); /* subdisk is stale */
	}
	if (rq->error == ENXIO) {			    /* the drive's down too */
	    log(LOG_ERR, "%s: fatal drive I/O error\n", DRIVE[rqe->driveno].label.name);
	    DRIVE[rqe->driveno].lasterror = rq->error;
	    set_drive_state(rqe->driveno,		    /* take the drive down */
		drive_down,
		setstate_force);
	}
    }
    /* Now update the statistics */
    if (bp->b_flags & B_READ) {				    /* read operation */
	DRIVE[rqe->driveno].reads++;
	DRIVE[rqe->driveno].bytes_read += bp->b_bcount;
	SD[rqe->sdno].reads++;
	SD[rqe->sdno].bytes_read += bp->b_bcount;
	PLEX[rqe->rqg->plexno].reads++;
	PLEX[rqe->rqg->plexno].bytes_read += bp->b_bcount;
    } else {						    /* write operation */
	DRIVE[rqe->driveno].writes++;
	DRIVE[rqe->driveno].bytes_written += bp->b_bcount;
	SD[rqe->sdno].writes++;
	SD[rqe->sdno].bytes_written += bp->b_bcount;
	PLEX[rqe->rqg->plexno].writes++;
	PLEX[rqe->rqg->plexno].bytes_written += bp->b_bcount;
    }
    rqg->active--;					    /* one less request active */
    if (rqg->flags & XFR_RECOVERY_READ) {		    /* recovery read, */
	int *sdata;					    /* source */
	int *data;					    /* and group data */
	int length;					    /* and count involved */
	int count;					    /* loop counter */
	struct rqelement *urqe = &rqg->rqe[rqg->badsdno];   /* rqe of the bad subdisk */

	/* XOR destination is the user data */
	sdata = (int *) &rqe->b.b_data[rqe->groupoffset << DEV_BSHIFT];	/* old data contents */
	data = (int *) &urqe->b.b_data[urqe->groupoffset << DEV_BSHIFT]; /* destination */
	length = urqe->grouplen << (DEV_BSHIFT - 2);	    /* and count involved */

	for (count = 0; count < length; count++)
	    data[count] ^= sdata[count];

#ifdef VINUMDEBUG
	if (debug & DEBUG_RESID) {
	    if ((rqg->active == 0)			    /* XXXX finished this group */
	    &&(*(char *) data != '<'))			    /* and not what we expected */
		Debugger("complete_request checksum");
	}
#endif

	/*
	 * In a normal read, we will normally read directly
	 * into the user buffer.  This doesn't work if
	 * we're also doing a recovery, so we have to
	 * copy it 
	 */
	if (rqe->flags & XFR_NORMAL_READ) {		    /* normal read as well, */
	    char *src = &rqe->b.b_data[rqe->dataoffset << DEV_BSHIFT]; /* read data is here */
	    char *dst;

	    dst = (char *) ubp->b_data + (rqe->useroffset << DEV_BSHIFT); /* where to put it in user buffer */
	    length = rqe->datalen << DEV_BSHIFT;	    /* and count involved */
	    bcopy(src, dst, length);			    /* move it */
	}
    } else if ((rqg->flags & (XFR_NORMAL_WRITE | XFR_DEGRADED_WRITE)) /* RAID 5 group write operation  */
    &&(rqg->active == 0))				    /* and we've finished phase 1 */
	complete_raid5_write(rqe);
    if (rqg->active == 0)				    /* request group finished, */
	rq->active--;					    /* one less */
    if (rq->active == 0) {				    /* request finished, */
#if VINUMDEBUG
	if (debug & DEBUG_RESID) {
	    if (ubp->b_resid != 0)			    /* still something to transfer? */
		Debugger("resid");

	    {
		int i;
		for (i = 0; i < ubp->b_bcount; i += 512)    /* XXX debug */
		    if (((char *) ubp->b_data)[i] != '<') { /* and not what we expected */
			log(LOG_DEBUG,
			    "At 0x%x (offset 0x%x): '%c' (0x%x)\n",
			    (int) (&((char *) ubp->b_data)[i]),
			    i,
			    ((char *) ubp->b_data)[i],
			    ((char *) ubp->b_data)[i]);
			Debugger("complete_request checksum");
		    }
	    }
	}
#endif

	if (rq->error) {				    /* did we have an error? */
	    if (rq->isplex) {				    /* plex operation, */
		ubp->b_flags |= B_ERROR;		    /* yes, propagate to user */
		ubp->b_error = rq->error;
	    } else					    /* try to recover */
		queue_daemon_request(daemonrq_ioerror, (union daemoninfo) rq); /* let the daemon complete */
	} else {
	    ubp->b_resid = 0;				    /* completed our transfer */
	    if (rq->isplex == 0)			    /* volume request, */
		VOL[rq->volplex.volno].active--;	    /* another request finished */
	    biodone(ubp);				    /* top level buffer completed */
	    freerq(rq);					    /* return the request storage */
	}
    }
}


/* Free a request block and anything hanging off it */
void 
freerq(struct request *rq)
{
    struct rqgroup *rqg;
    struct rqgroup *nrqg;				    /* next in chain */
    int rqno;

    for (rqg = rq->rqg; rqg != NULL; rqg = nrqg) {	    /* through the whole request chain */
	for (rqno = 0; rqno < rqg->count; rqno++)
	    if ((rqg->rqe[rqno].flags & XFR_MALLOCED)	    /* data buffer was malloced, */
	    &&rqg->rqe[rqno].b.b_data)			    /* and the allocation succeeded */
		Free(rqg->rqe[rqno].b.b_data);		    /* free it */
	nrqg = rqg->next;				    /* note the next one */
	Free(rqg);					    /* and free this one */
    }
    Free(rq);						    /* free the request itself */
}

void 
free_rqg(struct rqgroup *rqg)
{
    if ((rqg->flags & XFR_GROUPOP)			    /* RAID 5 request */
&&(rqg->rqe) /* got a buffer structure */
    &&(rqg->rqe->b.b_data))				    /* and it has a buffer allocated */
	Free(rqg->rqe->b.b_data);			    /* free it */
}

/* I/O on subdisk completed */
void 
sdio_done(struct buf *bp)
{
    struct sdbuf *sbp;

    sbp = (struct sdbuf *) bp;
    if (sbp->b.b_flags & B_ERROR) {			    /* had an error */
	bp->b_flags |= B_ERROR;
	bp->b_error = sbp->b.b_error;
    }
    bp->b_resid = sbp->b.b_resid;
    biodone(sbp->bp);					    /* complete the caller's I/O */
    /* Now update the statistics */
    if (bp->b_flags & B_READ) {				    /* read operation */
	DRIVE[sbp->driveno].reads++;
	DRIVE[sbp->driveno].bytes_read += bp->b_bcount;
	SD[sbp->sdno].reads++;
	SD[sbp->sdno].bytes_read += bp->b_bcount;
    } else {						    /* write operation */
	DRIVE[sbp->driveno].writes++;
	DRIVE[sbp->driveno].bytes_written += bp->b_bcount;
	SD[sbp->sdno].writes++;
	SD[sbp->sdno].bytes_written += bp->b_bcount;
    }
    Free(sbp);
}

/* Start the second phase of a RAID5 group write operation. */
/*
 * XXX This could be improved on.  It's quite CPU intensive,
 * and doing it at the end tends to lump it all together.
 * We should do this a transfer at a time 
 */
void 
complete_raid5_write(struct rqelement *rqe)
{
    int *sdata;						    /* source */
    int *pdata;						    /* and parity block data */
    int length;						    /* and count involved */
    int count;						    /* loop counter */
    int rqno;						    /* request index */
    int rqoffset;					    /* offset of request data from parity data */
    struct buf *bp;					    /* user buffer header */
    struct request *rq;					    /* pointer to our request */
    struct rqgroup *rqg;				    /* and to the request group */
    struct rqelement *prqe;				    /* point to the parity block */
    struct drive *drive;				    /* drive to access */

    rqg = rqe->rqg;					    /* and to our request group */
    rq = rqg->rq;					    /* point to our request */
    bp = rq->bp;					    /* user's buffer header */
    prqe = &rqg->rqe[0];				    /* point to the parity block */

    /*
     * If we get to this function, we have normal or
     * degraded writes, or a combination of both.  We do
     * the same thing in each case: we perform an
     * exclusive or to the parity block.  The only
     * difference is the origin of the data and the
     * address range. 
     */

    if (rqe->flags & XFR_DEGRADED_WRITE) {		    /* do the degraded write stuff */
	pdata = (int *) (&prqe->b.b_data[(prqe->groupoffset) << DEV_BSHIFT]); /* parity data pointer */
	bzero(pdata, prqe->grouplen << DEV_BSHIFT);	    /* start with nothing in the parity block */

	/* Now get what data we need from each block */
	for (rqno = 1; rqno < rqg->count; rqno++) {	    /* for all the data blocks */
	    /*
	     * This can do with improvement.  If we're doing
	     * both a degraded and a normal write, we don't
	     * need to xor (nor to read) the part of the block
	     * that we're going to overwrite.  FIXME XXX 
	     */
	    rqe = &rqg->rqe[rqno];			    /* this request */
	    sdata = (int *) (&rqe->b.b_data[rqe->groupoffset << DEV_BSHIFT]); /* old data */
	    length = rqe->grouplen << (DEV_BSHIFT - 2);	    /* and count involved */

	    /*
	     * add the data block to the parity block.  Before
	     * we started the request, we zeroed the parity
	     * block, so the result of adding all the other
	     * blocks and the block we want to write will be
	     * the correct parity block.  
	     */
	    /* XXX do this in assembler */
	    for (count = 0; count < length; count++)
		pdata[count] ^= sdata[count];
	    if ((rqe->flags & XFR_MALLOCED)		    /* the buffer was malloced, */
	    &&((rqg->flags & XFR_NORMAL_WRITE) == 0)) {	    /* and we have no normal write, */
		Free(rqe->b.b_data);			    /* free it now */
		rqe->flags &= ~XFR_MALLOCED;
	    }
	}
    }
    if (rqg->flags & XFR_NORMAL_WRITE) {		    /* do normal write stuff */
	/* Get what data we need from each block */
	for (rqno = 1; rqno < rqg->count; rqno++) {	    /* for all the data blocks */
	    rqe = &rqg->rqe[rqno];			    /* this request */
	    if ((rqe->flags & (XFR_DATA_BLOCK | XFR_BAD_SUBDISK | XFR_NORMAL_WRITE))
		== (XFR_DATA_BLOCK | XFR_NORMAL_WRITE)) {   /* good data block to write */
		sdata = (int *) &rqe->b.b_data[rqe->dataoffset << DEV_BSHIFT]; /* old data contents */
		rqoffset = rqe->dataoffset + rqe->sdoffset - prqe->sdoffset; /* corresponding parity block offset */
		pdata = (int *) (&prqe->b.b_data[rqoffset << DEV_BSHIFT]); /* parity data pointer */
		length = rqe->datalen << (DEV_BSHIFT - 2);  /* and count involved */
		/*
		 * "remove" the old data block
		 * from the parity block 
		 */
		/* XXX do this in assembler */
		if ((pdata < ((int *) prqe->b.b_data))
		    || (&pdata[length] > ((int *) (prqe->b.b_data + prqe->b.b_bcount)))
		    || (sdata < ((int *) rqe->b.b_data))
		    || (&sdata[length] > ((int *) (rqe->b.b_data + rqe->b.b_bcount))))
		    Debugger("Bounds overflow");	    /* XXX */
		for (count = 0; count < length; count++)
		    pdata[count] ^= sdata[count];

		/* "add" the new data block */
		sdata = (int *) (&bp->b_data[rqe->useroffset << DEV_BSHIFT]); /* new data */
		if ((sdata < ((int *) bp->b_data))
		    || (&sdata[length] > ((int *) (bp->b_data + bp->b_bcount))))
		    Debugger("Bounds overflow");	    /* XXX */
		for (count = 0; count < length; count++)
		    pdata[count] ^= sdata[count];

		/* Free the malloced buffer */
		if (rqe->flags & XFR_MALLOCED) {	    /* the buffer was malloced, */
		    Free(rqe->b.b_data);		    /* free it */
		    rqe->flags &= ~XFR_MALLOCED;
		} else
		    Debugger("not malloced");		    /* XXX */

		if ((rqe->b.b_flags & B_READ)		    /* this was a read */
		&&((rqe->flags & XFR_BAD_SUBDISK) == 0)) {  /* and we can write this block */
		    rqe->b.b_flags &= ~(B_READ | B_DONE);   /* we're writing now */
		    rqe->b.b_flags |= B_CALL;		    /* call us when you're done */
		    rqe->flags &= ~XFR_PARITYOP;	    /* reset flags that brought use here */
		    rqe->b.b_data = &bp->b_data[rqe->useroffset << DEV_BSHIFT];	/* point to the user data */
		    rqe->b.b_bcount = rqe->datalen << DEV_BSHIFT; /* length to write */
		    rqe->b.b_bufsize = rqe->b.b_bcount;	    /* don't claim more */
		    rqe->b.b_resid = rqe->b.b_bcount;	    /* nothing transferred */
		    rqe->b.b_blkno += rqe->dataoffset;	    /* point to the correct block */
		    rqg->active++;			    /* another active request */
		    rqe->b.b_vp->v_numoutput++;		    /* one more output going */
		    drive = &DRIVE[rqe->driveno];	    /* drive to access */
#if VINUMDEBUG
		    if (debug & DEBUG_ADDRESSES)
			log(LOG_DEBUG,
			    "  %s dev %d.%d, sd %d, offset 0x%x, devoffset 0x%x, length %ld\n",
			    rqe->b.b_flags & B_READ ? "Read" : "Write",
			    major(rqe->b.b_dev),
			    minor(rqe->b.b_dev),
			    rqe->sdno,
			    (u_int) (rqe->b.b_blkno - SD[rqe->sdno].driveoffset),
			    rqe->b.b_blkno,
			    rqe->b.b_bcount);		    /* XXX */
		    if (debug & DEBUG_NUMOUTPUT)
			log(LOG_DEBUG,
			    "  raid5.2 sd %d numoutput %ld\n",
			    rqe->sdno,
			    rqe->b.b_vp->v_numoutput);
		    if (debug & DEBUG_LASTREQS)
			logrq(loginfo_raid5_data, (union rqinfou) rqe, bp);
#endif
		    (*bdevsw(rqe->b.b_dev)->d_strategy) (&rqe->b);
		}
	    }
	}
    }
    /* Finally, write the parity block */
    rqe = &rqg->rqe[0];
    rqe->b.b_flags &= ~(B_READ | B_DONE);		    /* we're writing now */
    rqe->b.b_flags |= B_CALL;				    /* call us when you're done */
    rqe->flags &= ~XFR_PARITYOP;			    /* reset flags that brought use here */
    rqg->flags &= ~XFR_PARITYOP;			    /* reset flags that brought use here */
    rqe->b.b_bcount = rqe->buflen << DEV_BSHIFT;	    /* length to write */
    rqe->b.b_bufsize = rqe->b.b_bcount;			    /* don't claim we have more */
    rqe->b.b_resid = rqe->b.b_bcount;			    /* nothing transferred */
    rqg->active++;					    /* another active request */
    rqe->b.b_vp->v_numoutput++;				    /* one more output going */
    drive = &DRIVE[rqe->driveno];			    /* drive to access */
#if VINUMDEBUG
    if (debug & DEBUG_ADDRESSES)
	log(LOG_DEBUG,
	    "  %s dev %d.%d, sd %d, offset 0x%x, devoffset 0x%x, length %ld\n",
	    rqe->b.b_flags & B_READ ? "Read" : "Write",
	    major(rqe->b.b_dev),
	    minor(rqe->b.b_dev),
	    rqe->sdno,
	    (u_int) (rqe->b.b_blkno - SD[rqe->sdno].driveoffset),
	    rqe->b.b_blkno,
	    rqe->b.b_bcount);				    /* XXX */
    if (debug & DEBUG_NUMOUTPUT)
	log(LOG_DEBUG,
	    "  raid5.3 sd %d numoutput %ld\n",
	    rqe->sdno,
	    rqe->b.b_vp->v_numoutput);
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_raid5_parity, (union rqinfou) rqe, bp);
#endif
    (*bdevsw(rqe->b.b_dev)->d_strategy) (&rqe->b);
}

/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/iso88025.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include "contrib/dev/oltr/trlld.h"
#include "contrib/dev/oltr/if_oltrvar.h"

/*
 * Glue function prototypes for PMW kit IO
 */

#ifndef TRlldInlineIO
static void DriverOutByte	__P((unsigned short, unsigned char));
static void DriverOutWord	__P((unsigned short, unsigned short));
static void DriverOutDword	__P((unsigned short, unsigned long));
static void DriverRepOutByte	__P((unsigned short, unsigned char  *, int));
static void DriverRepOutWord	__P((unsigned short, unsigned short *, int));
static void DriverRepOutDword	__P((unsigned short, unsigned long  *, int));
static unsigned char  DriverInByte __P((unsigned short));
static unsigned short DriverInWord __P((unsigned short));
static unsigned long  DriverInDword __P((unsigned short));
static void DriverRepInByte	__P((unsigned short, unsigned char  *, int));
static void DriverRepInWord	__P((unsigned short, unsigned short *, int));
static void DriverRepInDword	__P((unsigned short, unsigned long  *, int));
#endif /*TRlldInlineIO*/
static void DriverSuspend	__P((unsigned short));
static void DriverStatus	__P((void *, TRlldStatus_t *));
static void DriverCloseCompleted __P((void *));
static void DriverStatistics	__P((void *, TRlldStatistics_t *));
static void DriverTransmitFrameCompleted __P((void *, void *, int));
static void DriverReceiveFrameCompleted	__P((void *, int, int, void *, int));

TRlldDriver_t LldDriver = {
	TRLLD_VERSION,
#ifndef TRlldInlineIO
	DriverOutByte,
	DriverOutWord,
	DriverOutDword,
	DriverRepOutByte,
	DriverRepOutWord,
	DriverRepOutDword,
	DriverInByte,
	DriverInWord,
	DriverInDword,
	DriverRepInByte,
	DriverRepInWord,
	DriverRepInDword,
#endif /*TRlldInlineIO*/
	DriverSuspend,
	DriverStatus,
	DriverCloseCompleted,
	DriverStatistics,
	DriverTransmitFrameCompleted,
	DriverReceiveFrameCompleted,
};


static void oltr_start		__P((struct ifnet *));
static void oltr_start_locked	__P((struct ifnet *));
static void oltr_close		__P((struct oltr_softc *));
static void oltr_init		__P((void *));
static void oltr_init_locked	__P((struct oltr_softc *));
static int oltr_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void oltr_intr		__P((void *));
static int oltr_ifmedia_upd	__P((struct ifnet *));
static void oltr_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));



int
oltr_attach(device_t dev)
{

	struct oltr_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp;
	int		rc = 0;
	int		media = IFM_TOKEN|IFM_TOK_UTP16;

	mtx_init(&sc->oltr_lock, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init_mtx(&sc->oltr_poll_timer, &sc->oltr_lock, 0);
	/*callout_init_mtx(&sc->oltr_stat_timer, &sc->oltr_lock, 0);*/
	ifp = sc->ifp = if_alloc(IFT_ISO88025);
	if (ifp == NULL) {
		device_printf(dev, "couldn't if_alloc()");
		mtx_destroy(&sc->oltr_lock);
		return (-1);
	}
	
	/*
	 * Allocate interrupt
	 */
	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
		(sc->config.mode & TRLLD_MODE_SHARE_INTERRUPT) ?
		RF_ACTIVE | RF_SHAREABLE : RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		if_free(ifp);
		mtx_destroy(&sc->oltr_lock);
		return (-1);
	}

	/*
	 * Do the ifnet initialization
	 */
	ifp->if_softc	= sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_init	= oltr_init;
	ifp->if_start	= oltr_start;
	ifp->if_ioctl	= oltr_ioctl;
	ifp->if_flags	= IFF_BROADCAST;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, oltr_ifmedia_upd, oltr_ifmedia_sts);
	rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_16MBPS);
	switch(sc->config.type) {
	case TRLLD_ADAPTER_PCI7:	/* OC-3540 */
		ifmedia_add(&sc->ifmedia, IFM_TOKEN|IFM_TOK_UTP100, 0, NULL);
		/* FALL THROUGH */
	case TRLLD_ADAPTER_PCI4:	/* OC-3139 */
	case TRLLD_ADAPTER_PCI5:	/* OC-3140 */
	case TRLLD_ADAPTER_PCI6:	/* OC-3141 */
		ifmedia_add(&sc->ifmedia, IFM_TOKEN|IFM_AUTO, 0, NULL);
		media = IFM_TOKEN|IFM_AUTO;
		rc = TRlldSetSpeed(sc->TRlldAdapter, 0);
		/* FALL THROUGH */
	default:
		ifmedia_add(&sc->ifmedia, IFM_TOKEN|IFM_TOK_UTP4, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_TOKEN|IFM_TOK_UTP16, 0, NULL);
		break;
	}
	sc->ifmedia.ifm_media = media;
	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Attach the interface
	 */

	iso88025_ifattach(ifp, sc->config.macaddress, ISO88025_BPF_SUPPORTED);

	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE, NULL,
			oltr_intr, sc, &sc->oltr_intrhand)) {
		device_printf(dev, "couldn't setup interrupt\n");
		iso88025_ifdetach(ifp, ISO88025_BPF_SUPPORTED);
                bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
		if_free(ifp);
		mtx_destroy(&sc->oltr_lock);
                return (-1);
	}

	return(0);
}


static void
oltr_intr(void *xsc)
{
	struct oltr_softc		*sc = (struct oltr_softc *)xsc;

	OLTR_LOCK(sc);
	if (DEBUG_MASK & DEBUG_INT)
		printf("I");

	TRlldInterruptService(sc->TRlldAdapter);
	OLTR_UNLOCK(sc);

	return;
}

static void
oltr_start(struct ifnet *ifp)
{
	struct oltr_softc 	*sc = ifp->if_softc;

	OLTR_LOCK(sc);
	oltr_start_locked(ifp);
	OLTR_UNLOCK(sc);
}

static void
oltr_start_locked(struct ifnet *ifp)
{
	struct oltr_softc 	*sc = ifp->if_softc;
	struct mbuf		*m0, *m;
	int			copy_len, buffer, frame, fragment, rc;
	
	/*
	 * Check to see if output is already active
	 */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

outloop:

	/*
	 * Make sure we have buffers to transmit with
	 */
	if (sc->tx_avail <= 0) {
		if_printf(ifp, "tx queue full\n");
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return;
	}

	if (sc->restart == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			return;
	} else {
		m = sc->restart;
		sc->restart = NULL;
	}

	m0 = m;
	frame = RING_BUFFER(sc->tx_frame);
	buffer = RING_BUFFER(sc->tx_head);
	fragment = 0;
	copy_len = 0;
	sc->frame_ring[frame].FragmentCount = 0;
	
	while (copy_len < m0->m_pkthdr.len) {
		sc->frame_ring[frame].FragmentCount++;
		if (sc->frame_ring[frame].FragmentCount > sc->tx_avail)
			goto nobuffers;
		sc->frame_ring[frame].TransmitFragment[fragment].VirtualAddress = sc->tx_ring[buffer].data;
		sc->frame_ring[frame].TransmitFragment[fragment].PhysicalAddress = sc->tx_ring[buffer].address;
		sc->frame_ring[frame].TransmitFragment[fragment].count = MIN(m0->m_pkthdr.len - copy_len, TX_BUFFER_LEN);
		m_copydata(m0, copy_len, MIN(m0->m_pkthdr.len - copy_len, TX_BUFFER_LEN), sc->tx_ring[buffer].data);
		copy_len += MIN(m0->m_pkthdr.len - copy_len, TX_BUFFER_LEN);
		fragment++;
		buffer = RING_BUFFER((buffer + 1));
	}

	rc = TRlldTransmitFrame(sc->TRlldAdapter, &sc->frame_ring[frame], (void *)&sc->frame_ring[frame]);

	if (rc != TRLLD_TRANSMIT_OK) {
		if_printf(ifp, "TRlldTransmitFrame returned %d\n", rc);
		ifp->if_oerrors++;
		goto bad;
	}

	sc->tx_avail -= sc->frame_ring[frame].FragmentCount;
	sc->tx_head = RING_BUFFER((sc->tx_head + sc->frame_ring[frame].FragmentCount));
	sc->tx_frame++;

	BPF_MTAP(ifp, m0);
	/*ifp->if_opackets++;*/

bad:
	m_freem(m0);

	goto outloop;

nobuffers:

	if_printf(ifp, "queue full\n");
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	ifp->if_oerrors++;
	/*m_freem(m0);*/
	sc->restart = m0;

	return;
}

static void
oltr_close(struct oltr_softc *sc)
{
	/*if_printf(sc->ifp, "oltr_close\n");*/

	oltr_stop(sc);

	mtx_sleep(sc, &sc->oltr_lock, PWAIT, "oltrclose", 30*hz);
}

void
oltr_stop(struct oltr_softc *sc)
{
	struct ifnet 		*ifp = sc->ifp;

	/*if_printf(ifp, "oltr_stop\n");*/

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	TRlldClose(sc->TRlldAdapter, 0);
	sc->state = OL_CLOSING;
	callout_stop(&sc->oltr_poll_timer);
	/*callout_stop(&sc->oltr_stat_timer);*/
}

static void
oltr_init(void * xsc)
{
	struct oltr_softc 	*sc = (struct oltr_softc *)xsc;

	OLTR_LOCK(sc);
	oltr_init_locked(sc);
	OLTR_UNLOCK(sc);
}

static void
oltr_init_locked(struct oltr_softc *sc)
{
	struct ifnet		*ifp = sc->ifp;
	struct ifmedia		*ifm = &sc->ifmedia;
	int			poll = 0, i, rc = 0;
	int			work_size;

	/*
	 * Check adapter state, don't allow multiple inits
	 */
	if (sc->state > OL_CLOSED) {
		if_printf(ifp, "adapter not ready\n");
		return;
	}

	/*
	 * Initialize Adapter
	 */
	if ((rc = TRlldAdapterInit(&LldDriver, sc->TRlldAdapter, sc->TRlldAdapter_phys,
	    (void *)sc, &sc->config)) != TRLLD_INIT_OK) {
		switch(rc) {
		case TRLLD_INIT_NOT_FOUND:
			if_printf(ifp, "adapter not found\n");
			break;
		case TRLLD_INIT_UNSUPPORTED:
			if_printf(ifp, "adapter not supported by low level driver\n");
			break;
		case TRLLD_INIT_PHYS16:
			if_printf(ifp, "adapter memory block above 16M cannot DMA\n");
			break;
		case TRLLD_INIT_VERSION:
			if_printf(ifp, "low level driver version mismatch\n");
			break;
		default:
			if_printf(ifp, "unknown init error %d\n", rc);
			break;
		}
		goto init_failed;
	}
	sc->state = OL_INIT;

	switch(sc->config.type) {
	case TRLLD_ADAPTER_PCI4:        /* OC-3139 */
		work_size = 32 * 1024;
		break;
	case TRLLD_ADAPTER_PCI7:        /* OC-3540 */
		work_size = 256;
		break;
	default:
		work_size = 0;
	}

	if (work_size) {
		if ((sc->work_memory = malloc(work_size, M_DEVBUF, M_NOWAIT)) == NULL) {
			if_printf(ifp, "failed to allocate work memory (%d octets).\n", work_size);
		} else {
		TRlldAddMemory(sc->TRlldAdapter, sc->work_memory,
		    vtophys(sc->work_memory), work_size);
		}
	}

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		rc = TRlldSetSpeed(sc->TRlldAdapter, 0); /* TRLLD_SPEED_AUTO */
		break;
	case IFM_TOK_UTP4:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_4MBPS);
		break;
	case IFM_TOK_UTP16:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_16MBPS);
		break;
	case IFM_TOK_UTP100:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_100MBPS);
		break;
	}

	/*
	 * Download adapter micro-code
	 */
	if (bootverbose)
		if_printf(ifp, "Downloading adapter microcode: ");

	switch(sc->config.mactype) {
	case TRLLD_MAC_TMS:
		rc = TRlldDownload(sc->TRlldAdapter, TRlldMacCode);
		if (bootverbose)
			printf("TMS-380");
		break;
	case TRLLD_MAC_HAWKEYE:
		rc = TRlldDownload(sc->TRlldAdapter, TRlldHawkeyeMac);
		if (bootverbose)
			printf("Hawkeye");
		break;
	case TRLLD_MAC_BULLSEYE:
		rc = TRlldDownload(sc->TRlldAdapter, TRlldBullseyeMac);
		if (bootverbose)
			printf("Bullseye");
		break;
	default:
		if (bootverbose)
			printf("unknown - failed!\n");
		goto init_failed;
		break;
	}

	/*
	 * Check download status
	 */
	switch(rc) {
	case TRLLD_DOWNLOAD_OK:
		if (bootverbose)
			printf(" - ok\n");
		break;
	case TRLLD_DOWNLOAD_ERROR:
		if (bootverbose)
			printf(" - failed\n");
		else
			if_printf(ifp, "adapter microcode download failed\n");
		goto init_failed;
		break;
	case TRLLD_STATE:
		if (bootverbose)
			printf(" - not ready\n");
		goto init_failed;
		break;
	}

	/*
	 * Wait for self-test to complete
	 */
	i = 0;
	while ((poll++ < SELF_TEST_POLLS) && (sc->state < OL_READY)) {
		if (DEBUG_MASK & DEBUG_INIT)
			printf("p");
		DELAY(TRlldPoll(sc->TRlldAdapter) * 1000);
		if (TRlldInterruptService(sc->TRlldAdapter) != 0)
			if (DEBUG_MASK & DEBUG_INIT) printf("i");
	}

	if (sc->state != OL_CLOSED) {
		if_printf(ifp, "self-test failed\n");
		goto init_failed;
	}

	/*
	 * Set up adapter poll
	 */
	callout_reset(&sc->oltr_poll_timer, 1, oltr_poll, sc);

	sc->state = OL_OPENING;

	/*
	 * Open the adapter
	 */
	rc = TRlldOpen(sc->TRlldAdapter, IF_LLADDR(sc->ifp), sc->GroupAddress,
		sc->FunctionalAddress, 1552, sc->AdapterMode);
	switch(rc) {
		case TRLLD_OPEN_OK:
			break;
		case TRLLD_OPEN_STATE:
			if_printf(ifp, "adapter not ready for open\n");
			return;
		case TRLLD_OPEN_ADDRESS_ERROR:
			if_printf(ifp, "illegal MAC address\n");
			return;
		case TRLLD_OPEN_MODE_ERROR:
			if_printf(ifp, "illegal open mode\n");
			return;
		default:
			if_printf(ifp, "unknown open error (%d)\n", rc);
			return;
	}

	/*
	 * Set promiscious mode for now...
	 */
	TRlldSetPromiscuousMode(sc->TRlldAdapter, TRLLD_PROM_LLC);
	ifp->if_flags |= IFF_PROMISC;

	/*
	 * Block on the ring insert and set a timeout
	 */
	mtx_sleep(sc, &sc->oltr_lock, PWAIT, "oltropen", 30*hz);

	/*
	 * Set up receive buffer ring
	 */
	for (i = 0; i < RING_BUFFER_LEN; i++) {
		rc = TRlldReceiveFragment(sc->TRlldAdapter, (void *)sc->rx_ring[i].data,
			sc->rx_ring[i].address, RX_BUFFER_LEN, (void *)sc->rx_ring[i].index);
		if (rc != TRLLD_RECEIVE_OK) {
			if_printf(ifp, "adapter refused receive fragment %d (rc = %d)\n", i, rc);
			break;
		}	
	}

	sc->tx_avail = RING_BUFFER_LEN;
	sc->tx_head = 0;
	sc->tx_frame = 0;

	sc->restart = NULL;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	/*
	 * Set up adapter statistics poll
	 */
	/*callout_reset(&sc->oltr_stat_timer, 1*hz, oltr_stat, sc);*/

	return;

init_failed:
	sc->state = OL_DEAD;
	return;
}

static int
oltr_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct oltr_softc 	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	int 			error = 0;

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = iso88025_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		OLTR_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			oltr_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				oltr_close(sc);
			}
		}
		OLTR_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	return(error);
}


void
oltr_poll(void *arg)
{
	struct oltr_softc *sc = (struct oltr_softc *)arg;

	if (DEBUG_MASK & DEBUG_POLL) printf("P");

	/* Set up next adapter poll */
	callout_reset(&sc->oltr_poll_timer,
	    (TRlldPoll(sc->TRlldAdapter) * hz / 1000), oltr_poll, sc);
}

#ifdef NOTYET
void
oltr_stat(void *arg)
{
	struct oltr_softc	*sc = (struct oltr_softc *)arg;

	/* Set up next adapter poll */
	callout_reset(&sc->oltr_stat_timer, 1*hz, oltr_stat, sc);
	if (TRlldGetStatistics(sc->TRlldAdapter, &sc->current, 0) != 0) {
		/*if_printf(sc->ifp, "statistics available immediately...\n");*/
		DriverStatistics((void *)sc, &sc->current);
	}
}
#endif
static int
oltr_ifmedia_upd(struct ifnet *ifp)
{
	struct oltr_softc 	*sc = ifp->if_softc;
	struct ifmedia		*ifm = &sc->ifmedia;
	int			rc;

	if (IFM_TYPE(ifm->ifm_media) != IFM_TOKEN)
		return(EINVAL);

	OLTR_LOCK(sc);
	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		rc = TRlldSetSpeed(sc->TRlldAdapter, 0); /* TRLLD_SPEED_AUTO */
		break;
	case IFM_TOK_UTP4:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_4MBPS);
		break;
	case IFM_TOK_UTP16:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_16MBPS);
		break;
	case IFM_TOK_UTP100:
		rc = TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_100MBPS);
		break;
	default:
		rc = EINVAL;
		break;
	}
	OLTR_UNLOCK(sc);

	return(rc);

}

static void
oltr_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct oltr_softc	*sc = ifp->if_softc;
	struct ifmedia		*ifm = &sc->ifmedia;

	/*if_printf(ifp, "oltr_ifmedia_sts\n");*/

	OLTR_LOCK(sc);
	ifmr->ifm_active = IFM_TYPE(ifm->ifm_media)|IFM_SUBTYPE(ifm->ifm_media);
	OLTR_UNLOCK(sc);
}

/*
 * ---------------------- PMW Callback Functions -----------------------
 */

void
DriverStatistics(void *DriverHandle, TRlldStatistics_t *statistics)
{
#ifdef NOTYET
	struct oltr_softc		*sc = (struct oltr_softc *)DriverHandle;

	if (sc->statistics.LineErrors != statistics->LineErrors)
		if_printf(ifp, "Line Errors %lu\n",
		    statistics->LineErrors);
	if (sc->statistics.InternalErrors != statistics->InternalErrors)
		if_printf(ifp, "Internal Errors %lu\n",
		    statistics->InternalErrors);
	if (sc->statistics.BurstErrors != statistics->BurstErrors)
		if_printf(ifp, "Burst Errors %lu\n",
		    statistics->BurstErrors);
	if (sc->statistics.AbortDelimiters != statistics->AbortDelimiters)
		if_printf(ifp, "Abort Delimiters %lu\n",
		    statistics->AbortDelimiters);
	if (sc->statistics.ARIFCIErrors != statistics->ARIFCIErrors)
		if_printf(ifp, "ARIFCI Errors %lu\n",
		    statistics->ARIFCIErrors);
	if (sc->statistics.LostFrames != statistics->LostFrames)
		if_printf(ifp, "Lost Frames %lu\n",
		    statistics->LostFrames);
	if (sc->statistics.CongestionErrors != statistics->CongestionErrors)
		if_printf(ifp, "Congestion Errors %lu\n",
		    statistics->CongestionErrors);
	if (sc->statistics.FrequencyErrors != statistics->FrequencyErrors)
		if_printf(ifp, "Frequency Errors %lu\n",
		    statistics->FrequencyErrors);
	if (sc->statistics.TokenErrors != statistics->TokenErrors)
		if_printf(ifp, "Token Errors %lu\n",
		    statistics->TokenErrors);
	if (sc->statistics.DMABusErrors != statistics->DMABusErrors)
		if_printf(ifp, "DMA Bus Errors %lu\n",
		    statistics->DMABusErrors);
	if (sc->statistics.DMAParityErrors != statistics->DMAParityErrors)
		if_printf(ifp, "DMA Parity Errors %lu\n",
		    statistics->DMAParityErrors);
	if (sc->statistics.ReceiveLongFrame != statistics->ReceiveLongFrame)
		if_printf(ifp, "Long frames received %lu\n",
		    statistics->ReceiveLongFrame);
	if (sc->statistics.ReceiveCRCErrors != statistics->ReceiveCRCErrors)
		if_printf(ifp, "Receive CRC Errors %lu\n",
		    statistics->ReceiveCRCErrors);
	if (sc->statistics.ReceiveOverflow != statistics->ReceiveOverflow)
		if_printf(ifp, "Recieve overflows %lu\n",
		    statistics->ReceiveOverflow);
	if (sc->statistics.TransmitUnderrun != statistics->TransmitUnderrun)
		if_printf(ifp, "Frequency Errors %lu\n",
		    statistics->TransmitUnderrun);
	bcopy(statistics, &sc->statistics, sizeof(TRlldStatistics_t));
#endif
}

static void
DriverSuspend(unsigned short MicroSeconds)
{
    DELAY(MicroSeconds);
}


static void
DriverStatus(void *DriverHandle, TRlldStatus_t *Status)
{
	struct oltr_softc	*sc = (struct oltr_softc *)DriverHandle;
	struct ifnet		*ifp = sc->ifp;

	char *Protocol[] = { /* 0 */ "Unknown",
			     /* 1 */ "TKP",
			     /* 2 */ "TXI" };
	char *Timeout[]  = { /* 0 */ "command",
			     /* 1 */ "transmit",
			     /* 2 */ "interrupt" };

	OLTR_ASSERT_LOCKED(sc);
	switch (Status->Type) {

	case TRLLD_STS_ON_WIRE:
		if_printf(ifp, "ring insert (%d Mbps - %s)\n",
		    Status->Specification.OnWireInformation.Speed,
		    Protocol[Status->Specification.OnWireInformation.AccessProtocol]);
		sc->state = OL_OPEN;
		wakeup(sc);
		break;
	case TRLLD_STS_SELFTEST_STATUS:
		if (Status->Specification.SelftestStatus == TRLLD_ST_OK) {
			sc->state = OL_CLOSED;
			if (bootverbose)
				if_printf(ifp, "self test complete\n");
		}
		if (Status->Specification.SelftestStatus & TRLLD_ST_ERROR) {
			if_printf(ifp, "Adapter self test error %d",
			Status->Specification.SelftestStatus & ~TRLLD_ST_ERROR);
			sc->state = OL_DEAD;
		}
		if (Status->Specification.SelftestStatus & TRLLD_ST_TIMEOUT) {
			if_printf(ifp, "Adapter self test timed out.\n");
			sc->state = OL_DEAD;
		}
		break;
	case TRLLD_STS_INIT_STATUS:
		if (Status->Specification.InitStatus == 0x800) {
			oltr_stop(sc);
			ifmedia_set(&sc->ifmedia, IFM_TOKEN|IFM_TOK_UTP16);
			TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_16MBPS);
			oltr_init_locked(sc);
			break;
		}
		if_printf(ifp, "adapter init failure 0x%03x\n",
		    Status->Specification.InitStatus);
		oltr_stop(sc);
		break;
	case TRLLD_STS_RING_STATUS:
		if (Status->Specification.RingStatus) {
			if_printf(ifp, "Ring status change: ");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_SIGNAL_LOSS)
				printf(" [Signal Loss]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_HARD_ERROR)
				printf(" [Hard Error]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_SOFT_ERROR)
				printf(" [Soft Error]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_TRANSMIT_BEACON)
				printf(" [Beacon]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_LOBE_WIRE_FAULT)
				printf(" [Wire Fault]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_AUTO_REMOVAL_ERROR)
				printf(" [Auto Removal]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_REMOVE_RECEIVED)
				printf(" [Remove Received]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_COUNTER_OVERFLOW)
				printf(" [Counter Overflow]");
			if (Status->Specification.RingStatus &
			    TRLLD_RS_SINGLE_STATION)
				printf(" [Single Station]");
			if (Status->Specification.RingStatus &
				TRLLD_RS_RING_RECOVERY)
				printf(" [Ring Recovery]");
			printf("\n");	
		}
		break;
	case TRLLD_STS_ADAPTER_CHECK:
		if_printf(ifp, "adapter check (%04x %04x %04x %04x)\n",
		    Status->Specification.AdapterCheck[0],
		    Status->Specification.AdapterCheck[1],
		    Status->Specification.AdapterCheck[2],
		    Status->Specification.AdapterCheck[3]);
		sc->state = OL_DEAD;
		oltr_stop(sc);
		break;
	case TRLLD_STS_PROMISCUOUS_STOPPED:
		if_printf(ifp, "promiscuous mode ");
		if (Status->Specification.PromRemovedCause == 1)
			printf("remove received.");
		if (Status->Specification.PromRemovedCause == 2)
			printf("poll failure.");
		if (Status->Specification.PromRemovedCause == 2)
			printf("buffer size failure.");
		printf("\n");
		ifp->if_flags &= ~IFF_PROMISC;
		break;
	case TRLLD_STS_LLD_ERROR:
		if_printf(ifp, "low level driver internal error ");
		printf("(%04x %04x %04x %04x).\n",
		    Status->Specification.InternalError[0],
		    Status->Specification.InternalError[1],
		    Status->Specification.InternalError[2],
		    Status->Specification.InternalError[3]);
		sc->state = OL_DEAD;
		oltr_stop(sc);
		break;
	case TRLLD_STS_ADAPTER_TIMEOUT:
		if_printf(ifp, "adapter %s timeout.\n",
		    Timeout[Status->Specification.AdapterTimeout]);
		break;
	default:
		if_printf(ifp, "driver status Type = %d\n", Status->Type);
		break;

	}
	if (Status->Closed) {
		sc->state = OL_CLOSING;
		oltr_stop(sc);
	}

}

static void
DriverCloseCompleted(void *DriverHandle)
{
	struct oltr_softc		*sc = (struct oltr_softc *)DriverHandle;

	OLTR_ASSERT_LOCKED(sc);
	if_printf(sc->ifp, "adapter closed\n");
	wakeup(sc);
	sc->state = OL_CLOSED;
}

static void
DriverTransmitFrameCompleted(void *DriverHandle, void *FrameHandle, int TransmitStatus)
{
	struct oltr_softc	*sc = (struct oltr_softc *)DriverHandle;
	struct ifnet		*ifp = sc->ifp;
	TRlldTransmit_t		*frame = (TRlldTransmit_t *)FrameHandle;
	
	/*if_printf(ifp, "DriverTransmitFrameCompleted\n");*/

	OLTR_ASSERT_LOCKED(sc);
	if (TransmitStatus != TRLLD_TRANSMIT_OK) {
		ifp->if_oerrors++;
		if_printf(ifp, "transmit error %d\n", TransmitStatus);
	} else {
		ifp->if_opackets++;
	}
	
	sc->tx_avail += frame->FragmentCount;

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		if_printf(ifp, "queue restart\n");
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		oltr_start_locked(ifp);
	}


}

static void
DriverReceiveFrameCompleted(void *DriverHandle, int ByteCount, int FragmentCount, void *FragmentHandle, int ReceiveStatus)
{
	struct oltr_softc 	*sc = (struct oltr_softc *)DriverHandle;
	struct ifnet		*ifp = sc->ifp;
	struct mbuf		*m0, *m1, *m;
	int			frame_len = ByteCount, i = (int)FragmentHandle, rc;
	int			mbuf_offset, mbuf_size, frag_offset, copy_length;
	char			*fragment = sc->rx_ring[RING_BUFFER(i)].data;
	
	OLTR_ASSERT_LOCKED(sc);
	if (sc->state > OL_CLOSED) {
		if (ReceiveStatus == TRLLD_RCV_OK) {
			MGETHDR(m0, M_DONTWAIT, MT_DATA);
			mbuf_size = MHLEN - 2;
			if (!m0) {
				ifp->if_ierrors++;
				goto dropped;
			}
			if (ByteCount + 2 > MHLEN) {
				MCLGET(m0, M_DONTWAIT);
				mbuf_size = MCLBYTES - 2;
				if (!(m0->m_flags & M_EXT)) {
					m_freem(m0);
					ifp->if_ierrors++;
					goto dropped;
				}
			}
			m0->m_pkthdr.rcvif = ifp;
			m0->m_pkthdr.len = ByteCount;
			m0->m_len = 0;
			m0->m_data += 2;

			m = m0;
			mbuf_offset = 0;
			frag_offset = 0;
			while (frame_len) {
				copy_length = MIN3(frame_len,
				    (RX_BUFFER_LEN - frag_offset),
				    (mbuf_size - mbuf_offset));
				bcopy(fragment + frag_offset, mtod(m, char *) +
				    mbuf_offset, copy_length);
				m->m_len += copy_length;
				mbuf_offset += copy_length;
				frag_offset += copy_length;
				frame_len -= copy_length;
			
				if (frag_offset == RX_BUFFER_LEN) {
					fragment =
					    sc->rx_ring[RING_BUFFER(++i)].data;
					frag_offset = 0;
				}
				if ((mbuf_offset == mbuf_size) && (frame_len > 0)) {
					MGET(m1, M_DONTWAIT, MT_DATA);
					mbuf_size = MHLEN;
					if (!m1) {
						ifp->if_ierrors++;
						m_freem(m0);
						goto dropped;
					}
					if (frame_len > MHLEN) {
						MCLGET(m1, M_DONTWAIT);
						mbuf_size = MCLBYTES;
						if (!(m1->m_flags & M_EXT)) {
							m_freem(m0);
							m_freem(m1);
							ifp->if_ierrors++;
							goto dropped;
						}
					}
					m->m_next = m1;
					m = m1;
					mbuf_offset = 0;
					m->m_len = 0;
				}
			}
			OLTR_UNLOCK(sc);
			iso88025_input(ifp, m0);
			OLTR_LOCK(sc);
		} else {	/* Receiver error */
			if (ReceiveStatus != TRLLD_RCV_NO_DATA) {
				if_printf(ifp, "receive error %d\n",
				    ReceiveStatus);
				ifp->if_ierrors++;
			}
		}

dropped:
		i = (int)FragmentHandle;
		while (FragmentCount--) {
			rc = TRlldReceiveFragment(sc->TRlldAdapter,
			    (void *)sc->rx_ring[RING_BUFFER(i)].data,
			    sc->rx_ring[RING_BUFFER(i)].address,
			    RX_BUFFER_LEN, (void *)sc->rx_ring[RING_BUFFER(i)].index);
			if (rc != TRLLD_RECEIVE_OK) {
				if_printf(ifp, "adapter refused receive fragment %d (rc = %d)\n", i, rc);
				break;
			}
			i++;
		}
	}
}


/*
 * ---------------------------- PMW Glue -------------------------------
 */

#ifndef TRlldInlineIO

static void
DriverOutByte(unsigned short IOAddress, unsigned char value)
{
	outbv(IOAddress, value);
}

static void
DriverOutWord(unsigned short IOAddress, unsigned short value)
{
	outw(IOAddress, value);
}

static void
DriverOutDword(unsigned short IOAddress, unsigned long value)
{
	outl(IOAddress, value);
}

static void
DriverRepOutByte(unsigned short IOAddress, unsigned char *DataPointer, int ByteCount)
{
	outsb(IOAddress, (void *)DataPointer, ByteCount);
}

static void
DriverRepOutWord(unsigned short IOAddress, unsigned short *DataPointer, int WordCount)
{
	outsw(IOAddress, (void *)DataPointer, WordCount);
}

static void
DriverRepOutDword(unsigned short IOAddress, unsigned long *DataPointer, int DWordCount)
{
	outsl(IOAddress, (void *)DataPointer, DWordCount);
}

static unsigned char
DriverInByte(unsigned short IOAddress)
{
	return(inbv(IOAddress));
}

static unsigned short
DriverInWord(unsigned short IOAddress)
{
	return(inw(IOAddress));
}

static unsigned long
DriverInDword(unsigned short IOAddress)
{
	return(inl(IOAddress));
}

static void
DriverRepInByte(unsigned short IOAddress, unsigned char *DataPointer, int ByteCount)
{
	insb(IOAddress, (void *)DataPointer, ByteCount);
}

static void
DriverRepInWord(unsigned short IOAddress, unsigned short *DataPointer, int WordCount)
{
	insw(IOAddress, (void *)DataPointer, WordCount);
}
static void
DriverRepInDword( unsigned short IOAddress, unsigned long *DataPointer, int DWordCount)
{
	insl(IOAddress, (void *)DataPointer, DWordCount);
}
#endif /* TRlldInlineIO */

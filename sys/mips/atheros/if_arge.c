/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * AR71XX gigabit ethernet driver
 */
#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/resource.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

MODULE_DEPEND(arge, ether, 1, 1, 1);
MODULE_DEPEND(arge, miibus, 1, 1, 1);

#include "miibus_if.h"

#include <mips/atheros/ar71xxreg.h>
#include <mips/atheros/if_argevar.h>

#undef ARGE_DEBUG
#ifdef ARGE_DEBUG
#define dprintf printf
#else
#define dprintf(x, arg...)
#endif

static int arge_attach(device_t);
static int arge_detach(device_t);
static void arge_flush_ddr(struct arge_softc *);
static int arge_ifmedia_upd(struct ifnet *);
static void arge_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int arge_ioctl(struct ifnet *, u_long, caddr_t);
static void arge_init(void *);
static void arge_init_locked(struct arge_softc *);
static void arge_link_task(void *, int);
static int arge_miibus_readreg(device_t, int, int);
static void arge_miibus_statchg(device_t);
static int arge_miibus_writereg(device_t, int, int, int);
static int arge_probe(device_t);
static void arge_reset_dma(struct arge_softc *);
static int arge_resume(device_t);
static int arge_rx_ring_init(struct arge_softc *);
static int arge_tx_ring_init(struct arge_softc *);
#ifdef DEVICE_POLLING
static int arge_poll(struct ifnet *, enum poll_cmd, int);
#endif
static int arge_shutdown(device_t);
static void arge_start(struct ifnet *);
static void arge_start_locked(struct ifnet *);
static void arge_stop(struct arge_softc *);
static int arge_suspend(device_t);

static int arge_rx_locked(struct arge_softc *);
static void arge_tx_locked(struct arge_softc *);
static void arge_intr(void *);
static int arge_intr_filter(void *);
static void arge_tick(void *);

static void arge_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static int arge_dma_alloc(struct arge_softc *);
static void arge_dma_free(struct arge_softc *);
static int arge_newbuf(struct arge_softc *, int);
static __inline void arge_fixup_rx(struct mbuf *);

static device_method_t arge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arge_probe),
	DEVMETHOD(device_attach,	arge_attach),
	DEVMETHOD(device_detach,	arge_detach),
	DEVMETHOD(device_suspend,	arge_suspend),
	DEVMETHOD(device_resume,	arge_resume),
	DEVMETHOD(device_shutdown,	arge_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	arge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	arge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	arge_miibus_statchg),

	{ 0, 0 }
};

static driver_t arge_driver = {
	"arge",
	arge_methods,
	sizeof(struct arge_softc)
};

static devclass_t arge_devclass;

DRIVER_MODULE(arge, nexus, arge_driver, arge_devclass, 0, 0);
DRIVER_MODULE(miibus, arge, miibus_driver, miibus_devclass, 0, 0);

/*
 * RedBoot passes MAC address to entry point as environment 
 * variable. platfrom_start parses it and stores in this variable
 */
extern uint32_t ar711_base_mac[ETHER_ADDR_LEN];

/*
 * Flushes all 
 */
static void
arge_flush_ddr(struct arge_softc *sc)
{

	ATH_WRITE_REG(sc->arge_ddr_flush_reg, 1);
	while (ATH_READ_REG(sc->arge_ddr_flush_reg) & 1)
		;

	ATH_WRITE_REG(sc->arge_ddr_flush_reg, 1);
	while (ATH_READ_REG(sc->arge_ddr_flush_reg) & 1)
		;
}

static int 
arge_probe(device_t dev)
{

	device_set_desc(dev, "Atheros AR71xx built-in ethernet interface");
	return (0);
}

static int
arge_attach(device_t dev)
{
	uint8_t			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	struct arge_softc	*sc;
	int			error = 0, rid, phynum;
	uint32_t		reg, rnd;
	int			is_base_mac_empty, i;

	sc = device_get_softc(dev);
	sc->arge_dev = dev;
	sc->arge_mac_unit = device_get_unit(dev);

	KASSERT(((sc->arge_mac_unit == 0) || (sc->arge_mac_unit == 1)), 
	    ("if_arge: Only MAC0 and MAC1 supported"));
	if (sc->arge_mac_unit == 0) {
		sc->arge_ddr_flush_reg = AR71XX_WB_FLUSH_GE0;
		sc->arge_pll_reg = AR71XX_PLL_ETH_INT0_CLK;
	} else {
		sc->arge_ddr_flush_reg = AR71XX_WB_FLUSH_GE1;
		sc->arge_pll_reg = AR71XX_PLL_ETH_INT1_CLK;
	}

	/*
	 *  Get which PHY of 5 available we should use for this unit
	 */
	if (resource_int_value(device_get_name(dev), device_get_unit(dev), 
	    "phy", &phynum) != 0) {
		/*
		 * Use port 4 (WAN) for GE0. For any other port use 
		 * its PHY the same as its unit number 
		 */
		if (sc->arge_mac_unit == 0)
			phynum = 4;
		else
			phynum = sc->arge_mac_unit;

		device_printf(dev, "No PHY specified, using %d\n", phynum);
	}

	sc->arge_phy_num = phynum;


	mtx_init(&sc->arge_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init_mtx(&sc->arge_stat_callout, &sc->arge_mtx, 0);
	TASK_INIT(&sc->arge_link_task, 0, arge_link_task, sc);

	/* Map control/status registers. */
	sc->arge_rid = 0;
	sc->arge_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 
	    &sc->arge_rid, RF_ACTIVE);

	if (sc->arge_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate interrupts */
	rid = 0;
	sc->arge_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, 
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->arge_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate ifnet structure. */
	ifp = sc->arge_ifp = if_alloc(IFT_ETHER);

	if (ifp == NULL) {
		device_printf(dev, "couldn't allocate ifnet structure\n");
		error = ENOSPC;
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = arge_ioctl;
	ifp->if_start = arge_start;
	ifp->if_init = arge_init;
	sc->arge_if_flags = ifp->if_flags;

	/* XXX: add real size */
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	is_base_mac_empty = 1;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		eaddr[i] = ar711_base_mac[i] & 0xff;
		if (eaddr[i] != 0)
			is_base_mac_empty = 0;
	}

	if (is_base_mac_empty) {
		/*
		 * No MAC address configured. Generate the random one.
		 */
                if  (bootverbose)
			device_printf(dev, 
			    "Generating random ethernet address.\n");

		rnd = arc4random();
		eaddr[0] = 'b';
		eaddr[1] = 's';
		eaddr[2] = 'd';
		eaddr[3] = (rnd >> 24) & 0xff;
		eaddr[4] = (rnd >> 16) & 0xff;
		eaddr[5] = (rnd >> 8) & 0xff;
	}

	if (arge_dma_alloc(sc) != 0) {
		error = ENXIO;
		goto fail;
	}

	/* Initialize the MAC block */
	
	/* Step 1. Soft-reset MAC */
	ARGE_SET_BITS(sc, AR71XX_MAC_CFG1, MAC_CFG1_SOFT_RESET);
	DELAY(20);

	/* Step 2. Punt the MAC core from the central reset register */
	reg = ATH_READ_REG(AR71XX_RST_RESET);
	if (sc->arge_mac_unit == 0) 
		reg |= RST_RESET_GE0_MAC;
	else if (sc->arge_mac_unit == 1) 
		reg |= RST_RESET_GE1_MAC;
	ATH_WRITE_REG(AR71XX_RST_RESET, reg);
	DELAY(100);
	reg = ATH_READ_REG(AR71XX_RST_RESET);
	if (sc->arge_mac_unit == 0) 
		reg &= ~RST_RESET_GE0_MAC;
	else if (sc->arge_mac_unit == 1) 
		reg &= ~RST_RESET_GE1_MAC;
	ATH_WRITE_REG(AR71XX_RST_RESET, reg);

	/* Step 3. Reconfigure MAC block */
	ARGE_WRITE(sc, AR71XX_MAC_CFG1, 
		MAC_CFG1_SYNC_RX | MAC_CFG1_RX_ENABLE |
		MAC_CFG1_SYNC_TX | MAC_CFG1_TX_ENABLE);

	reg = ARGE_READ(sc, AR71XX_MAC_CFG2);
	reg |= MAC_CFG2_ENABLE_PADCRC | MAC_CFG2_LENGTH_FIELD ;
	ARGE_WRITE(sc, AR71XX_MAC_CFG2, reg);

	ARGE_WRITE(sc, AR71XX_MAC_MAX_FRAME_LEN, 1536);

	/* Reset MII bus */
	ARGE_WRITE(sc, AR71XX_MAC_MII_CFG, MAC_MII_CFG_RESET);
	DELAY(100);
	ARGE_WRITE(sc, AR71XX_MAC_MII_CFG, MAC_MII_CFG_CLOCK_DIV_28);
	DELAY(100);

	/* 
	 * Set all Ethernet address registers to the same initial values
	 * set all four addresses to 66-88-aa-cc-dd-ee 
	 */
	ARGE_WRITE(sc, AR71XX_MAC_STA_ADDR1, 
	    (eaddr[2] << 24) | (eaddr[3] << 16) | (eaddr[4] << 8)  | eaddr[5]);
	ARGE_WRITE(sc, AR71XX_MAC_STA_ADDR2, (eaddr[0] << 8) | eaddr[1]);

	ARGE_WRITE(sc, AR71XX_MAC_FIFO_CFG0, 
	    FIFO_CFG0_ALL << FIFO_CFG0_ENABLE_SHIFT);
	ARGE_WRITE(sc, AR71XX_MAC_FIFO_CFG1, 0x0fff0000);
	ARGE_WRITE(sc, AR71XX_MAC_FIFO_CFG2, 0x00001fff);

	ARGE_WRITE(sc, AR71XX_MAC_FIFO_RX_FILTMATCH, 
	    FIFO_RX_FILTMATCH_DEFAULT);

	ARGE_WRITE(sc, AR71XX_MAC_FIFO_RX_FILTMASK, 
	    FIFO_RX_FILTMASK_DEFAULT);

	/* Do MII setup. */
	if (mii_phy_probe(dev, &sc->arge_miibus,
	    arge_ifmedia_upd, arge_ifmedia_sts)) {
		device_printf(dev, "MII without any phy!\n");
		error = ENXIO;
		goto fail;
	}

	/* Call MI attach routine. */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->arge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    arge_intr_filter, arge_intr, sc, &sc->arge_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error) 
		arge_detach(dev);

	return (error);
}

static int
arge_detach(device_t dev)
{
	struct arge_softc	*sc = device_get_softc(dev);
	struct ifnet		*ifp = sc->arge_ifp;

	KASSERT(mtx_initialized(&sc->arge_mtx), ("arge mutex not initialized"));

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		ARGE_LOCK(sc);
		sc->arge_detach = 1;
#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING)
			ether_poll_deregister(ifp);
#endif

		arge_stop(sc);
		ARGE_UNLOCK(sc);
		taskqueue_drain(taskqueue_swi, &sc->arge_link_task);
		ether_ifdetach(ifp);
	}

	if (sc->arge_miibus)
		device_delete_child(dev, sc->arge_miibus);
	bus_generic_detach(dev);

	if (sc->arge_intrhand)
		bus_teardown_intr(dev, sc->arge_irq, sc->arge_intrhand);

	if (sc->arge_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->arge_rid, 
		    sc->arge_res);

	if (ifp)
		if_free(ifp);

	arge_dma_free(sc);

	mtx_destroy(&sc->arge_mtx);

	return (0);

}

static int
arge_suspend(device_t dev)
{

	panic("%s", __func__);
	return 0;
}

static int
arge_resume(device_t dev)
{

	panic("%s", __func__);
	return 0;
}

static int
arge_shutdown(device_t dev)
{
	struct arge_softc	*sc;

	sc = device_get_softc(dev);

	ARGE_LOCK(sc);
	arge_stop(sc);
	ARGE_UNLOCK(sc);

	return (0);
}

static int
arge_miibus_readreg(device_t dev, int phy, int reg)
{
	struct arge_softc * sc = device_get_softc(dev);
	int i, result;
	uint32_t addr = (phy << MAC_MII_PHY_ADDR_SHIFT) 
	    | (reg & MAC_MII_REG_MASK);

	if (phy != sc->arge_phy_num)
		return (0);

	ARGE_WRITE(sc, AR71XX_MAC_MII_CMD, MAC_MII_CMD_WRITE);
	ARGE_WRITE(sc, AR71XX_MAC_MII_ADDR, addr);
	ARGE_WRITE(sc, AR71XX_MAC_MII_CMD, MAC_MII_CMD_READ);

	i = ARGE_MII_TIMEOUT;
	while ((ARGE_READ(sc, AR71XX_MAC_MII_INDICATOR) & 
	    MAC_MII_INDICATOR_BUSY) && (i--))
		DELAY(5);

	if (i < 0) {
		dprintf("%s timedout\n", __func__);
		/* XXX: return ERRNO istead? */
		return (-1);
	}

	result = ARGE_READ(sc, AR71XX_MAC_MII_STATUS) & MAC_MII_STATUS_MASK;
	ARGE_WRITE(sc, AR71XX_MAC_MII_CMD, MAC_MII_CMD_WRITE);
	dprintf("%s: phy=%d, reg=%02x, value[%08x]=%04x\n", __func__, 
		 phy, reg, addr, result);

	return (result);
}

static int
arge_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct arge_softc * sc = device_get_softc(dev);
	int i;
	uint32_t addr = 
	    (phy << MAC_MII_PHY_ADDR_SHIFT) | (reg & MAC_MII_REG_MASK);

	dprintf("%s: phy=%d, reg=%02x, value=%04x\n", __func__, 
	    phy, reg, data);

	ARGE_WRITE(sc, AR71XX_MAC_MII_ADDR, addr);
	ARGE_WRITE(sc, AR71XX_MAC_MII_CONTROL, data);

	i = ARGE_MII_TIMEOUT;
	while ((ARGE_READ(sc, AR71XX_MAC_MII_INDICATOR) & 
	    MAC_MII_INDICATOR_BUSY) && (i--))
		DELAY(5);

	if (i < 0) {
		dprintf("%s timedout\n", __func__);
		/* XXX: return ERRNO istead? */
		return (-1);
	}

	return (0);
}

static void
arge_miibus_statchg(device_t dev)
{
	struct arge_softc		*sc;

	sc = device_get_softc(dev);
	taskqueue_enqueue(taskqueue_swi, &sc->arge_link_task);
}

static void
arge_link_task(void *arg, int pending)
{
	struct arge_softc	*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;
	uint32_t		media;
	uint32_t		cfg, ifcontrol, rx_filtmask, pll, sec_cfg;

	sc = (struct arge_softc *)arg;

	ARGE_LOCK(sc);
	mii = device_get_softc(sc->arge_miibus);
	ifp = sc->arge_ifp;
	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		ARGE_UNLOCK(sc);
		return;
	}

	if (mii->mii_media_status & IFM_ACTIVE) {

		media = IFM_SUBTYPE(mii->mii_media_active);

		if (media != IFM_NONE) {
			sc->arge_link_status = 1;

			cfg = ARGE_READ(sc, AR71XX_MAC_CFG2);
			cfg &= ~(MAC_CFG2_IFACE_MODE_1000 
			    | MAC_CFG2_IFACE_MODE_10_100 
			    | MAC_CFG2_FULL_DUPLEX);

			if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
				cfg |= MAC_CFG2_FULL_DUPLEX;

			ifcontrol = ARGE_READ(sc, AR71XX_MAC_IFCONTROL);
			ifcontrol &= ~MAC_IFCONTROL_SPEED;
			rx_filtmask = 
			    ARGE_READ(sc, AR71XX_MAC_FIFO_RX_FILTMASK);
			rx_filtmask &= ~FIFO_RX_MASK_BYTE_MODE;

			switch(media) {
			case IFM_10_T:
				cfg |= MAC_CFG2_IFACE_MODE_10_100;
				pll = PLL_ETH_INT_CLK_10;
				break;
			case IFM_100_TX:
				cfg |= MAC_CFG2_IFACE_MODE_10_100;
				ifcontrol |= MAC_IFCONTROL_SPEED;
				pll = PLL_ETH_INT_CLK_100;
				break;
			case IFM_1000_T:
			case IFM_1000_SX:
				cfg |= MAC_CFG2_IFACE_MODE_1000;
				rx_filtmask |= FIFO_RX_MASK_BYTE_MODE;
				pll = PLL_ETH_INT_CLK_1000;
				break;
			default:
				pll = PLL_ETH_INT_CLK_100;
				device_printf(sc->arge_dev, 
				    "Unknown media %d\n", media);
			}

			ARGE_WRITE(sc, AR71XX_MAC_FIFO_TX_THRESHOLD,
			    0x008001ff);

			ARGE_WRITE(sc, AR71XX_MAC_CFG2, cfg);
			ARGE_WRITE(sc, AR71XX_MAC_IFCONTROL, ifcontrol);
			ARGE_WRITE(sc, AR71XX_MAC_FIFO_RX_FILTMASK, 
			    rx_filtmask);

			/* set PLL registers */
			sec_cfg = ATH_READ_REG(AR71XX_PLL_CPU_CONFIG);
			sec_cfg &= ~(3 << 17);
			sec_cfg |= (2 << 17);

			ATH_WRITE_REG(AR71XX_PLL_CPU_CONFIG, sec_cfg);
			DELAY(100);

			ATH_WRITE_REG(sc->arge_pll_reg, pll);

			sec_cfg |= (3 << 17);
			ATH_WRITE_REG(AR71XX_PLL_CPU_CONFIG, sec_cfg);
			DELAY(100);

			sec_cfg &= ~(3 << 17);
			ATH_WRITE_REG(AR71XX_PLL_CPU_CONFIG, sec_cfg);
			DELAY(100);
		}
	} else
		sc->arge_link_status = 0;

	ARGE_UNLOCK(sc);
}

static void
arge_reset_dma(struct arge_softc *sc)
{
	ARGE_WRITE(sc, AR71XX_DMA_RX_CONTROL, 0);
	ARGE_WRITE(sc, AR71XX_DMA_TX_CONTROL, 0);

	ARGE_WRITE(sc, AR71XX_DMA_RX_DESC, 0);
	ARGE_WRITE(sc, AR71XX_DMA_TX_DESC, 0);

	/* Clear all possible RX interrupts */
	while(ARGE_READ(sc, AR71XX_DMA_RX_STATUS) & DMA_RX_STATUS_PKT_RECVD)
		ARGE_WRITE(sc, AR71XX_DMA_RX_STATUS, DMA_RX_STATUS_PKT_RECVD);

	/* 
	 * Clear all possible TX interrupts
	 */
	while(ARGE_READ(sc, AR71XX_DMA_TX_STATUS) & DMA_TX_STATUS_PKT_SENT)
		ARGE_WRITE(sc, AR71XX_DMA_TX_STATUS, DMA_TX_STATUS_PKT_SENT);

	/* 
	 * Now Rx/Tx errors
	 */
	ARGE_WRITE(sc, AR71XX_DMA_RX_STATUS, 
	    DMA_RX_STATUS_BUS_ERROR | DMA_RX_STATUS_OVERFLOW);
	ARGE_WRITE(sc, AR71XX_DMA_TX_STATUS, 
	    DMA_TX_STATUS_BUS_ERROR | DMA_TX_STATUS_UNDERRUN);
}



static void
arge_init(void *xsc)
{
	struct arge_softc	 *sc = xsc;

	ARGE_LOCK(sc);
	arge_init_locked(sc);
	ARGE_UNLOCK(sc);
}

static void
arge_init_locked(struct arge_softc *sc)
{
	struct ifnet		*ifp = sc->arge_ifp;
	struct mii_data		*mii;

	ARGE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->arge_miibus);

	arge_stop(sc);

	/* Init circular RX list. */
	if (arge_rx_ring_init(sc) != 0) {
		device_printf(sc->arge_dev,
		    "initialization failed: no memory for rx buffers\n");
		arge_stop(sc);
		return;
	}

	/* Init tx descriptors. */
	arge_tx_ring_init(sc);

	arge_reset_dma(sc);

	sc->arge_link_status = 0;
	mii_mediachg(mii);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->arge_stat_callout, hz, arge_tick, sc);

	ARGE_WRITE(sc, AR71XX_DMA_TX_DESC, ARGE_TX_RING_ADDR(sc, 0));
	ARGE_WRITE(sc, AR71XX_DMA_RX_DESC, ARGE_RX_RING_ADDR(sc, 0));

	/* Start listening */
	ARGE_WRITE(sc, AR71XX_DMA_RX_CONTROL, DMA_RX_CONTROL_EN);

	/* Enable interrupts */
	ARGE_WRITE(sc, AR71XX_DMA_INTR, DMA_INTR_ALL);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
arge_encap(struct arge_softc *sc, struct mbuf **m_head)
{
	struct arge_txdesc	*txd;
	struct arge_desc	*desc, *prev_desc;
	bus_dma_segment_t	txsegs[ARGE_MAXFRAGS];
	int			error, i, nsegs, prod, prev_prod;
	struct mbuf		*m;

	ARGE_LOCK_ASSERT(sc);

	/*
	 * Fix mbuf chain, all fragments should be 4 bytes aligned and
	 * even 4 bytes
	 */
	m = *m_head;
	if((mtod(m, intptr_t) & 3) != 0) {
		m = m_defrag(*m_head, M_DONTWAIT);
		if (m == NULL) {
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
	}

	prod = sc->arge_cdata.arge_tx_prod;
	txd = &sc->arge_cdata.arge_txdesc[prod];
	error = bus_dmamap_load_mbuf_sg(sc->arge_cdata.arge_tx_tag, 
	    txd->tx_dmamap, *m_head, txsegs, &nsegs, BUS_DMA_NOWAIT);

	if (error == EFBIG) {
		panic("EFBIG");
	} else if (error != 0)
		return (error);

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	/* Check number of available descriptors. */
	if (sc->arge_cdata.arge_tx_cnt + nsegs >= (ARGE_TX_RING_COUNT - 1)) {
		bus_dmamap_unload(sc->arge_cdata.arge_tx_tag, txd->tx_dmamap);
		return (ENOBUFS);
	}

	txd->tx_m = *m_head;
	bus_dmamap_sync(sc->arge_cdata.arge_tx_tag, txd->tx_dmamap,
	    BUS_DMASYNC_PREWRITE);

	/* 
	 * Make a list of descriptors for this packet. DMA controller will
	 * walk through it while arge_link is not zero.
	 */
	prev_prod = prod;
	desc = prev_desc = NULL;
	for (i = 0; i < nsegs; i++) {
		desc = &sc->arge_rdata.arge_tx_ring[prod];
		desc->packet_ctrl = ARGE_DMASIZE(txsegs[i].ds_len);

		if (txsegs[i].ds_addr & 3)
			panic("TX packet address unaligned\n");

		desc->packet_addr = txsegs[i].ds_addr;
		
		/* link with previous descriptor */
		if (prev_desc)
			prev_desc->packet_ctrl |= ARGE_DESC_MORE;

		sc->arge_cdata.arge_tx_cnt++;
		prev_desc = desc;
		ARGE_INC(prod, ARGE_TX_RING_COUNT);
	}

	/* Update producer index. */
	sc->arge_cdata.arge_tx_prod = prod;

	/* Sync descriptors. */
	bus_dmamap_sync(sc->arge_cdata.arge_tx_ring_tag,
	    sc->arge_cdata.arge_tx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Start transmitting */
	ARGE_WRITE(sc, AR71XX_DMA_TX_CONTROL, DMA_TX_CONTROL_EN);
	return (0);
}

static void
arge_start(struct ifnet *ifp)
{
	struct arge_softc	 *sc;

	sc = ifp->if_softc;

	ARGE_LOCK(sc);
	arge_start_locked(ifp);
	ARGE_UNLOCK(sc);
}

static void
arge_start_locked(struct ifnet *ifp)
{
	struct arge_softc	*sc;
	struct mbuf		*m_head;
	int			enq;

	sc = ifp->if_softc;

	ARGE_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || sc->arge_link_status == 0 )
		return;

	arge_flush_ddr(sc);

	for (enq = 0; !IFQ_DRV_IS_EMPTY(&ifp->if_snd) &&
	    sc->arge_cdata.arge_tx_cnt < ARGE_TX_RING_COUNT - 2; ) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;


		/*
		 * Pack the data into the transmit ring.
		 */
		if (arge_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		enq++;
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		ETHER_BPF_MTAP(ifp, m_head);
	}
}

static void
arge_stop(struct arge_softc *sc)
{
	struct ifnet	    *ifp;

	ARGE_LOCK_ASSERT(sc);

	ifp = sc->arge_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	callout_stop(&sc->arge_stat_callout);

	/* mask out interrupts */
	ARGE_WRITE(sc, AR71XX_DMA_INTR, 0);

	arge_reset_dma(sc);
}


static int
arge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct arge_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error;
#ifdef DEVICE_POLLING
	int			mask;
#endif

	switch (command) {
	case SIOCSIFFLAGS:
		ARGE_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if (((ifp->if_flags ^ sc->arge_if_flags)
				    & (IFF_PROMISC | IFF_ALLMULTI)) != 0)
					arge_rx_locked(sc);
			} else {
				if (!sc->arge_detach)
					arge_init_locked(sc);
			}
		} else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			arge_stop(sc);
		}
		sc->arge_if_flags = ifp->if_flags;
		ARGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* XXX: implement SIOCDELMULTI */
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->arge_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
        case SIOCSIFCAP:
		/* XXX: Check other capabilities */
#ifdef DEVICE_POLLING
                mask = ifp->if_capenable ^ ifr->ifr_reqcap;
                if (mask & IFCAP_POLLING) {
                        if (ifr->ifr_reqcap & IFCAP_POLLING) {
				ARGE_WRITE(sc, AR71XX_DMA_INTR, 0);
                                error = ether_poll_register(arge_poll, ifp);
                                if (error)
                                        return error;
                                ARGE_LOCK(sc);
                                ifp->if_capenable |= IFCAP_POLLING;
                                ARGE_UNLOCK(sc);
                        } else {
				ARGE_WRITE(sc, AR71XX_DMA_INTR, DMA_INTR_ALL);
                                error = ether_poll_deregister(ifp);
                                ARGE_LOCK(sc);
                                ifp->if_capenable &= ~IFCAP_POLLING;
                                ARGE_UNLOCK(sc);
                        }
                }
		error = 0;
                break;
#endif
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

/*
 * Set media options.
 */
static int
arge_ifmedia_upd(struct ifnet *ifp)
{
	struct arge_softc		*sc;
	struct mii_data		*mii;
	struct mii_softc	*miisc;
	int			error;

	sc = ifp->if_softc;
	ARGE_LOCK(sc);
	mii = device_get_softc(sc->arge_miibus);
	if (mii->mii_instance) {
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);
	ARGE_UNLOCK(sc);

	return (error);
}

/*
 * Report current media status.
 */
static void
arge_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct arge_softc		*sc = ifp->if_softc;
	struct mii_data		*mii;

	mii = device_get_softc(sc->arge_miibus);
	ARGE_LOCK(sc);
	mii_pollstat(mii);
	ARGE_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

struct arge_dmamap_arg {
	bus_addr_t	arge_busaddr;
};

static void
arge_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct arge_dmamap_arg	*ctx;

	if (error != 0)
		return;
	ctx = arg;
	ctx->arge_busaddr = segs[0].ds_addr;
}

static int
arge_dma_alloc(struct arge_softc *sc)
{
	struct arge_dmamap_arg	ctx;
	struct arge_txdesc	*txd;
	struct arge_rxdesc	*rxd;
	int			error, i;

	/* Create parent DMA tag. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->arge_dev),	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->arge_cdata.arge_parent_tag);
	if (error != 0) {
		device_printf(sc->arge_dev, "failed to create parent DMA tag\n");
		goto fail;
	}
	/* Create tag for Tx ring. */
	error = bus_dma_tag_create(
	    sc->arge_cdata.arge_parent_tag,	/* parent */
	    ARGE_RING_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    ARGE_TX_DMA_SIZE,		/* maxsize */
	    1,				/* nsegments */
	    ARGE_TX_DMA_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->arge_cdata.arge_tx_ring_tag);
	if (error != 0) {
		device_printf(sc->arge_dev, "failed to create Tx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx ring. */
	error = bus_dma_tag_create(
	    sc->arge_cdata.arge_parent_tag,	/* parent */
	    ARGE_RING_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    ARGE_RX_DMA_SIZE,		/* maxsize */
	    1,				/* nsegments */
	    ARGE_RX_DMA_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->arge_cdata.arge_rx_ring_tag);
	if (error != 0) {
		device_printf(sc->arge_dev, "failed to create Rx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Tx buffers. */
	error = bus_dma_tag_create(
	    sc->arge_cdata.arge_parent_tag,	/* parent */
	    sizeof(uint32_t), 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES * ARGE_MAXFRAGS,	/* maxsize */
	    ARGE_MAXFRAGS,		/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->arge_cdata.arge_tx_tag);
	if (error != 0) {
		device_printf(sc->arge_dev, "failed to create Tx DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx buffers. */
	error = bus_dma_tag_create(
	    sc->arge_cdata.arge_parent_tag,	/* parent */
	    ARGE_RX_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES,			/* maxsize */
	    ARGE_MAXFRAGS,		/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->arge_cdata.arge_rx_tag);
	if (error != 0) {
		device_printf(sc->arge_dev, "failed to create Rx DMA tag\n");
		goto fail;
	}

	/* Allocate DMA'able memory and load the DMA map for Tx ring. */
	error = bus_dmamem_alloc(sc->arge_cdata.arge_tx_ring_tag,
	    (void **)&sc->arge_rdata.arge_tx_ring, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->arge_cdata.arge_tx_ring_map);
	if (error != 0) {
		device_printf(sc->arge_dev,
		    "failed to allocate DMA'able memory for Tx ring\n");
		goto fail;
	}

	ctx.arge_busaddr = 0;
	error = bus_dmamap_load(sc->arge_cdata.arge_tx_ring_tag,
	    sc->arge_cdata.arge_tx_ring_map, sc->arge_rdata.arge_tx_ring,
	    ARGE_TX_DMA_SIZE, arge_dmamap_cb, &ctx, 0);
	if (error != 0 || ctx.arge_busaddr == 0) {
		device_printf(sc->arge_dev,
		    "failed to load DMA'able memory for Tx ring\n");
		goto fail;
	}
	sc->arge_rdata.arge_tx_ring_paddr = ctx.arge_busaddr;

	/* Allocate DMA'able memory and load the DMA map for Rx ring. */
	error = bus_dmamem_alloc(sc->arge_cdata.arge_rx_ring_tag,
	    (void **)&sc->arge_rdata.arge_rx_ring, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->arge_cdata.arge_rx_ring_map);
	if (error != 0) {
		device_printf(sc->arge_dev,
		    "failed to allocate DMA'able memory for Rx ring\n");
		goto fail;
	}

	ctx.arge_busaddr = 0;
	error = bus_dmamap_load(sc->arge_cdata.arge_rx_ring_tag,
	    sc->arge_cdata.arge_rx_ring_map, sc->arge_rdata.arge_rx_ring,
	    ARGE_RX_DMA_SIZE, arge_dmamap_cb, &ctx, 0);
	if (error != 0 || ctx.arge_busaddr == 0) {
		device_printf(sc->arge_dev,
		    "failed to load DMA'able memory for Rx ring\n");
		goto fail;
	}
	sc->arge_rdata.arge_rx_ring_paddr = ctx.arge_busaddr;

	/* Create DMA maps for Tx buffers. */
	for (i = 0; i < ARGE_TX_RING_COUNT; i++) {
		txd = &sc->arge_cdata.arge_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_dmamap = NULL;
		error = bus_dmamap_create(sc->arge_cdata.arge_tx_tag, 0,
		    &txd->tx_dmamap);
		if (error != 0) {
			device_printf(sc->arge_dev,
			    "failed to create Tx dmamap\n");
			goto fail;
		}
	}
	/* Create DMA maps for Rx buffers. */
	if ((error = bus_dmamap_create(sc->arge_cdata.arge_rx_tag, 0,
	    &sc->arge_cdata.arge_rx_sparemap)) != 0) {
		device_printf(sc->arge_dev,
		    "failed to create spare Rx dmamap\n");
		goto fail;
	}
	for (i = 0; i < ARGE_RX_RING_COUNT; i++) {
		rxd = &sc->arge_cdata.arge_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_dmamap = NULL;
		error = bus_dmamap_create(sc->arge_cdata.arge_rx_tag, 0,
		    &rxd->rx_dmamap);
		if (error != 0) {
			device_printf(sc->arge_dev,
			    "failed to create Rx dmamap\n");
			goto fail;
		}
	}

fail:
	return (error);
}

static void
arge_dma_free(struct arge_softc *sc)
{
	struct arge_txdesc	*txd;
	struct arge_rxdesc	*rxd;
	int			i;

	/* Tx ring. */
	if (sc->arge_cdata.arge_tx_ring_tag) {
		if (sc->arge_cdata.arge_tx_ring_map)
			bus_dmamap_unload(sc->arge_cdata.arge_tx_ring_tag,
			    sc->arge_cdata.arge_tx_ring_map);
		if (sc->arge_cdata.arge_tx_ring_map &&
		    sc->arge_rdata.arge_tx_ring)
			bus_dmamem_free(sc->arge_cdata.arge_tx_ring_tag,
			    sc->arge_rdata.arge_tx_ring,
			    sc->arge_cdata.arge_tx_ring_map);
		sc->arge_rdata.arge_tx_ring = NULL;
		sc->arge_cdata.arge_tx_ring_map = NULL;
		bus_dma_tag_destroy(sc->arge_cdata.arge_tx_ring_tag);
		sc->arge_cdata.arge_tx_ring_tag = NULL;
	}
	/* Rx ring. */
	if (sc->arge_cdata.arge_rx_ring_tag) {
		if (sc->arge_cdata.arge_rx_ring_map)
			bus_dmamap_unload(sc->arge_cdata.arge_rx_ring_tag,
			    sc->arge_cdata.arge_rx_ring_map);
		if (sc->arge_cdata.arge_rx_ring_map &&
		    sc->arge_rdata.arge_rx_ring)
			bus_dmamem_free(sc->arge_cdata.arge_rx_ring_tag,
			    sc->arge_rdata.arge_rx_ring,
			    sc->arge_cdata.arge_rx_ring_map);
		sc->arge_rdata.arge_rx_ring = NULL;
		sc->arge_cdata.arge_rx_ring_map = NULL;
		bus_dma_tag_destroy(sc->arge_cdata.arge_rx_ring_tag);
		sc->arge_cdata.arge_rx_ring_tag = NULL;
	}
	/* Tx buffers. */
	if (sc->arge_cdata.arge_tx_tag) {
		for (i = 0; i < ARGE_TX_RING_COUNT; i++) {
			txd = &sc->arge_cdata.arge_txdesc[i];
			if (txd->tx_dmamap) {
				bus_dmamap_destroy(sc->arge_cdata.arge_tx_tag,
				    txd->tx_dmamap);
				txd->tx_dmamap = NULL;
			}
		}
		bus_dma_tag_destroy(sc->arge_cdata.arge_tx_tag);
		sc->arge_cdata.arge_tx_tag = NULL;
	}
	/* Rx buffers. */
	if (sc->arge_cdata.arge_rx_tag) {
		for (i = 0; i < ARGE_RX_RING_COUNT; i++) {
			rxd = &sc->arge_cdata.arge_rxdesc[i];
			if (rxd->rx_dmamap) {
				bus_dmamap_destroy(sc->arge_cdata.arge_rx_tag,
				    rxd->rx_dmamap);
				rxd->rx_dmamap = NULL;
			}
		}
		if (sc->arge_cdata.arge_rx_sparemap) {
			bus_dmamap_destroy(sc->arge_cdata.arge_rx_tag,
			    sc->arge_cdata.arge_rx_sparemap);
			sc->arge_cdata.arge_rx_sparemap = 0;
		}
		bus_dma_tag_destroy(sc->arge_cdata.arge_rx_tag);
		sc->arge_cdata.arge_rx_tag = NULL;
	}

	if (sc->arge_cdata.arge_parent_tag) {
		bus_dma_tag_destroy(sc->arge_cdata.arge_parent_tag);
		sc->arge_cdata.arge_parent_tag = NULL;
	}
}

/*
 * Initialize the transmit descriptors.
 */
static int
arge_tx_ring_init(struct arge_softc *sc)
{
	struct arge_ring_data	*rd;
	struct arge_txdesc	*txd;
	bus_addr_t		addr;
	int			i;

	sc->arge_cdata.arge_tx_prod = 0;
	sc->arge_cdata.arge_tx_cons = 0;
	sc->arge_cdata.arge_tx_cnt = 0;
	sc->arge_cdata.arge_tx_pkts = 0;

	rd = &sc->arge_rdata;
	bzero(rd->arge_tx_ring, sizeof(rd->arge_tx_ring));
	for (i = 0; i < ARGE_TX_RING_COUNT; i++) {
		if (i == ARGE_TX_RING_COUNT - 1)
			addr = ARGE_TX_RING_ADDR(sc, 0);
		else
			addr = ARGE_TX_RING_ADDR(sc, i + 1);
		rd->arge_tx_ring[i].packet_ctrl = ARGE_DESC_EMPTY;
		rd->arge_tx_ring[i].next_desc = addr;
		txd = &sc->arge_cdata.arge_txdesc[i];
		txd->tx_m = NULL;
	}

	bus_dmamap_sync(sc->arge_cdata.arge_tx_ring_tag,
	    sc->arge_cdata.arge_tx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
arge_rx_ring_init(struct arge_softc *sc)
{
	struct arge_ring_data	*rd;
	struct arge_rxdesc	*rxd;
	bus_addr_t		addr;
	int			i;

	sc->arge_cdata.arge_rx_cons = 0;

	rd = &sc->arge_rdata;
	bzero(rd->arge_rx_ring, sizeof(rd->arge_rx_ring));
	for (i = 0; i < ARGE_RX_RING_COUNT; i++) {
		rxd = &sc->arge_cdata.arge_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->desc = &rd->arge_rx_ring[i];
		if (i == ARGE_RX_RING_COUNT - 1)
			addr = ARGE_RX_RING_ADDR(sc, 0);
		else
			addr = ARGE_RX_RING_ADDR(sc, i + 1);
		rd->arge_rx_ring[i].next_desc = addr;
		if (arge_newbuf(sc, i) != 0) {
			return (ENOBUFS);
		}
	}

	bus_dmamap_sync(sc->arge_cdata.arge_rx_ring_tag,
	    sc->arge_cdata.arge_rx_ring_map,
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
arge_newbuf(struct arge_softc *sc, int idx)
{
	struct arge_desc		*desc;
	struct arge_rxdesc	*rxd;
	struct mbuf		*m;
	bus_dma_segment_t	segs[1];
	bus_dmamap_t		map;
	int			nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, sizeof(uint64_t));

	if (bus_dmamap_load_mbuf_sg(sc->arge_cdata.arge_rx_tag,
	    sc->arge_cdata.arge_rx_sparemap, m, segs, &nsegs, 0) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	rxd = &sc->arge_cdata.arge_rxdesc[idx];
	if (rxd->rx_m != NULL) {
		bus_dmamap_unload(sc->arge_cdata.arge_rx_tag, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = sc->arge_cdata.arge_rx_sparemap;
	sc->arge_cdata.arge_rx_sparemap = map;
	rxd->rx_m = m;
	desc = rxd->desc;
	if (segs[0].ds_addr & 3)
		panic("RX packet address unaligned");
	desc->packet_addr = segs[0].ds_addr;
	desc->packet_ctrl = ARGE_DESC_EMPTY | ARGE_DMASIZE(segs[0].ds_len);

	bus_dmamap_sync(sc->arge_cdata.arge_rx_ring_tag,
	    sc->arge_cdata.arge_rx_ring_map,
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

static __inline void
arge_fixup_rx(struct mbuf *m)
{
        int		i;
        uint16_t	*src, *dst;

	src = mtod(m, uint16_t *);
	dst = src - 1;

	for (i = 0; i < m->m_len / sizeof(uint16_t); i++) {
		*dst++ = *src++;
	}

	if (m->m_len % sizeof(uint16_t))
		*(uint8_t *)dst = *(uint8_t *)src;

	m->m_data -= ETHER_ALIGN;
}

#ifdef DEVICE_POLLING
static int
arge_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct arge_softc *sc = ifp->if_softc;
	int rx_npkts = 0;

        if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ARGE_LOCK(sc);
		arge_tx_locked(sc);
		rx_npkts = arge_rx_locked(sc);
		ARGE_UNLOCK(sc);
        }

	return (rx_npkts);
}
#endif /* DEVICE_POLLING */


static void
arge_tx_locked(struct arge_softc *sc)
{
	struct arge_txdesc	*txd;
	struct arge_desc	*cur_tx;
	struct ifnet		*ifp;
	uint32_t		ctrl;
	int			cons, prod;

	ARGE_LOCK_ASSERT(sc);

	cons = sc->arge_cdata.arge_tx_cons;
	prod = sc->arge_cdata.arge_tx_prod;
	if (cons == prod)
		return;

	bus_dmamap_sync(sc->arge_cdata.arge_tx_ring_tag,
	    sc->arge_cdata.arge_tx_ring_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	ifp = sc->arge_ifp;
	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (; cons != prod; ARGE_INC(cons, ARGE_TX_RING_COUNT)) {
		cur_tx = &sc->arge_rdata.arge_tx_ring[cons];
		ctrl = cur_tx->packet_ctrl;
		/* Check if descriptor has "finished" flag */
		if ((ctrl & ARGE_DESC_EMPTY) == 0)
			break;

		ARGE_WRITE(sc, AR71XX_DMA_TX_STATUS, DMA_TX_STATUS_PKT_SENT);

		sc->arge_cdata.arge_tx_cnt--;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		txd = &sc->arge_cdata.arge_txdesc[cons];

		ifp->if_opackets++;

		bus_dmamap_sync(sc->arge_cdata.arge_tx_tag, txd->tx_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->arge_cdata.arge_tx_tag, txd->tx_dmamap);

		/* Free only if it's first descriptor in list */
		if (txd->tx_m)
			m_freem(txd->tx_m);
		txd->tx_m = NULL;

		/* reset descriptor */
		cur_tx->packet_addr = 0;
	}

	sc->arge_cdata.arge_tx_cons = cons;

	bus_dmamap_sync(sc->arge_cdata.arge_tx_ring_tag,
	    sc->arge_cdata.arge_tx_ring_map, BUS_DMASYNC_PREWRITE);
}


static int
arge_rx_locked(struct arge_softc *sc)
{
	struct arge_rxdesc	*rxd;
	struct ifnet		*ifp = sc->arge_ifp;
	int			cons, prog, packet_len, i;
	struct arge_desc	*cur_rx;
	struct mbuf		*m;
	int			rx_npkts = 0;

	ARGE_LOCK_ASSERT(sc);

	cons = sc->arge_cdata.arge_rx_cons;

	bus_dmamap_sync(sc->arge_cdata.arge_rx_ring_tag,
	    sc->arge_cdata.arge_rx_ring_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (prog = 0; prog < ARGE_RX_RING_COUNT; 
	    ARGE_INC(cons, ARGE_RX_RING_COUNT)) {
		cur_rx = &sc->arge_rdata.arge_rx_ring[cons];
		rxd = &sc->arge_cdata.arge_rxdesc[cons];
		m = rxd->rx_m;

		if ((cur_rx->packet_ctrl & ARGE_DESC_EMPTY) != 0)
		       break;	

		ARGE_WRITE(sc, AR71XX_DMA_RX_STATUS, DMA_RX_STATUS_PKT_RECVD);

		prog++;

		packet_len = ARGE_DMASIZE(cur_rx->packet_ctrl);
		bus_dmamap_sync(sc->arge_cdata.arge_rx_tag, rxd->rx_dmamap,
		    BUS_DMASYNC_POSTREAD);
		m = rxd->rx_m;

		arge_fixup_rx(m);
		m->m_pkthdr.rcvif = ifp;
		/* Skip 4 bytes of CRC */
		m->m_pkthdr.len = m->m_len = packet_len - ETHER_CRC_LEN;
		ifp->if_ipackets++;
		rx_npkts++;

		ARGE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		ARGE_LOCK(sc);
		cur_rx->packet_addr = 0;
	}

	if (prog > 0) {

		i = sc->arge_cdata.arge_rx_cons;
		for (; prog > 0 ; prog--) {
			if (arge_newbuf(sc, i) != 0) {
				device_printf(sc->arge_dev, 
				    "Failed to allocate buffer\n");
				break;
			}
			ARGE_INC(i, ARGE_RX_RING_COUNT);
		}

		bus_dmamap_sync(sc->arge_cdata.arge_rx_ring_tag,
		    sc->arge_cdata.arge_rx_ring_map,
		    BUS_DMASYNC_PREWRITE);

		sc->arge_cdata.arge_rx_cons = cons;
	}

	return (rx_npkts);
}

static int
arge_intr_filter(void *arg)
{
	struct arge_softc	*sc = arg;
	uint32_t		status, ints;

	status = ARGE_READ(sc, AR71XX_DMA_INTR_STATUS);
	ints = ARGE_READ(sc, AR71XX_DMA_INTR);

#if 0
	dprintf("int mask(filter) = %b\n", ints,
	    "\20\10RX_BUS_ERROR\7RX_OVERFLOW\5RX_PKT_RCVD"
	    "\4TX_BUS_ERROR\2TX_UNDERRUN\1TX_PKT_SENT");
	dprintf("status(filter) = %b\n", status, 
	    "\20\10RX_BUS_ERROR\7RX_OVERFLOW\5RX_PKT_RCVD"
	    "\4TX_BUS_ERROR\2TX_UNDERRUN\1TX_PKT_SENT");
#endif

	if (status & DMA_INTR_ALL) {
		sc->arge_intr_status |= status;
		ARGE_WRITE(sc, AR71XX_DMA_INTR, 0);
		return (FILTER_SCHEDULE_THREAD);
	} 

	sc->arge_intr_status = 0;
	return (FILTER_STRAY);
}

static void
arge_intr(void *arg)
{
	struct arge_softc	*sc = arg;
	uint32_t		status;

	status = ARGE_READ(sc, AR71XX_DMA_INTR_STATUS);
	status |= sc->arge_intr_status;

#if 0
	dprintf("int status(intr) = %b\n", status, 
	    "\20\10\7RX_OVERFLOW\5RX_PKT_RCVD"
	    "\4TX_BUS_ERROR\2TX_UNDERRUN\1TX_PKT_SENT");
#endif

	/* 
	 * Is it our interrupt at all? 
	 */
	if (status == 0)
		return;

	if (status & DMA_INTR_RX_BUS_ERROR) {
		ARGE_WRITE(sc, AR71XX_DMA_RX_STATUS, DMA_RX_STATUS_BUS_ERROR);
		device_printf(sc->arge_dev, "RX bus error");
		return;
	}

	if (status & DMA_INTR_TX_BUS_ERROR) {
		ARGE_WRITE(sc, AR71XX_DMA_TX_STATUS, DMA_TX_STATUS_BUS_ERROR);
		device_printf(sc->arge_dev, "TX bus error");
		return;
	}

	ARGE_LOCK(sc);

	if (status & DMA_INTR_RX_PKT_RCVD)
		arge_rx_locked(sc);

	/* 
	 * RX overrun disables the receiver. 
	 * Clear indication and re-enable rx. 
	 */
	if ( status & DMA_INTR_RX_OVERFLOW) {
		ARGE_WRITE(sc, AR71XX_DMA_RX_STATUS, DMA_RX_STATUS_OVERFLOW);
		ARGE_WRITE(sc, AR71XX_DMA_RX_CONTROL, DMA_RX_CONTROL_EN);
	}

	if (status & DMA_INTR_TX_PKT_SENT)
		arge_tx_locked(sc);
	/* 
	 * Underrun turns off TX. Clear underrun indication. 
	 * If there's anything left in the ring, reactivate the tx. 
	 */
	if (status & DMA_INTR_TX_UNDERRUN) {
		ARGE_WRITE(sc, AR71XX_DMA_TX_STATUS, DMA_TX_STATUS_UNDERRUN);
		if (sc->arge_cdata.arge_tx_pkts > 0 ) {
			ARGE_WRITE(sc, AR71XX_DMA_TX_CONTROL, 
			    DMA_TX_CONTROL_EN);
		}
	}

	/*
	 * We handled all bits, clear status
	 */
	sc->arge_intr_status = 0;
	ARGE_UNLOCK(sc);
	/*
	 * re-enable all interrupts 
	 */
	ARGE_WRITE(sc, AR71XX_DMA_INTR, DMA_INTR_ALL);
}


static void
arge_tick(void *xsc)
{
	struct arge_softc	*sc = xsc;
	struct mii_data		*mii;

	ARGE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->arge_miibus);
	mii_tick(mii);
	callout_reset(&sc->arge_stat_callout, hz, arge_tick, sc);
}

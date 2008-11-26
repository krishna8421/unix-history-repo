/*-
 * Copyright (c) 1999 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

/*
 * i686 memory range operations
 *
 * This code will probably be impenetrable without reference to the
 * Intel Pentium Pro documentation.
 */

static char *mem_owner_bios = "BIOS";

#define	MR686_FIXMTRR	(1<<0)

#define	mrwithin(mr, a)							\
	(((a) >= (mr)->mr_base) && ((a) < ((mr)->mr_base + (mr)->mr_len)))
#define	mroverlap(mra, mrb)						\
	(mrwithin(mra, mrb->mr_base) || mrwithin(mrb, mra->mr_base))

#define	mrvalid(base, len) 						\
	((!(base & ((1 << 12) - 1))) &&	/* base is multiple of 4k */	\
	    ((len) >= (1 << 12)) &&	/* length is >= 4k */		\
	    powerof2((len)) &&		/* ... and power of two */	\
	    !((base) & ((len) - 1)))	/* range is not discontiuous */

#define	mrcopyflags(curr, new)						\
	(((curr) & ~MDF_ATTRMASK) | ((new) & MDF_ATTRMASK))

static int mtrrs_disabled;
TUNABLE_INT("machdep.disable_mtrrs", &mtrrs_disabled);
SYSCTL_INT(_machdep, OID_AUTO, disable_mtrrs, CTLFLAG_RDTUN,
    &mtrrs_disabled, 0, "Disable i686 MTRRs.");

static void	i686_mrinit(struct mem_range_softc *sc);
static int	i686_mrset(struct mem_range_softc *sc,
		    struct mem_range_desc *mrd, int *arg);
static void	i686_mrAPinit(struct mem_range_softc *sc);

static struct mem_range_ops i686_mrops = {
	i686_mrinit,
	i686_mrset,
	i686_mrAPinit
};

/* XXX for AP startup hook */
static u_int64_t mtrrcap, mtrrdef;

/* The bitmask for the PhysBase and PhysMask fields of the variable MTRRs. */
static u_int64_t mtrr_physmask;

static struct mem_range_desc *mem_range_match(struct mem_range_softc *sc,
		    struct mem_range_desc *mrd);
static void	i686_mrfetch(struct mem_range_softc *sc);
static int	i686_mtrrtype(int flags);
static int	i686_mrt2mtrr(int flags, int oldval);
static int	i686_mtrrconflict(int flag1, int flag2);
static void	i686_mrstore(struct mem_range_softc *sc);
static void	i686_mrstoreone(void *arg);
static struct mem_range_desc *i686_mtrrfixsearch(struct mem_range_softc *sc,
		    u_int64_t addr);
static int	i686_mrsetlow(struct mem_range_softc *sc,
		    struct mem_range_desc *mrd, int *arg);
static int	i686_mrsetvariable(struct mem_range_softc *sc,
		    struct mem_range_desc *mrd, int *arg);

/* i686 MTRR type to memory range type conversion */
static int i686_mtrrtomrt[] = {
	MDF_UNCACHEABLE,
	MDF_WRITECOMBINE,
	MDF_UNKNOWN,
	MDF_UNKNOWN,
	MDF_WRITETHROUGH,
	MDF_WRITEPROTECT,
	MDF_WRITEBACK
};

#define	MTRRTOMRTLEN (sizeof(i686_mtrrtomrt) / sizeof(i686_mtrrtomrt[0]))

static int
i686_mtrr2mrt(int val)
{

	if (val < 0 || val >= MTRRTOMRTLEN)
		return (MDF_UNKNOWN);
	return (i686_mtrrtomrt[val]);
}

/*
 * i686 MTRR conflicts. Writeback and uncachable may overlap.
 */
static int
i686_mtrrconflict(int flag1, int flag2)
{

	flag1 &= MDF_ATTRMASK;
	flag2 &= MDF_ATTRMASK;
	if (flag1 == flag2 ||
	    (flag1 == MDF_WRITEBACK && flag2 == MDF_UNCACHEABLE) ||
	    (flag2 == MDF_WRITEBACK && flag1 == MDF_UNCACHEABLE))
		return (0);
	return (1);
}

/*
 * Look for an exactly-matching range.
 */
static struct mem_range_desc *
mem_range_match(struct mem_range_softc *sc, struct mem_range_desc *mrd)
{
	struct mem_range_desc *cand;
	int i;

	for (i = 0, cand = sc->mr_desc; i < sc->mr_ndesc; i++, cand++)
		if ((cand->mr_base == mrd->mr_base) &&
		    (cand->mr_len == mrd->mr_len))
			return (cand);
	return (NULL);
}

/*
 * Fetch the current mtrr settings from the current CPU (assumed to
 * all be in sync in the SMP case).  Note that if we are here, we
 * assume that MTRRs are enabled, and we may or may not have fixed
 * MTRRs.
 */
static void
i686_mrfetch(struct mem_range_softc *sc)
{
	struct mem_range_desc *mrd;
	u_int64_t msrv;
	int i, j, msr;

	mrd = sc->mr_desc;

	/* Get fixed-range MTRRs. */
	if (sc->mr_cap & MR686_FIXMTRR) {
		msr = MSR_MTRR64kBase;
		for (i = 0; i < (MTRR_N64K / 8); i++, msr++) {
			msrv = rdmsr(msr);
			for (j = 0; j < 8; j++, mrd++) {
				mrd->mr_flags =
				    (mrd->mr_flags & ~MDF_ATTRMASK) |
				    i686_mtrr2mrt(msrv & 0xff) | MDF_ACTIVE;
				if (mrd->mr_owner[0] == 0)
					strcpy(mrd->mr_owner, mem_owner_bios);
				msrv = msrv >> 8;
			}
		}
		msr = MSR_MTRR16kBase;
		for (i = 0; i < (MTRR_N16K / 8); i++, msr++) {
			msrv = rdmsr(msr);
			for (j = 0; j < 8; j++, mrd++) {
				mrd->mr_flags =
				    (mrd->mr_flags & ~MDF_ATTRMASK) |
				    i686_mtrr2mrt(msrv & 0xff) | MDF_ACTIVE;
				if (mrd->mr_owner[0] == 0)
					strcpy(mrd->mr_owner, mem_owner_bios);
				msrv = msrv >> 8;
			}
		}
		msr = MSR_MTRR4kBase;
		for (i = 0; i < (MTRR_N4K / 8); i++, msr++) {
			msrv = rdmsr(msr);
			for (j = 0; j < 8; j++, mrd++) {
				mrd->mr_flags =
				    (mrd->mr_flags & ~MDF_ATTRMASK) |
				    i686_mtrr2mrt(msrv & 0xff) | MDF_ACTIVE;
				if (mrd->mr_owner[0] == 0)
					strcpy(mrd->mr_owner, mem_owner_bios);
				msrv = msrv >> 8;
			}
		}
	}

	/* Get remainder which must be variable MTRRs. */
	msr = MSR_MTRRVarBase;
	for (; (mrd - sc->mr_desc) < sc->mr_ndesc; msr += 2, mrd++) {
		msrv = rdmsr(msr);
		mrd->mr_flags = (mrd->mr_flags & ~MDF_ATTRMASK) |
		    i686_mtrr2mrt(msrv & MTRR_PHYSBASE_TYPE);
		mrd->mr_base = msrv & mtrr_physmask;
		msrv = rdmsr(msr + 1);
		mrd->mr_flags = (msrv & MTRR_PHYSMASK_VALID) ?
		    (mrd->mr_flags | MDF_ACTIVE) :
		    (mrd->mr_flags & ~MDF_ACTIVE);

		/* Compute the range from the mask. Ick. */
		mrd->mr_len = (~(msrv & mtrr_physmask) &
		    (mtrr_physmask | 0xfffLL)) + 1;
		if (!mrvalid(mrd->mr_base, mrd->mr_len))
			mrd->mr_flags |= MDF_BOGUS;

		/* If unclaimed and active, must be the BIOS. */
		if ((mrd->mr_flags & MDF_ACTIVE) && (mrd->mr_owner[0] == 0))
			strcpy(mrd->mr_owner, mem_owner_bios);
	}
}

/*
 * Return the MTRR memory type matching a region's flags
 */
static int
i686_mtrrtype(int flags)
{
	int i;

	flags &= MDF_ATTRMASK;

	for (i = 0; i < MTRRTOMRTLEN; i++) {
		if (i686_mtrrtomrt[i] == MDF_UNKNOWN)
			continue;
		if (flags == i686_mtrrtomrt[i])
			return (i);
	}
	return (-1);
}

static int
i686_mrt2mtrr(int flags, int oldval)
{
	int val;

	if ((val = i686_mtrrtype(flags)) == -1)
		return (oldval & 0xff);
	return (val & 0xff);
}

/*
 * Update running CPU(s) MTRRs to match the ranges in the descriptor
 * list.
 *
 * XXX Must be called with interrupts enabled.
 */
static void
i686_mrstore(struct mem_range_softc *sc)
{
#ifdef SMP
	/*
	 * We should use ipi_all_but_self() to call other CPUs into a
	 * locking gate, then call a target function to do this work.
	 * The "proper" solution involves a generalised locking gate
	 * implementation, not ready yet.
	 */
	smp_rendezvous(NULL, i686_mrstoreone, NULL, sc);
#else
	disable_intr();				/* disable interrupts */
	i686_mrstoreone(sc);
	enable_intr();
#endif
}

/*
 * Update the current CPU's MTRRs with those represented in the
 * descriptor list.  Note that we do this wholesale rather than just
 * stuffing one entry; this is simpler (but slower, of course).
 */
static void
i686_mrstoreone(void *arg)
{
	struct mem_range_softc *sc = arg;
	struct mem_range_desc *mrd;
	u_int64_t omsrv, msrv;
	int i, j, msr;
	u_int cr4save;

	mrd = sc->mr_desc;

	/* Disable PGE. */
	cr4save = rcr4();
	if (cr4save & CR4_PGE)
		load_cr4(cr4save & ~CR4_PGE);

	/* Disable caches (CD = 1, NW = 0). */
	load_cr0((rcr0() & ~CR0_NW) | CR0_CD);

	/* Flushes caches and TLBs. */
	wbinvd();

	/* Disable MTRRs (E = 0). */
	wrmsr(MSR_MTRRdefType, rdmsr(MSR_MTRRdefType) & ~MTRR_DEF_ENABLE);

	/* Set fixed-range MTRRs. */
	if (sc->mr_cap & MR686_FIXMTRR) {
		msr = MSR_MTRR64kBase;
		for (i = 0; i < (MTRR_N64K / 8); i++, msr++) {
			msrv = 0;
			omsrv = rdmsr(msr);
			for (j = 7; j >= 0; j--) {
				msrv = msrv << 8;
				msrv |= i686_mrt2mtrr((mrd + j)->mr_flags,
				    omsrv >> (j * 8));
			}
			wrmsr(msr, msrv);
			mrd += 8;
		}
		msr = MSR_MTRR16kBase;
		for (i = 0; i < (MTRR_N16K / 8); i++, msr++) {
			msrv = 0;
			omsrv = rdmsr(msr);
			for (j = 7; j >= 0; j--) {
				msrv = msrv << 8;
				msrv |= i686_mrt2mtrr((mrd + j)->mr_flags,
				    omsrv >> (j * 8));
			}
			wrmsr(msr, msrv);
			mrd += 8;
		}
		msr = MSR_MTRR4kBase;
		for (i = 0; i < (MTRR_N4K / 8); i++, msr++) {
			msrv = 0;
			omsrv = rdmsr(msr);
			for (j = 7; j >= 0; j--) {
				msrv = msrv << 8;
				msrv |= i686_mrt2mtrr((mrd + j)->mr_flags,
				    omsrv >> (j * 8));
			}
			wrmsr(msr, msrv);
			mrd += 8;
		}
	}

	/* Set remainder which must be variable MTRRs. */
	msr = MSR_MTRRVarBase;
	for (; (mrd - sc->mr_desc) < sc->mr_ndesc; msr += 2, mrd++) {
		/* base/type register */
		omsrv = rdmsr(msr);
		if (mrd->mr_flags & MDF_ACTIVE) {
			msrv = mrd->mr_base & mtrr_physmask;
			msrv |= i686_mrt2mtrr(mrd->mr_flags, omsrv);
		} else {
			msrv = 0;
		}
		wrmsr(msr, msrv);

		/* mask/active register */
		if (mrd->mr_flags & MDF_ACTIVE) {
			msrv = MTRR_PHYSMASK_VALID |
			    (~(mrd->mr_len - 1) & mtrr_physmask);
		} else {
			msrv = 0;
		}
		wrmsr(msr + 1, msrv);
	}

	/* Flush caches, TLBs. */
	wbinvd();

	/* Enable MTRRs. */
	wrmsr(MSR_MTRRdefType, rdmsr(MSR_MTRRdefType) | MTRR_DEF_ENABLE);

	/* Enable caches (CD = 0, NW = 0). */
	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));

	/* Restore PGE. */
	load_cr4(cr4save);
}

/*
 * Hunt for the fixed MTRR referencing (addr)
 */
static struct mem_range_desc *
i686_mtrrfixsearch(struct mem_range_softc *sc, u_int64_t addr)
{
	struct mem_range_desc *mrd;
	int i;

	for (i = 0, mrd = sc->mr_desc; i < (MTRR_N64K + MTRR_N16K + MTRR_N4K);
	     i++, mrd++)
		if ((addr >= mrd->mr_base) &&
		    (addr < (mrd->mr_base + mrd->mr_len)))
			return (mrd);
	return (NULL);
}

/*
 * Try to satisfy the given range request by manipulating the fixed
 * MTRRs that cover low memory.
 *
 * Note that we try to be generous here; we'll bloat the range out to
 * the next higher/lower boundary to avoid the consumer having to know
 * too much about the mechanisms here.
 *
 * XXX note that this will have to be updated when we start supporting
 * "busy" ranges.
 */
static int
i686_mrsetlow(struct mem_range_softc *sc, struct mem_range_desc *mrd, int *arg)
{
	struct mem_range_desc *first_md, *last_md, *curr_md;

	/* Range check. */
	if (((first_md = i686_mtrrfixsearch(sc, mrd->mr_base)) == NULL) ||
	    ((last_md = i686_mtrrfixsearch(sc, mrd->mr_base + mrd->mr_len - 1)) == NULL))
		return (EINVAL);

	/* Check that we aren't doing something risky. */
	if (!(mrd->mr_flags & MDF_FORCE))
		for (curr_md = first_md; curr_md <= last_md; curr_md++) {
			if ((curr_md->mr_flags & MDF_ATTRMASK) == MDF_UNKNOWN)
				return (EACCES);
		}

	/* Set flags, clear set-by-firmware flag. */
	for (curr_md = first_md; curr_md <= last_md; curr_md++) {
		curr_md->mr_flags = mrcopyflags(curr_md->mr_flags &
		    ~MDF_FIRMWARE, mrd->mr_flags);
		bcopy(mrd->mr_owner, curr_md->mr_owner, sizeof(mrd->mr_owner));
	}

	return (0);
}

/*
 * Modify/add a variable MTRR to satisfy the request.
 *
 * XXX needs to be updated to properly support "busy" ranges.
 */
static int
i686_mrsetvariable(struct mem_range_softc *sc, struct mem_range_desc *mrd,
    int *arg)
{
	struct mem_range_desc *curr_md, *free_md;
	int i;

	/*
	 * Scan the currently active variable descriptors, look for
	 * one we exactly match (straight takeover) and for possible
	 * accidental overlaps.
	 *
	 * Keep track of the first empty variable descriptor in case
	 * we can't perform a takeover.
	 */
	i = (sc->mr_cap & MR686_FIXMTRR) ? MTRR_N64K + MTRR_N16K + MTRR_N4K : 0;
	curr_md = sc->mr_desc + i;
	free_md = NULL;
	for (; i < sc->mr_ndesc; i++, curr_md++) {
		if (curr_md->mr_flags & MDF_ACTIVE) {
			/* Exact match? */
			if ((curr_md->mr_base == mrd->mr_base) &&
			    (curr_md->mr_len == mrd->mr_len)) {

				/* Whoops, owned by someone. */
				if (curr_md->mr_flags & MDF_BUSY)
					return (EBUSY);

				/* Check that we aren't doing something risky */
				if (!(mrd->mr_flags & MDF_FORCE) &&
				    ((curr_md->mr_flags & MDF_ATTRMASK) ==
				    MDF_UNKNOWN))
					return (EACCES);

				/* Ok, just hijack this entry. */
				free_md = curr_md;
				break;
			}

			/* Non-exact overlap? */
			if (mroverlap(curr_md, mrd)) {
				/* Between conflicting region types? */
				if (i686_mtrrconflict(curr_md->mr_flags,
				    mrd->mr_flags))
					return (EINVAL);
			}
		} else if (free_md == NULL) {
			free_md = curr_md;
		}
	}

	/* Got somewhere to put it? */
	if (free_md == NULL)
		return (ENOSPC);

	/* Set up new descriptor. */
	free_md->mr_base = mrd->mr_base;
	free_md->mr_len = mrd->mr_len;
	free_md->mr_flags = mrcopyflags(MDF_ACTIVE, mrd->mr_flags);
	bcopy(mrd->mr_owner, free_md->mr_owner, sizeof(mrd->mr_owner));
	return (0);
}

/*
 * Handle requests to set memory range attributes by manipulating MTRRs.
 */
static int
i686_mrset(struct mem_range_softc *sc, struct mem_range_desc *mrd, int *arg)
{
	struct mem_range_desc *targ;
	int error = 0;

	switch(*arg) {
	case MEMRANGE_SET_UPDATE:
		/*
		 * Make sure that what's being asked for is even
		 * possible at all.
		 */
		if (!mrvalid(mrd->mr_base, mrd->mr_len) ||
		    i686_mtrrtype(mrd->mr_flags) == -1)
			return (EINVAL);

#define	FIXTOP	((MTRR_N64K * 0x10000) + (MTRR_N16K * 0x4000) + (MTRR_N4K * 0x1000))

		/* Are the "low memory" conditions applicable? */
		if ((sc->mr_cap & MR686_FIXMTRR) &&
		    ((mrd->mr_base + mrd->mr_len) <= FIXTOP)) {
			if ((error = i686_mrsetlow(sc, mrd, arg)) != 0)
				return (error);
		} else {
			/* It's time to play with variable MTRRs. */
			if ((error = i686_mrsetvariable(sc, mrd, arg)) != 0)
				return (error);
		}
		break;

	case MEMRANGE_SET_REMOVE:
		if ((targ = mem_range_match(sc, mrd)) == NULL)
			return (ENOENT);
		if (targ->mr_flags & MDF_FIXACTIVE)
			return (EPERM);
		if (targ->mr_flags & MDF_BUSY)
			return (EBUSY);
		targ->mr_flags &= ~MDF_ACTIVE;
		targ->mr_owner[0] = 0;
		break;

	default:
		return (EOPNOTSUPP);
	}

	/* Update the hardware. */
	i686_mrstore(sc);

	/* Refetch to see where we're at. */
	i686_mrfetch(sc);
	return (0);
}

/*
 * Work out how many ranges we support, initialise storage for them,
 * and fetch the initial settings.
 */
static void
i686_mrinit(struct mem_range_softc *sc)
{
	struct mem_range_desc *mrd;
	u_int regs[4];
	int i, nmdesc = 0, pabits;

	mtrrcap = rdmsr(MSR_MTRRcap);
	mtrrdef = rdmsr(MSR_MTRRdefType);

	/* For now, bail out if MTRRs are not enabled. */
	if (!(mtrrdef & MTRR_DEF_ENABLE)) {
		if (bootverbose)
			printf("CPU supports MTRRs but not enabled\n");
		return;
	}
	nmdesc = mtrrcap & MTRR_CAP_VCNT;
	if (bootverbose)
		printf("Pentium Pro MTRR support enabled\n");

	/*
	 * Determine the size of the PhysMask and PhysBase fields in
	 * the variable range MTRRs.  If the extended CPUID 0x80000008
	 * is present, use that to figure out how many physical
	 * address bits the CPU supports.  Otherwise, default to 36
	 * address bits.
	 */
	if (cpu_exthigh >= 0x80000008) {
		do_cpuid(0x80000008, regs);
		pabits = regs[0] & 0xff;
	} else
		pabits = 36;
	mtrr_physmask = ((1ULL << pabits) - 1) & ~0xfffULL;

	/* If fixed MTRRs supported and enabled. */
	if ((mtrrcap & MTRR_CAP_FIXED) && (mtrrdef & MTRR_DEF_FIXED_ENABLE)) {
		sc->mr_cap = MR686_FIXMTRR;
		nmdesc += MTRR_N64K + MTRR_N16K + MTRR_N4K;
	}

	sc->mr_desc = malloc(nmdesc * sizeof(struct mem_range_desc), M_MEMDESC,
	    M_WAITOK | M_ZERO);
	sc->mr_ndesc = nmdesc;

	mrd = sc->mr_desc;

	/* Populate the fixed MTRR entries' base/length. */
	if (sc->mr_cap & MR686_FIXMTRR) {
		for (i = 0; i < MTRR_N64K; i++, mrd++) {
			mrd->mr_base = i * 0x10000;
			mrd->mr_len = 0x10000;
			mrd->mr_flags = MDF_FIXBASE | MDF_FIXLEN |
			    MDF_FIXACTIVE;
		}
		for (i = 0; i < MTRR_N16K; i++, mrd++) {
			mrd->mr_base = i * 0x4000 + 0x80000;
			mrd->mr_len = 0x4000;
			mrd->mr_flags = MDF_FIXBASE | MDF_FIXLEN |
			    MDF_FIXACTIVE;
		}
		for (i = 0; i < MTRR_N4K; i++, mrd++) {
			mrd->mr_base = i * 0x1000 + 0xc0000;
			mrd->mr_len = 0x1000;
			mrd->mr_flags = MDF_FIXBASE | MDF_FIXLEN |
			    MDF_FIXACTIVE;
		}
	}

	/*
	 * Get current settings, anything set now is considered to
	 * have been set by the firmware. (XXX has something already
	 * played here?)
	 */
	i686_mrfetch(sc);
	mrd = sc->mr_desc;
	for (i = 0; i < sc->mr_ndesc; i++, mrd++) {
		if (mrd->mr_flags & MDF_ACTIVE)
			mrd->mr_flags |= MDF_FIRMWARE;
	}
}

/*
 * Initialise MTRRs on an AP after the BSP has run the init code.
 */
static void
i686_mrAPinit(struct mem_range_softc *sc)
{

	i686_mrstoreone(sc);
	wrmsr(MSR_MTRRdefType, mtrrdef);
}

static void
i686_mem_drvinit(void *unused)
{

	if (mtrrs_disabled)
		return;
	if (!(cpu_feature & CPUID_MTRR))
		return;
	if ((cpu_id & 0xf00) != 0x600 && (cpu_id & 0xf00) != 0xf00)
		return;
	if (cpu_vendor_id != CPU_VENDOR_INTEL &&
	    cpu_vendor_id != CPU_VENDOR_AMD)
		return;
	mem_range_softc.mr_op = &i686_mrops;
}
SYSINIT(i686memdev, SI_SUB_DRIVERS, SI_ORDER_FIRST, i686_mem_drvinit, NULL);

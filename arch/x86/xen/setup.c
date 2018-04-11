/*
 * Machine specific setup for xen
 *
 * Jeremy Fitzhardinge <jeremy@xensource.com>, XenSource Inc, 2007
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pm.h>
#include <linux/memblock.h>
#include <linux/cpuidle.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>

#include <asm/elf.h>
#include <asm/vdso.h>
#include <asm/e820/api.h>
#include <asm/setup.h>
#include <asm/acpi.h>
#include <asm/numa.h>
#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/vnuma.h>

#include <xen/xen.h>
#include <xen/page.h>
#include <xen/interface/callback.h>
#include <xen/interface/memory.h>
#include <xen/interface/physdev.h>
#include <xen/features.h>
#include <xen/hvc-console.h>
#include "xen-ops.h"
#include "vdso.h"
#include "mmu.h"

#define GB(x) ((uint64_t)(x) * 1024 * 1024 * 1024)

/* Amount of extra memory space we add to the e820 ranges */
struct xen_memory_region xen_extra_mem[XEN_EXTRA_MEM_MAX_REGIONS] __initdata;

/* Number of pages released from the initial allocation. */
unsigned long xen_released_pages;

unsigned long  max_pfn_to_use; 
extern unsigned long zone_movable_pfn[MAX_NUMNODES];
/* E820 map used during setting up memory. */
static struct e820_table xen_e820_table __initdata;

/*
 * Buffer used to remap identity mapped pages. We only need the virtual space.
 * The physical page behind this address is remapped as needed to different
 * buffer pages.
 */
#define REMAP_SIZE	(P2M_PER_PAGE - 3)
static struct {
	unsigned long	next_area_mfn;
	unsigned long	target_pfn;
	unsigned long	size;
	unsigned long	mfns[REMAP_SIZE];
} xen_remap_buf __initdata __aligned(PAGE_SIZE);
static unsigned long xen_remap_mfn __initdata = INVALID_P2M_ENTRY;

/* 
 * The maximum amount of extra memory compared to the base size.  The
 * main scaling factor is the size of struct page.  At extreme ratios
 * of base:extra, all the base memory can be filled with page
 * structures for the extra memory, leaving no space for anything
 * else.
 * 
 * 10x seems like a reasonable balance between scaling flexibility and
 * leaving a practically usable system.
 */

/* modified from lavoisier, from 10 to 200*/
#define EXTRA_MEM_RATIO		(10)

static bool xen_512gb_limit __initdata = IS_ENABLED(CONFIG_XEN_512GB);

static void __init xen_parse_512gb(void)
{
	bool val = false;
	char *arg;

	arg = strstr(xen_start_info->cmd_line, "xen_512gb_limit");
	if (!arg)
		return;

	arg = strstr(xen_start_info->cmd_line, "xen_512gb_limit=");
	if (!arg)
		val = true;
	else if (strtobool(arg + strlen("xen_512gb_limit="), &val))
		return;

	xen_512gb_limit = val;
}

static void __init xen_add_extra_mem(unsigned long start_pfn,
				     unsigned long n_pfns)
{
	int i;

	/*
	 * No need to check for zero size, should happen rarely and will only
	 * write a new entry regarded to be unused due to zero size.
	 */
        printk("Lavoisier in function %s , value of XEN_EXTRA_MEM_MAX_REGION : %d", __func__, XEN_EXTRA_MEM_MAX_REGIONS);
	for (i = 0; i < XEN_EXTRA_MEM_MAX_REGIONS; i++) {
		/* Add new region. */
		if (xen_extra_mem[i].n_pfns == 0) {
                        printk(" \n index of the xen_extra_mem array where this memory pfn [%lu- %lu ] has been newly added  : %d", start_pfn, start_pfn+n_pfns, i);
			xen_extra_mem[i].start_pfn = start_pfn;
			xen_extra_mem[i].n_pfns = n_pfns;
			break;
		}
		/* Append to existing region. */
		if (xen_extra_mem[i].start_pfn + xen_extra_mem[i].n_pfns ==
		    start_pfn) {
                        printk(" \n index of the xen_extra_mem array where this memory pfn [%lu- %lu ] has been added on the preexisting %lu pfns  : %d", start_pfn, start_pfn+n_pfns, xen_extra_mem[i].n_pfns, i);
			xen_extra_mem[i].n_pfns += n_pfns;
			break;
		}
	}
	if (i == XEN_EXTRA_MEM_MAX_REGIONS)
		printk(KERN_WARNING "Warning: not enough extra memory regions\n");
         // Modified by Lavoisier to test if the freeing of pages at boot  will work on this extra regions, conducting to the possibility to remove the pfn after. Note that the pfn is remvable if it is freed at boottime. 
	memblock_reserve(PFN_PHYS(start_pfn), PFN_PHYS(n_pfns));
}


		



static void __init xen_del_extra_mem(unsigned long start_pfn,
				     unsigned long n_pfns)
{
	int i;
	unsigned long start_r, size_r;

	for (i = 0; i < XEN_EXTRA_MEM_MAX_REGIONS; i++) {
		start_r = xen_extra_mem[i].start_pfn;
		size_r = xen_extra_mem[i].n_pfns;

		/* Start of region. */
		if (start_r == start_pfn) {
			BUG_ON(n_pfns > size_r);
			xen_extra_mem[i].start_pfn += n_pfns;
			xen_extra_mem[i].n_pfns -= n_pfns;
			break;
		}
		/* End of region. */
		if (start_r + size_r == start_pfn + n_pfns) {
			BUG_ON(n_pfns > size_r);
			xen_extra_mem[i].n_pfns -= n_pfns;
			break;
		}
		/* Mid of region. */
		if (start_pfn > start_r && start_pfn < start_r + size_r) {
			BUG_ON(start_pfn + n_pfns > start_r + size_r);
			xen_extra_mem[i].n_pfns = start_pfn - start_r;
			/* Calling memblock_reserve() again is okay. */
			xen_add_extra_mem(start_pfn + n_pfns, start_r + size_r -
					  (start_pfn + n_pfns));
			break;
		}
	}
	memblock_free(PFN_PHYS(start_pfn), PFN_PHYS(n_pfns));
}

/*
 * Called during boot before the p2m list can take entries beyond the
 * hypervisor supplied p2m list. Entries in extra mem are to be regarded as
 * invalid.
 */
unsigned long __ref xen_chk_extra_mem(unsigned long pfn)
{
	int i;

	for (i = 0; i < XEN_EXTRA_MEM_MAX_REGIONS; i++) {
		if (pfn >= xen_extra_mem[i].start_pfn &&
		    pfn < xen_extra_mem[i].start_pfn + xen_extra_mem[i].n_pfns)
			return INVALID_P2M_ENTRY;
	}

	return IDENTITY_FRAME(pfn);
}

/*
 * Mark all pfns of extra mem as invalid in p2m list.
 */
void __init xen_inv_extra_mem(void)
{
	unsigned long pfn, pfn_s, pfn_e;
	int i;

	for (i = 0; i < XEN_EXTRA_MEM_MAX_REGIONS; i++) {
		if (!xen_extra_mem[i].n_pfns)
			continue;
		pfn_s = xen_extra_mem[i].start_pfn;
		pfn_e = pfn_s + xen_extra_mem[i].n_pfns;
		for (pfn = pfn_s; pfn < pfn_e; pfn++)
			set_phys_to_machine(pfn, INVALID_P2M_ENTRY);
	}
}

/*
 * Finds the next RAM pfn available in the E820 map after min_pfn.
 * This function updates min_pfn with the pfn found and returns
 * the size of that range or zero if not found.
 */
static unsigned long __init xen_find_pfn_range(unsigned long *min_pfn)
{
	const struct e820_entry *entry = xen_e820_table.entries;
	unsigned int i;
	unsigned long done = 0;

	for (i = 0; i < xen_e820_table.nr_entries; i++, entry++) {
		unsigned long s_pfn;
		unsigned long e_pfn;

		if (entry->type != E820_TYPE_RAM)
			continue;

		e_pfn = PFN_DOWN(entry->addr + entry->size);

		/* We only care about E820 after this */
		if (e_pfn <= *min_pfn)
			continue;

		s_pfn = PFN_UP(entry->addr);

		/* If min_pfn falls within the E820 entry, we want to start
		 * at the min_pfn PFN.
		 */
		if (s_pfn <= *min_pfn) {
			done = e_pfn - *min_pfn;
		} else {
			done = e_pfn - s_pfn;
			*min_pfn = s_pfn;
		}
		break;
	}

	return done;
}

static int __init xen_free_mfn(unsigned long mfn)
{
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	set_xen_guest_handle(reservation.extent_start, &mfn);
	reservation.nr_extents = 1;

	return HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
}

/*
 * This releases a chunk of memory and then does the identity map. It's used
 * as a fallback if the remapping fails.
 */
static void __init xen_set_identity_and_release_chunk(unsigned long start_pfn,
			unsigned long end_pfn, unsigned long nr_pages)
{
	unsigned long pfn, end;
	int ret;

	WARN_ON(start_pfn > end_pfn);

	/* Release pages first. */
	end = min(end_pfn, nr_pages);
	for (pfn = start_pfn; pfn < end; pfn++) {
		unsigned long mfn = pfn_to_mfn(pfn);

		/* Make sure pfn exists to start with */
		if (mfn == INVALID_P2M_ENTRY || mfn_to_pfn(mfn) != pfn)
			continue;

		ret = xen_free_mfn(mfn);
		WARN(ret != 1, "Failed to release pfn %lx err=%d\n", pfn, ret);

		if (ret == 1) {
			xen_released_pages++;
			if (!__set_phys_to_machine(pfn, INVALID_P2M_ENTRY))
				break;
		} else
			break;
	}

	set_phys_range_identity(start_pfn, end_pfn);
}

/*
 * Helper function to update the p2m and m2p tables and kernel mapping.
 */
static void __init xen_update_mem_tables(unsigned long pfn, unsigned long mfn)
{
	struct mmu_update update = {
		.ptr = ((uint64_t)mfn << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE,
		.val = pfn
	};

	/* Update p2m */
	if (!set_phys_to_machine(pfn, mfn)) {
		WARN(1, "Failed to set p2m mapping for pfn=%ld mfn=%ld\n",
		     pfn, mfn);
		BUG();
	}

	/* Update m2p */
	if (HYPERVISOR_mmu_update(&update, 1, NULL, DOMID_SELF) < 0) {
		WARN(1, "Failed to set m2p mapping for mfn=%ld pfn=%ld\n",
		     mfn, pfn);
		BUG();
	}

	/* Update kernel mapping, but not for highmem. */
	if (pfn >= PFN_UP(__pa(high_memory - 1)))
		return;

	if (HYPERVISOR_update_va_mapping((unsigned long)__va(pfn << PAGE_SHIFT),
					 mfn_pte(mfn, PAGE_KERNEL), 0)) {
		WARN(1, "Failed to update kernel mapping for mfn=%ld pfn=%ld\n",
		      mfn, pfn);
		BUG();
	}
}

/*
 * This function updates the p2m and m2p tables with an identity map from
 * start_pfn to start_pfn+size and prepares remapping the underlying RAM of the
 * original allocation at remap_pfn. The information needed for remapping is
 * saved in the memory itself to avoid the need for allocating buffers. The
 * complete remap information is contained in a list of MFNs each containing
 * up to REMAP_SIZE MFNs and the start target PFN for doing the remap.
 * This enables us to preserve the original mfn sequence while doing the
 * remapping at a time when the memory management is capable of allocating
 * virtual and physical memory in arbitrary amounts, see 'xen_remap_memory' and
 * its callers.
 */
static void __init xen_do_set_identity_and_remap_chunk(
        unsigned long start_pfn, unsigned long size, unsigned long remap_pfn)
{
	unsigned long buf = (unsigned long)&xen_remap_buf;
	unsigned long mfn_save, mfn;
	unsigned long ident_pfn_iter, remap_pfn_iter;
	unsigned long ident_end_pfn = start_pfn + size;
	unsigned long left = size;
	unsigned int i, chunk;

	WARN_ON(size == 0);

	BUG_ON(xen_feature(XENFEAT_auto_translated_physmap));

	mfn_save = virt_to_mfn(buf);

	for (ident_pfn_iter = start_pfn, remap_pfn_iter = remap_pfn;
	     ident_pfn_iter < ident_end_pfn;
	     ident_pfn_iter += REMAP_SIZE, remap_pfn_iter += REMAP_SIZE) {
		chunk = (left < REMAP_SIZE) ? left : REMAP_SIZE;

		/* Map first pfn to xen_remap_buf */
		mfn = pfn_to_mfn(ident_pfn_iter);
		set_pte_mfn(buf, mfn, PAGE_KERNEL);

		/* Save mapping information in page */
		xen_remap_buf.next_area_mfn = xen_remap_mfn;
		xen_remap_buf.target_pfn = remap_pfn_iter;
		xen_remap_buf.size = chunk;
		for (i = 0; i < chunk; i++)
			xen_remap_buf.mfns[i] = pfn_to_mfn(ident_pfn_iter + i);

		/* Put remap buf into list. */
		xen_remap_mfn = mfn;

		/* Set identity map */
		set_phys_range_identity(ident_pfn_iter, ident_pfn_iter + chunk);

		left -= chunk;
	}

	/* Restore old xen_remap_buf mapping */
	set_pte_mfn(buf, mfn_save, PAGE_KERNEL);
}

/*
 * This function takes a contiguous pfn range that needs to be identity mapped
 * and:
 *
 *  1) Finds a new range of pfns to use to remap based on E820 and remap_pfn.
 *  2) Calls the do_ function to actually do the mapping/remapping work.
 *
 * The goal is to not allocate additional memory but to remap the existing
 * pages. In the case of an error the underlying memory is simply released back
 * to Xen and not remapped.
 */
static unsigned long __init xen_set_identity_and_remap_chunk(
	unsigned long start_pfn, unsigned long end_pfn, unsigned long nr_pages,
	unsigned long remap_pfn)
{
	unsigned long pfn;
	unsigned long i = 0;
	unsigned long n = end_pfn - start_pfn;
        printk(" \n Lavoisier wapet in function %s remapping the range [%lu,%lu], max  pages : %lu -",__func__,start_pfn,end_pfn,nr_pages);
	if (remap_pfn == 0)
		remap_pfn = nr_pages;

	while (i < n) {
		unsigned long cur_pfn = start_pfn + i;
		unsigned long left = n - i;
		unsigned long size = left;
		unsigned long remap_range_size;

		/* Do not remap pages beyond the current allocation */
                printk("\n finding the first candidate in e820");
                
		if (cur_pfn >= nr_pages) {
			/* Identity map remaining pages */
                        printk("\n we have cur_pfn = %lu >  nr_pages = %lu  -", cur_pfn, nr_pages);
			set_phys_range_identity(cur_pfn, cur_pfn + size);
			break;
		}
		if (cur_pfn + size > nr_pages)
			size = nr_pages - cur_pfn;

		remap_range_size = xen_find_pfn_range(&remap_pfn);
		if (!remap_range_size) {
			pr_warning("\n Unable to find available pfn range, not remapping identity pages\n");
			xen_set_identity_and_release_chunk(cur_pfn,
						cur_pfn + left, nr_pages);
			break;
		}
		/* Adjust size to fit in current e820 RAM region */
		if (size > remap_range_size)
			size = remap_range_size;
                printk("\n current pfn to remap %lu, size : %lu , rempapped at pfn: %lu", cur_pfn, size, remap_pfn);
		xen_do_set_identity_and_remap_chunk(cur_pfn, size, remap_pfn);

		/* Update variables to reflect new mappings. */
		i += size;
		remap_pfn += size;
	}

	/*
	 * If the PFNs are currently mapped, the VA mapping also needs
	 * to be updated to be 1:1.
	 */
        printk("\n updating virtual address mapping, from start pfn %lu to max pfn mapped %lu ", start_pfn, max_pfn_mapped);
	for (pfn = start_pfn; pfn <= max_pfn_mapped && pfn < end_pfn; pfn++){
	        if(pfn >= nr_pages){
                        //printk("\n the pfn %lu has not been remapped because out of bound, we don't care because we will not use it", pfn );
                        break;
                } 

          	(void)HYPERVISOR_update_va_mapping(
			(unsigned long)__va(pfn << PAGE_SHIFT),
			mfn_pte(pfn, PAGE_KERNEL_IO), 0);

        }
        
	return remap_pfn;
}

static unsigned long __init xen_count_remap_pages(
	unsigned long start_pfn, unsigned long end_pfn, unsigned long nr_pages,
	unsigned long remap_pages)
{
	if (start_pfn >= nr_pages)
		return remap_pages;

	return remap_pages + min(end_pfn, nr_pages) - start_pfn;
}

static unsigned long __init xen_foreach_remap_area(unsigned long nr_pages,
	unsigned long (*func)(unsigned long start_pfn, unsigned long end_pfn,
			      unsigned long nr_pages, unsigned long last_val))
{
	phys_addr_t start = 0;
	unsigned long ret_val = 0;
	const struct e820_entry *entry = xen_e820_table.entries;
	int i;

	/*
	 * Combine non-RAM regions and gaps until a RAM region (or the
	 * end of the map) is reached, then call the provided function
	 * to perform its duty on the non-RAM region.
	 *
	 * The combined non-RAM regions are rounded to a whole number
	 * of pages so any partial pages are accessible via the 1:1
	 * mapping.  This is needed for some BIOSes that put (for
	 * example) the DMI tables in a reserved region that begins on
	 * a non-page boundary.
	 */
	for (i = 0; i < xen_e820_table.nr_entries; i++, entry++) {
		phys_addr_t end = entry->addr + entry->size;
                printk("\n xen_foreach_remap_area  entry index :%d , type: %d ,  [%llu - %llu] ", i , entry->type , entry->addr , end);

		if (entry->type == E820_TYPE_RAM || i == xen_e820_table.nr_entries - 1) {
			unsigned long start_pfn = PFN_DOWN(start);
			unsigned long end_pfn = PFN_UP(end);
                        printk(" \n  corresponding pfns [%lu-%lu]  ", start_pfn, end_pfn);
			if (entry->type == E820_TYPE_RAM)
				end_pfn = PFN_UP(entry->addr);

                        printk(" \n  range to consider because of type ram pfns [%lu-%lu]  ", start_pfn, end_pfn);
			if (start_pfn < end_pfn){
				ret_val = func(start_pfn, end_pfn, nr_pages,
					       ret_val);
                                printk(" \n last call of the param function on range (pfns) [%lu-%lu] in %s ", start_pfn, end_pfn, __func__);
                        }
			start = end;
		}
	}
        printk("\n End of the function call %s on range end [ %llu -] , total released pages %lu, total remapped pages: %lu",__func__, start,xen_released_pages, ret_val);   
	return ret_val;
}

/*
 * Remap the memory prepared in xen_do_set_identity_and_remap_chunk().
 * The remap information (which mfn remap to which pfn) is contained in the
 * to be remapped memory itself in a linked list anchored at xen_remap_mfn.
 * This scheme allows to remap the different chunks in arbitrary order while
 * the resulting mapping will be independant from the order.
 */
void __init xen_remap_memory(void)
{
	unsigned long buf = (unsigned long)&xen_remap_buf; // xen_remap_buf contains the list of mfns to remap
	unsigned long mfn_save, pfn;
	unsigned long remapped = 0;
	unsigned int i;
	unsigned long pfn_s = ~0UL;
	unsigned long len = 0;

	mfn_save = virt_to_mfn(buf); // to save the first mfn remapped by this function 

	while (xen_remap_mfn != INVALID_P2M_ENTRY) {
		/* Map the remap information */
		set_pte_mfn(buf, xen_remap_mfn, PAGE_KERNEL);   // xen_remap_mfn contains the next mfn to remap ; buf point to it now 

		BUG_ON(xen_remap_mfn != xen_remap_buf.mfns[0]);

		pfn = xen_remap_buf.target_pfn;    // target_pfn is the previously reserved pfn in the extra memory for remapping
		for (i = 0; i < xen_remap_buf.size; i++) { 
			xen_update_mem_tables(pfn, xen_remap_buf.mfns[i]);
			remapped++;
			pfn++;
		}
		if (pfn_s == ~0UL || pfn == pfn_s) {   // at the beginning of remapping or ... maybe the same range?
			pfn_s = xen_remap_buf.target_pfn;
			len += xen_remap_buf.size;
		} else if (pfn_s + len == xen_remap_buf.target_pfn) {  // contigous range with the previous target pfns
			len += xen_remap_buf.size;
		} else {       // another target pfn range
			xen_del_extra_mem(pfn_s, len);   // the previous range is freed in the virtual machine ? so that allocation can be done on it. 
			pfn_s = xen_remap_buf.target_pfn; // save the current range to maybe free it later in the next round of the loop. 
			len = xen_remap_buf.size;
		}
		xen_remap_mfn = xen_remap_buf.next_area_mfn;
	}

	if (pfn_s != ~0UL && len)
		xen_del_extra_mem(pfn_s, len);

	set_pte_mfn(buf, mfn_save, PAGE_KERNEL);

	pr_info("Remapped %ld page(s)\n", remapped);
}

static unsigned long __init xen_get_pages_limit(void)
{
	unsigned long limit;

#ifdef CONFIG_X86_32
	limit = GB(64) / PAGE_SIZE;
        printk("\n config x86 32");
#else
	limit = MAXMEM / PAGE_SIZE;
	if (!xen_initial_domain() && xen_512gb_limit){
		limit = GB(512) / PAGE_SIZE;
		printk ("\n it is not the initial domain,limit = GB(512)/PAGE_SIZE = %llu / %lu= %lu ", GB(512), PAGE_SIZE, limit);
	}
#endif
	return limit;
}

static unsigned long __init xen_get_max_pages(void)
{
	unsigned long max_pages, limit;
	domid_t domid = DOMID_SELF;
	long ret;

	limit = xen_get_pages_limit();
	max_pages = limit;
        printk("\nLavoisier wapet in function %s, using the XENMEM_maximum_reservation hypercall, ",__func__);
        
	/*
	 * For the initial domain we use the maximum reservation as
	 * the maximum page.
	 *
	 * For guest domains the current maximum reservation reflects
	 * the current maximum rather than the static maximum. In this
	 * case the e820 map provided to us will cover the static
	 * maximum region.
	 */
	if (xen_initial_domain()) {
		ret = HYPERVISOR_memory_op(XENMEM_maximum_reservation, &domid);
		if (ret > 0)
			max_pages = ret;
	}
        //printk("return of the hypercall: %ld  \n limit : %ld (the final max_pages is the min)",ret,limit);
	return min(max_pages, limit);
}

static void __init xen_align_and_add_e820_region(phys_addr_t start,
						 phys_addr_t size, int type)
{
	phys_addr_t end = start + size;

	/* Align RAM regions to page boundaries. */
	if (type == E820_TYPE_RAM) {
		start = PAGE_ALIGN(start);
		end &= ~((phys_addr_t)PAGE_SIZE - 1);
	}

	e820__range_add(start, end - start, type);
}

static void __init xen_ignore_unusable(void)
{
	struct e820_entry *entry = xen_e820_table.entries;
	unsigned int i;

	for (i = 0; i < xen_e820_table.nr_entries; i++, entry++) {
		if (entry->type == E820_TYPE_UNUSABLE){
			entry->type = E820_TYPE_RAM;
                        printk("\n %s : modification of the  receive range %i, his type is changed from E820_TYPE_UNUSABLE to E820_TYPE_RAM",__func__,i);
                }
	}
}
/* return false if there is a e820 range that can be removed and assigned to the memory */
bool __init xen_is_e820_reserved(phys_addr_t start, phys_addr_t size)
{
	struct e820_entry *entry;
	unsigned mapcnt;
	phys_addr_t end;

	if (!size)
		return false;

	end = start + size;
	entry = xen_e820_table.entries;

	for (mapcnt = 0; mapcnt < xen_e820_table.nr_entries; mapcnt++) {
		if (entry->type == E820_TYPE_RAM && entry->addr <= start &&
		    (entry->addr + entry->size) >= end)
			return false;

		entry++;
	}

	return true;
}

/*
 * Find a free area in physical memory not yet reserved and compliant with
 * E820 map.
 * Used to relocate pre-allocated areas like initrd or p2m list which are in
 * conflict with the to be used E820 map.
 * In case no area is found, return 0. Otherwise return the physical address
 * of the area which is already reserved for convenience.
 */
phys_addr_t __init xen_find_free_area(phys_addr_t size)
{
	unsigned mapcnt;
	phys_addr_t addr, start;
	struct e820_entry *entry = xen_e820_table.entries;

	for (mapcnt = 0; mapcnt < xen_e820_table.nr_entries; mapcnt++, entry++) {
		if (entry->type != E820_TYPE_RAM || entry->size < size)
			continue;
		start = entry->addr;
		for (addr = start; addr < start + size; addr += PAGE_SIZE) {
			if (!memblock_is_reserved(addr))
				continue;
			start = addr + PAGE_SIZE;
			if (start + size > entry->addr + entry->size)
				break;
		}
		if (addr >= start + size) {
			memblock_reserve(start, size);
			return start;
		}
	}

	return 0;
}

/*
 * Like memcpy, but with physical addresses for dest and src.
 */
static void __init xen_phys_memcpy(phys_addr_t dest, phys_addr_t src,
				   phys_addr_t n)
{
	phys_addr_t dest_off, src_off, dest_len, src_len, len;
	void *from, *to;

	while (n) {
		dest_off = dest & ~PAGE_MASK;
		src_off = src & ~PAGE_MASK;
		dest_len = n;
		if (dest_len > (NR_FIX_BTMAPS << PAGE_SHIFT) - dest_off)
			dest_len = (NR_FIX_BTMAPS << PAGE_SHIFT) - dest_off;
		src_len = n;
		if (src_len > (NR_FIX_BTMAPS << PAGE_SHIFT) - src_off)
			src_len = (NR_FIX_BTMAPS << PAGE_SHIFT) - src_off;
		len = min(dest_len, src_len);
		to = early_memremap(dest - dest_off, dest_len + dest_off);
		from = early_memremap(src - src_off, src_len + src_off);
		memcpy(to, from, len);
		early_memunmap(to, dest_len + dest_off);
		early_memunmap(from, src_len + src_off);
		n -= len;
		dest += len;
		src += len;
	}
}

/*
 * Reserve Xen mfn_list.
 */
static void __init xen_reserve_xen_mfnlist(void)
{
	phys_addr_t start, size;

	if (xen_start_info->mfn_list >= __START_KERNEL_map) {
		start = __pa(xen_start_info->mfn_list);
		size = PFN_ALIGN(xen_start_info->nr_pages *
				 sizeof(unsigned long));
	} else {
		start = PFN_PHYS(xen_start_info->first_p2m_pfn);
		size = PFN_PHYS(xen_start_info->nr_p2m_frames);
	}

	memblock_reserve(start, size);
	if (!xen_is_e820_reserved(start, size))
		return;

#ifdef CONFIG_X86_32
	/*
	 * Relocating the p2m on 32 bit system to an arbitrary virtual address
	 * is not supported, so just give up.
	 */
	xen_raw_console_write("Xen hypervisor allocated p2m list conflicts with E820 map\n");
	BUG();
#else
	xen_relocate_p2m();
	memblock_free(start, size);
#endif
}




/* Lavoisier wapet : called to remove the pfn not supposed to be use by the virtual machine immediately after the startup*/
void xen_dynamic_remove_pfn(void){
       	pg_data_t *pgdat;
        struct zone *zone ;
        unsigned long temporal_start, rest;
        int i, num_online_nodes = num_online_nodes();
        printk(" \n In function remove pfn ");
        
        /*
       	for_each_online_pgdat(pgdat) {
       		struct zone *zone = pgdat->node_zones + ZONE_NORMAL;
       		printk("\n zone normal number : %d \n  ", ZONE_NORMAL); 
                printk(" removing the pfn %ld",  zone->zone_start_pfn);
       		offline_pages(zone->zone_start_pfn, 30);     
                printk("\n page offlined");
       	} */
       printk("number of nodes onlines %d ", num_online_nodes);
       for (i =  0 ; i <  num_online_nodes; i++) {
                pgdat = NODE_DATA(i);
                zone = pgdat->node_zones + ZONE_MOVABLE;
                

                printk("\n current node : %d", i);
                       
                printk("\n zone movable number : %d \n  ", ZONE_MOVABLE);
                printk(" zone movable start pfn %ld",  zone->zone_start_pfn);
   
                temporal_start = ((zone->zone_start_pfn + 1000  )>> pageblock_order) << pageblock_order;
                rest  = zone->zone_start_pfn +1000 - temporal_start;
                 
                printk(" removing the pfns  %ld  - %ld ",  temporal_start , temporal_start+pageblock_nr_pages);
                
                offline_pages(temporal_start, pageblock_nr_pages);     
                 
                zone = pgdat->node_zones + ZONE_NORMAL;

                printk("\n zone normal number : %d \n  ", ZONE_NORMAL); 
                printk(" zone normal start pfn %ld",  zone->zone_start_pfn);
                

               
        }
        
}




/* Lavoisier wapet to get log everywhere */
void send_printklog_tovmm(void){



       struct xen_debug_startup log;
        unsigned char *byte_array;

   /*start log*/
        byte_array=(unsigned char*)(log_buf_addr_get());
        log.nr_chars=log_buf_len_get();
        set_xen_guest_handle(log.buffer,byte_array);
        HYPERVISOR_memory_op(XENMEM_debug_startup,&log);
        /* end log*/

}
unsigned long  get_max_pfn_to_use(void){
               
      return max_pfn_to_use;

}

/**
 * machine_specific_memory_setup - Hook for machine specific memory setup.
 **/
char * __init xen_memory_setup(void)
{
	unsigned long max_pfn, pfn_s, n_pfns;
	phys_addr_t mem_end, addr, size, chunk_size;
	u32 type;
	int rc;
	struct xen_memory_map memmap;
        /* start log*/
      //  struct xen_debug_startup log;
      //  unsigned char *byte_array;
        /* end log*/
	unsigned long max_pages;
	unsigned long extra_pages = 0;
        
        //unsigned char tmp_byte_array[99]="test pourcent c";
	int i;
	int op;
        /*added when recopy*/
         phys_addr_t start_recopy = 0;
        unsigned long ret_val_recopy = 0;
        struct e820_entry *entry_recopy ;
        int i_recopy;


        /*added when recopy*/

        printk(" \n Lavoisier wapet in function %s", __func__);
	xen_parse_512gb();
	max_pfn = xen_get_pages_limit();
        printk(" \n ** informations from xen_start_info, nr_pages: %lu", xen_start_info->nr_pages);
        printk(" , max_pfn from xen_get_pages_limit : %lu",max_pfn);

	max_pfn = min(max_pfn, xen_start_info->nr_pages); 
        printk(" \n ** final max_pfn : %lu",max_pfn);
	mem_end = PFN_PHYS(max_pfn);
        printk(" \n corresponding memory address end mem_end: %llu",mem_end);

	memmap.nr_entries = ARRAY_SIZE(xen_e820_table.entries);
	set_xen_guest_handle(memmap.buffer, xen_e820_table.entries);

	op = xen_initial_domain() ?
		XENMEM_machine_memory_map :
		XENMEM_memory_map;
        /* modified by Lavoisier wapet (No longer useful, we use the default option) */
        /* op = xen_initial_domain() ?
                XENMEM_machine_memory_map :
                XENMEM_memory_phys_map;  */  //        XENMEM_memory_phys_map;

         i=0;
        
        

        printk("\n Hypercall call to obtain memmap, op %d (XENMEM_memory_map, to verify)",op);
        printk("\n before the hypercall : nr_entries of the map : %d",memmap.nr_entries);
         
	rc = HYPERVISOR_memory_op(op, &memmap);
        
	if (rc == -ENOSYS) {
                printk (" \n *** OH! There is a bug on the hypercall, the result where -ENOSYS");
		BUG_ON(xen_initial_domain());
                
		memmap.nr_entries = 1;
		xen_e820_table.entries[0].addr = 0ULL;
		xen_e820_table.entries[0].size = mem_end;
		/* 8MB slack (to balance backend allocations). */
		xen_e820_table.entries[0].size += 8ULL << 20;
		xen_e820_table.entries[0].type = E820_TYPE_RAM;
		rc = 0;
	}
	BUG_ON(rc);
	BUG_ON(memmap.nr_entries == 0);
        printk("\n after the hypercall : nr_entries of the map : %d",memmap.nr_entries);

	xen_e820_table.nr_entries = memmap.nr_entries;

	/*
	 * Xen won't allow a 1:1 mapping to be created to UNUSABLE
	 * regions, so if we're using the machine memory map leave the
	 * region as RAM as it is in the pseudo-physical map.
	 *
	 * UNUSABLE regions in domUs are not handled and will need
	 * a patch in the future.
	 */
	if (xen_initial_domain())
		xen_ignore_unusable();

	/* Make sure the Xen-supplied memory map is well-ordered. */
	e820__update_table(&xen_e820_table);

	max_pages = xen_get_max_pages();

	/* How many extra pages do we need due to remapping? */
        printk (" \n ****** max_pages before the remap count: %lu \n ****** max_pfn : %lu ", max_pages, max_pfn); 
	max_pages += xen_foreach_remap_area(max_pfn, xen_count_remap_pages);
        printk("\n******** number of used pages (+ all remapped areas)  max_pages: %lu ", max_pages);
        printk("\n*********** intial extra pages %lu",extra_pages);
	if (max_pages > max_pfn)
		extra_pages += max_pages - max_pfn;
          
        printk("***********  extra pages after adding difference %lu", extra_pages);
	/*
	 * Clamp the amount of extra memory to a EXTRA_MEM_RATIO
	 * factor the base size.  On non-highmem systems, the base
	 * size is the full initial memory allocation; on highmem it
	 * is limited to the max size of lowmem, so that it doesn't
	 * get completely filled.
	 *
	 * Make sure we have no memory above max_pages, as this area
	 * isn't handled by the p2m management.
	 *
	 * In principle there could be a problem in lowmem systems if
	 * the initial memory is also very large with respect to
	 * lowmem, but we won't try to deal with that here.
	 */
        
	extra_pages = min3(EXTRA_MEM_RATIO * min(max_pfn, PFN_DOWN(MAXMEM)),
			   extra_pages, max_pages - max_pfn);
        printk(" EXTRA_MEM_RATIO : %d , MAXMEM : %lu, PFN_DOWN(MAXMEM): %lu , extra_pages : %lu, max_pages-max_pfn : %lu", EXTRA_MEM_RATIO, MAXMEM, PFN_DOWN(MAXMEM), extra_pages, max_pages - max_pfn );
        printk("\n  final extra_pages: %lu \n **** processing received entries", extra_pages);
	i = 0;
	addr = xen_e820_table.entries[0].addr;
	size = xen_e820_table.entries[0].size;
	while (i < xen_e820_table.nr_entries) {
 
		bool discard = false;

		chunk_size = size;
		type = xen_e820_table.entries[i].type;
                printk("\n current entry index  :%d , type: %d ,  [%llu - %llu] ", i , type , xen_e820_table.entries[i].addr, xen_e820_table.entries[i].addr+xen_e820_table.entries[0].size);     
                printk(" \n current range to proccess : [%llu - %llu]", addr, addr+ size);        
		if (type == E820_TYPE_RAM) {
			if (addr < mem_end) { 
                                
				chunk_size = min(size, mem_end - addr);
                                printk(" \n  added chunck size: %llu", chunk_size );
			} else if (extra_pages) {
                               
				chunk_size = min(size, PFN_PHYS(extra_pages));
				pfn_s = PFN_UP(addr);
				n_pfns = PFN_DOWN(addr + chunk_size) - pfn_s;
                                printk("the authorized memory is %llu corresponding to pfns %lu, and lower than %llu",PFN_PHYS(extra_pages),extra_pages, size);
				extra_pages -= n_pfns;
				xen_add_extra_mem(pfn_s, n_pfns);
				xen_max_p2m_pfn = pfn_s + n_pfns;
                                printk(" \n the range added is [%llu-%llu] corresponding to pfn range [%lu-%lu]", addr, addr+chunk_size, pfn_s, (long unsigned int)(pfn_s + n_pfns)  );
                                printk("\n the current machine has extra pages  and the max p2m pfn of the virtual machine : %lu", xen_max_p2m_pfn);
                                max_pfn_to_use = xen_max_p2m_pfn;
			} else {

                                chunk_size = size;
                                pfn_s = PFN_UP(addr);
                                n_pfns = PFN_DOWN(addr + chunk_size) - pfn_s;
                                xen_max_p2m_pfn = pfn_s + n_pfns;
                                printk(" \n The last range to add  is [%llu-%llu] corresponding to pfn range [%lu-%lu], IT WILL BE DISCARDED", addr, addr+chunk_size, pfn_s, (long unsigned int)(pfn_s + n_pfns)  );
                                
				discard = true;

                        }
		}

		if (!discard){
                        printk("\n adding the range %llu - %llu to the e820, type : %d" , addr,addr+chunk_size, type);
			xen_align_and_add_e820_region(addr, chunk_size, type);
                }

		addr += chunk_size;
		size -= chunk_size;
		if (size == 0) {
			i++;
			if (i < xen_e820_table.nr_entries) {
				addr = xen_e820_table.entries[i].addr;
				size = xen_e820_table.entries[i].size;
			}
		}
	}

	/*
	 * Set the rest as identity mapped, in case PCI BARs are
	 * located here.
	 */
	set_phys_range_identity(addr / PAGE_SIZE, ~0ul);

	/*
	 * In domU, the ISA region is normal, usable memory, but we
	 * reserve ISA memory anyway because too many things poke
	 * about in there.
	 */
	e820__range_add(ISA_START_ADDRESS, ISA_END_ADDRESS - ISA_START_ADDRESS, E820_TYPE_RESERVED);

	e820__update_table(e820_table);

	/*
	 * Check whether the kernel itself conflicts with the target E820 map.
	 * Failing now is better than running into weird problems later due
	 * to relocating (and even reusing) pages with kernel text or data.
	 */
	if (xen_is_e820_reserved(__pa_symbol(_text),
			__pa_symbol(__bss_stop) - __pa_symbol(_text))) {
		xen_raw_console_write("Xen hypervisor allocated kernel memory conflicts with E820 map\n");
		BUG();
	}

	/*
	 * Check for a conflict of the hypervisor supplied page tables with
	 * the target E820 map.
	 */
	xen_pt_check_e820();

	xen_reserve_xen_mfnlist();


	/* Check for a conflict of the initrd with the target E820 map. (if the memory is not present in the e820, it will not be good) */
	if (xen_is_e820_reserved(boot_params.hdr.ramdisk_image,
				 boot_params.hdr.ramdisk_size)) {
		phys_addr_t new_area, start, size;

		new_area = xen_find_free_area(boot_params.hdr.ramdisk_size);
		if (!new_area) {
			xen_raw_console_write("Can't find new memory area for initrd needed due to E820 map conflict\n");
			BUG();
		}

		start = boot_params.hdr.ramdisk_image;
		size = boot_params.hdr.ramdisk_size;
		xen_phys_memcpy(new_area, start, size);
		pr_info("initrd moved from [mem %#010llx-%#010llx] to [mem %#010llx-%#010llx]\n",
			start, start + size, new_area, new_area + size);
		memblock_free(start, size);
		boot_params.hdr.ramdisk_image = new_area;
		boot_params.ext_ramdisk_image = new_area >> 32;
	}

	/*
	 * Set identity map on non-RAM pages and prepare remapping the
	 * underlying RAM.
	 */
        printk("\n preparation of remapped area ");
       



        // xen_foreach_remap_area(max_pfn, xen_set_identity_and_remap_chunk);
        //static unsigned long __init xen_foreach_remap_area(unsigned long nr_pages,)

        /*phys_addr_t start_recopy = 0;
        unsigned long ret_val_recopy = 0;
        struct e820_entry *entry_recopy ;
        int i_recopy;*/
        entry_recopy = xen_e820_table.entries;
        for (i_recopy = 0; i_recopy < xen_e820_table.nr_entries; i_recopy++, entry_recopy++) {
                phys_addr_t end_recopy = entry_recopy->addr + entry_recopy->size;
                 printk("\n xen_foreach_remap_area  entry index :%d , type: %d ,  [%llu - %llu] ", i , entry_recopy->type , entry_recopy->addr , end_recopy);
                if (entry_recopy->type == E820_TYPE_RAM || i_recopy == xen_e820_table.nr_entries - 1) {
                        unsigned long start_pfn_recopy = PFN_DOWN(start_recopy);
                        unsigned long end_pfn_recopy = PFN_UP(end_recopy);
                        printk(" \n  corresponding pfns [%lu-%lu]  ", start_pfn_recopy, end_pfn_recopy);


                        if (entry_recopy->type == E820_TYPE_RAM)
                                end_pfn_recopy = PFN_UP(entry_recopy->addr);

                        printk(" \n  range to consider because of type ram pfns [%lu-%lu]  ", start_pfn_recopy, end_pfn_recopy);
                        if (start_pfn_recopy < end_pfn_recopy){
                                ret_val_recopy = xen_set_identity_and_remap_chunk(start_pfn_recopy, end_pfn_recopy, max_pfn,
                                               ret_val_recopy);
                                printk(" \nrecopy  last call of the param function on range [%lu-%lu] in %s ", start_pfn_recopy, end_pfn_recopy, __func__);
                        }
                        start_recopy = end_recopy;
                }
        }
        printk("\nrecopy  End of the function call %s on range end [ %llu -] , total released pages %lu",__func__, start_recopy,xen_released_pages); 
        printk(" \n End of the function xen_memory_setup");
        //msleep(1);
        printk("\n After the sleep 1 ");

        printk(" \n after the sleep 2");
       // send_printklog_tovmm();

        pr_info("Released %ld page(s)\n", xen_released_pages);
        

	return "Xen";
}

/*
 * Machine specific memory setup for auto-translated guests.
 */
char * __init xen_auto_xlated_memory_setup(void)
{
	struct xen_memory_map memmap;
	int i;
	int rc;

	memmap.nr_entries = ARRAY_SIZE(xen_e820_table.entries);
	set_xen_guest_handle(memmap.buffer, xen_e820_table.entries);

	rc = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if (rc < 0)
		panic("No memory map (%d)\n", rc);

	xen_e820_table.nr_entries = memmap.nr_entries;

	e820__update_table(&xen_e820_table);

	for (i = 0; i < xen_e820_table.nr_entries; i++)
		e820__range_add(xen_e820_table.entries[i].addr, xen_e820_table.entries[i].size, xen_e820_table.entries[i].type);

	/* Remove p2m info, it is not needed. */
	xen_start_info->mfn_list = 0;
	xen_start_info->first_p2m_pfn = 0;
	xen_start_info->nr_p2m_frames = 0;

	return "Xen";
}

/*
 * Set the bit indicating "nosegneg" library variants should be used.
 * We only need to bother in pure 32-bit mode; compat 32-bit processes
 * can have un-truncated segments, so wrapping around is allowed.
 */
static void __init fiddle_vdso(void)
{
#ifdef CONFIG_X86_32
	u32 *mask = vdso_image_32.data +
		vdso_image_32.sym_VDSO32_NOTE_MASK;
	*mask |= 1 << VDSO_NOTE_NONEGSEG_BIT;
#endif
}

static int register_callback(unsigned type, const void *func)
{
	struct callback_register callback = {
		.type = type,
		.address = XEN_CALLBACK(__KERNEL_CS, func),
		.flags = CALLBACKF_mask_events,
	};

	return HYPERVISOR_callback_op(CALLBACKOP_register, &callback);
}

void xen_enable_sysenter(void)
{
	int ret;
	unsigned sysenter_feature;

#ifdef CONFIG_X86_32
	sysenter_feature = X86_FEATURE_SEP;
#else
	sysenter_feature = X86_FEATURE_SYSENTER32;
#endif

	if (!boot_cpu_has(sysenter_feature))
		return;

	ret = register_callback(CALLBACKTYPE_sysenter, xen_sysenter_target);
	if(ret != 0)
		setup_clear_cpu_cap(sysenter_feature);
}

void xen_enable_syscall(void)
{
#ifdef CONFIG_X86_64
	int ret;

	ret = register_callback(CALLBACKTYPE_syscall, xen_syscall_target);
	if (ret != 0) {
		printk(KERN_ERR "Failed to set syscall callback: %d\n", ret);
		/* Pretty fatal; 64-bit userspace has no other
		   mechanism for syscalls. */
	}

	if (boot_cpu_has(X86_FEATURE_SYSCALL32)) {
		ret = register_callback(CALLBACKTYPE_syscall32,
					xen_syscall32_target);
		if (ret != 0)
			setup_clear_cpu_cap(X86_FEATURE_SYSCALL32);
	}
#endif /* CONFIG_X86_64 */
}

void __init xen_pvmmu_arch_setup(void)
{
	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments);
	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_writable_pagetables);

	HYPERVISOR_vm_assist(VMASST_CMD_enable,
			     VMASST_TYPE_pae_extended_cr3);

	if (register_callback(CALLBACKTYPE_event, xen_hypervisor_callback) ||
	    register_callback(CALLBACKTYPE_failsafe, xen_failsafe_callback))
		BUG();

	xen_enable_sysenter();
	xen_enable_syscall();
}

/* This function is not called for HVM domains */
void __init xen_arch_setup(void)
{
	xen_panic_handler_init();
	if (!xen_feature(XENFEAT_auto_translated_physmap))
		xen_pvmmu_arch_setup();

#ifdef CONFIG_ACPI
	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
		printk(KERN_INFO "ACPI in unprivileged domain disabled\n");
		disable_acpi();
	}
#endif

	memcpy(boot_command_line, xen_start_info->cmd_line,
	       MAX_GUEST_CMDLINE > COMMAND_LINE_SIZE ?
	       COMMAND_LINE_SIZE : MAX_GUEST_CMDLINE);

	/* Set up idle, making sure it calls safe_halt() pvop */
	disable_cpuidle();
	disable_cpufreq();
	WARN_ON(xen_set_default_idle());
	fiddle_vdso();
#ifdef CONFIG_NUMA
	if (xen_initial_domain())
		numa_off = 1;
	else
		numa_off = 0;
#endif
}

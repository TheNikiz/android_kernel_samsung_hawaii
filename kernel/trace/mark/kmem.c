/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace kmem
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */

#include <linux/mm.h>
#include <linux/swap.h>
#include <trace/mark.h>
#include <trace/mark_base.h>

#define K(x) ((x) << (PAGE_SHIFT - 10))

#ifdef CONFIG_TRACE_MARK_KMEM_STAT

#define TRACE_MARK_AVAIL         "avail"
#define TRACE_MARK_CACHED        "cached"
#if 0 // not supported for lock issue
#define TRACE_MARK_SWAP          "swap"
#endif
#define TRACE_MARK_ACTIVE_ANON   "act_anon"
#define TRACE_MARK_INACTIVE_ANON "inact_anon"
#define TRACE_MARK_ACTIVE_FILE   "act_file"
#define TRACE_MARK_INACTIVE_FILE "inact_file"

static struct trace_kmem_mark_entry {
	const char* str;
	int enable;
} trace_kmem_mark_pages[NR_VM_ZONE_STAT_ITEMS + 1] = {
	{ "free_pages", 1 },        /* NR_FREE_PAGES */
// 	{ "lru_base", 0},           /* NR_LRU_BASE */
	{ "inact_anon", 1 },        /* NR_INACTIVE_ANON */
	{ "act_anon", 1 },          /* NR_ACTIVE_ANON */
	{ "inact_file", 0 },        /* NR_INACTIVE_FILE */
	{ "act_file", 0 },          /* NR_ACTIVE_FILE */
	{ "unevict", 0 },           /* NR_UNEVICTABLE */
	{ "mlock", 0 },             /* NR_MLOCK */
	{ "anon", 0 },              /* NR_ANON_PAGES */
	{ "file_mapped", 0 },       /* NR_FILE_MAPPED */
	{ "file_pages", 1 },        /* NR_FILE_PAGES */
	{ "file_dirty", 1 },        /* NR_FILE_DIRTY */
	{ "writeback", 1 },         /* NR_WRITEBACK */
	{ "slab_recl", 0 },         /* NR_SLAB_RECLAIMABLE */
	{ "slab_unrecl", 0 },       /* NR_SLAB_UNRECLAIMABLE */
	{ "pagetable", 0 },         /* NR_PAGETABLE */
	{ "kernel statk", 0 },      /* NR_KERNEL_STACK */
	{ "unstable_nfs", 0 },      /* NR_UNSTABLE_NFS */
	{ "bounce", 0 },            /* NR_BOUNCE */
	{ "vmscan_write", 0 },      /* NR_VMSCAN_WRITE */
	{ "vmscan_imm", 0 },        /* NR_VMSCAN_IMMEDIATE */
	{ "wb temp", 0 },           /* NR_WRITEBACK_TEMP */
	{ "isol_anon", 0 },         /* NR_ISOLATED_ANON */
	{ "isol_file", 0 },         /* NR_ISOLATED_FILE */
	{ "shmem", 0 },             /* NR_SHMEM */
	{ "dirtied", 0 },           /* NR_DIRTIED */
	{ "written", 0 },           /* NR_WRITTEN */
#ifdef CONFIG_NUMA
	{ "hit", 0 },               /* NUMA_HIT */
	{ "miss", 0 },              /* NUMA_MISS */
	{ "foreign", 0 },           /* NUMA_FOREIGN */
	{ "inter_hit", 0 },         /* NUMA_INTERLEAVE_HIT */
	{ "local", 0 },             /* NUMA_LOCAL */
	{ "other", 0 },             /* NUMA_OTHER */
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	{ "anon_tr_huge", 0 },      /* NR_ANON_TRANSPARENT_HUGEPAGES */
#endif
#ifdef CONFIG_CMA
	{ "free_cma", 0 },          /* NR_FREE_CMA_PAGES */
	{ "cma_base", 0},           /* NR_STAT_CMA_BASE */
	{ "cma_inact_anon", 1 },    /* NR_CMA_INACTIVE_ANON */
	{ "cma_act_anon", 1 },      /* NR_CMA_ACTIVE_ANON */
	{ "cma_inact_file", 1 },    /* NR_CMA_INACTIVE_FILE */
	{ "cma_act_file", 1 },      /* NR_CMA_ACTIVE_FILE */
	{ "cma_unevict", 1 },       /* NR_CMA_UNEVICTABLE */
	{ "cma_contig", 1 },        /* NR_CMA_CONTIG_PAGES */
	{ "vm_zone_stat", 0 },      /* NR_VM_ZONE_STAT_ITEMS */
#endif
};

static unsigned long avail[2];
static unsigned long cached[2];

#ifdef TRACE_MARK_SWAP
static unsigned long swap[2];
#endif
/**
 * @todo: re-resolve below warning without +1,
 *        "error: array subscript is above array bounds [-Werror=array-bounds]"
 */
static unsigned long pages[2][NR_VM_ZONE_STAT_ITEMS + 1];

static void trace_mark_kmem_state_page(enum zone_stat_item item) 
{
	if (!trace_kmem_mark_pages[item].enable) return;

	if (pages[0][item] != pages[1][item])
		trace_mark_int(0, trace_kmem_mark_pages[item].str, K(pages[0][item]), "");
}

void trace_mark_kmem_stat(enum zone_stat_item item)
{
	int i;
#ifdef TRACE_MARK_SWAP
	struct sysinfo si;
#endif

	if (!trace_mark_enabled()) return;

	/**
	 * page state
	 */
	for (i = NR_FREE_PAGES; i < NR_VM_ZONE_STAT_ITEMS; i++)
		pages[0][i] = global_page_state(i);

	if (item < NR_VM_ZONE_STAT_ITEMS) {
		trace_mark_kmem_state_page(item);
	} else {
		for (i = NR_FREE_PAGES; i < NR_VM_ZONE_STAT_ITEMS; i++)
			trace_mark_kmem_state_page(item);
	}

	/**
	 * Free and cached
	 */
	if ((cached[0] = (pages[0][NR_FILE_PAGES] - total_swapcache_pages())) < 0)
		cached[0] = 0;

	if (cached[0] != cached[1])
		trace_mark_int(0, TRACE_MARK_CACHED, K(cached[0]), "");

	if ((avail[0] = pages[0][NR_FREE_PAGES] + cached[0]) != avail[1])
		trace_mark_int(0, TRACE_MARK_AVAIL, K(avail[0]), "");

#ifdef TRACE_MARK_SWAP
	/**
	 * Swap
	 */
	si_swapinfo(&si);

	if ((swap[0] = si.totalswap - si.freeswap) != swap[1])
		trace_mark_int(0, TRACE_MARK_SWAP, K(swap[0]), "");
#endif

	/**
	 * Backup
	 */
	avail[1] = avail[0];
	cached[1] = cached[0];
#ifdef TRACE_MARK_SWAP
	swap[1] = swap[0];
#endif
	memcpy(&pages[1], &pages[0], sizeof(pages[0]));
}
#endif

#define TRACE_MARK_LMK_CNT       "lmk_cnt"

static unsigned long lmk_count[2];

void trace_mark_lmk_count(unsigned long count) {
	if (!trace_mark_enabled()) return;

	if ((lmk_count[0] = count) != lmk_count[1]) {
		trace_mark_int(0, TRACE_MARK_LMK_CNT, count, "");

		lmk_count[1] = lmk_count[0];
	}
}

void trace_mark_page_begin(const char* title, struct page* page) {
	if (trace_mark_enabled()) {
		char args[TRACE_MARK_STR_LEN];

		snprintf(args, TRACE_MARK_STR_LEN, "page=0x%p;pfn=%lu", page, page_to_pfn(page));

		trace_mark_begin(title, args);
	}
}
EXPORT_SYMBOL(trace_mark_page_begin);

void trace_mark_page_end(const char* title, struct page* page) {
	if (trace_mark_enabled()) {
		char args[TRACE_MARK_STR_LEN];

		snprintf(args, TRACE_MARK_STR_LEN, "page=0x%p;pfn=%lu", page, page_to_pfn(page));

		trace_mark_end(args);
	}
}
EXPORT_SYMBOL(trace_mark_page_end);

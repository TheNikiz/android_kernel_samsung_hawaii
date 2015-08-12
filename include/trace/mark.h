/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace mark.
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */

int trace_mark_enabled(void);

void trace_mark(
		char event,
		unsigned long ppid,
		const char* name,
		const char* args,
		const char* category);

void trace_mark_int(
		unsigned long ppid,
		const char* name,
		unsigned long value,
		const char* category);

void trace_mark_begin(
		const char* title,
		const char* args);

void trace_mark_end(unsigned char* args);

void trace_mark_start(
		const char* name,
		unsigned long cookie);

void trace_mark_finish(
		unsigned char* name,
		unsigned long cookie);

/**
 * kmem
 */
#ifdef CONFIG_TRACE_MARK_KMEM_STAT
void trace_mark_kmem_stat(enum zone_stat_item item);
#endif

void trace_mark_lmk_count(unsigned long count);

void trace_mark_page_begin(const char* title, struct page* page);

void trace_mark_page_end(const char* title, struct page* page);

/**
 * syscall
 */
extern unsigned long trace_mark_syscall_tgid;

int trace_mark_syscall_enter(struct pt_regs* regs, long id);
int trace_mark_syscall_exit(struct pt_regs* regs, long ret);

#ifdef CONFIG_TRACE_MARK_MM_RSS
/**
 * mm
 */
void trace_mark_mm_rss_stat(struct mm_struct* mm, int member);
#endif

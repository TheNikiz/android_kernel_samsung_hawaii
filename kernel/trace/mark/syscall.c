/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace mark for kmem
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */

#include <asm/syscall.h>
#include <linux/sched.h>
#include <uapi/asm/unistd.h>

#include <trace/mark_base.h>
#include <trace/mark.h>

unsigned long trace_mark_syscall_tgid = 0;

void trace_mark_syscall_read(struct pt_regs* regs, long scid) {
	unsigned long args[6];
	char title[TRACE_MARK_STR_LEN];
	char param[TRACE_MARK_STR_LEN];

	syscall_get_arguments(current, regs, 0, 6, args);

	snprintf(title, TRACE_MARK_STR_LEN, "read(%d, 0x%p, %u)",
			(int) args[0],
			(const void*) args[1],
			(size_t) args[2]);

	snprintf(param, TRACE_MARK_STR_LEN, "scid=%ld", scid);

	trace_mark_begin(title, param);
}

void trace_mark_syscall_write(struct pt_regs* regs, long scid) {
	unsigned long args[6];
	char title[TRACE_MARK_STR_LEN];
	char param[TRACE_MARK_STR_LEN];

	syscall_get_arguments(current, regs, 0, 6, args);

	snprintf(title, TRACE_MARK_STR_LEN, "write(%d, 0x%p, %u)",
			(int) args[0],
			(const void*) args[1],
			(size_t) args[2]);

	snprintf(param, TRACE_MARK_STR_LEN, "scid=%ld", scid);

	trace_mark_begin(title, param);
}

void trace_mark_syscall_open(struct pt_regs* regs, long scid) {
	unsigned long args[6];
	char title[TRACE_MARK_STR_LEN];
	char param[TRACE_MARK_STR_LEN];

	syscall_get_arguments(current, regs, 0, 6, args);

	snprintf(title, TRACE_MARK_STR_LEN, "open(%s, 0x%x, 0x%x)",
			(const char*) args[0],
			(int) args[1],
			(mode_t) args[2]);

	snprintf(param, TRACE_MARK_STR_LEN, "scid=%ld", scid);

	trace_mark_begin(title, param);
}

void trace_mark_syscall_close(struct pt_regs* regs, long scid) {
	unsigned long args[6];
	char title[TRACE_MARK_STR_LEN];
	char param[TRACE_MARK_STR_LEN];

	syscall_get_arguments(current, regs, 0, 6, args);

	snprintf(title, TRACE_MARK_STR_LEN, "close(%d)",
			(int) args[0]);

	snprintf(param, TRACE_MARK_STR_LEN, "scid=%ld", scid);

	trace_mark_begin(title, param);
}

int trace_mark_syscall_available(long scid) {
	return (trace_mark_syscall_tgid == current->tgid);
}

int trace_mark_syscall_return(struct pt_regs* regs, long scid, long ret) {
	char args[TRACE_MARK_STR_LEN] = "";

	snprintf(args, TRACE_MARK_STR_LEN, "scid=%ld;ret=%ld;", scid, ret);

	trace_mark_end(args);

	return 1;
}

static int trace_mark_syscall_dispatch(struct pt_regs* regs, long scid, long ret, int enter) {
#define case_syscall(name, regs, scid, ret, enter) \
	case __NR_##name: {\
		if (enter) { \
			trace_mark_syscall_##name(regs, scid); \
			break; \
		} else { \
			trace_mark_syscall_return(regs, scid, ret); \
			break; \
		} \
	}

	if (!trace_mark_enabled()) return 0;

	if (!trace_mark_syscall_available(scid)) return 0;

	switch (scid) {
		case_syscall(read, regs, scid, ret, enter);
		case_syscall(write, regs, scid, ret, enter);
		case_syscall(open, regs, scid, ret, enter);
		case_syscall(close, regs, scid, ret, enter);
		default:
			return 0;
	}

	return 1;
}
int trace_mark_syscall_enter(struct pt_regs* regs, long id) {
	return trace_mark_syscall_dispatch(regs, id, 0 /* not used */, 1);
}

int trace_mark_syscall_exit(struct pt_regs* regs, long ret) {
	return trace_mark_syscall_dispatch(regs, current_thread_info()->syscall, ret, 0);
}

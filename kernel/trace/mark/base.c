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

#include <trace/mark_base.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mark.h>

extern int tracing_is_enabled(void);

int trace_mark_enabled(void) {
	return tracing_is_enabled();
}

EXPORT_SYMBOL(trace_mark_enabled);

void trace_mark(
		char event,
		unsigned long ppid,
		const char* name,
		const char* args,
		const char* category) {
	if (trace_mark_enabled()) {
		trace_tracing_mark_write(event, ppid, name, args, category);
	}
}
EXPORT_SYMBOL(trace_mark);

void trace_mark_int(
		unsigned long ppid,
		const char* name,
		unsigned long value,
		const char* category) {
	if (trace_mark_enabled()) {
		char args[TRACE_MARK_STR_LEN];

		snprintf(args, TRACE_MARK_STR_LEN, "%ld", value);

		trace_mark('C', ppid, name, args, category);
	}
}
EXPORT_SYMBOL(trace_mark_int);

void trace_mark_begin(
		const char* title,
		const char* args) {
	if (trace_mark_enabled()) {
		trace_mark('B', current->tgid, title, args, current->comm);
	}
}
EXPORT_SYMBOL(trace_mark_begin);

void trace_mark_end(unsigned char* args) {
	if (trace_mark_enabled()) {
		trace_mark('E', current->tgid, "", args, current->comm);
	}
}
EXPORT_SYMBOL(trace_mark_end);

void trace_mark_start(
		const char* name,
		unsigned long cookie) {
	if (trace_mark_enabled()) {
		char args[TRACE_MARK_STR_LEN];

		snprintf(args, TRACE_MARK_STR_LEN, "%ld", cookie);

		trace_mark('S', current->tgid, name, args, current->comm);
	}
}
EXPORT_SYMBOL(trace_mark_start);

void trace_mark_finish(
		unsigned char* name,
		unsigned long cookie) {
	if (trace_mark_enabled()) {
		char args[TRACE_MARK_STR_LEN];

		snprintf(args, TRACE_MARK_STR_LEN, "%ld", cookie);

		trace_mark('F', current->tgid, name, args, current->comm);
	}
}
EXPORT_SYMBOL(trace_mark_finish);


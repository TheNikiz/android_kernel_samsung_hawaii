/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace mark
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mark

#if !defined(_TRACE_MARK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MARK_H

#include <linux/types.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(mark,

	TP_PROTO(char event_type,
		unsigned long ppid,
		const char* name,
		const char* args,
		const char* category),
	
	TP_ARGS(event_type, ppid, name, args, category),

	TP_STRUCT__entry(
		__field(           char, event_type)
		__field(  unsigned long, ppid)
		__array(           char, name, TRACE_MARK_STR_LEN)
		__array(           char, args, TRACE_MARK_STR_LEN)
		__array(           char, category, TRACE_MARK_STR_LEN)
	),

	TP_fast_assign(
		__entry->event_type = event_type;
		__entry->ppid = ppid;
		memcpy(__entry->name, name, TRACE_MARK_STR_LEN);
		memcpy(__entry->args, args, TRACE_MARK_STR_LEN);
		memcpy(__entry->category, category, TRACE_MARK_STR_LEN);
	),

	TP_printk("%c|%lu|%s|%s|%s",
		__entry->event_type, __entry->ppid, __entry->name, __entry->args, __entry->category
	)
);

DEFINE_EVENT(mark, tracing_mark_write,

	TP_PROTO(char event_type,
		unsigned long ppid,
		const char* name,
		const char* args,
		const char* category),
	
	TP_ARGS(event_type, ppid, name, args, category)
);

#endif /* _TRACE_MARK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

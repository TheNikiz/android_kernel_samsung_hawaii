#undef TRACE_SYSTEM
#define TRACE_SYSTEM net

#if !defined(_TRACE_NET_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NET_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/tracepoint.h>

TRACE_EVENT(net_dev_xmit,

	TP_PROTO(struct sk_buff *skb,
		 int rc,
		 struct net_device *dev,
		 unsigned int skb_len),

	TP_ARGS(skb, rc, dev, skb_len),

	TP_STRUCT__entry(
		__field(	void *,		skbaddr		)
		__field(	unsigned int,	len		)
		__field(	int,		rc		)
		__string(	name,		dev->name	)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = skb_len;
		__entry->rc = rc;
		__assign_str(name, dev->name);
	),

	TP_printk("dev=%s skbaddr=%p len=%u rc=%d",
		__get_str(name), __entry->skbaddr, __entry->len, __entry->rc)
);

DECLARE_EVENT_CLASS(net_dev_template,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__field(	void *,		skbaddr		)
		__field(	unsigned int,	len		)
		__string(	name,		skb->dev->name	)
#ifdef CONFIG_TRACE_MARK_NET_SKB_DUMP
		__array(	char,		dump)
#endif
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = skb->len;
		__assign_str(name, skb->dev->name);
#ifdef CONFIG_TRACE_MARK_NET_SKB_DUMP
		trace_mark_net_skb_dump(__entry->dump, TRACE_MARK_NET_SKB_DUMP_LEN, skb);
#endif
	),

#ifdef CONFIG_TRACE_MARK_NET_SKB_DUMP
	TP_printk("dev=%s skbaddr=%p len=%u dump=[%s]",
		__get_str(name), __entry->skbaddr, __entry->len, __entry->dump)
#else
	TP_printk("dev=%s skbaddr=%p len=%u",
		__get_str(name), __entry->skbaddr, __entry->len)
#endif
)

DEFINE_EVENT(net_dev_template, net_dev_queue,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

DEFINE_EVENT(net_dev_template, netif_receive_skb,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

DEFINE_EVENT(net_dev_template, netif_rx,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);
#endif /* _TRACE_NET_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

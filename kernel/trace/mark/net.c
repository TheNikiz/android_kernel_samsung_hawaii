/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * Trace mark for net
 *
 * @autorh kumhyun.cho@samsung.com
 * @since 2014.02.14
 */

#ifdef CONFIG_TRACE_MARK_NET_SKB_DUMP
int trace_mark_net_skb_dump(char* dump, size_t len, struct sk_buff* skb) {
	int i;

	for (i = 0; i < skb->data_len; i++)
		dump[i] = isprint(skb->data[i]) ? skb->data[i] : '.';
	
	return i;
}
#endif

#include "stdio.h"

#include <linux/export.h>
#include <net/ipv6.h>
/*
 * Skip any extension headers. This is used by the ICMP module.
 *
 * Note that strictly speaking this conflicts with RFC 2460 4.0:
 * ...The contents and semantics of each extension header determine whether
 * or not to proceed to the next header.  Therefore, extension headers must
 * be processed strictly in the order they appear in the packet; a
 * receiver must not, for example, scan through a packet looking for a
 * particular kind of extension header and process that header prior to
 * processing all preceding ones.
 *
 * We do exactly this. This is a protocol bug. We can't decide after a
 * seeing an unknown discard-with-error flavour TLV option if it's a
 * ICMP error message or not (errors should never be send in reply to
 * ICMP error messages).
 *
 * But I see no other way to do this. This might need to be reexamined
 * when Linux implements ESP (and maybe AUTH) headers.
 * --AK
 *
 * This function parses (probably truncated) exthdr set "hdr".
 * "nexthdrp" initially points to some place,
 * where type of the first header can be found.
 *
 * It skips all well-known exthdrs, and returns pointer to the start
 * of unparsable area i.e. the first header with unknown type.
 * If it is not NULL *nexthdr is updated by type/protocol of this header.
 *
 * NOTES: - if packet terminated with NEXTHDR_NONE it returns NULL.
 *        - it may return pointer pointing beyond end of packet,
 *	    if the last recognized header is truncated in the middle.
 *        - if packet is truncated, so that all parsed headers are skipped,
 *	    it returns NULL.
 *	  - First fragment header is skipped, not-first ones
 *	    are considered as unparsable.
 *	  - Reports the offset field of the final fragment header so it is
 *	    possible to tell whether this is a first fragment, later fragment,
 *	    or not fragmented.
 *	  - ESP is unparsable for now and considered like
 *	    normal payload protocol.
 *	  - Note also special handling of AUTH header. Thanks to IPsec wizards.
 *
 * --ANK (980726)
 */

int ipv6_skip_exthdr(const struct sk_buff *skb, int start, u8 *nexthdrp,
		     __be16 *frag_offp)
{
	return 0;
}

int tcp_gro_complete(struct sk_buff *skb)
{
	return 0;
}

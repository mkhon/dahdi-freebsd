#ifndef _LINUX_NETDEVICE_H_
#define _LINUX_NETDEVICE_H_

#include <sys/socket.h>
#include <net/if.h>			/* IFNAMSIZ */

#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/if_ether.h>

/**
 * Network device statistics
 */
struct net_device_stats {
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received		*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;

	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};

/**
 * Network device
 */
struct net_device {
	char name[IFNAMSIZ];
	struct net_device_stats stats;
	u_long state;

	unsigned long trans_start;
	unsigned long last_rx;

	unsigned char *dev_addr;
};

/**
 * Get network device private data
 */
static inline void *netdev_priv(struct net_device *dev)
{
	return (char *) dev + sizeof(struct net_device);
}

enum netdev_state_t
{
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
};

static inline int netif_carrier_ok(const struct net_device *dev)
{
	return !test_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

static inline void netif_carrier_on(struct net_device *dev)
{
	clear_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

static inline void netif_carrier_off(struct net_device *dev)
{
	set_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

#endif /* _LINUX_NETDEVICE_H_ */

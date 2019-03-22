#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_ether.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

static struct rte_mempool *mbuf_pool;

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

static __inline__ uint32_t wrapsum(uint32_t sum) {
	sum = ~sum & 0xFFFF;
	return (htons(sum));
}

static __inline__ uint32_t checksum(void *data, unsigned nbytes, uint32_t sum) {
	uint32_t i = 0;
	unsigned char *buf = (unsigned char *) data;

	for (i = 0; i < (nbytes & ~1U); i += 2) {
		sum += (uint16_t) ntohs(*((uint16_t *) (buf + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	if (i < nbytes) {
		sum += buf[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	return (sum);
}

static struct rte_mbuf* create_udp_packet(uint32_t port, void* data,
		uint32_t data_len, uint32_t s_ip, uint32_t s_port, uint32_t d_ip, uint32_t d_port) {
	struct rte_mbuf* m = NULL;
	struct ether_hdr * eth = NULL;
	struct ipv4_hdr * ip_hdr = NULL;
	struct udp_hdr * udp_hdr = NULL;
	unsigned char* buf = NULL;

	m = rte_pktmbuf_alloc(mbuf_pool);
	if (unlikely(m == NULL))
		return NULL;

	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	ip_hdr = (struct ipv4_hdr *) &eth[1];
	udp_hdr = (struct udp_hdr *) &ip_hdr[1];

	buf = (unsigned char *) &udp_hdr[1];
	memcpy(buf, data, data_len);

	eth->ether_type = htons(ETHER_TYPE_IPv4);
	memset(ip_hdr, 0, sizeof(struct ipv4_hdr));
	memset(udp_hdr, 0, sizeof(struct udp_hdr));

    rte_eth_macaddr_get(port, &eth->s_addr);

    struct ether_addr dst_mac;
    dst_mac.addr_bytes[0] = 0x00;
    dst_mac.addr_bytes[1] = 0x50;
    dst_mac.addr_bytes[2] = 0x56;
    dst_mac.addr_bytes[3] = 0xc0;
    dst_mac.addr_bytes[4] = 0x00;
    dst_mac.addr_bytes[5] = 0x01;

    ether_addr_copy(&dst_mac, &eth->d_addr);

	ip_hdr->version_ihl = 0x45;
	ip_hdr->total_length = htons(
			data_len + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
	ip_hdr->time_to_live = 255;
	ip_hdr->type_of_service = 0;

	ip_hdr->fragment_offset = 0;
	ip_hdr->next_proto_id = (IPPROTO_UDP);

	ip_hdr->src_addr = s_ip;
	ip_hdr->dst_addr = d_ip;
	ip_hdr->hdr_checksum = wrapsum(
			checksum((unsigned char *) ip_hdr, sizeof(struct ipv4_hdr), 0)); //cksum(ip_hdr, sizeof(struct ipv4_hdr), 0);
	udp_hdr->src_port = htons(s_port);
	udp_hdr->dst_port = htons(d_port);
	udp_hdr->dgram_len = htons(data_len + sizeof(struct udp_hdr));

	udp_hdr->dgram_cksum = wrapsum(
			checksum((unsigned char *) udp_hdr, sizeof(struct udp_hdr),
					checksum(data, data_len,
							checksum((unsigned char *) &ip_hdr->src_addr,
									2 * sizeof(ip_hdr->src_addr),
									IPPROTO_UDP
											+ (uint32_t) ntohs(
													udp_hdr->dgram_len)))));

	m->pkt_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)
			+ sizeof(struct udp_hdr) + data_len;
	m->data_len = m->pkt_len;

	return m;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */

int
main(int argc, char *argv[])
{
	
	unsigned nb_ports;
	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is a port to send/receive on. */
	nb_ports = rte_eth_dev_count();
	
	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
    portid = 0;
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
                portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	// des ip: 192.168.3.1
    struct rte_mbuf * udp = create_udp_packet(0, "hello world", 12, rte_cpu_to_be_32(IPv4(192,168,3,3)), 233, rte_cpu_to_be_32(IPv4(192,168,3,1)), 233);

    /* Send burst of TX packets, to second port of pair. */
    const uint16_t nb_tx = rte_eth_tx_burst(portid, 0, &udp, 1);

    rte_pktmbuf_free(udp);

	return 0;
}

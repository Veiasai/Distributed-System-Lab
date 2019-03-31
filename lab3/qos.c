#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000 * 46,  .cbs = 2048, .ebs = 2048},
}

FLOW_METER app_flows[APP_FLOWS_MAX];

/**
 * srTCM
 */
int
qos_meter_init(void)
{
    uint32_t i, j;
	int ret;

	for (i = 0, j = 0; i < APP_FLOWS_MAX;
			i ++, j = (j + 1) % RTE_DIM(PARAMS)) {
		ret = FUNC_CONFIG(&app_flows[i], &PARAMS[j]);
		if (ret)
			return ret;
	}

    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    /* to do */
	return FUNC_METER(&app_flows[flow_id], time, pkt_len);
}


/**
 * WRED
 */

int
qos_dropper_init(void)
{
    /* to do */
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */
}
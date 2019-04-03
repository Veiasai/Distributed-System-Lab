#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"
#include "rte_cycles.h"

#define FUNC_METER(a,b,c) rte_meter_srtcm_color_blind_check(a,b,c)
#define FUNC_CONFIG   rte_meter_srtcm_config
#define PARAMS        app_srtcm_params
#define FLOW_METER    struct rte_meter_srtcm

static struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX] = {
	{.cir = 171800000,  .cbs = 160000000, .ebs = 170000000},
	{.cir = 85900000,  .cbs = 80000000, .ebs = 85000000},
	{.cir = 42850000,  .cbs = 40000000, .ebs = 42000000},
	{.cir = 21425000,  .cbs = 20000000, .ebs = 21000000},
};

FLOW_METER app_flows[APP_FLOWS_MAX];

/**
 * srTCM
 */
int
qos_meter_init(void)
{
    uint32_t i;
	int ret;
	for (i = 0; i < APP_FLOWS_MAX; i++) {
		ret = FUNC_CONFIG(&app_flows[i], &PARAMS[i]);
		if (ret)
			return ret;
	}
	printf("meter init ok\n");
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    /* to do */
	return (enum qos_color) FUNC_METER(&app_flows[flow_id], time, pkt_len);
}


/**
 * WRED
 */
static struct rte_red_params r_param[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {
    {
        {.min_th = 220, .max_th = 230, .maxp_inv = 50, .wq_log2 = 8},
        {.min_th = 1, .max_th = 50, .maxp_inv = 20, .wq_log2 = 8},
        {.min_th = 1, .max_th = 2, .maxp_inv = 1, .wq_log2 = 8}
    },
    {
        {.min_th = 105, .max_th = 115, .maxp_inv = 10, .wq_log2 = 4},
        {.min_th = 1, .max_th = 25, .maxp_inv = 10, .wq_log2 = 4},
        {.min_th = 1, .max_th = 2, .maxp_inv = 1, .wq_log2 = 4}
    },
    {
        {.min_th = 52, .max_th = 62, .maxp_inv = 10, .wq_log2 = 2},
        {.min_th = 1, .max_th = 12, .maxp_inv = 5, .wq_log2 = 2},
        {.min_th = 1, .max_th = 2, .maxp_inv = 1, .wq_log2 = 2}
    },
    {
        {.min_th = 21, .max_th = 31, .maxp_inv = 10, .wq_log2 = 1},
        {.min_th = 1, .max_th = 6, .maxp_inv = 3, .wq_log2 = 1},
        {.min_th = 1, .max_th = 2, .maxp_inv = 1, .wq_log2 = 1}
    }
};

static struct rte_red_config r_cfg[APP_FLOWS_MAX][e_RTE_METER_COLORS];

static struct rte_red r[APP_FLOWS_MAX][e_RTE_METER_COLORS];

unsigned queues[APP_FLOWS_MAX];

uint64_t round_t;


int
qos_dropper_init(void)
{
    /* to do */
	uint32_t i, j;
    int ret;

    for (i = 0; i < APP_FLOWS_MAX; i++) {
        for (j = 0; j < e_RTE_METER_COLORS; j++) {
            ret = rte_red_rt_data_init(&r[i][j]);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Failed to initialize red run-time datas\n");
            ret = rte_red_config_init(&r_cfg[i][j],
                r_param[i][j].wq_log2,
                r_param[i][j].min_th,
                r_param[i][j].max_th,
                r_param[i][j].maxp_inv
            );
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "Invalid red params for flow %d :color %d, error code:%d\n", i + 1, j, ret);
        }

        queues[i] = 0;
    }

    round_t = 0;
	printf("drop init ok\n");
    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    if (time != round_t) {
        for (int i = 0; i < APP_FLOWS_MAX; i++)
            queues[i] = 0;
        round_t = time;
    }

    if (rte_red_enqueue(&r_cfg[flow_id][color], &r[flow_id][color], queues[flow_id], time) == 0) {
        queues[flow_id]++;
        return 0;
    }

    return 1;
}
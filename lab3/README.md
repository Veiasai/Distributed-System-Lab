# Lab 3

## 参数分析

### 令牌产生速度

- main的注释里写了。
  - `1000 packets per period`
  - `640 bytes per packet averagely`
- 而一个period，`time += 1000000;`，单位为ns。
- 也就是说，每秒钟要发 640 * 1000 * 1000 Bytes，每个flow 1.6e8 Bytes。
- 根据lab3 pdf，flow1的bandwidth为1.28Gbps，约1.7e8 Bytes / s。四个flow的比例为8421，由此可得出令牌桶中cir参数大小，和网速对应即可，ebs和cbs则适当小于最大网速。

### 丢弃参数

- per period要发送1000包，根据上面的计算，即使是flow1，也只能承受1/4，所以max queue在250左右。
- 随手匀给绿色230，黄色50，红色0，其他flow按比例调整。再根据运行数据，微调。
- 此时运行的话，各个流的比例是正确的。

### wg—log

- 然而一个令人窒息的参数，wg-log2，这个参数极大的影响了结果，而且我并不知道怎么测算。
- 据说是queue大小调整的速率，那么一般来说，这个参数也应该符合速率比例，即8421。
- 通过实际运行，使得这个参数，让flow1恰好跑满，其他flow依次按length比例降低。

## API

### Meter

  ```c
  int
  rte_meter_srtcm_config(struct rte_meter_srtcm *m,
    struct rte_meter_srtcm_params *params);
  ```

初始化每个流的srtcm的runtime data。

  ``` c
  static inline enum rte_meter_color
  rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
    uint64_t time,
    uint32_t pkt_len);
  ```

给到来的每一个包染色，传入的srtcm为每个流对应的runtime data。

### Dropper

 ```c
 int
 rte_red_config_init(struct rte_red_config *red_cfg,
    const uint16_t wq_log2,
    const uint16_t min_th,
    const uint16_t max_th,
    const uint16_t maxp_inv);
 ```

初始化config。

 ```c
 int
 rte_red_rt_data_init(struct rte_red *red);
 ```

初始化dropper的runtime data。

```c
 static inline int
 rte_red_enqueue(const struct rte_red_config *red_cfg,
    struct rte_red *red,
    const unsigned q,
    const uint64_t time);
```

判断一个包是否能入队。
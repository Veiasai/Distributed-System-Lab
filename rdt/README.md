# Lab 1 Readme

## Info

- 刘劲锋
- 516030910237
- ljf1025@sjtu.edu.cn

## Design

- 模仿tcp，两端都有一个sliding window。
- 由于无法阻塞上层流，使用一个vecctor缓存所有packet。
- 每次收到上层包或ack，都会发送处于window中，且从未发送过的包。
- 给packet编一个id，随机取一个开头然后自增。
- receiver每次收到包都发送ack，返回的值是有序包的最大值。（小于该id的都已经收到）
- Sender timeout则将处于window中，且未收到ack的包都发送一次。（从这里来说属于go-back）
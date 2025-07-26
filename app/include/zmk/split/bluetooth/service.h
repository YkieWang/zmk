/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/events/sensor_event.h>
#include <zmk/sensors.h>

#define ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN 9

struct sensor_event {
    uint8_t sensor_index;

    uint8_t channel_data_size;
    struct zmk_sensor_channel_data channel_data[ZMK_SENSOR_EVENT_MAX_CHANNELS];
} __packed;

struct zmk_split_run_behavior_data {
    uint8_t position;
    uint8_t source;
    uint8_t state;
    uint32_t param1;
    uint32_t param2;
} __packed;

struct zmk_split_run_behavior_payload {
    struct zmk_split_run_behavior_data data;
    char behavior_dev[ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN];
} __packed;

// 新增：睡眠管理命令定义
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
enum zmk_split_sleep_command {
    ZMK_SPLIT_SLEEP_NORMAL = 0,    // 正常睡眠
    ZMK_SPLIT_SLEEP_MANAGED = 1,   // 托管睡眠（可远程唤醒）
    ZMK_SPLIT_SLEEP_DEEP = 2,      // 深度睡眠
    ZMK_SPLIT_WAKEUP = 3,          // 唤醒命令
};

struct zmk_split_sleep_mgmt_data {
    uint8_t command;
    uint8_t source_id;
    uint32_t timestamp;
} __packed;

// 新增函数声明
int zmk_split_send_sleep_command(enum zmk_split_sleep_command cmd);
int zmk_split_send_wakeup_command(void);
#endif

int zmk_split_bt_position_pressed(uint8_t position);
int zmk_split_bt_position_released(uint8_t position);
int zmk_split_bt_sensor_triggered(uint8_t sensor_index,
                                  const struct zmk_sensor_channel_data channel_data[],
                                  size_t channel_data_size);

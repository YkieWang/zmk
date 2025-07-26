/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

enum zmk_activity_state { 
    ZMK_ACTIVITY_ACTIVE,
    ZMK_ACTIVITY_IDLE, 
    ZMK_ACTIVITY_SLEEP,
    ZMK_ACTIVITY_REMOTE_SLEEP,    // 新增：远程控制的睡眠状态
    ZMK_ACTIVITY_MANAGED_SLEEP    // 新增：被管理的睡眠状态（外设端）
};

enum zmk_activity_state zmk_activity_get_state(void);
int zmk_activity_set_state(enum zmk_activity_state state);  // 新增：强制设置状态
bool zmk_activity_is_sleep_state(enum zmk_activity_state state);  // 新增：判断是否为睡眠状态

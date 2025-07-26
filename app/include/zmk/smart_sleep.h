/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// 智能睡眠功能函数声明
int zmk_smart_sleep_central(void);
int zmk_smart_sleep_peripheral(void);
int zmk_smart_sleep_standalone(void);
int zmk_smart_sleep_execute(void);
int zmk_smart_sleep_wakeup(void);

// 状态查询函数
bool zmk_smart_sleep_is_sleeping(void);

// 调试和监控函数
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP_DEBUG)
void zmk_smart_sleep_status_report(void);
void zmk_smart_sleep_enable_debug(void);
#endif 
/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// 外设深度睡眠控制命令
#define PERIPHERAL_DEEP_SLEEP_ON    1
#define PERIPHERAL_DEEP_SLEEP_OFF   0
#define PERIPHERAL_DEEP_SLEEP_TOGGLE 2

// 别名，便于使用
#define PDSLEEP_ON     PERIPHERAL_DEEP_SLEEP_ON
#define PDSLEEP_OFF    PERIPHERAL_DEEP_SLEEP_OFF
#define PDSLEEP_TOGGLE PERIPHERAL_DEEP_SLEEP_TOGGLE 
/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/activity.h>
#include <zmk/ble.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
#include <zmk/split/bluetooth/service.h>
#endif

// 智能睡眠状态管理
struct smart_sleep_state {
    bool is_sleeping;
    bool keep_bt_alive;
    struct bt_conn *central_conn;  // 外设端保存中央端连接
    struct k_work_delayable wakeup_work;
};

static struct smart_sleep_state sleep_state = {0};

// 睡眠配置参数
struct smart_sleep_config {
    uint16_t bt_interval_min;     // 蓝牙最小间隔（低功耗）
    uint16_t bt_interval_max;     // 蓝牙最大间隔
    uint16_t bt_latency;          // 蓝牙延迟
    uint16_t bt_timeout;          // 蓝牙超时
    uint32_t scan_suspend_ms;     // 按键扫描暂停时间
};

static const struct smart_sleep_config sleep_config = {
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP_BT_INTERVAL_MIN)
    .bt_interval_min = CONFIG_ZMK_SMART_SLEEP_BT_INTERVAL_MIN,
#else
    .bt_interval_min = 80,   // 100ms (80 * 1.25ms)
#endif
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP_BT_INTERVAL_MAX)
    .bt_interval_max = CONFIG_ZMK_SMART_SLEEP_BT_INTERVAL_MAX,
#else
    .bt_interval_max = 160,  // 200ms (160 * 1.25ms)
#endif
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP_BT_LATENCY)
    .bt_latency = CONFIG_ZMK_SMART_SLEEP_BT_LATENCY,
#else
    .bt_latency = 4,         // 跳过4个连接事件
#endif
    .bt_timeout = 500,       // 5s超时
    .scan_suspend_ms = 100,  // 100ms后暂停扫描
};

// 声明分体键盘相关函数
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
int zmk_split_send_sleep_command(enum zmk_split_sleep_command cmd);
#endif

// 中央端智能睡眠
int zmk_smart_sleep_central(void) {
    LOG_INF("Central entering smart sleep");
    
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
    // 1. 通知所有外设进入托管睡眠
    zmk_split_send_sleep_command(ZMK_SPLIT_SLEEP_MANAGED);
    
    // 2. 等待外设确认
    k_sleep(K_MSEC(100));
#endif
    
    // 3. 暂停非关键设备
    const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
    if (kscan && device_is_ready(kscan)) {
        pm_device_action_run(kscan, PM_DEVICE_ACTION_SUSPEND);
    }
    
    // 4. 降低蓝牙功耗但保持连接
    // 这里需要调用BLE模块的低功耗模式设置
    // zmk_ble_set_low_power_mode(true);  // 这个函数需要在BLE模块中实现
    
    // 5. 设置状态并进入低功耗模式
    sleep_state.is_sleeping = true;
    sleep_state.keep_bt_alive = true;
    
    // 进入suspend-to-idle模式（保持蓝牙）
    // pm_state_force(0u, &(struct pm_state_info){
    //     .state = PM_STATE_SUSPEND_TO_IDLE,
    //     .min_residency_us = 0,
    //     .exit_latency_us = 0
    // });
    
    return 0;
}

// 外设端智能睡眠  
int zmk_smart_sleep_peripheral(void) {
    LOG_INF("Peripheral entering smart sleep");
    
    // 1. 保存中央端连接
    sleep_state.central_conn = zmk_ble_active_conn();
    if (!sleep_state.central_conn) {
        LOG_WRN("No central connection, falling back to deep sleep");
        return -ENOTCONN;
    }
    
    // 2. 暂停按键扫描
    const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
    if (kscan && device_is_ready(kscan)) {
        pm_device_action_run(kscan, PM_DEVICE_ACTION_SUSPEND);
    }
    
    // 3. 设置蓝牙低功耗参数
    struct bt_le_conn_param low_power_param = {
        .interval_min = sleep_config.bt_interval_min,
        .interval_max = sleep_config.bt_interval_max,
        .latency = sleep_config.bt_latency,
        .timeout = sleep_config.bt_timeout,
    };
    
    int ret = bt_conn_le_param_update(sleep_state.central_conn, &low_power_param);
    if (ret) {
        LOG_ERR("Failed to update BT params: %d", ret);
    }
    
    // 4. 设置蓝牙为唤醒源
    const struct device *bt_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_bt_uart));
    if (bt_dev && device_is_ready(bt_dev)) {
        pm_device_wakeup_enable(bt_dev, true);
    }
    
    // 5. 设置状态并进入低功耗
    sleep_state.is_sleeping = true;
    sleep_state.keep_bt_alive = true;
    
    return 0;
}

// 独立键盘智能睡眠
int zmk_smart_sleep_standalone(void) {
    LOG_INF("Standalone keyboard entering smart sleep");
    
    // 对于独立键盘，可以更激进地睡眠
    // 但仍保持USB连接（如果有）
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    if (zmk_usb_is_powered()) {
        // USB供电时保持连接
        const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
        if (kscan && device_is_ready(kscan)) {
            pm_device_action_run(kscan, PM_DEVICE_ACTION_SUSPEND);
        }
        
        sleep_state.is_sleeping = true;
        return 0;
    }
#endif
    
    // 电池供电时使用智能睡眠策略
    const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
    if (kscan && device_is_ready(kscan)) {
        pm_device_action_run(kscan, PM_DEVICE_ACTION_SUSPEND);
    }
    
    sleep_state.is_sleeping = true;
    return 0;
}

// 智能睡眠唤醒
int zmk_smart_sleep_wakeup(void) {
    if (!sleep_state.is_sleeping) {
        return 0;
    }
    
    LOG_INF("Waking up from smart sleep");
    
    // 1. 恢复按键扫描
    const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
    if (kscan && device_is_ready(kscan)) {
        pm_device_action_run(kscan, PM_DEVICE_ACTION_RESUME);
    }
    
    // 2. 恢复正常蓝牙参数
    if (sleep_state.central_conn) {
        struct bt_le_conn_param normal_param = {
            .interval_min = 6,    // 7.5ms
            .interval_max = 12,   // 15ms
            .latency = 0,         // 无延迟
            .timeout = 300,       // 3s超时
        };
        
        bt_conn_le_param_update(sleep_state.central_conn, &normal_param);
    }
    
    // 3. 恢复CPU性能
    // pm_state_clear(PM_STATE_SUSPEND_TO_IDLE);
    
    // 4. 重置状态
    sleep_state.is_sleeping = false;
    
    return 0;
}

// 执行智能睡眠
int zmk_smart_sleep_execute(void) {
    enum zmk_activity_state state = zmk_activity_get_state();
    
    switch (state) {
        case ZMK_ACTIVITY_REMOTE_SLEEP:
            if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)) {
                return zmk_smart_sleep_central();
            } else if (IS_ENABLED(CONFIG_ZMK_SPLIT)) {
                return zmk_smart_sleep_peripheral();
            } else {
                return zmk_smart_sleep_standalone();
            }
            
        case ZMK_ACTIVITY_MANAGED_SLEEP:
            return zmk_smart_sleep_peripheral();
            
        default:
            LOG_WRN("Invalid state for smart sleep: %d", state);
            return -EINVAL;
    }
}

// 获取当前睡眠状态
bool zmk_smart_sleep_is_sleeping(void) {
    return sleep_state.is_sleeping;
}

// 状态报告函数（用于调试）
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP_DEBUG)
void zmk_smart_sleep_status_report(void) {
    LOG_INF("=== Smart Sleep Status Report ===");
    LOG_INF("Current activity state: %d", zmk_activity_get_state());
    LOG_INF("Sleep active: %s", sleep_state.is_sleeping ? "YES" : "NO");
    LOG_INF("BT keepalive: %s", sleep_state.keep_bt_alive ? "YES" : "NO");
    
    if (sleep_state.central_conn) {
        struct bt_conn_info info;
        int ret = bt_conn_get_info(sleep_state.central_conn, &info);
        if (ret == 0) {
            LOG_INF("BT connection - interval: %d, latency: %d, timeout: %d", 
                    info.le.interval, info.le.latency, info.le.timeout);
        }
    } else {
        LOG_INF("BT connection: NONE");
    }
    
    // 设备状态检查
    const struct device *kscan = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zmk_kscan));
    if (kscan && device_is_ready(kscan)) {
        enum pm_device_state kscan_state;
        pm_device_state_get(kscan, &kscan_state);
        LOG_INF("Kscan device state: %s", 
                kscan_state == PM_DEVICE_STATE_ACTIVE ? "ACTIVE" : "SUSPENDED");
    }
    
    LOG_INF("================================");
}

// 定期状态报告（调试模式）
static void debug_status_timer_handler(struct k_timer *timer) {
    zmk_smart_sleep_status_report();
}

K_TIMER_DEFINE(debug_status_timer, debug_status_timer_handler, NULL);

void zmk_smart_sleep_enable_debug(void) {
    k_timer_start(&debug_status_timer, K_SECONDS(10), K_SECONDS(30));
}
#endif /* CONFIG_ZMK_SMART_SLEEP_DEBUG */ 
/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/activity.h>
#include <zmk/smart_sleep.h>
#include <zmk/split/bluetooth/service.h>
#include <zmk/split/bluetooth/uuid.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

// 中央端实现

// 发送睡眠命令到外设
int zmk_split_send_sleep_command(enum zmk_split_sleep_command cmd) {
    struct zmk_split_sleep_mgmt_data data = {
        .command = cmd,
        .source_id = 0, // 中央端ID
        .timestamp = k_uptime_get()
    };
    
    LOG_DBG("Sending sleep command %d to peripherals", cmd);
    
    // 这里需要访问外设连接列表，实际实现需要与central.c协调
    // 为了简化，这里返回成功，实际实现需要遍历连接的外设
    
    return 0;
}

// 发送唤醒命令
int zmk_split_send_wakeup_command(void) {
    return zmk_split_send_sleep_command(ZMK_SPLIT_WAKEUP);
}

#else

// 外设端实现

// 处理睡眠管理命令
static ssize_t split_sleep_mgmt_write(struct bt_conn *conn,
                                    const struct bt_gatt_attr *attr,
                                    const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags) {
    if (len != sizeof(struct zmk_split_sleep_mgmt_data)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
    struct zmk_split_sleep_mgmt_data *data = (struct zmk_split_sleep_mgmt_data *)buf;
    
    LOG_DBG("Received sleep command: %d from source: %d", data->command, data->source_id);
    
    switch (data->command) {
        case ZMK_SPLIT_SLEEP_NORMAL:
            zmk_activity_set_state(ZMK_ACTIVITY_SLEEP);
            zmk_smart_sleep_peripheral();
            break;
            
        case ZMK_SPLIT_SLEEP_MANAGED:
            zmk_activity_set_state(ZMK_ACTIVITY_MANAGED_SLEEP);
            zmk_smart_sleep_peripheral();
            break;
            
        case ZMK_SPLIT_SLEEP_DEEP:
            zmk_activity_set_state(ZMK_ACTIVITY_SLEEP);
            // 这里调用传统的深度睡眠
            // zmk_pm_suspend_devices();
            // sys_poweroff();
            break;
            
        case ZMK_SPLIT_WAKEUP:
            if (zmk_activity_is_sleep_state(zmk_activity_get_state())) {
                zmk_smart_sleep_wakeup();
                zmk_activity_set_state(ZMK_ACTIVITY_ACTIVE);
            }
            break;
            
        default:
            LOG_WRN("Unknown sleep command: %d", data->command);
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }
    
    return len;
}

// GATT服务定义中添加新特征
// 注意：实际实现需要在peripheral.c的GATT服务定义中添加
// BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZMK_SPLIT_BT_CHAR_SLEEP_MGMT_UUID),
//                       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
//                       BT_GATT_PERM_WRITE,
//                       NULL, split_sleep_mgmt_write, NULL),

// 空实现函数（外设端不发送命令）
int zmk_split_send_sleep_command(enum zmk_split_sleep_command cmd) {
    return 0;
}

int zmk_split_send_wakeup_command(void) {
    return 0;
}

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */ 
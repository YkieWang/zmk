/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_remote_sleep

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/activity.h>
#include <zmk/smart_sleep.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
#include <zmk/split/bluetooth/service.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 远程睡眠命令类型
enum remote_sleep_command {
    REMOTE_SLEEP_ENTER = 0,      // 进入远程睡眠
    REMOTE_SLEEP_EXIT = 1,       // 退出远程睡眠
    REMOTE_SLEEP_TOGGLE = 2,     // 切换远程睡眠状态
};

struct behavior_remote_sleep_config {
    enum remote_sleep_command default_command;
    bool affect_peripherals;     // 是否影响外设
    uint32_t delay_ms;          // 延迟执行时间
};

// 声明分体键盘相关函数
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
int zmk_split_send_sleep_command(enum zmk_split_sleep_command cmd);
int zmk_split_send_wakeup_command(void);
#endif

static int behavior_remote_sleep_init(const struct device *dev) { 
    return 0; 
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_remote_sleep_config *config = dev->config;
    
    enum remote_sleep_command cmd = binding->param1;
    if (cmd > REMOTE_SLEEP_TOGGLE) {
        cmd = config->default_command;
    }
    
    enum zmk_activity_state current_state = zmk_activity_get_state();
    
    switch (cmd) {
        case REMOTE_SLEEP_ENTER:
            if (!zmk_activity_is_sleep_state(current_state)) {
                LOG_INF("Entering remote sleep mode");
                
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
                // 如果是中央端且需要影响外设
                if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && config->affect_peripherals) {
                    zmk_split_send_sleep_command(ZMK_SPLIT_SLEEP_MANAGED);
                }
#endif
                
                zmk_activity_set_state(ZMK_ACTIVITY_REMOTE_SLEEP);
                zmk_smart_sleep_execute();
            }
            break;
            
        case REMOTE_SLEEP_EXIT:
            if (zmk_activity_is_sleep_state(current_state)) {
                LOG_INF("Exiting remote sleep mode");
                zmk_smart_sleep_wakeup();
                zmk_activity_set_state(ZMK_ACTIVITY_ACTIVE);
                
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
                // 唤醒外设
                if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && config->affect_peripherals) {
                    zmk_split_send_wakeup_command();
                }
#endif
            }
            break;
            
        case REMOTE_SLEEP_TOGGLE:
            if (zmk_activity_is_sleep_state(current_state)) {
                // 当前在睡眠，则唤醒
                LOG_INF("Waking up from remote sleep");
                zmk_smart_sleep_wakeup();
                zmk_activity_set_state(ZMK_ACTIVITY_ACTIVE);
                
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
                if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && config->affect_peripherals) {
                    zmk_split_send_wakeup_command();
                }
#endif
            } else {
                // 当前清醒，则睡眠
                LOG_INF("Entering remote sleep");
                
#if IS_ENABLED(CONFIG_ZMK_SPLIT_SLEEP_MGMT)
                if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && config->affect_peripherals) {
                    zmk_split_send_sleep_command(ZMK_SPLIT_SLEEP_MANAGED);
                }
#endif
                
                zmk_activity_set_state(ZMK_ACTIVITY_REMOTE_SLEEP);
                zmk_smart_sleep_execute();
            }
            break;
    }
    
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    // 远程睡眠在按下时执行，释放时不做处理
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_remote_sleep_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

// 设备树实例化宏
#define BRS_INST(n)                                                                               \
    static const struct behavior_remote_sleep_config brs_config_##n = {                         \
        .default_command = DT_INST_PROP_OR(n, default_command, REMOTE_SLEEP_TOGGLE),           \
        .affect_peripherals = DT_INST_PROP_OR(n, affect_peripherals, true),                    \
        .delay_ms = DT_INST_PROP_OR(n, delay_ms, 0),                                           \
    };                                                                                           \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_remote_sleep_init, NULL, NULL, &brs_config_##n,        \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                  \
                            &behavior_remote_sleep_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BRS_INST) 
/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_psleep

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/pm/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/physical_layouts.h>

#if IS_ENABLED(CONFIG_ZMK_DISPLAY)
#include <zmk/display.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
#include <zmk/rgb_underglow.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT)
#include <zmk/backlight.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 外设深度睡眠控制命令
#define PERIPHERAL_DEEP_SLEEP_ON    1
#define PERIPHERAL_DEEP_SLEEP_OFF   0
#define PERIPHERAL_DEEP_SLEEP_TOGGLE 2  // 新增toggle命令

// 别名，便于使用
#define PDSLEEP_ON     PERIPHERAL_DEEP_SLEEP_ON
#define PDSLEEP_OFF    PERIPHERAL_DEEP_SLEEP_OFF
#define PDSLEEP_TOGGLE PERIPHERAL_DEEP_SLEEP_TOGGLE

// 外设睡眠状态跟踪
static bool peripheral_in_deep_sleep = false;

#define IS_SPLIT_PERIPHERAL                                                                        \
    (IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

// LED指示函数（用于调试）
static void indicate_sleep_state(bool sleeping) {
    // 简单的延迟指示，也可以通过RGB等其他方式
    if (sleeping) {
        LOG_INF("🛌 ENTERING SLEEP MODE");
    } else {
        LOG_INF("⏰ WAKING UP FROM SLEEP");
    }
}

static int peripheral_deep_sleep_enter(void) {
    if (peripheral_in_deep_sleep) {
        LOG_DBG("Peripheral already in deep sleep");
        return 0;
    }
    
    LOG_INF("Peripheral entering deep sleep mode with kscan disabled");
    peripheral_in_deep_sleep = true;
    indicate_sleep_state(true); // 进入睡眠时指示
    
    // 1. RGB指示进入睡眠状态（红色闪烁3次）
#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    // 先保存当前RGB状态，然后红色闪烁指示
    for (int i = 0; i < 3; i++) {
        zmk_rgb_underglow_set_hsb(0, 100, 50);  // 红色
        k_msleep(200);
        zmk_rgb_underglow_off();
        k_msleep(200);
    }
    LOG_DBG("RGB sleep indication complete");
#endif
    
    // 2. 关闭显示（如果有）
#ifdef CONFIG_ZMK_DISPLAY
    // 强制设置活动状态为空闲，触发显示关闭
    struct zmk_activity_state_changed idle_event = {
        .state = ZMK_ACTIVITY_IDLE
    };
    raise_zmk_activity_state_changed(idle_event);
    LOG_DBG("Display turned off");
#endif
    
    // 3. 关闭键盘背光
#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT)
    zmk_backlight_off();
    LOG_DBG("Backlight turned off");
#endif
    
    // 4. 获取并关闭kscan以进一步降低功耗
    int selected_index = zmk_physical_layouts_get_selected();
    if (selected_index >= 0) {
        struct zmk_physical_layout const *const *layouts;
        size_t layouts_count = zmk_physical_layouts_get_list(&layouts);
        
        if (selected_index < layouts_count && layouts[selected_index]->kscan) {
            const struct device *kscan = layouts[selected_index]->kscan;
            
            // 禁用kscan回调
            int err = kscan_disable_callback(kscan);
            if (err) {
                LOG_ERR("Failed to disable kscan callback: %d", err);
            } else {
                LOG_DBG("Kscan callback disabled");
            }
            
            // 通过电源管理进入低功耗模式
#if IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)
            err = pm_device_runtime_put(kscan);
            if (err) {
                LOG_WRN("Failed to put kscan device to low power: %d", err);
            } else {
                LOG_DBG("Kscan device in low power mode");
            }
#elif IS_ENABLED(CONFIG_PM_DEVICE)
            err = pm_device_action_run(kscan, PM_DEVICE_ACTION_SUSPEND);
            if (err) {
                LOG_WRN("Failed to suspend kscan device: %d", err);
            } else {
                LOG_DBG("Kscan device suspended");
            }
#endif
        }
    }
    
    // 5. 保持BLE连接和系统最小运行
    // 注意：不调用sys_poweroff()，保持最小系统运行以维持BLE
    
    LOG_INF("Peripheral deep sleep mode activated (kscan disabled, BLE maintained)");
    LOG_WRN("Note: Peripheral can only be woken by central device command");
    return 0;
}

static int peripheral_deep_sleep_exit(void) {
    if (!peripheral_in_deep_sleep) {
        LOG_DBG("Peripheral not in deep sleep");
        return 0;
    }
    
    LOG_INF("Peripheral exiting deep sleep mode, re-enabling kscan");
    peripheral_in_deep_sleep = false;
    indicate_sleep_state(false); // 退出睡眠时指示
    
    // 1. RGB指示唤醒状态（绿色闪烁5次）
#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    for (int i = 0; i < 5; i++) {
        zmk_rgb_underglow_set_hsb(120, 100, 50);  // 绿色
        k_msleep(100);
        zmk_rgb_underglow_off();
        k_msleep(100);
    }
    LOG_DBG("RGB wake indication complete");
#endif
    
    // 2. 重新启用kscan
    int selected_index = zmk_physical_layouts_get_selected();
    if (selected_index >= 0) {
        struct zmk_physical_layout const *const *layouts;
        size_t layouts_count = zmk_physical_layouts_get_list(&layouts);
        
        if (selected_index < layouts_count && layouts[selected_index]->kscan) {
            const struct device *kscan = layouts[selected_index]->kscan;
            
            // 通过电源管理恢复设备
#if IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)
            int err = pm_device_runtime_get(kscan);
            if (err) {
                LOG_ERR("Failed to get kscan device from low power: %d", err);
            } else {
                LOG_DBG("Kscan device resumed from low power");
            }
#elif IS_ENABLED(CONFIG_PM_DEVICE)
            int err = pm_device_action_run(kscan, PM_DEVICE_ACTION_RESUME);
            if (err) {
                LOG_ERR("Failed to resume kscan device: %d", err);
            } else {
                LOG_DBG("Kscan device resumed");
            }
#endif
            
            // 重新启用kscan回调
            int err = kscan_enable_callback(kscan);
            if (err) {
                LOG_ERR("Failed to re-enable kscan callback: %d", err);
            } else {
                LOG_DBG("Kscan callback re-enabled");
            }
        }
    }
    
    // 3. 恢复为活跃状态，触发显示等外设重新开启
    struct zmk_activity_state_changed active_event = {
        .state = ZMK_ACTIVITY_ACTIVE
    };
    raise_zmk_activity_state_changed(active_event);
    
    // 4. 恢复键盘背光（如果之前是开启的）
#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT)
    zmk_backlight_on();
    LOG_DBG("Backlight restored");
#endif
    
    // 5. 恢复RGB到正常状态
#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW)
    zmk_rgb_underglow_on();
    LOG_DBG("RGB underglow restored");
#endif
    
    LOG_INF("Peripheral deep sleep mode deactivated, all functions restored");
    return 0;
}

static int behavior_peripheral_sleep_init(const struct device *dev) { 
    return 0; 
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case PDSLEEP_ON:
        LOG_INF("Peripheral deep sleep ON command received");
        peripheral_deep_sleep_enter();
        break;
    case PDSLEEP_OFF:
        LOG_INF("Peripheral deep sleep OFF command received");
        peripheral_deep_sleep_exit();
        break;
    case PDSLEEP_TOGGLE:
        LOG_INF("Peripheral deep sleep TOGGLE command received");
        if (peripheral_in_deep_sleep) {
            LOG_INF("Currently in deep sleep, exiting...");
            peripheral_deep_sleep_exit();
        } else {
            LOG_INF("Currently awake, entering deep sleep...");
            peripheral_deep_sleep_enter();
        }
        break;
    default:
        LOG_ERR("Unknown peripheral sleep command: %d", binding->param1);
        return -EINVAL;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_peripheral_sleep_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    // 修改为GLOBAL locality，让命令发送到所有设备（左手和右手）
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

// 简化实例定义，不需要配置结构
BEHAVIOR_DT_INST_DEFINE(0, behavior_peripheral_sleep_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_peripheral_sleep_driver_api); 
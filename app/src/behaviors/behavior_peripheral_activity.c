/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_peripheral_activity

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 外设活动控制命令
#define PERIPHERAL_FORCE_IDLE   1
#define PERIPHERAL_FORCE_ACTIVE 0

static bool peripheral_forced_idle = false;

#define IS_SPLIT_PERIPHERAL                                                                        \
    (IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

static int peripheral_force_idle_state(bool force_idle) {
    if (!IS_SPLIT_PERIPHERAL) {
        LOG_DBG("Not a peripheral, ignoring activity control");
        return 0;
    }
    
    peripheral_forced_idle = force_idle;
    
    if (force_idle) {
        LOG_INF("Peripheral forced into idle state");
        // 强制触发空闲状态变化事件
        struct zmk_activity_state_changed event = {
            .state = ZMK_ACTIVITY_IDLE
        };
        raise_zmk_activity_state_changed(event);
    } else {
        LOG_INF("Peripheral released from forced idle");
        // 强制触发活跃状态变化事件
        struct zmk_activity_state_changed event = {
            .state = ZMK_ACTIVITY_ACTIVE
        };
        raise_zmk_activity_state_changed(event);
    }
    
    return 0;
}

// 拦截活动状态变化，在强制空闲时阻止自动恢复
static int peripheral_activity_listener(const zmk_event_t *eh) {
    if (!IS_SPLIT_PERIPHERAL || !peripheral_forced_idle) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    const struct zmk_activity_state_changed *activity_ev;
    if ((activity_ev = as_zmk_activity_state_changed(eh)) != NULL) {
        if (activity_ev->state == ZMK_ACTIVITY_ACTIVE) {
            LOG_DBG("Blocking auto-active while in forced idle");
            return ZMK_EV_EVENT_HANDLED;  // 阻止自动恢复到活跃状态
        }
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

static int behavior_peripheral_activity_init(const struct device *dev) {
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case PERIPHERAL_FORCE_IDLE:
        return peripheral_force_idle_state(true);
    case PERIPHERAL_FORCE_ACTIVE:
        return peripheral_force_idle_state(false);
    default:
        LOG_ERR("Unknown peripheral activity command: %d", binding->param1);
        return -ENOTSUP;
    }
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_peripheral_activity_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,  // 只影响事件源
};

// 注册活动状态监听器
ZMK_LISTENER(peripheral_activity, peripheral_activity_listener);
ZMK_SUBSCRIPTION(peripheral_activity, zmk_activity_state_changed);

BEHAVIOR_DT_INST_DEFINE(0, behavior_peripheral_activity_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, 
                        &behavior_peripheral_activity_driver_api); 
/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/sensor_event.h>

#include <zmk/pm.h>

#include <zmk/activity.h>

// 智能睡眠相关声明
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP)
int zmk_smart_sleep_central(void);
int zmk_smart_sleep_peripheral(void);
int zmk_smart_sleep_standalone(void);
#endif

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
#include <zmk/usb.h>
#endif

bool is_usb_power_present(void) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    return zmk_usb_is_powered();
#else
    return false;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
}

static enum zmk_activity_state activity_state;

static uint32_t activity_last_uptime;

#define MAX_IDLE_MS CONFIG_ZMK_IDLE_TIMEOUT

#if IS_ENABLED(CONFIG_ZMK_SLEEP)
#define MAX_SLEEP_MS CONFIG_ZMK_IDLE_SLEEP_TIMEOUT
#endif

int raise_event(void) {
    return raise_zmk_activity_state_changed(
        (struct zmk_activity_state_changed){.state = activity_state});
}

int set_state(enum zmk_activity_state state) {
    if (activity_state == state)
        return 0;

    activity_state = state;
    return raise_event();
}

enum zmk_activity_state zmk_activity_get_state(void) { return activity_state; }

// 新增：强制设置状态的接口
int zmk_activity_set_state(enum zmk_activity_state state) {
    return set_state(state);
}

// 新增：判断是否为睡眠状态
bool zmk_activity_is_sleep_state(enum zmk_activity_state state) {
    return (state == ZMK_ACTIVITY_SLEEP || 
            state == ZMK_ACTIVITY_REMOTE_SLEEP || 
            state == ZMK_ACTIVITY_MANAGED_SLEEP);
}

int activity_event_listener(const zmk_event_t *eh) {
    activity_last_uptime = k_uptime_get();

    return set_state(ZMK_ACTIVITY_ACTIVE);
}

void activity_work_handler(struct k_work *work) {
    int32_t current = k_uptime_get();
    int32_t inactive_time = current - activity_last_uptime;
    
#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP)
    // 如果当前是远程控制状态，不自动进入睡眠
    if (activity_state == ZMK_ACTIVITY_REMOTE_SLEEP || 
        activity_state == ZMK_ACTIVITY_MANAGED_SLEEP) {
        return;
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_SLEEP)
    if (inactive_time > MAX_SLEEP_MS && !is_usb_power_present()) {
        // Put devices in suspend power mode before sleeping
        set_state(ZMK_ACTIVITY_SLEEP);

#if IS_ENABLED(CONFIG_ZMK_SMART_SLEEP)
        // 智能睡眠：根据设备角色选择不同策略
        if (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)) {
            if (zmk_smart_sleep_central() < 0) {
                // 如果智能睡眠失败，回退到传统睡眠
                goto traditional_sleep;
            }
        } else if (IS_ENABLED(CONFIG_ZMK_SPLIT)) {
            if (zmk_smart_sleep_peripheral() < 0) {
                goto traditional_sleep;
            }
        } else {
            if (zmk_smart_sleep_standalone() < 0) {
                goto traditional_sleep;
            }
        }
        return;
        
traditional_sleep:
#endif
        if (zmk_pm_suspend_devices() < 0) {
            LOG_ERR("Failed to suspend all the devices");
            zmk_pm_resume_devices();
            return;
        }

        sys_poweroff();
    } else
#endif /* IS_ENABLED(CONFIG_ZMK_SLEEP) */
        if (inactive_time > MAX_IDLE_MS) {
            set_state(ZMK_ACTIVITY_IDLE);
        }
}

K_WORK_DEFINE(activity_work, activity_work_handler);

void activity_expiry_function(struct k_timer *_timer) { k_work_submit(&activity_work); }

K_TIMER_DEFINE(activity_timer, activity_expiry_function, NULL);

static int activity_init(void) {
    activity_last_uptime = k_uptime_get();

    k_timer_start(&activity_timer, K_SECONDS(1), K_SECONDS(1));
    return 0;
}

ZMK_LISTENER(activity, activity_event_listener);
ZMK_SUBSCRIPTION(activity, zmk_position_state_changed);
ZMK_SUBSCRIPTION(activity, zmk_sensor_event);

SYS_INIT(activity_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

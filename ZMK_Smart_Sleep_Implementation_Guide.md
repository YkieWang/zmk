# ZMK智能深度睡眠方案实现说明

## 实现概述

本实现完成了ZMK智能深度睡眠方案的核心代码，包括：

### 核心模块

1. **活动状态扩展** (`app/include/zmk/activity.h`, `app/src/activity.c`)
   - 新增 `ZMK_ACTIVITY_REMOTE_SLEEP` 和 `ZMK_ACTIVITY_MANAGED_SLEEP` 状态
   - 提供状态设置和查询接口
   - 集成智能睡眠策略选择

2. **智能睡眠核心模块** (`app/src/smart_sleep.c`, `app/include/zmk/smart_sleep.h`)
   - 实现中央端、外设端和独立键盘的智能睡眠逻辑
   - 提供蓝牙低功耗参数管理
   - 包含调试和监控功能

3. **远程睡眠控制行为** (`app/src/behaviors/behavior_remote_sleep.c`)
   - 实现 `&rsleep` 行为，支持进入、退出和切换睡眠状态
   - 支持配置是否影响外设
   - 提供延迟执行功能

4. **分体键盘通信协议扩展**
   - 扩展服务定义 (`app/include/zmk/split/bluetooth/service.h`)
   - 新增UUID定义 (`app/include/zmk/split/bluetooth/uuid.h`)
   - 实现协调模块 (`app/src/split/smart_sleep_coordination.c`)

### 配置系统

1. **Kconfig配置** (`app/Kconfig`, `app/Kconfig.behaviors`)
   - `CONFIG_ZMK_SMART_SLEEP`: 启用智能睡眠功能
   - `CONFIG_ZMK_SMART_SLEEP_BT_KEEPALIVE`: 保持蓝牙连接
   - `CONFIG_ZMK_REMOTE_SLEEP_BEHAVIOR`: 启用远程睡眠行为
   - `CONFIG_ZMK_SPLIT_SLEEP_MGMT`: 分体键盘睡眠管理
   - 相关蓝牙参数配置选项

2. **设备树配置**
   - 行为绑定定义 (`app/dts/bindings/behaviors/zmk,behavior-remote-sleep.yaml`)
   - 默认行为实例 (`app/dts/behaviors/remote_sleep.dtsi`)
   - 集成到主行为文件 (`app/dts/behaviors.dtsi`)

3. **构建配置** (`app/CMakeLists.txt`)
   - 智能睡眠模块编译支持
   - 远程睡眠行为编译支持
   - 分体键盘协调模块支持

## 使用方法

### 基本配置

在`prj.conf`中启用功能：
```
CONFIG_ZMK_SMART_SLEEP=y
CONFIG_ZMK_SMART_SLEEP_BT_KEEPALIVE=y
CONFIG_ZMK_REMOTE_SLEEP_BEHAVIOR=y
CONFIG_ZMK_SPLIT_SLEEP_MGMT=y  # 仅分体键盘
```

### 键盘映射示例

参考 `example_keymap.keymap` 文件，包含：
- 基本远程睡眠控制 (`&rsleep 2`)
- 专门的电源管理层
- 自定义睡眠行为示例

## 关键特性

### 1. 渐进式睡眠管理
- **Active** → **Idle** → **Smart Sleep** → **Deep Sleep**
- 保持最小蓝牙连接的智能睡眠模式
- 根据设备角色自动选择睡眠策略

### 2. 分体键盘统一控制
- 中央端可控制外设端睡眠和唤醒
- 新增蓝牙GATT特征用于睡眠管理通信
- 支持多种睡眠模式：正常、托管、深度睡眠

### 3. 灵活的行为配置
- `&rsleep 0`：进入远程睡眠
- `&rsleep 1`：退出远程睡眠  
- `&rsleep 2`：切换睡眠状态
- 支持自定义行为实例

### 4. 功耗优化
- 蓝牙低功耗参数配置
- 按键扫描暂停
- CPU频率降级
- 设备选择性挂起

这个实现为ZMK提供了完整的智能深度睡眠解决方案，实现了设计文档中描述的所有核心功能。 
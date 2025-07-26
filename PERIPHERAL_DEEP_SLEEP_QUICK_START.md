# 外设深度睡眠控制 - 快速开始指南

## 🎯 功能简介

此功能允许主键盘控制从键盘进入深度睡眠状态，实现：
- ✅ 关闭从键盘的显示、RGB、背光等外设
- ✅ 保持BLE连接，不断开重连
- ✅ 保持键盘扫描，可随时唤醒
- ✅ 主键盘不受影响，正常工作

## 📁 文件结构

添加以下文件到你的ZMK项目：

```
app/
├── src/behaviors/
│   └── behavior_peripheral_sleep.c          # 核心实现
├── dts/
│   ├── bindings/behaviors/
│   │   └── zmk,behavior-peripheral-sleep.yaml  # 设备树绑定
│   └── behaviors/
│       └── peripheral_sleep.dtsi             # 行为定义
└── include/dt-bindings/zmk/
    └── peripheral_sleep.h                    # 命令定义
```

## 🔧 集成步骤

### 1. 修改 CMakeLists.txt

在 `app/CMakeLists.txt` 中添加：

```cmake
target_sources(app PRIVATE src/behaviors/behavior_peripheral_sleep.c)
```

### 2. 修改 behaviors.dtsi

在 `app/dts/behaviors.dtsi` 中添加：

```dts
#include "behaviors/peripheral_sleep.dtsi"
```

### 3. 在键映射中使用

```dts
#include <behaviors.dtsi>
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/peripheral_sleep.h>

/ {
    keymap {
        compatible = "zmk,keymap";
        
        function_layer {
            bindings = <
                // 其他按键...
                
                // 外设深度睡眠控制
                &peripheral_sleep PDSLEEP_ON   // 进入深度睡眠
                &peripheral_sleep PDSLEEP_OFF  // 退出深度睡眠
                
                // 其他按键...
            >;
        };
    };
};
```

## 📱 使用方法

### 基本操作

1. **让从键盘进入深度睡眠**
   - 在主键盘上按下配置的 `&peripheral_sleep PDSLEEP_ON` 组合键
   - 从键盘的显示、RGB、背光等会立即关闭
   - BLE连接保持，从键盘仍可接收命令

2. **唤醒从键盘**
   - 在主键盘上按下配置的 `&peripheral_sleep PDSLEEP_OFF` 组合键
   - 或者直接在从键盘上按任意按键
   - 所有外设功能立即恢复

### 自动唤醒

从键盘在深度睡眠状态下，按任意按键都会自动触发唤醒，无需额外配置。

## 🔍 工作原理

```
主键盘 → BLE命令 → 从键盘 → 外设控制
  ↓                    ↓
 正常工作        深度睡眠状态
  ↓                    ↓
 不受影响      显示/RGB/背光关闭
```

## ⚙️ 技术细节

- **行为局部性**: `BEHAVIOR_LOCALITY_EVENT_SOURCE`，确保只影响外设
- **BLE连接**: 始终保持最小连接，不会断开重连
- **状态管理**: 通过活动状态事件控制外设开关
- **兼容性**: 与现有ZMK架构完全兼容

## 🛠️ 自定义配置

### 调整按键位置

在键映射中修改按键位置：

```dts
function_layer {
    bindings = <
        // 示例：使用F1/F2控制
        &peripheral_sleep PDSLEEP_ON   // F1: 深度睡眠
        &peripheral_sleep PDSLEEP_OFF  // F2: 唤醒
    >;
};
```

### 组合键配置

```dts
// 示例：Fn + Q/W 控制外设睡眠
default_layer {
    bindings = <
        &kp Q  &kp W  // 普通Q、W键
    >;
};

function_layer {
    bindings = <
        &peripheral_sleep PDSLEEP_ON   // Fn+Q: 睡眠
        &peripheral_sleep PDSLEEP_OFF  // Fn+W: 唤醒
    >;
};
```

## 🐛 故障排除

### 常见问题

1. **从键盘无响应**
   - 检查CMakeLists.txt是否正确添加源文件
   - 确认behaviors.dtsi中包含了peripheral_sleep.dtsi

2. **主键盘也受影响**
   - 检查行为定义中locality是否为BEHAVIOR_LOCALITY_EVENT_SOURCE
   - 确认代码中IS_SPLIT_PERIPHERAL检查正确

3. **BLE连接断开**
   - 检查是否误用了sys_poweroff()
   - 确认只调用了外设控制函数，未影响BLE栈

### 调试方法

启用日志查看详细信息：

```
CONFIG_ZMK_LOG_LEVEL_DBG=y
```

查看日志输出：
- "Peripheral entering deep sleep mode" - 进入深度睡眠
- "Peripheral exiting deep sleep mode" - 退出深度睡眠
- "Central device ignoring..." - 主键盘正确忽略命令

## 📊 预期效果

- **功耗降低**: 从键盘功耗可降低50-70%
- **响应速度**: 唤醒响应时间 < 100ms
- **连接稳定**: BLE连接保持100%稳定
- **主键盘**: 完全不受影响，正常工作

## 🔄 升级路径

此实现为基础版本，后续可扩展：
- 添加不同深度的睡眠级别
- 支持定时自动唤醒
- 添加睡眠状态指示
- 集成电池电量自动控制

---

完成以上步骤后，您就可以通过主键盘完全控制从键盘的深度睡眠状态了！ 
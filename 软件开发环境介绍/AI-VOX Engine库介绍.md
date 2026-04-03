# AI VOX Engine - AI语音交互引擎库（Arduino小智AI）

## 1. 概述

AI VOX Engine 是一款面向Arduino平台的轻量级AI语音交互开发库，提供从音频采集到云端交互到音频播放的全链路解决方案。本库封装了语音交互的核心流程，涵盖音频采集-音频编码-云端通信-音频解码-音频播放等环节，开发者通过简洁的API即可快速实现语音交互功能，同时支持丰富的状态监控和交互事件反馈。

## 2. 特性

- 基础语音聊天功能
- MCP外设查询与控制（IOT功能已废弃）
- WebSocket V1 版本（MQTT + UDP已废弃）
- 支持自定义OTA服务器、WebSocket服务器以及Headers
- 支持按键唤醒、语音唤醒和语音打断（唤醒词：“你好小智”）
- 支持Arduino IDE、PlatformIO编译环境

## 3. 示例介绍

| 示例路径 | 核心功能 | 硬件相关说明 |
| --- | --- | --- |
| ai-vox3 | 基础语音聊天、按键唤醒/语音唤醒/打断（“你好小智”）、LCD屏幕显示、语音外设控制（音量0~100、屏幕亮度0~100、板载RGB LED(WS2812)开关） | 仅支持AI-VOX3开发板 |

> 示例的配置、编译和运行等详细信息见下文「示例详情」章节

## 4. 下载库

下载链接：[AI VOX Engine 库下载和版本说明](https://dcnmu33qx4fc.feishu.cn/docx/OvkBd63NUor3sKx1271cHxjPnsL)

> 注意！请下载最新版本！

## 5. 安装库

安装路径：Arduino IDE → 项目 → 导入库 → 添加.zip库 → 选择ai_vox-x.x.x.zip → 点击打开 → 等待安装完成

## 6. 扩展示例

AI VOX Engine库的扩展示例展示了通过MCP工具实现语音控制外设（如WS2812B RGB灯环、舵机、电机等）的具体方法，参考文档：[AI VOX Engine MCP 编程指南](https://dcnmu33qx4fc.feishu.cn/docx/EcXxdXiuJomKDyxjSoIc6af0nig)

针对Arduino编程，我们专门进行了优化，简化了编写MCP工具的难度，提供了标准的接口，简化了代码逻辑，使开发人员更专注于MCP功能，请参考路径文档：[AI小智-智能体-MCP介绍/04_AI-VOX3 Engine MCP API介绍]，后续所有示例都基于优化版MCP API进行设计开发。

## 7. 安装ESP32开发板管理

1. 打开开发板管理器：Arduino IDE → 工具 → 开发板 → 开发板管理器
2. 搜索“ESP32”，选择“esp32 by espressif Systems”，安装版本3.2.0-cn（经验证稳定，低版本可能出现头文件缺失等错误）
3. 点击安装/更新按钮，等待安装完成

> 注意！3.2.0是经过验证的版本，低版本可能无法编译通过，会有头文件缺失的提示或其他错误提示
>
> 有时候安装会失败，请多试几次

## 8. 示例详情

### 8.1 ai-vox3

#### 8.1.1 功能

- 基础语音聊天功能
- 按键唤醒、语音唤醒和打断（唤醒词：“你好小智”）
- LCD屏幕显示
- 语音外设控制（MCP功能）：音量调节（0~100）、屏幕亮度调节（0~100）、板载RGB LED(WS2812)开关（亮白色/关闭）

> 仅支持AI-VOX3开发板

#### 8.1.2 安装依赖库

1. 打开管理库：Arduino IDE → 项目 → 导入库 → 管理库
2. 搜索并安装“lvgl by kisvegabor”，版本9.2.2

#### 8.1.3 打开示例

路径：Arduino IDE → 文件 → 示例 → AI VOX → ai-vox3 → ai-vox3

#### 8.1.4 配置（Arduino IDE菜单栏）

1. 关闭编译警告：文件 → 首选项 → 编译器警告 → 设置为“无” → 确定
2. 开发板类型：ESP32S3 Dev Module
3. Flash Size：16M
4. Partition Scheme：Custom
5. PSRAM：OPI PSRAM

#### 8.1.5 编译并运行

- 运行结果：LCD屏幕显示交互信息，按下Boot按键或说“你好小智”唤醒对话，需按提示完成设备激活（如有激活信息）；说话过程中可通过“你好小智”打断
- 外设控制：
  - 语音指令控制板载RGB LED(WS2812)开关（亮白色/关闭）
  - 语音指令调节喇叭音量（0~100）
  - 语音指令调节屏幕亮度（0~100）

## 9. 源码

- GitHub仓库：<https://github.com/nulllaborg/ai_vox>
- 版本发布：<https://github.com/nulllaborg/ai_vox/releases>
- 编码规范：遵循Google C++ 编程规范（<https://google.github.io/styleguide/cppguide.html）>
- 许可证：MIT License

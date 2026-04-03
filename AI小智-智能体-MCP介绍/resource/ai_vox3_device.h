#pragma once

/**
 * @file ai_vox3_device.h
 * @brief AI VOX3 设备初始化和事件处理接口
 * 
 * 这个头文件定义了设备的初始化和主循环处理函数
 */

/**
 * @brief 初始化AI VOX3设备的所有硬件和软件组件
 * 
 * 包括:
 * - I2C总线初始化
 * - LED驱动初始化
 * - 显示屏初始化
 * - 音频设备(ES8311)初始化
 * - 按钮初始化
 * - WiFi配置
 * - MCP工具注册
 * - AI引擎初始化和启动
 */
void InitializeDevice();

/**
 * @brief 处理主循环中的事件
 * 
 * 包括:
 * - 处理观察者事件(文本接收、激活、状态变更等)
 * - 处理MCP工具调用(音量控制、LED控制)
 * - 更新显示屏
 * - 可选的内存信息打印(当PRINT_HEAP_INFO_INTERVAL宏定义时)
 */
void ProcessMainLoop();

// ========== User MCP registration helpers ==========
#include <functional>
#include <string>

// 前向声明，避免在头文件中强制包含 ai_vox 头
namespace ai_vox {
class Engine;
struct McpToolCallEvent;
}

// 注册一个用户侧的 "MCP 工具声明器"。传入的 lambda 会在 InitMcpTools() 内被调用，
// 并接收一个 ai_vox::Engine&，用户可在该 lambda 中调用 engine.AddMcpTool(...) 来注册工具（包含参数 schema）。
void RegisterUserMcpDeclarator(const std::function<void(ai_vox::Engine&)>& declarator);

// 注册一个用户侧的 MCP 调用处理器。当收到与 name 匹配的 MCP 调用时，
// ai_vox3 会调用 handler，handler 负责调用 engine.SendMcpCallResponse/SendMcpCallError（或在 handler 内部完成所需处理）。
void RegisterUserMcpHandler(const std::string& name, const std::function<void(const ai_vox::McpToolCallEvent&)>& handler);

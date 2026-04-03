#include <Arduino.h>
#include "ai_vox3_device.h"
#include "ai_vox_engine.h"

// ========== LED 控制 MCP 工具 ==========

/**
 * @brief MCP工具 - LED 开启
 *
 * 该函数注册一个名为 "user.led_on" 的MCP工具，用于开启用户LED
 */
void mcp_tool_led_on()
{
    // 注册工具声明器，定义工具的名称、描述和参数
    RegisterUserMcpDeclarator([](ai_vox::Engine &engine)
                              {
                                  engine.AddMcpTool("user.led_on",
                                                    "Turn on user LED",
                                                    {}); // 无参数
                              });

    // 注册工具处理器
    RegisterUserMcpHandler("user.led_on", [](const ai_vox::McpToolCallEvent &ev)
                           {
        printf("LED on\n");
        digitalWrite(1, HIGH);
        ai_vox::Engine::GetInstance().SendMcpCallResponse(ev.id, true); });
}

/**
 * @brief MCP工具 - LED 关闭
 *
 * 该函数注册一个名为 "user.led_off" 的MCP工具，用于关闭用户LED
 */
void mcp_tool_led_off()
{
    // 注册工具声明器，定义工具的名称、描述和参数
    RegisterUserMcpDeclarator([](ai_vox::Engine &engine)
                              {
                                  engine.AddMcpTool("user.led_off",
                                                    "Turn off user LED",
                                                    {}); // 无参数
                              });

    // 注册工具处理器
    RegisterUserMcpHandler("user.led_off", [](const ai_vox::McpToolCallEvent &ev)
                           {
        printf("LED off\n");
        digitalWrite(1, LOW);
        ai_vox::Engine::GetInstance().SendMcpCallResponse(ev.id, true); });
}

// ========== Setup 和 Loop ==========

void setup()
{
    // 初始化 LED 开启工具
    mcp_tool_led_on();

    // 初始化 LED 关闭工具
    mcp_tool_led_off();

    // 初始化设备服务
    InitializeDevice();
}

void loop()
{
    // 处理设备服务主循环事件
    ProcessMainLoop();
}

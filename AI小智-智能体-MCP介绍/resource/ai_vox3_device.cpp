#include "ai_vox3_device.h"

#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "ai_vox_engine.h"
#include "audio_device/audio_device_es8311.h"
#include "components/espressif/button/button_gpio.h"
#include "components/espressif/button/iot_button.h"
#include "components/espressif/esp_audio_codec/esp_audio_simple_dec.h"
#include "components/espressif/esp_audio_codec/esp_mp3_dec.h"
#include "components/wifi_configurator/wifi_configurator.h"
#include "display.h"
#include "led_strip.h"
#include "network_config_mode_mp3.h"
#include "network_connected_mp3.h"
#include "notification_0_mp3.h"
#include <map>
#include <vector>
#include <functional>
#include <string>

#include "wifi_config.h"

namespace {

#ifndef ARDUINO_ESP32S3_DEV
#error "This example only supports ESP32S3-Dev board."
#endif

#ifndef CONFIG_SPIRAM_MODE_OCT
#error "This example requires PSRAM to OPI PSRAM. Please enable it in Arduino IDE."
#endif

// ==================== 配置常量 ====================

/**
 *  SC_TYPE_ESPTOUCH            protocol: ESPTouch，支持ESPTouch APP配网
 *  SC_TYPE_AIRKISS,            protocol: AirKiss，支持小程序配网
 *  SC_TYPE_ESPTOUCH_AIRKISS,   protocol: ESPTouch and AirKiss ，支持ESPTouch APP配网，支持小程序配网
 *  SC_TYPE_ESPTOUCH_V2,        protocol: ESPTouch v2, 支持ESPTouch APP配网
 */
constexpr smartconfig_type_t kSmartConfigType = SC_TYPE_ESPTOUCH_AIRKISS;

constexpr auto kButtonBoot = GPIO_NUM_0;
constexpr auto kLcdBacklightPin = GPIO_NUM_16;

// SPI LCD引脚配置
constexpr auto kSt7789Sda = GPIO_NUM_21;   // SPI MOSI
constexpr auto kSt7789Scl = GPIO_NUM_17;   // SPI SCLK
constexpr auto kSt7789Csx = GPIO_NUM_15;   // SPI CS
constexpr auto kSt7789Dcx = GPIO_NUM_14;   // SPI DC

constexpr auto kWs2812LedPin = GPIO_NUM_41;

// ES8311音频芯片引脚配置
constexpr auto kEs8311Mclk = GPIO_NUM_11;
constexpr auto kEs8311Sclk = GPIO_NUM_10;
constexpr auto kEs8311Lrck = GPIO_NUM_8;
constexpr auto kEs8311Dsdin = GPIO_NUM_7;
constexpr auto kEs8311Asdout = GPIO_NUM_9;

// I2C引脚配置
constexpr auto kI2cScl = GPIO_NUM_12;
constexpr auto kI2cSda = GPIO_NUM_13;
constexpr auto kI2CPort = I2C_NUM_1;

// 显示屏配置
constexpr auto kDisplaySpiMode = 0;
constexpr uint32_t kDisplayWidth = 240;
constexpr uint32_t kDisplayHeight = 240;
constexpr bool kDisplayMirrorX = false;
constexpr bool kDisplayMirrorY = false;
constexpr bool kDisplayInvertColor = true;
constexpr bool kDisplaySwapXY = false;
constexpr auto kDisplayRgbElementOrder = LCD_RGB_ELEMENT_ORDER_RGB;

// 音频设备配置
constexpr uint8_t kEs8311I2cAddress = 0x30;
constexpr uint32_t kAudioSampleRate = 16000;

// ==================== 全局变量 ====================

i2c_master_bus_handle_t g_i2c_master_bus_handle = nullptr;
std::shared_ptr<ai_vox::AudioDeviceEs8311> g_audio_device_es8311;
std::unique_ptr<Display> g_display;
auto g_observer = std::make_shared<ai_vox::Observer>();
button_handle_t g_button_boot_handle = nullptr;

bool g_led_on = false;
led_strip_handle_t g_led_strip;

// ========== 用户注册的 MCP 回调存储 ==========
std::map<std::string, std::function<void(const ai_vox::McpToolCallEvent&)>> g_user_mcp_handlers;
std::vector<std::function<void(ai_vox::Engine&)>> g_user_mcp_declarators;

// ==================== 硬件初始化函数 ====================

void InitI2cBus() {
  const i2c_master_bus_config_t i2c_master_bus_config = {
      .i2c_port = kI2CPort,
      .sda_io_num = kI2cSda,
      .scl_io_num = kI2cScl,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags = {
          .enable_internal_pullup = 1,
          .allow_pd = 0,
      },
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_bus_config, &g_i2c_master_bus_handle));
  printf("g_i2c_master_bus: %p\n", g_i2c_master_bus_handle);
}

void InitEs8311() {
  g_audio_device_es8311 = std::make_shared<ai_vox::AudioDeviceEs8311>(
      g_i2c_master_bus_handle,
      kEs8311I2cAddress,
      kI2CPort,
      kAudioSampleRate,
      kEs8311Mclk,
      kEs8311Sclk,
      kEs8311Lrck,
      kEs8311Asdout,
      kEs8311Dsdin);
}

void InitDisplay() {
  pinMode(kLcdBacklightPin, OUTPUT);
  analogWrite(kLcdBacklightPin, 255);

  spi_bus_config_t buscfg{
      .mosi_io_num = kSt7789Sda,
      .miso_io_num = GPIO_NUM_NC,
      .sclk_io_num = kSt7789Scl,
      .quadwp_io_num = GPIO_NUM_NC,
      .quadhd_io_num = GPIO_NUM_NC,
      .data4_io_num = GPIO_NUM_NC,
      .data5_io_num = GPIO_NUM_NC,
      .data6_io_num = GPIO_NUM_NC,
      .data7_io_num = GPIO_NUM_NC,
      .data_io_default_level = false,
      .max_transfer_sz = kDisplayWidth * kDisplayHeight * sizeof(uint16_t),
      .flags = 0,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
      .intr_flags = 0,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t panel_io = nullptr;
  esp_lcd_panel_handle_t panel = nullptr;

  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.cs_gpio_num = kSt7789Csx;
  io_config.dc_gpio_num = kSt7789Dcx;
  io_config.spi_mode = kDisplaySpiMode;
  io_config.pclk_hz = 40 * 1000 * 1000;
  io_config.trans_queue_depth = 10;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = -1;
  panel_config.rgb_ele_order = kDisplayRgbElementOrder;
  panel_config.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_panel_invert_color(panel, kDisplayInvertColor);
  esp_lcd_panel_swap_xy(panel, kDisplaySwapXY);
  esp_lcd_panel_mirror(panel, kDisplayMirrorX, kDisplayMirrorY);

  g_display = std::make_unique<Display>(panel_io, panel, kDisplayWidth, kDisplayHeight, 0, 0, 
                                       kDisplayMirrorX, kDisplayMirrorY, kDisplaySwapXY);
  g_display->Start();
}

void InitLed() {
  led_strip_config_t strip_config = {
      .strip_gpio_num = kWs2812LedPin,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags = {
          .invert_out = false,
      }};

  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .mem_block_symbols = 0,
      .flags = {
          .with_dma = 0,
      }};
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip));
  ESP_ERROR_CHECK(led_strip_clear(g_led_strip));
}

void InitButton() {
  printf("init button\n");
  const button_config_t btn_cfg = {
      .long_press_time = 1000,
      .short_press_time = 50,
  };

  const button_gpio_config_t gpio_cfg = {
      .gpio_num = kButtonBoot,
      .active_level = 0,
      .enable_power_save = false,
      .disable_pull = false,
  };

  ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &g_button_boot_handle));
}

// ==================== 音频播放函数 ====================

void PlayMp3(const uint8_t* data, size_t size) {
  auto ret = esp_mp3_dec_register();
  if (ret != ESP_AUDIO_ERR_OK) {
    printf("Failed to register mp3 decoder: %d\n", ret);
    abort();
  }

  esp_audio_simple_dec_handle_t decoder = nullptr;
  esp_audio_simple_dec_cfg_t audio_dec_cfg{
      .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
      .dec_cfg = nullptr,
      .cfg_size = 0,
  };
  ret = esp_audio_simple_dec_open(&audio_dec_cfg, &decoder);
  if (ret != ESP_AUDIO_ERR_OK) {
    printf("Failed to open mp3 decoder: %d\n", ret);
    abort();
  }
  g_audio_device_es8311->OpenOutput(16000);

  esp_audio_simple_dec_raw_t raw = {
      .buffer = const_cast<uint8_t*>(data),
      .len = size,
      .eos = true,
      .consumed = 0,
      .frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE,
  };

  uint8_t* frame_data = (uint8_t*)malloc(4096);
  esp_audio_simple_dec_out_t out_frame = {
      .buffer = frame_data,
      .len = 4096,
      .needed_size = 0,
      .decoded_size = 0,
  };

  while (raw.len > 0) {
    const auto ret = esp_audio_simple_dec_process(decoder, &raw, &out_frame);
    if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
      out_frame.buffer = reinterpret_cast<uint8_t*>(realloc(out_frame.buffer, out_frame.needed_size));
      if (out_frame.buffer == nullptr) {
        break;
      }
      out_frame.len = out_frame.needed_size;
      continue;
    }

    if (ret != ESP_AUDIO_ERR_OK) {
      break;
    }

    g_audio_device_es8311->Write(reinterpret_cast<int16_t*>(out_frame.buffer), out_frame.decoded_size >> 1);
    raw.len -= raw.consumed;
    raw.buffer += raw.consumed;
  }

  free(frame_data);

  g_audio_device_es8311->CloseOutput();
  esp_audio_simple_dec_close(decoder);
  esp_audio_dec_unregister(ESP_AUDIO_TYPE_MP3);
}

// ==================== WiFi配置函数 ====================

void ConfigureWifi() {
  printf("configure wifi\n");
  auto wifi_configurator = std::make_unique<WifiConfigurator>(WiFi, kSmartConfigType);

  ESP_ERROR_CHECK(iot_button_register_cb(
      g_button_boot_handle,
      BUTTON_PRESS_DOWN,
      nullptr,
      [](void*, void* data) {
        printf("boot button pressed\n");
        static_cast<WifiConfigurator*>(data)->StartSmartConfig();
      },
      wifi_configurator.get()));

  g_display->ShowStatus("网络配置中");
  PlayMp3(kNotification0mp3, sizeof(kNotification0mp3));

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  printf("wifi config start with wifi: %s, %s\n", WIFI_SSID, WIFI_PASSWORD);
  wifi_configurator->Start(WIFI_SSID, WIFI_PASSWORD);
#else
  printf("wifi config start\n");
  wifi_configurator->Start();
#endif

  while (true) {
    const auto state = wifi_configurator->WaitStateChanged();
    if (state == WifiConfigurator::State::kConnecting) {
      printf("wifi connecting\n");
      g_display->ShowStatus("网络连接中");
    } else if (state == WifiConfigurator::State::kSmartConfiguring) {
      printf("wifi smart configuring\n");
      g_display->ShowStatus("配网模式");
      PlayMp3(kNetworkConfigModeMp3, sizeof(kNetworkConfigModeMp3));
    } else if (state == WifiConfigurator::State::kFinished) {
      break;
    }
  }

  iot_button_unregister_cb(g_button_boot_handle, BUTTON_PRESS_DOWN, nullptr);

  printf("wifi connected\n");
  printf("- mac address: %s\n", WiFi.macAddress().c_str());
  printf("- bssid:       %s\n", WiFi.BSSIDstr().c_str());
  printf("- ssid:        %s\n", WiFi.SSID().c_str());
  printf("- ip:          %s\n", WiFi.localIP().toString().c_str());
  printf("- gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
  printf("- subnet mask: %s\n", WiFi.subnetMask().toString().c_str());

  g_display->ShowStatus("网络已连接");
  PlayMp3(kNetworkConnectedMp3, sizeof(kNetworkConnectedMp3));
}

// ==================== MCP工具注册函数 ====================

void InitMcpTools() {
  auto& engine = ai_vox::Engine::GetInstance();
  
  engine.AddMcpTool("self.audio_speaker.set_volume",
                    "Set the volume of the audio speaker.",
                    {
                        {
                            "volume",
                            ai_vox::ParamSchema<int64_t>{
                                .default_value = std::nullopt,
                                .min = 0,
                                .max = 100,
                            },
                        },
                    });

  engine.AddMcpTool("self.audio_speaker.get_volume",
                    "Get the volume of the audio speaker.",
                    {});

//   engine.AddMcpTool("self.led.set",
//                     "Set the state of the LED, true for on, false for off.",
//                     {
//                         {
//                             "state",
//                             ai_vox::ParamSchema<bool>{
//                                 .default_value = std::nullopt,
//                             },
//                         },
//                     });

//   engine.AddMcpTool("self.led.get",
//                     "Get the state of the LED, true for on, false for off.",
//                     {});

  // 调用用户注册的 declarator，使用户可以在 declarator 中调用 engine.AddMcpTool(...)
  for (auto &declarator : g_user_mcp_declarators) {
    declarator(engine);
  }
}

// ==================== 内存信息打印函数 ====================

#ifdef PRINT_HEAP_INFO_INTERVAL
void PrintMemInfo() {
  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    printf("SPIRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size, total_size >> 10, free_size, free_size >> 10, min_free_size, min_free_size >> 10);
  }

  if (heap_caps_get_total_size(MALLOC_CAP_INTERNAL) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    printf("IRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size, total_size >> 10, free_size, free_size >> 10, min_free_size, min_free_size >> 10);
  }

  if (heap_caps_get_total_size(MALLOC_CAP_DEFAULT) > 0) {
    const auto total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    const auto free_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    const auto min_free_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("DRAM total size: %zu B (%zu KB), free size: %zu B (%zu KB), minimum free size: %zu B (%zu KB)\n",
           total_size, total_size >> 10, free_size, free_size >> 10, min_free_size, min_free_size >> 10);
  }
}
#endif

// ==================== 事件处理函数 ====================

void HandleMcpToolCall(const ai_vox::McpToolCallEvent& event) {
  auto& engine = ai_vox::Engine::GetInstance();

  if ("self.audio_speaker.set_volume" == event.name) {
    const auto volume_ptr = event.param<int64_t>("volume");
    if (volume_ptr != nullptr) {
      printf("on mcp tool call: self.audio_speaker.set_volume, volume: %" PRId64 "\n", *volume_ptr);
      g_audio_device_es8311->set_volume(*volume_ptr);
      engine.SendMcpCallResponse(event.id, true);
    } else {
      engine.SendMcpCallError(event.id, "Missing valid argument: volume");
    }
  } else if ("self.audio_speaker.get_volume" == event.name) {
    const auto volume = g_audio_device_es8311->volume();
    printf("on mcp tool call: self.audio_speaker.get_volume, volume: %" PRIu16 "\n", volume);
    engine.SendMcpCallResponse(event.id, volume);
  } else if ("self.led.set" == event.name) {
    const auto state_ptr = event.param<bool>("state");
    if (state_ptr != nullptr) {
      printf("on mcp tool call: self.led.set, state: %d\n", *state_ptr);
      if (*state_ptr) {
        digitalWrite(1, HIGH);
      } else {
        digitalWrite(1, LOW);
      }
      ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));
      g_led_on = *state_ptr;
      engine.SendMcpCallResponse(event.id, true);
    } else {
      engine.SendMcpCallError(event.id, "Missing valid argument: state");
    }
  } else if ("self.led.get" == event.name) {
    printf("on mcp tool call: self.led.get, state: %d\n", digitalRead(1));
    engine.SendMcpCallResponse(event.id, digitalRead(1));
  }

  // 用户注册的 handler（用户在 main.cpp 中通过 RegisterUserMcpHandler 注册）
  auto it = g_user_mcp_handlers.find(event.name);
  if (it != g_user_mcp_handlers.end()) {
    it->second(event);
    return;
  }
}

}  // namespace

// ==================== 公共API实现 ====================

void InitializeDevice() {
  Serial.begin(115200);
  pinMode(1, OUTPUT);
  
  // 硬件初始化
  InitLed();
  InitI2cBus();
  InitDisplay();
  InitEs8311();
  InitButton();

  // 检查SPIRAM
  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) == 0) {
    g_display->SetChatMessage(Display::Role::kSystem, "No SPIRAM available, please check your board.");
    while (true) {
      printf("No SPIRAM available, please check your board.\n");
      delay(1000);
    }
  }

  // WiFi配置
  g_display->ShowStatus("初始化");
  ConfigureWifi();

  // AI引擎初始化
  InitMcpTools();

  auto& ai_vox_engine = ai_vox::Engine::GetInstance();
  ai_vox_engine.SetObserver(g_observer);
  ai_vox_engine.SetOtaUrl("https://api.tenclass.net/xiaozhi/ota/");
  ai_vox_engine.ConfigWebsocket("wss://api.tenclass.net/xiaozhi/v1/",
                                {
                                    {"Authorization", "Bearer test-token"},
                                });
  printf("engine starting\n");
  g_display->ShowStatus("AI引擎启动中");

  ai_vox_engine.Start(g_audio_device_es8311, g_audio_device_es8311);

  printf("engine started\n");

  // 注册按钮回调
  ESP_ERROR_CHECK(iot_button_register_cb(
      g_button_boot_handle,
      BUTTON_PRESS_DOWN,
      nullptr,
      [](void* button_handle, void* usr_data) {
        printf("boot button pressed\n");
        ai_vox::Engine::GetInstance().Advance();
      },
      nullptr));
}

void ProcessMainLoop() {
#ifdef PRINT_HEAP_INFO_INTERVAL
  static uint32_t s_print_heap_info_time = 0;
  if (s_print_heap_info_time == 0 || millis() - s_print_heap_info_time >= PRINT_HEAP_INFO_INTERVAL) {
    s_print_heap_info_time = millis();
    PrintMemInfo();
  }
#endif

  auto& engine = ai_vox::Engine::GetInstance();
  const auto events = g_observer->PopEvents();

  for (auto& event : events) {
    if (auto text_received_event = std::get_if<ai_vox::TextReceivedEvent>(&event)) {
      printf("on text received: %s\n", text_received_event->content.c_str());
    } 
    else if (auto activation_event = std::get_if<ai_vox::ActivationEvent>(&event)) {
      printf("activation code: %s, message: %s\n", activation_event->code.c_str(), activation_event->message.c_str());
      g_display->ShowStatus("激活设备");
      g_display->SetChatMessage(Display::Role::kSystem, activation_event->message);
    } 
    else if (auto state_changed_event = std::get_if<ai_vox::StateChangedEvent>(&event)) {
      switch (state_changed_event->new_state) {
        case ai_vox::ChatState::kIdle:
          printf("Idle\n");
          break;
        case ai_vox::ChatState::kInitted:
          printf("Initted\n");
          g_display->ShowStatus("初始化完成");
          break;
        case ai_vox::ChatState::kLoading:
          printf("Loading...\n");
          g_display->ShowStatus("加载协议中");
          break;
        case ai_vox::ChatState::kLoadingFailed:
          printf("Loading failed, please retry\n");
          g_display->ShowStatus("加载协议失败，请重试");
          break;
        case ai_vox::ChatState::kStandby:
          printf("Standby\n");
          g_display->ShowStatus("待命");
          break;
        case ai_vox::ChatState::kConnecting:
          printf("Connecting...\n");
          g_display->ShowStatus("连接中...");
          break;
        case ai_vox::ChatState::kListening:
          printf("Listening...\n");
          g_display->ShowStatus("聆听中");
          break;
        case ai_vox::ChatState::kSpeaking:
          printf("Speaking...\n");
          g_display->ShowStatus("说话中");
          break;
        default:
          break;
      }
    } 
    else if (auto emotion_event = std::get_if<ai_vox::EmotionEvent>(&event)) {
      printf("emotion: %s\n", emotion_event->emotion.c_str());
      g_display->SetEmotion(emotion_event->emotion);
    } 
    else if (auto chat_message_event = std::get_if<ai_vox::ChatMessageEvent>(&event)) {
      switch (chat_message_event->role) {
        case ai_vox::ChatRole::kAssistant:
          printf("role: assistant, content: %s\n", chat_message_event->content.c_str());
          g_display->SetChatMessage(Display::Role::kAssistant, chat_message_event->content);
          break;
        case ai_vox::ChatRole::kUser:
          printf("role: user, content: %s\n", chat_message_event->content.c_str());
          g_display->SetChatMessage(Display::Role::kUser, chat_message_event->content);
          break;
      }
    } 
    else if (auto mcp_tool_call_event = std::get_if<ai_vox::McpToolCallEvent>(&event)) {
      printf("on mcp tool call: %s\n", mcp_tool_call_event->ToString().c_str());
      HandleMcpToolCall(*mcp_tool_call_event);
    }
  }
}

// ========== 用户注册 API 实现 ==========
void RegisterUserMcpDeclarator(const std::function<void(ai_vox::Engine&)>& declarator) {
  g_user_mcp_declarators.push_back(declarator);
}

void RegisterUserMcpHandler(const std::string& name, const std::function<void(const ai_vox::McpToolCallEvent&)>& handler) {
  g_user_mcp_handlers[name] = handler;
}

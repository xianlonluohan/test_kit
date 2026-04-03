# Arduino IDE开发环境搭建

此文档旨在介绍Arduino IDE2.0的安装教程及AI-VOX3主板在Arduino IDE2.0平台的使用方法。

请点击 [ArduinoIDE下载链接](https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.7_Windows_64bit.msi) 下载最新IDE2.0版本并安装（链接提供的为Windows平台64位的2.3.7版本安装包）。

> **注意：** 安装路径中不能有中文，否则会有一些奇怪的问题。

1. 打开Ardunio IDE2，点击Arduino IDE菜单栏：【文件】-->【首选项】

*将<https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json> 这个网址复制到附加管理器地址*

![add_esp](picture/add_esp.png)

1. 菜单栏点击 【工具】->【开发板】->【开发板管理器】搜索esp32，然后安装官方板esp32包，版本选择3.2.0-cn版本，如下图所示：

![arduinoIDEESP32VersionInstall](picture/install_esp32_320.png)

安装完成后，打开IDE，先选择主板，并且更改开发板的部分配置，如下图所示：

![arduino_board_choice](picture/ai-vox_board_config.png)

具体的配置有：

* 关闭编译警告：文件 → 首选项 → 编译器警告 → 设置为“无” → 确定
* 开发板类型：ESP32S3 Dev Module
* Flash Size：16M
* Partition Scheme：Custom
* PSRAM：OPI PSRAM

1. 下载AI-VOX-Engine库的**最新版本**，链接：[AI VOX Engine 库下载和版本说明](https://dcnmu33qx4fc.feishu.cn/docx/OvkBd63NUor3sKx1271cHxjPnsL) 。菜单栏点击【项目】->【导入库】->【添加.ZIP库】，在弹出的窗口中找到下载好的ai_vox-x.x.x.zip -> 点击打开 -> 等待安装完成

![install_ai_vox_engine_lib](picture/install_ai_vox_engine_lib.png)

更新AI-VOX-Engine库时，需要先删除旧版本，然后再次采用上面的方式安装新版本的库。删除方法如下：

菜单栏点击【文件】->【首选项】，找到【项目文件夹地址】，复制路径，如下图：

![alt text](picture/open_library_addr.png)

复制路径后，打开文件管理器，进入该路径，找到libraries文件夹，删除AI_VOX文件夹即可。

![delete_ai_vox_lib](picture/delete_ai_vox_lib.png)

1. 将写好程序点击上传按钮，等待程序上传成功，如下图。

![download](picture/build_download.png)

点击串口工具就可以看到串口的打印。如下图

> **注意：** 如果您程序无误，选择的主板也正确但是依然报错，请检查你电脑用户名字是否包含中文，如果包含中文请修改用户变量 “TEMP” “TMP”字段即可。修改方法请自行网上查资料。

![print](picture/print.png)

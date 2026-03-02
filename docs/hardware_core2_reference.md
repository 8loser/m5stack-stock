# M5Stack Core2 硬體參考（AI/韌體開發）

本文件整理自 M5Stack Core2 官方文件，聚焦「開發最常用、最需要快速查找」的資訊。

日常開發請先看 `docs/hardware_quick_ref.md`；本檔保留完整來源與背景資訊。

## 官方來源

- Product / PinMap: https://docs.m5stack.com/en/core/core2
- PinMap 錨點: https://docs.m5stack.com/en/core/core2/#pinmap
- Arduino 程式庫（可作為周邊初始化參考）: https://github.com/m5stack/M5Core2
- ESP-IDF 程式庫（可作為驅動行為參考）: https://github.com/m5stack/M5Core2
- Schematic（SCH）: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/core2/core2_sch.pdf
- PCB 圖: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/core2/core2_pcb.pdf
- 尺寸圖（Dimension）: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/core2/core2_dimension.pdf

## 核心規格（官方）

- SoC: ESP32-D0WDQ6-V3（Xtensa Dual Core, up to 240MHz）
- RAM / Flash: 520KB SRAM + 8MB PSRAM / 16MB Flash
- 顯示器: 2.0" TFT，320x240，驅動 ILI9342C，電容觸控 FT6336U
- PMIC: AXP192
- RTC: BM8563
- IMU: MPU6886（6-axis）
- 音訊: I2S 功放 NS4168 + 喇叭，麥克風 SPM1423（PDM）
- 無線: 2.4GHz Wi-Fi（802.11 b/g/n）+ Bluetooth
- 電池: 390mAh（內建）
- 其他: 振動馬達、MicroSD、I2C 擴充（PORT A/C）

## Core2 板上 PinMap（官方）

以下為官方列出的板上主要功能對應點位：

| 功能 | GPIO / 介面 |
|---|---|
| LCD_MOSI | GPIO23 |
| LCD_CLK | GPIO18 |
| LCD_CS | GPIO5 |
| LCD_DC | GPIO15 |
| LCD_RST | AXP192 控制 |
| LCD_BL | AXP192 控制 |
| TOUCH_SDA | GPIO21 |
| TOUCH_SCL | GPIO22 |
| TOUCH_INT | GPIO39 |
| I2S_BCK | GPIO12 |
| I2S_LRCK | GPIO0 |
| I2S_DATA | GPIO2 |
| PDM_DATA (MIC) | GPIO34 |
| PDM_CLK (MIC) | GPIO0 |
| TF 卡（MISO/MOSI/SCK/CS） | GPIO38 / GPIO23 / GPIO18 / GPIO4 |
| IR | GPIO9 |
| RTC_INT | GPIO35 |
| AXP192_INT | GPIO35 |
| MPU6886_INT | GPIO38 |

外部擴充（官方）：

| 介面 | GPIO |
|---|---|
| PORT A (I2C) | G32 / G33 |
| PORT B (UART) | TX17 / RX16 |
| PORT C (Grove) | G13 / G14 |
| M-BUS SPI | SCK18 / MISO38 / MOSI23 |

## 本專案目前使用的硬體點位（`include/app_config.h`）

| 功能 | 目前設定 |
|---|---|
| LCD MOSI/CLK/CS/DC | GPIO23 / GPIO18 / GPIO5 / GPIO15 |
| Touch SDA/SCL/INT | GPIO21 / GPIO22 / GPIO39 |
| Speaker I2S BCK/WS/DATA | GPIO12 / GPIO0 / GPIO2 |
| I2C 位址 | AXP192=`0x34`, FT6336U=`0x38`, BM8563=`0x51` |

說明：
- 目前 `components/board/board.c` 使用單一 I2C bus (`I2C_NUM_0`) 管理 AXP192、FT6336U、BM8563。
- LCD Reset / Backlight 由 AXP192 控制，非直接 GPIO。

## 維護規則

- 若更新 `include/app_config.h` 的硬體點位，需同步更新本文件「本專案目前使用的硬體點位」章節。
- 若官方文件有修訂，請以「官方來源」連結為準，更新規格與 pinmap。
- 更新日期：2026-03-02

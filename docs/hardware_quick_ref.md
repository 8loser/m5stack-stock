# M5Stack Core2 開發速查（低 Token）

本檔給 AI/人類快速查詢，僅保留高頻資訊。完整規格請看 `docs/hardware_core2_reference.md`。

## 已占用 GPIO（本專案）

| 類別 | GPIO | 用途 |
|---|---|---|
| LCD SPI | 23 / 18 / 5 / 15 | MOSI / CLK / CS / DC |
| Touch I2C | 21 / 22 | SDA / SCL |
| Touch INT | 39 | 觸控中斷 |
| Speaker I2S | 12 / 0 / 2 | BCK / WS / DATA |

## 高風險衝突

| 對象 | 風險 |
|---|---|
| GPIO0 | I2S LRCK 與 MIC PDM CLK 共用 |
| GPIO18 / GPIO23 | LCD SPI 與 TF card SPI 共用 |
| I2C_NUM_0 | AXP192 / FT6336U / BM8563 共用 |

## I2C 位址

| 裝置 | 位址 |
|---|---|
| AXP192 | `0x34` |
| FT6336U | `0x38` |
| BM8563 | `0x51` |

## 5 步檢查

1. 新周邊是否使用已占用 GPIO。
2. 是否踩到 `GPIO0/18/23` 共用情境。
3. I2C 位址是否與 `0x34/0x38/0x51` 衝突。
4. SPI 是否有獨立 CS 與正確 bus。
5. 若改了 pin，是否同步更新 `include/app_config.h` 與文件。

#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "mcp_server.h"
#include "led/single_led.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#define TAG "CustomXiaozhiS3"


// ============================================================
// RELAY CONTROLLER
// ============================================================
class RelayController {
public:
    RelayController() {
        gpio_config_t cfg = {};

        cfg.pin_bit_mask =
            (1ULL << RELAY1_GPIO) |
            (1ULL << RELAY2_GPIO);

        cfg.mode = GPIO_MODE_OUTPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;

        ESP_ERROR_CHECK(
            gpio_config(&cfg)
        );

        Set(1, false);
        Set(2, false);
    }

    void Set(int relay, bool on) {
        gpio_num_t pin = GPIO_NUM_NC;

        if (relay == 1) {
            pin = RELAY1_GPIO;
        } else if (relay == 2) {
            pin = RELAY2_GPIO;
        }

        if (pin == GPIO_NUM_NC) {
            return;
        }

#if RELAY_ACTIVE_HIGH
        gpio_set_level(
            pin,
            on ? 1 : 0
        );
#else
        gpio_set_level(
            pin,
            on ? 0 : 1
        );
#endif
    }

    bool Get(int relay) const {
        gpio_num_t pin = GPIO_NUM_NC;

        if (relay == 1) {
            pin = RELAY1_GPIO;
        } else if (relay == 2) {
            pin = RELAY2_GPIO;
        }

        if (pin == GPIO_NUM_NC) {
            return false;
        }

        int level =
            gpio_get_level(pin);

#if RELAY_ACTIVE_HIGH
        return level != 0;
#else
        return level == 0;
#endif
    }
};


// ============================================================
// DS18B20
//
// GPIO9
//
// DATA membutuhkan resistor pull-up 4.7K ke 3.3V.
// ============================================================
class DS18B20 {
private:
    static constexpr gpio_num_t PIN =
        TEMP_SENSOR_GPIO;

    static void DelayUs(uint32_t us) {
        esp_rom_delay_us(us);
    }

    static void DriveLow() {
        gpio_set_direction(
            PIN,
            GPIO_MODE_OUTPUT_OD
        );

        gpio_set_level(
            PIN,
            0
        );
    }

    static void ReleaseBus() {
        gpio_set_direction(
            PIN,
            GPIO_MODE_INPUT_OUTPUT_OD
        );

        gpio_set_level(
            PIN,
            1
        );
    }

    static bool Reset() {
        DriveLow();

        DelayUs(480);

        ReleaseBus();

        DelayUs(70);

        bool present =
            (gpio_get_level(PIN) == 0);

        DelayUs(410);

        return present;
    }

    static void WriteBit(bool bit) {
        DriveLow();

        if (bit) {
            DelayUs(6);

            ReleaseBus();

            DelayUs(64);
        } else {
            DelayUs(60);

            ReleaseBus();

            DelayUs(10);
        }
    }

    static bool ReadBit() {
        DriveLow();

        DelayUs(6);

        ReleaseBus();

        DelayUs(9);

        bool bit =
            gpio_get_level(PIN) != 0;

        DelayUs(55);

        return bit;
    }

    static void WriteByte(uint8_t value) {
        for (int i = 0; i < 8; ++i) {
            WriteBit(
                (value & 0x01) != 0
            );

            value >>= 1;
        }
    }

    static uint8_t ReadByte() {
        uint8_t value = 0;

        for (int i = 0; i < 8; ++i) {
            if (ReadBit()) {
                value |=
                    (1 << i);
            }
        }

        return value;
    }

    static uint8_t Crc8(
        const uint8_t* data,
        int len
    ) {
        uint8_t crc = 0;

        while (len--) {
            uint8_t inbyte =
                *data++;

            for (
                uint8_t i = 8;
                i;
                --i
            ) {
                uint8_t mix =
                    (crc ^ inbyte) & 0x01;

                crc >>= 1;

                if (mix) {
                    crc ^= 0x8C;
                }

                inbyte >>= 1;
            }
        }

        return crc;
    }

public:
    DS18B20() {
        gpio_config_t cfg = {};

        cfg.pin_bit_mask =
            (1ULL << PIN);

        cfg.mode =
            GPIO_MODE_INPUT_OUTPUT_OD;

        cfg.pull_up_en =
            GPIO_PULLUP_ENABLE;

        cfg.pull_down_en =
            GPIO_PULLDOWN_DISABLE;

        cfg.intr_type =
            GPIO_INTR_DISABLE;

        ESP_ERROR_CHECK(
            gpio_config(&cfg)
        );

        ReleaseBus();
    }

    bool ReadCelsius(
        float& temperature
    ) {
        uint8_t data[9] = {};

        if (!Reset()) {
            return false;
        }

        // Skip ROM
        WriteByte(0xCC);

        // Write scratchpad
        WriteByte(0x4E);

        // TH
        WriteByte(0x00);

        // TL
        WriteByte(0x00);

        // 10-bit resolution
        WriteByte(0x3F);

        // Start conversion
        if (!Reset()) {
            return false;
        }

        WriteByte(0xCC);

        WriteByte(0x44);

        // Maximum 10-bit conversion time
        vTaskDelay(
            pdMS_TO_TICKS(200)
        );

        // Read scratchpad
        if (!Reset()) {
            return false;
        }

        WriteByte(0xCC);

        WriteByte(0xBE);

        for (int i = 0; i < 9; ++i) {
            data[i] =
                ReadByte();
        }

        if (
            Crc8(data, 8)
            != data[8]
        ) {
            return false;
        }

        int16_t raw =
            static_cast<int16_t>(
                (data[1] << 8) |
                data[0]
            );

        temperature =
            static_cast<float>(
                raw
            ) / 16.0f;

        if (
            temperature < -55.0f ||
            temperature > 125.0f
        ) {
            return false;
        }

        return true;
    }
};


// ============================================================
// DFPLAYER MINI
//
// ESP32 GPIO10 TX -> DFPlayer RX
// ESP32 GPIO11 RX <- DFPlayer TX
//
// UART1
// 9600 baud
// ============================================================
class DFPlayerMini {
private:
    uart_port_t uart_ =
        DFPLAYER_UART;

    void SendCommand(
        uint8_t command,
        uint16_t parameter
    ) {
        uint8_t frame[10] = {};

        frame[0] = 0x7E;
        frame[1] = 0xFF;
        frame[2] = 0x06;
        frame[3] = command;
        frame[4] = 0x00;

        frame[5] =
            static_cast<uint8_t>(
                (parameter >> 8) &
                0xFF
            );

        frame[6] =
            static_cast<uint8_t>(
                parameter &
                0xFF
            );

        uint16_t sum =
            frame[1] +
            frame[2] +
            frame[3] +
            frame[4] +
            frame[5] +
            frame[6];

        uint16_t checksum =
            static_cast<uint16_t>(
                0 - sum
            );

        frame[7] =
            static_cast<uint8_t>(
                (checksum >> 8) &
                0xFF
            );

        frame[8] =
            static_cast<uint8_t>(
                checksum &
                0xFF
            );

        frame[9] = 0xEF;

        uart_write_bytes(
            uart_,
            reinterpret_cast<const char*>(
                frame
            ),
            sizeof(frame)
        );

        uart_wait_tx_done(
            uart_,
            pdMS_TO_TICKS(100)
        );
    }

public:
    DFPlayerMini() {
        uart_config_t cfg = {};

        cfg.baud_rate = 9600;

        cfg.data_bits =
            UART_DATA_8_BITS;

        cfg.parity =
            UART_PARITY_DISABLE;

        cfg.stop_bits =
            UART_STOP_BITS_1;

        cfg.flow_ctrl =
            UART_HW_FLOWCTRL_DISABLE;

        cfg.source_clk =
            UART_SCLK_DEFAULT;

        ESP_ERROR_CHECK(
            uart_driver_install(
                uart_,
                1024,
                0,
                0,
                nullptr,
                0
            )
        );

        ESP_ERROR_CHECK(
            uart_param_config(
                uart_,
                &cfg
            )
        );

        ESP_ERROR_CHECK(
            uart_set_pin(
                uart_,
                DFPLAYER_TX_GPIO,
                DFPLAYER_RX_GPIO,
                UART_PIN_NO_CHANGE,
                UART_PIN_NO_CHANGE
            )
        );

        vTaskDelay(
            pdMS_TO_TICKS(300)
        );

        // Reset DFPlayer
        SendCommand(
            0x0C,
            0
        );

        vTaskDelay(
            pdMS_TO_TICKS(500)
        );

        // Default volume
        SetVolume(20);
    }

    void Play(
        uint16_t track
    ) {
        if (track < 1) {
            track = 1;
        }

        SendCommand(
            0x03,
            track
        );
    }

    void Stop() {
        SendCommand(
            0x16,
            0
        );
    }

    void Next() {
        SendCommand(
            0x01,
            0
        );
    }

    void Previous() {
        SendCommand(
            0x02,
            0
        );
    }

    void SetVolume(
        int volume
    ) {
        if (volume < 0) {
            volume = 0;
        }

        if (volume > 30) {
            volume = 30;
        }

        SendCommand(
            0x06,
            static_cast<uint16_t>(
                volume
            )
        );
    }
};


// ============================================================
// CUSTOM XIAOZHI S3 BOARD
// ============================================================
class CustomXiaozhiS3Board :
    public WifiBoard {

private:

    // --------------------------------------------------------
    // OLED
    // --------------------------------------------------------
    i2c_master_bus_handle_t
        display_i2c_bus_ = nullptr;

    esp_lcd_panel_io_handle_t
        panel_io_ = nullptr;

    esp_lcd_panel_handle_t
        panel_ = nullptr;

    Display* display_ = nullptr;


    // --------------------------------------------------------
    // BOOT BUTTON
    // --------------------------------------------------------
    Button boot_button_;


    // --------------------------------------------------------
    // HARDWARE
    // --------------------------------------------------------
    RelayController relay_;

    DS18B20 temperature_;

    DFPlayerMini dfplayer_;


    // ========================================================
    // INITIALIZE OLED I2C
    // ========================================================
    void InitializeDisplayI2c() {

        i2c_master_bus_config_t
            bus_config = {};

        bus_config.i2c_port =
            I2C_NUM_0;

        bus_config.sda_io_num =
            DISPLAY_SDA_PIN;

        bus_config.scl_io_num =
            DISPLAY_SCL_PIN;

        bus_config.clk_source =
            I2C_CLK_SRC_DEFAULT;

        bus_config.glitch_ignore_cnt =
            7;

        bus_config.intr_priority =
            0;

        bus_config.trans_queue_depth =
            0;

        bus_config.flags
            .enable_internal_pullup =
            1;

        esp_err_t ret =
            i2c_new_master_bus(
                &bus_config,
                &display_i2c_bus_
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED I2C bus gagal: %s",
                esp_err_to_name(ret)
            );

            display_i2c_bus_ =
                nullptr;

            return;
        }

        ESP_LOGI(
            TAG,
            "OLED I2C bus siap"
        );
    }


    // ========================================================
    // INITIALIZE SSD1306
    //
    // IMPORTANT:
    // OLED tidak terpasang bukan error fatal.
    // Firmware tetap melanjutkan boot.
    // ========================================================
    void InitializeSsd1306Display() {

        if (
            display_i2c_bus_ ==
            nullptr
        ) {
            ESP_LOGW(
                TAG,
                "OLED dilewati karena I2C bus tidak tersedia"
            );

            return;
        }


        esp_lcd_panel_io_i2c_config_t
            io_config = {};

        io_config.dev_addr =
            0x3C;

        io_config.on_color_trans_done =
            nullptr;

        io_config.user_ctx =
            nullptr;

        io_config.control_phase_bytes =
            1;

        io_config.dc_bit_offset =
            6;

        io_config.lcd_cmd_bits =
            8;

        io_config.lcd_param_bits =
            8;

        io_config.flags
            .dc_low_on_data =
            0;

        io_config.flags
            .disable_control_phase =
            0;

        io_config.scl_speed_hz =
            400 * 1000;


        esp_err_t ret =
            esp_lcd_new_panel_io_i2c_v2(
                display_i2c_bus_,
                &io_config,
                &panel_io_
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED tidak dapat membuat I2C panel: %s",
                esp_err_to_name(ret)
            );

            panel_io_ =
                nullptr;

            return;
        }


        esp_lcd_panel_dev_config_t
            panel_config = {};

        panel_config.reset_gpio_num =
            -1;

        panel_config.bits_per_pixel =
            1;


        esp_lcd_panel_ssd1306_config_t
            ssd1306_config = {};

        ssd1306_config.height =
            DISPLAY_HEIGHT;

        panel_config.vendor_config =
            &ssd1306_config;


        ret =
            esp_lcd_new_panel_ssd1306(
                panel_io_,
                &panel_config,
                &panel_
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "SSD1306 tidak terdeteksi: %s",
                esp_err_to_name(ret)
            );

            panel_ =
                nullptr;

            return;
        }


        ret =
            esp_lcd_panel_reset(
                panel_
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED reset gagal: %s",
                esp_err_to_name(ret)
            );

            panel_ =
                nullptr;

            return;
        }


        ret =
            esp_lcd_panel_init(
                panel_
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED init gagal: %s",
                esp_err_to_name(ret)
            );

            panel_ =
                nullptr;

            return;
        }


        ret =
            esp_lcd_panel_invert_color(
                panel_,
                false
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED invert color gagal: %s",
                esp_err_to_name(ret)
            );
        }


        ret =
            esp_lcd_panel_disp_on_off(
                panel_,
                true
            );

        if (ret != ESP_OK) {

            ESP_LOGW(
                TAG,
                "OLED display ON gagal: %s",
                esp_err_to_name(ret)
            );
        }


        display_ =
            new OledDisplay(
                panel_io_,
                panel_,
                DISPLAY_WIDTH,
                DISPLAY_HEIGHT,
                DISPLAY_MIRROR_X,
                DISPLAY_MIRROR_Y
            );


        ESP_LOGI(
            TAG,
            "SSD1306 OLED berhasil diinisialisasi"
        );
    }


    // ========================================================
    // STARTUP LOGO
    //
    // Menampilkan identitas perangkat setelah UI Xiaozhi selesai
    // dibuat. Logo dibuat langsung dengan LVGL agar tidak perlu
    // file gambar/asset tambahan.
    //
    // Tampilan:
    //   [ STACKED ARCHIVE LOGO ]
    //       Created by
    //        Arsiparis
    // ========================================================
    void ShowStartupLogo() {
        if (display_ == nullptr) {
            return;
        }

        xTaskCreate(
            [](void* arg) {
                auto* board =
                    static_cast<CustomXiaozhiS3Board*>(arg);

                // Beri waktu kepada Application untuk menyelesaikan
                // SetupUI() bawaan OledDisplay.
                vTaskDelay(pdMS_TO_TICKS(700));

                if (board->display_ == nullptr) {
                    vTaskDelete(nullptr);
                    return;
                }

                // Tunggu maksimal beberapa detik sampai SetupUI benar-benar siap.
                for (int i = 0; i < 30; ++i) {
                    if (board->display_->IsSetupUICalled()) {
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                if (!board->display_->IsSetupUICalled()) {
                    ESP_LOGW(
                        TAG,
                        "Startup logo dibatalkan: SetupUI belum siap"
                    );
                    vTaskDelete(nullptr);
                    return;
                }

                lv_obj_t* logo_screen = nullptr;

                {
                    DisplayLockGuard lock(board->display_);

                    logo_screen = lv_obj_create(
                        lv_screen_active()
                    );

                    lv_obj_set_size(
                        logo_screen,
                        128,
                        64
                    );

                    lv_obj_center(logo_screen);

                    lv_obj_set_style_bg_color(
                        logo_screen,
                        lv_color_black(),
                        0
                    );

                    lv_obj_set_style_bg_opa(
                        logo_screen,
                        LV_OPA_COVER,
                        0
                    );

                    lv_obj_set_style_border_width(
                        logo_screen,
                        0,
                        0
                    );

                    lv_obj_set_style_radius(
                        logo_screen,
                        0,
                        0
                    );

                    lv_obj_set_style_pad_all(
                        logo_screen,
                        0,
                        0
                    );

                    lv_obj_clear_flag(
                        logo_screen,
                        LV_OBJ_FLAG_SCROLLABLE
                    );

                    // -------------------------------
                    // Logo ARSIP:
                    // tiga kotak arsip bertumpuk
                    // -------------------------------
                    const int box_x = 38;
                    const int box_w = 52;
                    const int box_h = 9;

                    const int box_y[3] = {
                        5,
                        15,
                        25
                    };

                    for (int i = 0; i < 3; ++i) {
                        lv_obj_t* box =
                            lv_obj_create(logo_screen);

                        lv_obj_set_size(
                            box,
                            box_w,
                            box_h
                        );

                        lv_obj_set_pos(
                            box,
                            box_x,
                            box_y[i]
                        );

                        lv_obj_set_style_bg_opa(
                            box,
                            LV_OPA_TRANSP,
                            0
                        );

                        lv_obj_set_style_border_width(
                            box,
                            2,
                            0
                        );

                        lv_obj_set_style_border_color(
                            box,
                            lv_color_white(),
                            0
                        );

                        lv_obj_set_style_radius(
                            box,
                            1,
                            0
                        );

                        lv_obj_set_style_pad_all(
                            box,
                            0,
                            0
                        );

                        // Garis kecil di dalam kotak untuk
                        // memberi kesan dokumen/arsip.
                        lv_obj_t* line =
                            lv_obj_create(box);

                        lv_obj_set_size(
                            line,
                            30,
                            2
                        );

                        lv_obj_align(
                            line,
                            LV_ALIGN_CENTER,
                            0,
                            0
                        );

                        lv_obj_set_style_bg_color(
                            line,
                            lv_color_white(),
                            0
                        );

                        lv_obj_set_style_bg_opa(
                            line,
                            LV_OPA_COVER,
                            0
                        );

                        lv_obj_set_style_border_width(
                            line,
                            0,
                            0
                        );
                    }

                    // -------------------------------
                    // Created by
                    // -------------------------------
                    lv_obj_t* created_by =
                        lv_label_create(logo_screen);

                    lv_label_set_text(
                        created_by,
                        "Created by"
                    );

                    lv_obj_set_style_text_color(
                        created_by,
                        lv_color_white(),
                        0
                    );

                    lv_obj_align(
                        created_by,
                        LV_ALIGN_TOP_MID,
                        0,
                        39
                    );

                    // -------------------------------
                    // Arsiparis
                    // -------------------------------
                    lv_obj_t* arsiparis =
                        lv_label_create(logo_screen);

                    lv_label_set_text(
                        arsiparis,
                        "Arsiparis"
                    );

                    lv_obj_set_style_text_color(
                        arsiparis,
                        lv_color_white(),
                        0
                    );

                    lv_obj_align(
                        arsiparis,
                        LV_ALIGN_TOP_MID,
                        0,
                        51
                    );
                }

                // Tahan logo selama sekitar 2,5 detik.
                vTaskDelay(pdMS_TO_TICKS(2500));

                if (board->display_ != nullptr &&
                    logo_screen != nullptr) {

                    DisplayLockGuard lock(board->display_);

                    if (lv_obj_is_valid(logo_screen)) {
                        lv_obj_del(logo_screen);
                    }
                }

                ESP_LOGI(
                    TAG,
                    "Startup logo ARSIP selesai"
                );

                vTaskDelete(nullptr);
            },
            "startup_logo",
            4096,
            this,
            1,
            nullptr
        );
    }


    // ========================================================
    // BUTTON
    // ========================================================
    void InitializeButtons() {

        boot_button_.OnClick(
            [this]() {

                auto& app =
                    Application::GetInstance();


                if (
                    app.GetDeviceState() ==
                    kDeviceStateStarting
                ) {

                    EnterWifiConfigMode();

                    return;
                }


                app.ToggleChatState();
            }
        );
    }


    // ========================================================
    // INTERNAL MCP TOOLS
    // ========================================================
    void InitializeTools() {

        auto& mcp =
            McpServer::GetInstance();


        // ====================================================
        // RELAY SET
        // ====================================================
        mcp.AddTool(
            "self.relay.set",

            "Mengontrol Kipas atau Lampu. "
            "relay 1 adalah Kipas dan relay 2 adalah Lampu. "
            "state true=nyala false=mati.",

            PropertyList({

                Property(
                    "relay",
                    kPropertyTypeInteger,
                    1,
                    1,
                    2
                ),

                Property(
                    "state",
                    kPropertyTypeBoolean
                )
            }),

            [this](
                const PropertyList&
                    properties
            ) -> ReturnValue {

                int relay =
                    properties[
                        "relay"
                    ].value<int>();

                bool state =
                    properties[
                        "state"
                    ].value<bool>();


                relay_.Set(
                    relay,
                    state
                );


                char result[96];

                snprintf(
                    result,
                    sizeof(result),
                    "%s sekarang %s",
                    relay == 1 ? "Kipas" : "Lampu",
                    state
                        ? "nyala"
                        : "mati"
                );

                return std::string(
                    result
                );
            }
        );


        // ====================================================
        // RELAY GET
        // ====================================================
        mcp.AddTool(
            "self.relay.get",

            "Membaca status Kipas atau Lampu. "
            "relay 1 adalah Kipas dan relay 2 adalah Lampu.",

            PropertyList({

                Property(
                    "relay",
                    kPropertyTypeInteger,
                    1,
                    1,
                    2
                )
            }),

            [this](
                const PropertyList&
                    properties
            ) -> ReturnValue {

                int relay =
                    properties[
                        "relay"
                    ].value<int>();


                bool state =
                    relay_.Get(
                        relay
                    );


                char result[96];

                snprintf(
                    result,
                    sizeof(result),
                    "%s %s",
                    relay == 1 ? "Kipas" : "Lampu",
                    state
                        ? "nyala"
                        : "mati"
                );

                return std::string(
                    result
                );
            }
        );


        // ====================================================
        // TEMPERATURE
        // ====================================================
        mcp.AddTool(
            "self.temperature.get",

            "Membaca suhu dari sensor "
            "DS18B20 yang terhubung "
            "ke GPIO9.",

            PropertyList(),

            [this](
                const PropertyList&
            ) -> ReturnValue {

                float temperature =
                    0.0f;


                if (
                    !temperature_
                        .ReadCelsius(
                            temperature
                        )
                ) {

                    return std::string(
                        "Sensor suhu tidak "
                        "terdeteksi atau "
                        "pembacaan gagal."
                    );
                }


                char result[128];

                snprintf(
                    result,
                    sizeof(result),
                    "Suhu saat ini %.1f "
                    "derajat Celsius.",
                    temperature
                );

                return std::string(
                    result
                );
            }
        );


        // ====================================================
        // DFPLAYER PLAY
        // ====================================================
        mcp.AddTool(
            "self.dfplayer.play",

            "Memutar file MP3 berdasarkan "
            "nomor track pada DFPlayer Mini.",

            PropertyList({

                Property(
                    "track",
                    kPropertyTypeInteger,
                    1,
                    1,
                    3000
                )
            }),

            [this](
                const PropertyList&
                    properties
            ) -> ReturnValue {

                int track =
                    properties[
                        "track"
                    ].value<int>();


                dfplayer_.Play(
                    static_cast<uint16_t>(
                        track
                    )
                );


                char result[96];

                snprintf(
                    result,
                    sizeof(result),
                    "Memutar track %d.",
                    track
                );

                return std::string(
                    result
                );
            }
        );


        // ====================================================
        // DFPLAYER STOP
        // ====================================================
        mcp.AddTool(
            "self.dfplayer.stop",

            "Menghentikan pemutaran "
            "DFPlayer Mini.",

            PropertyList(),

            [this](
                const PropertyList&
            ) -> ReturnValue {

                dfplayer_.Stop();

                return std::string(
                    "Pemutaran dihentikan."
                );
            }
        );


        // ====================================================
        // DFPLAYER NEXT
        // ====================================================
        mcp.AddTool(
            "self.dfplayer.next",

            "Memutar track berikutnya "
            "pada DFPlayer Mini.",

            PropertyList(),

            [this](
                const PropertyList&
            ) -> ReturnValue {

                dfplayer_.Next();

                return std::string(
                    "Memutar track berikutnya."
                );
            }
        );


        // ====================================================
        // DFPLAYER PREVIOUS
        // ====================================================
        mcp.AddTool(
            "self.dfplayer.previous",

            "Memutar track sebelumnya "
            "pada DFPlayer Mini.",

            PropertyList(),

            [this](
                const PropertyList&
            ) -> ReturnValue {

                dfplayer_.Previous();

                return std::string(
                    "Memutar track sebelumnya."
                );
            }
        );


        // ====================================================
        // DFPLAYER VOLUME
        // ====================================================
        mcp.AddTool(
            "self.dfplayer.volume",

            "Mengatur volume DFPlayer Mini "
            "dari 0 sampai 30.",

            PropertyList({

                Property(
                    "volume",
                    kPropertyTypeInteger,
                    20,
                    0,
                    30
                )
            }),

            [this](
                const PropertyList&
                    properties
            ) -> ReturnValue {

                int volume =
                    properties[
                        "volume"
                    ].value<int>();


                dfplayer_.SetVolume(
                    volume
                );


                char result[96];

                snprintf(
                    result,
                    sizeof(result),
                    "Volume DFPlayer "
                    "diatur ke %d dari 30.",
                    volume
                );

                return std::string(
                    result
                );
            }
        );


        ESP_LOGI(
            TAG,
            "Custom MCP tools registered"
        );
    }


public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================
    CustomXiaozhiS3Board()
        : boot_button_(
            BOOT_BUTTON_GPIO
        ) {

        ESP_LOGI(
            TAG,
            "Initializing Custom Xiaozhi "
            "ESP32-S3 N16R8"
        );


        // OLED
        InitializeDisplayI2c();

        InitializeSsd1306Display();


        // BOOT button
        InitializeButtons();


        // MCP
        InitializeTools();

        // Startup identity OLED.
        ShowStartupLogo();


        ESP_LOGI(
            TAG,
            "Custom hardware initialized"
        );

        ESP_LOGI(
            TAG,
            "Relay1 GPIO=%d",
            RELAY1_GPIO
        );

        ESP_LOGI(
            TAG,
            "Relay2 GPIO=%d",
            RELAY2_GPIO
        );

        ESP_LOGI(
            TAG,
            "DS18B20 GPIO=%d",
            TEMP_SENSOR_GPIO
        );

        ESP_LOGI(
            TAG,
            "DFPlayer TX=%d RX=%d",
            DFPLAYER_TX_GPIO,
            DFPLAYER_RX_GPIO
        );
    }


    // ========================================================
    // LED
    // ========================================================
    virtual Led* GetLed() override {

        static SingleLed led(
            BUILTIN_LED_GPIO
        );

        return &led;
    }


    // ========================================================
    // AUDIO CODEC
    // ========================================================
    virtual AudioCodec* GetAudioCodec()
        override {

        static NoAudioCodecSimplex
            audio_codec(

                AUDIO_INPUT_SAMPLE_RATE,

                AUDIO_OUTPUT_SAMPLE_RATE,

                AUDIO_I2S_SPK_GPIO_BCLK,

                AUDIO_I2S_SPK_GPIO_LRCK,

                AUDIO_I2S_SPK_GPIO_DOUT,

                AUDIO_I2S_MIC_GPIO_SCK,

                AUDIO_I2S_MIC_GPIO_WS,

                AUDIO_I2S_MIC_GPIO_DIN
            );

        return &audio_codec;
    }


    // ========================================================
    // DISPLAY
    // ========================================================
    virtual Display* GetDisplay()
        override {

        return display_;
    }
};


// ============================================================
// REGISTER BOARD
// ============================================================
DECLARE_BOARD(
    CustomXiaozhiS3Board
);
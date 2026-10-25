#include "sdkconfig.h"

#if CONFIG_XIAOZHI_NETWORK_ETHERNET
#include "ethernet_board.h"
#else
#include "wifi_board.h"
#endif
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>

#include "button.h"

#include "esp_video.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "WaveshareEsp32p4nano"

#if CONFIG_XIAOZHI_NETWORK_ETHERNET
using WaveshareEsp32p4nanoBase = EthernetBoard;
#else
using WaveshareEsp32p4nanoBase = WifiBoard;
#endif

class WaveshareEsp32p4nano : public WaveshareEsp32p4nanoBase {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Display *display__;
    EspVideo* camera_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeLCD() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_20;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_21;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = 240 * 240 * sizeof(uint16_t);

        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_2;
        io_config.dc_gpio_num = GPIO_NUM_3;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        esp_lcd_panel_io_handle_t io_handle = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_6;
        panel_config.bits_per_pixel = 16;

        esp_lcd_panel_handle_t panel_handle = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
        esp_lcd_panel_invert_color(panel_handle, true);

        display__ = new SpiLcdDisplay(io_handle, panel_handle, 240, 240, 0, 0, false, false, false);
    }

    void InitializeTouch() {}

    void InitializeCamera() {
        esp_video_init_csi_config_t base_csi_config = {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = codec_i2c_bus_,
                .freq = 400000,
            },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin  = GPIO_NUM_NC,
        };
        esp_video_init_config_t cam_config = { .csi = &base_csi_config };
        camera_ = new EspVideo(cam_config);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
#if CONFIG_XIAOZHI_NETWORK_ETHERNET
            if (app.GetDeviceState() != kDeviceStateStarting) {
                app.ToggleChatState();
            }
#else
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
#endif
        });
    }

public:
    WaveshareEsp32p4nano() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeLCD();
        InitializeTouch();
        InitializeCamera();
        InitializeButtons();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display__; }
    virtual Camera* GetCamera() override { return camera_; }
    virtual Backlight *GetBacklight() override { return nullptr; }
};

DECLARE_BOARD(WaveshareEsp32p4nano);

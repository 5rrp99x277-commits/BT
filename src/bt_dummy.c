#include "pico/stdlib.h"
#include "pico/stdio_uart.h"
#include "bsp/board.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

//--------------------------------------------------------------------
// BT Dummy for Canon TM-350
//
// This RP2040 emulates a "placeholder" Bluetooth device on USB.
// When the host (Canon printer) enables WiFi, it expects BT to
// disappear from the bus for ~3 seconds (real CYW4373 powers down
// its BT radio). This device does exactly that when triggered by
// the WiFi RP2040 via GPIO.
//
// Connection to WiFi RP2040:
//   WiFi RP2040 GPIO3 (OUT)  ──►  BT RP2040 GPIO2 (IN)
//   GND ────────────────────────  GND (common)
//--------------------------------------------------------------------

#define TRIGGER_PIN     2   // GPIO2: input from WiFi RP2040
#define DISCONNECT_MS   3000

static bool was_triggered = false;

//--------------------------------------------------------------------
// USB descriptors
//--------------------------------------------------------------------
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF, // Vendor specific
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x04B4, // Cypress
    .idProduct          = 0x0BDD, // BT placeholder (different from WiFi 0x0BDC)
    .bcdDevice          = 0x0001,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_TOTAL_LEN    (9 + 9 + 7 + 7)

uint8_t const desc_fs_configuration[] = {
    // Config descriptor
    9, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(CONFIG_TOTAL_LEN),
    1, 1, 0,
    0x80, 0x32,

    // Interface: vendor
    9, TUSB_DESC_INTERFACE,
    0, 0, 2,
    TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 0,

    // EP81 IN bulk
    7, TUSB_DESC_ENDPOINT, 0x81,
    TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

    // EP02 OUT bulk
    7, TUSB_DESC_ENDPOINT, 0x02,
    TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_fs_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t desc_str[32];
    const char *str;

    if (index == 0) {
        desc_str[1] = 0x0409;
        desc_str[0] = (TUSB_DESC_STRING << 8) | 4;
        return desc_str;
    }
    switch (index) {
        case 1: str = "Cypress"; break;
        case 2: str = "BT Placeholder"; break;
        case 3: str = "000000000002"; break;
        default: return NULL;
    }
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) len = 31;
    for (uint8_t i = 0; i < len; i++) desc_str[1+i] = str[i];
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

//--------------------------------------------------------------------
// Main
//--------------------------------------------------------------------
int main(void) {
    board_init();
    stdio_uart_init_full(uart0, 115200, 0, 1);
    sleep_ms(100);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);
    gpio_pull_down(TRIGGER_PIN);

    printf("\r\n=== BT Dummy for Canon TM-350 ===\r\n");
    printf("Trigger pin: GPIO%d (from WiFi RP2040)\r\n", TRIGGER_PIN);
    printf("Disconnect duration: %d ms\r\n", DISCONNECT_MS);

    tusb_init();
    tud_connect();
    printf("Connected to USB\r\n");

    while (1) {
        tud_task();

        bool trigger = gpio_get(TRIGGER_PIN);

        if (trigger && !was_triggered) {
            // Rising edge — WiFi was enabled, disconnect BT for 3 seconds
            printf("TRIGGER received — disconnecting BT for %d ms\r\n", DISCONNECT_MS);
            tud_disconnect();

            uint32_t start = to_ms_since_boot(get_absolute_time());
            while ((to_ms_since_boot(get_absolute_time()) - start) < DISCONNECT_MS) {
                tud_task();
                sleep_ms(10);
            }

            tud_connect();
            printf("BT reconnected\r\n");
        }

        was_triggered = trigger;
        sleep_ms(10);
    }
}

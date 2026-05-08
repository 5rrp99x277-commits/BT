/*
  BT Dummy for Canon TM-350
  
  Логика: по импульсу HIGH на GPIO3 → disconnect на 6 секунд.
  WiFi дает короткий импульс (10us), BT сам отключается.
  
  Board: RP2040-Zero
  
  Connection:
    WiFi RP2040 GPIO3 (OUT)  ---->  GPIO3 (IN) this RP2040
    GND  --------------------------  GND (common)
*/

#include "pico/stdlib.h"
#include "pico/stdio_uart.h"
#include "bsp/board.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define TRIGGER_PIN     3
#define DISCONNECT_MS   6000

static int bt_state = 0;  // 0=connected, 1=disconnected
static bool was_high = false;

// === USB descriptors ===
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xE0,
    .bDeviceSubClass    = 0x01,
    .bDeviceProtocol    = 0x01,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x04B4,
    .idProduct          = 0x0BDA,
    .bcdDevice          = 0x0001,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_TOTAL_LEN (9 + 9 + 7 + 7)

uint8_t const desc_fs_configuration[] = {
    9, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(CONFIG_TOTAL_LEN),
    1, 1, 0,
    0x80, 0x32,

    9, TUSB_DESC_INTERFACE,
    0, 0, 2,
    TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 0,

    7, TUSB_DESC_ENDPOINT, 0x81,
    TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

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
        case 2: str = "Bluetooth USB Adapter"; break;
        case 3: str = "000000000002"; break;
        default: return NULL;
    }
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) len = 31;
    for (uint8_t i = 0; i < len; i++) desc_str[1+i] = str[i];
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

int main(void) {
    board_init();
    stdio_uart_init_full(uart0, 115200, 0, 1);
    sleep_ms(100);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);
    gpio_pull_down(TRIGGER_PIN);

    printf("\r\n=== BT Dummy (pulse triggered) ===\r\n");

    tusb_init();
    tud_connect();
    bt_state = 0;
    printf("Connected\r\n");

    while (1) {
        tud_task();
        
        bool trigger = gpio_get(TRIGGER_PIN);
        
        // Detect rising edge: LOW -> HIGH (pulse from WiFi)
        if (trigger && !was_high && bt_state == 0) {
            printf("PULSE detected — disconnect %d ms\r\n", DISCONNECT_MS);
            tud_disconnect();
            bt_state = 1;
        }
        
        was_high = trigger;
        
        // If disconnected, count 6 seconds
        if (bt_state == 1) {
            uint32_t start = to_ms_since_boot(get_absolute_time());
            while ((to_ms_since_boot(get_absolute_time()) - start) < DISCONNECT_MS) {
                tud_task();
                sleep_ms(10);
                // If another pulse during disconnect, extend? No, just continue.
            }
            tud_connect();
            bt_state = 0;
            printf("Reconnected\r\n");
        }
        
        sleep_ms(1);
    }
}

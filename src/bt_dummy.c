/*
  USB Flash Drive Dummy for Canon TM-350
  
  Emulates a USB Mass Storage device (flash drive).
  On GPIO3 pulse → disconnects for 6 seconds.
  
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

// === RAM Disk (4KB, empty) ===
#define DISK_BLOCK_NUM  8
#define DISK_BLOCK_SIZE 512
static uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];

// === USB descriptors: Mass Storage ===
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,  // Class at interface level
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x0781, // SanDisk (common flash vendor)
    .idProduct          = 0x5567, // Cruzer Blade
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_TOTAL_LEN (9 + 9 + 7 + 7)

uint8_t const desc_fs_configuration[] = {
    // Config
    9, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(CONFIG_TOTAL_LEN),
    1, 1, 0,
    0x80, 0x32,

    // Interface: Mass Storage (SCSI/Bulk-only)
    9, TUSB_DESC_INTERFACE,
    0, 0, 2,
    0x08,  // bInterfaceClass = Mass Storage
    0x06,  // bInterfaceSubClass = SCSI transparent
    0x50,  // bInterfaceProtocol = Bulk-only
    0,

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
        case 1: str = "SanDisk"; break;
        case 2: str = "Cruzer Blade"; break;
        case 3: str = "4C530001230412109182"; break;
        default: return NULL;
    }
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) len = 31;
    for (uint8_t i = 0; i < len; i++) desc_str[1+i] = str[i];
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

// === MSC (Mass Storage Class) callbacks ===

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void)lun; (void)power_condition; (void)start; (void)load_eject;
    return true;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "SanDisk ", 8);
    memcpy(product_id, "Cruzer Blade    ", 16);
    memcpy(product_rev, "1.00", 4);
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    *block_count = DISK_BLOCK_NUM;
    *block_size  = DISK_BLOCK_SIZE;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void)lun;
    uint32_t count = 0;
    while (count < bufsize) {
        if (lba >= DISK_BLOCK_NUM) break;
        uint32_t chunk = DISK_BLOCK_SIZE - offset;
        if (chunk > bufsize - count) chunk = bufsize - count;
        memcpy((uint8_t*)buffer + count, msc_disk[lba] + offset, chunk);
        count += chunk;
        offset = 0;
        lba++;
    }
    return count;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    uint32_t count = 0;
    while (count < bufsize) {
        if (lba >= DISK_BLOCK_NUM) break;
        uint32_t chunk = DISK_BLOCK_SIZE - offset;
        if (chunk > bufsize - count) chunk = bufsize - count;
        memcpy(msc_disk[lba] + offset, buffer + count, chunk);
        count += chunk;
        offset = 0;
        lba++;
    }
    return count;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    (void)lun; (void)scsi_cmd; (void)buffer; (void)bufsize;
    return 0;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

// === Main ===
int main(void) {
    board_init();
    stdio_uart_init_full(uart0, 115200, 0, 1);
    sleep_ms(100);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_IN);
    gpio_pull_down(TRIGGER_PIN);

    // Clear disk
    memset(msc_disk, 0, sizeof(msc_disk));

    printf("\r\n=== USB Flash Drive Dummy ===\r\n");

    tusb_init();
    tud_connect();
    bt_state = 0;
    printf("Connected\r\n");

    while (1) {
        tud_task();
        
        bool trigger = gpio_get(TRIGGER_PIN);
        
        // Detect rising edge: LOW -> HIGH (pulse from WiFi)
        if (trigger && !was_high && bt_state == 0) {
            printf("PULSE — disconnect %d ms\r\n", DISCONNECT_MS);
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
            }
            tud_connect();
            bt_state = 0;
            printf("Reconnected\r\n");
        }
        
        sleep_us(100);
    }
}

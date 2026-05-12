#include "pico/stdlib.h"
#include "pico/stdio_uart.h"
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

#define DISK_BLOCK_NUM  8
#define DISK_BLOCK_SIZE 512
static uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0781,
    .idProduct = 0x5567,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_TOTAL_LEN (9 + 9 + 7 + 7)

uint8_t const desc_fs_configuration[] = {
    9, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(CONFIG_TOTAL_LEN),
    1, 1, 0, 0x80, 0x32,
    9, TUSB_DESC_INTERFACE,
    0, 0, 2, 0x08, 0x06, 0x50, 0,
    7, TUSB_DESC_ENDPOINT, 0x81,
    TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    7, TUSB_DESC_ENDPOINT, 0x02,
    TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_fs_configuration;
}
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
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
    for (uint8_t i = 0; i < len; i++) desc_str[1 + i] = str[i];
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    *block_count = DISK_BLOCK_NUM;
    *block_size = DISK_BLOCK_SIZE;
}
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
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void)lun;
    uint32_t count = 0;
    while (count < bufsize) {
        if (lba >= DISK_BLOCK_NUM) break;
        uint32_t chunk = DISK_BLOCK_SIZE - offset;
        if (chunk > bufsize - count) chunk = bufsize - count;
        memcpy((uint8_t *)buffer + count, msc_disk[lba] + offset, chunk);
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

int main(void) {
    board_init();
    stdio_uart_init_full(uart0, 115200, 0, 1);
    sleep_ms(100);

    memset(msc_disk, 0, sizeof(msc_disk));

    tusb_init();
    tud_connect();

    while (1) {
        tud_task();
        sleep_ms(10);
    }
}

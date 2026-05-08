//==============================================================================
// BT-emulator firmware for second RP2040 Zero
//
// Role: present a USB device on hub port 2 to make plotter see "WiFi + BT"
//       topology of real CYW4373. Listens to a control GPIO from the main
//       (WiFi) RP2040 -- when that line goes LOW, this MCU calls
//       tud_disconnect() so the plotter's hub sees a "BT removed" event.
//       When the line returns HIGH, we tud_connect() and the plotter
//       sees BT come back.
//
// USB class: HID generic (simplest enumeration, plotter doesn't actually
//            send anything to it -- just needs to see device descriptor).
//
// Wiring:
//   USB-A male  -> hub port 2
//   GP3         -> control line from main RP2040 GP3 (HIGH = visible, LOW = hide)
//   GND         -> common with main RP2040 GND
//   UART TX GP0 -> 115200 8N1 for diagnostics (optional)
//==============================================================================

#include "pico/stdlib.h"
#include "tusb.h"
#include <stdio.h>
#include <stdbool.h>

#define CTRL_PIN 3   // input from main RP2040: HIGH = stay connected, LOW = disconnect

static bool currently_connected = false;

static void apply_state(bool want_connected) {
    if (want_connected == currently_connected) return;
    if (want_connected) {
        printf("CTRL pin HIGH -> tud_connect()\r\n");
        tud_connect();
    } else {
        printf("CTRL pin LOW  -> tud_disconnect()\r\n");
        tud_disconnect();
    }
    currently_connected = want_connected;
}

int main(void) {
    stdio_init_all();
    sleep_ms(50);
    printf("\r\n--- BT emulator boot ---\r\n");

    // Control input from main RP2040
    gpio_init(CTRL_PIN);
    gpio_set_dir(CTRL_PIN, GPIO_IN);
    gpio_pull_up(CTRL_PIN);   // if cable disconnected -> default HIGH = visible

    // Init TinyUSB
    tud_init(BOARD_TUD_RHPORT);

    // Read initial state of CTRL pin and apply it.
    // tud_init() already calls tud_connect() internally; if pin is LOW at boot
    // we override with disconnect.
    bool initial = gpio_get(CTRL_PIN);
    if (!initial) {
        tud_disconnect();
        currently_connected = false;
        printf("Initial CTRL pin LOW -- starting disconnected\r\n");
    } else {
        currently_connected = true;
        printf("Initial CTRL pin HIGH -- starting connected\r\n");
    }

    // Simple debounce: require 3 consecutive identical reads (3 ms apart)
    bool last_stable = currently_connected;
    bool last_raw = currently_connected;
    uint8_t same_count = 0;
    absolute_time_t next_sample = make_timeout_time_ms(3);

    while (1) {
        tud_task();

        if (absolute_time_diff_us(get_absolute_time(), next_sample) <= 0) {
            next_sample = make_timeout_time_ms(3);
            bool raw = gpio_get(CTRL_PIN);
            if (raw == last_raw) {
                if (same_count < 3) same_count++;
            } else {
                same_count = 0;
                last_raw = raw;
            }
            if (same_count >= 3 && raw != last_stable) {
                last_stable = raw;
                apply_state(raw);
            }
        }
    }
}

//==============================================================================
// TinyUSB device callbacks (minimal -- we don't process data, only enumerate)
//==============================================================================
void tud_mount_cb(void)         { printf("USB mounted\r\n"); }
void tud_umount_cb(void)        { printf("USB unmounted\r\n"); }
void tud_suspend_cb(bool en)    { (void)en; printf("USB suspend\r\n"); }
void tud_resume_cb(void)        { printf("USB resume\r\n"); }

// HID required callbacks (we don't send/receive anything -- return zeros)
uint16_t tud_hid_get_report_cb(uint8_t inst, uint8_t id, hid_report_type_t t,
                                uint8_t* buf, uint16_t len) {
    (void)inst; (void)id; (void)t; (void)buf; (void)len;
    return 0;
}
void tud_hid_set_report_cb(uint8_t inst, uint8_t id, hid_report_type_t t,
                            uint8_t const* buf, uint16_t len) {
    (void)inst; (void)id; (void)t; (void)buf; (void)len;
}

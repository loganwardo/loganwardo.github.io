#include "Timer.h"
#include "lcd.h"
#include "uart.h"
#include "movement.h"
#include "open_interface.h"
#include "adc.h"
#include "servo.h"
#include "ping.h"

#include <stdio.h>
#include <math.h>

// ==============================
// Constants
// ==============================
#define PI           3.14159265358979323846f
#define IR_THRESHOLD 700      // IR ADC value above which a surface is present
#define MAX_DIST_CM  100.0f   // PING readings at/above this treated as open space

// ==============================
// GUI scan
//
// Streams 3-column data: Angle  IR  Dist
// GUI uses IR for edge detection, PING for distance.
//
// Header:     "Angle\tIR\tDist\n"
// Data lines: "<deg>\t<ir_raw>\t<dist_cm>\n"
// Terminator: "END\n"
// ==============================
void perform_gui_scan(void) {
    char buf[100];
    int  a;

    uart_sendStr("Angle\tIR\tDist\n");

    for (a = 0; a <= 180; a += 2) {
        servo_move(a);
        timer_waitMillis(50);          // servo settle

        uint16_t ir_raw = adc_read();
        timer_waitMillis(20);          // small gap before PING to avoid cross-talk

        ping_trigger();
        timer_waitMillis(50);
        float dist = ping_getDistance();
        if (dist >= MAX_DIST_CM) dist = MAX_DIST_CM;

        sprintf(buf, "%d\t%d\t%.2f\n", a, ir_raw, dist);
        uart_sendStr(buf);
    }

    servo_move(90);                    // return to centre — stops holding current
    uart_sendStr("END\n");
}

// ==============================
// Harvest sound
// C5 - E5 - G5 chime, song slot 0
// ==============================
void harvestFruit(void) {
    unsigned char notes[3]     = {72, 76, 79};
    unsigned char durations[3] = {16, 16, 32}; // 1/64ths of a second

    oi_loadSong(0, 3, notes, durations);
    oi_play_song(0);
}

// ==============================
// Honk sound
// Descending two-note horn, song slot 1
// ==============================
void honk(void) {
    unsigned char notes[2]     = {81, 69};   // A5 then A4
    unsigned char durations[2] = {8,  16};

    oi_loadSong(1, 2, notes, durations);
    oi_play_song(1);
}

// ==============================
// Main
// ==============================

int main(void) {
    timer_init();
    lcd_init();
    uart_init();
    adc_init();
    servo_init();
    ping_init();

    oi_t *sensor = oi_alloc();
    oi_init(sensor);

    uart_sendStr("HARVESTBOT READY\r\n");

    // Show battery on LCD at startup
    int bat = (int)(100.0f * sensor->batteryCharge / sensor->batteryCapacity);
    lcd_printf("HARVESTBOT READY\nBattery: %d%%", bat);

    char cmd = 0;

    while (1) {
        cmd = uart_receive_nonblocking();

        // Skip stray newline / carriage-return bytes
        if (cmd == '\n' || cmd == '\r') {
            timer_waitMillis(5);
            continue;
        }

        // Idle - small delay to reduce battery draw from busy-polling
        if (cmd == 0) {
            timer_waitMillis(5);
            continue;
        }

        // Scan (M / m / z)
        if (cmd == 'M' || cmd == 'm' || cmd == 'z') {
            lcd_printf("Scanning...");
            perform_gui_scan();
            lcd_printf("HARVESTBOT READY");
        }

        // Drive Forward 150mm (W)
        else if (cmd == 'w') {
            lcd_printf("Moving Forward\n150 mm");
            move_forward(sensor, 150);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Fine Forward 50mm (f — sent by Up arrow key)
        else if (cmd == 'f') {
            lcd_printf("Fine Forward\n50 mm");
            move_forward(sensor, 50);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Move Backward (S)
        else if (cmd == 's') {
            lcd_printf("Moving Backward");
            turn_right(sensor, 180);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Turn Left 90deg (A)
        else if (cmd == 'a') {
            lcd_printf("Turning Left\n90 deg");
            turn_left(sensor, 90);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Turn Right 90deg (D)
        else if (cmd == 'd') {
            lcd_printf("Turning Right\n90 deg");
            turn_right(sensor, 90);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Manual Turn (Q = left, E = right, then digit)
        // GUI sends both bytes in one TCP packet: e.g. "q3" = turn left 30deg.
        // manual_turn() reads the direction byte, then calls uart_receive()
        // for the digit both are already in the UART buffer.
        else if (cmd == 'q' || cmd == 'e') {
            lcd_printf("Manual Turn");
            char temp = cmd;
            lcd_printf("Before CHARACTER RECIEVED");
            char char_received = uart_receive();
            lcd_printf("CHARACTER RECIEVED");
            manual_turn(sensor, temp, char_received);
            uart_sendStr("DONE\n");
            lcd_printf("HARVESTBOT READY");
        }

        // Harvest (X)
        else if (cmd == 'x') {
            lcd_printf("Harvesting!");
            harvestFruit();
            lcd_printf("HARVESTBOT READY");
        }

        // Honk (G)
        else if (cmd == 'g') {
            lcd_printf("HONK!");
            honk();
            lcd_printf("HARVESTBOT READY");
        }
    }
}

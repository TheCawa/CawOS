#include "drivers/rtc.h"
#include "drivers/io.h"

static uint8_t cmos_read(uint8_t reg) {
    port_byte_out(0x70, reg);
    return port_byte_in(0x71);
}

uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}
void rtc_get_time(rtc_time_t* time) {
    while (cmos_read(0x0A) & 0x80);
    time->second = cmos_read(0x00);
    time->minute = cmos_read(0x02);
    time->hour   = cmos_read(0x04);
    time->day    = cmos_read(0x07);
    time->month  = cmos_read(0x08);
    time->year   = cmos_read(0x09);
    time->second = bcd_to_bin(time->second);
    time->minute = bcd_to_bin(time->minute);
    time->hour   = bcd_to_bin(time->hour);
    time->day    = bcd_to_bin(time->day);
    time->month  = bcd_to_bin(time->month);
    time->year   = bcd_to_bin(time->year);
    time->year += 2000;
}
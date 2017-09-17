#include "spi.h"
#include "usb.h"

#include <string.h>


int main(void) {
	usb_init();
    spi_init();
    USB_MouseReport_Data_t mouse_report;

    int x = 0;
	while(1) {
        while(spi_transfer(0) != 0xFF);
        uint8_t x = spi_transfer(0);
        uint8_t y = spi_transfer(0);
        uint8_t b = spi_transfer(0);
        mouse_report.X = x;
        mouse_report.Y = y;
        mouse_report.Button = b;
        x %= 127;
        usb_mouse_send_report(mouse_report);
	}
}


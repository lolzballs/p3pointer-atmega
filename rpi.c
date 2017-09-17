#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define SPI_DEV "/dev/spidev0.0"

int transfer(int fd, uint8_t x, uint8_t y, uint8_t b) {
    uint8_t coords[] = { 0xFF, x, y, b };

    return write(fd, coords, 4);
}

int main() {
       printf("Open\n");
    int fd = open(SPI_DEV, O_RDWR);
    if (fd < 0) {
        perror("Could not create");
    }

    printf("IOctl\n");
    int speed = 100000;
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    int o = 0;
    while (1) {
        uint8_t x, y, b;
        printf("Pre scanf\n");
        scanf("%hhu%hhu%hhu", &x, &y, &b);
        printf("Sending: %d %d %d\n", x, y, b);
        int ret = transfer(fd, x, y, b);
        //int ret = transfer(fd, o, o, 0);
        o++;
        o %= 127;

        x %= 127;
        if (ret < 0) {
            perror("Could not send");
        }

        usleep(10000);
    }
}

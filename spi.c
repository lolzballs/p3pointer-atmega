#include "spi.h"

#include <avr/pgmspace.h>
#include <avr/io.h>

void spi_init(void) {
    DDRB = _BV(DDB2) | _BV(DDB0); // MISO is output
    PORTB = 0;
    SPCR = _BV(SPE);
}

uint8_t spi_transfer(uint8_t v) {
    SPDR = v;
    while(!(SPSR & _BV(SPIF)));
    return SPDR;
}

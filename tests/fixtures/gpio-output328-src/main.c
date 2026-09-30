/* GPIO output: drives PD4 HIGH, PD5 LOW on startup, then swaps when button pressed.
   Wokwi parity: expect-pin (pin_voltage) + set-control sequence. */
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

static void uart_init(void) {
    const uint16_t ubrr = 103;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xff);
    UCSR0B = _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void uart_putc(char c) {
    while (!(UCSR0A & _BV(UDRE0))) {}
    UDR0 = c;
}

static void uart_puts(const char* s) {
    while (*s) uart_putc(*s++);
}

int main(void) {
    uart_init();

    DDRD  |= _BV(PD4) | _BV(PD5);
    PORTD |=  _BV(PD4);
    PORTD &= ~_BV(PD5);

    _delay_ms(100);
    uart_puts("gpio-ready\r\n");
    uart_puts("pd4:high pd5:low\r\n");

    DDRD  &= ~_BV(PD2);
    PORTD |=  _BV(PD2);

    for (;;) {
        if (!((PIND >> PD2) & 1)) {
            PORTD ^= _BV(PD4) | _BV(PD5);
            uart_puts("gpio-toggled\r\n");
            _delay_ms(200);
        }
    }
}

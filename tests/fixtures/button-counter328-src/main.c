/* Button counter: counts presses on PD2, prints count on each press.
   Wokwi parity: repeated set-control + wait-serial count pattern. */
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

static void uart_putu(uint16_t v) {
    char buf[8];
    uint8_t i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) uart_putc(buf[i]);
}

int main(void) {
    uart_init();

    DDRD  &= ~_BV(PD2);
    PORTD |=  _BV(PD2);

    _delay_ms(100);
    uart_puts("counter-ready\r\n");

    uint8_t last = 1;
    uint16_t count = 0;
    for (;;) {
        uint8_t cur = (PIND >> PD2) & 1;
        if (cur != last) {
            _delay_ms(5);
            cur = (PIND >> PD2) & 1;
            if (cur != last) {
                last = cur;
                if (cur == 0) {
                    count++;
                    uart_puts("press:");
                    uart_putu(count);
                    uart_puts("\r\n");
                }
            }
        }
    }
}

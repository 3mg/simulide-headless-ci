/* Button detect: monitors PD2 (external interrupt), prints pressed/released.
   Wokwi parity: set-control (Push button) + wait-serial pattern. */
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

    DDRD  &= ~_BV(PD2);
    PORTD |=  _BV(PD2);  // pull-up

    _delay_ms(100);
    uart_puts("btn-ready\r\n");

    uint8_t last = 1;
    for (;;) {
        uint8_t cur = (PIND >> PD2) & 1;
        if (cur != last) {
            _delay_ms(5);  // simple debounce
            cur = (PIND >> PD2) & 1;
            if (cur != last) {
                last = cur;
                if (cur == 0) uart_puts("btn-pressed\r\n");
                else          uart_puts("btn-released\r\n");
            }
        }
    }
}

/* Serial echo + GPIO toggle: reads bytes from UART and echoes "got:<c>\r\n".
   'H' drives PD4 high, 'L' drives PD4 low.
   Wokwi parity: write-serial (send_serial) + expect-pin (pin_state). */
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

static void uart_init(void) {
    const uint16_t ubrr = 103; /* 9600 baud @ 16MHz */
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xff);
    UCSR0B = _BV(TXEN0) | _BV(RXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void uart_putc(char c) {
    while (!(UCSR0A & _BV(UDRE0))) {}
    UDR0 = c;
}

static void uart_puts(const char* s) {
    while (*s) uart_putc(*s++);
}

static uint8_t uart_available(void) {
    return (UCSR0A & _BV(RXC0)) != 0;
}

static char uart_getc(void) {
    while (!uart_available()) {}
    return (char)UDR0;
}

int main(void) {
    uart_init();

    DDRD  |= _BV(PD4);
    PORTD &= ~_BV(PD4);

    _delay_ms(100);
    uart_puts("echo-ready\r\n");

    for (;;) {
        if (uart_available()) {
            char c = uart_getc();
            if (c == 'H')      PORTD |=  _BV(PD4);
            else if (c == 'L') PORTD &= ~_BV(PD4);

            uart_puts("got:");
            uart_putc(c);
            uart_puts("\r\n");
        }
    }
}

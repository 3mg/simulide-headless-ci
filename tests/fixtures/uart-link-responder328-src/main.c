#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

static void uart_init(void) {
    const uint16_t ubrr = 103; // 9600 baud @ 16 MHz
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xff);
    UCSR0B = _BV(TXEN0) | _BV(RXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void uart_putc(char c) {
    while (!(UCSR0A & _BV(UDRE0))) {
    }
    UDR0 = c;
}

static void uart_puts(const char* s) {
    while (*s) uart_putc(*s++);
}

int main(void) {
    uart_init();

    char buf[6] = {0};
    uint8_t pos = 0;
    for (;;) {
        while (!(UCSR0A & _BV(RXC0))) {
        }
        char c = UDR0;
        if (c == '\r' || c == '\n') {
            if (pos >= 4 &&
                buf[0] == 'P' && buf[1] == 'I' &&
                buf[2] == 'N' && buf[3] == 'G') {
                _delay_ms(20);
                uart_puts("PONG\r\n");
            }
            pos = 0;
            continue;
        }
        if (pos < sizeof(buf) - 1) buf[pos++] = c;
    }
}

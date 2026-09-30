#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

static void uart_init(void) {
    const uint16_t ubrr = 103; // 9600 baud @ 16 MHz
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xff);
    UCSR0B = _BV(TXEN0);
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
    DDRD &= (uint8_t)~_BV(PD2);
    PORTD &= (uint8_t)~_BV(PD2);
    _delay_ms(50);

    uint8_t last = (PIND & _BV(PD2)) ? 1 : 0;
    uint8_t rises = 0;
    while (rises < 8) {
        uint8_t now = (PIND & _BV(PD2)) ? 1 : 0;
        if (!last && now) rises++;
        last = now;
    }

    uart_puts("clock-count-ok\r\n");
    for (;;) {
    }
}

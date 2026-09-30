/* ADC continuous: reads ADC0 every 200ms, prints classified value.
   Wokwi parity: set-control (pot) + wait-serial pattern. */
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

static void adc_init(void) {
    ADMUX  = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

static uint16_t adc_read(void) {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {}
    return ADC;
}

int main(void) {
    uart_init();
    adc_init();
    _delay_ms(100);
    uart_puts("adc-ready\r\n");

    for (;;) {
        _delay_ms(50);
        uint16_t v = adc_read();
        uart_puts("adc:");
        uart_putu(v);
        uart_puts("\r\n");
        if (v < 100)        uart_puts("adc-low\r\n");
        else if (v > 900)   uart_puts("adc-high\r\n");
        else                uart_puts("adc-mid\r\n");
    }
}

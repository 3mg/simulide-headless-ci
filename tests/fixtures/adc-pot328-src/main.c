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

static void adc_init(void) {
    ADMUX = _BV(REFS0); // AVcc reference, ADC0 channel
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

static uint16_t adc_read(void) {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
    return ADC;
}

int main(void) {
    uart_init();
    adc_init();
    _delay_ms(100);

    uint16_t value = adc_read();
    if (value > 400 && value < 700) uart_puts("adc-mid\r\n");
    else if (value >= 700) uart_puts("adc-high\r\n");
    else uart_puts("adc-low\r\n");

    for (;;) {
    }
}

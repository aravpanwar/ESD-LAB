#include <LPC17xx.h>

void lcd_init() {
    // LCD initialization commands (4-bit mode)
    unsigned char init_cmds[] = {0x30,0x30,0x30,0x20,0x28,0x0C,0x06,0x01,0x80};
    for(int i=0; i<9; i++) {
        // Send each command to LCD
    }
}

void lcd_puts(char *str) {
    while(*str) {
        // Send character to LCD
        str++;
    }
}

void lcd_gotoxy(int x, int y) {
    // Set LCD cursor position
    unsigned char addr = (y==0) ? (0x80+x) : (0xC0+x);
    // Send command to LCD
}

int main(void) {
    unsigned int adc_ch4, adc_ch5, diff;
    char buffer[16];
    
    SystemInit();
    
    // 1. Power up ADC
    LPC_SC->PCONP |= (1 << 12);
    
    // 2. Configure ADC pins (P1.30 = AD0.4, P1.31 = AD0.5)
    LPC_PINCON->PINSEL3 |= (0x5 << 28);  // Set to ADC function
    
    // 3. Initialize LCD
    lcd_init();
    
    while(1) {
        // 4. Read ADC channel 4
        LPC_ADC->ADCR = (1<<4) | (1<<21) | (1<<24);  // Start CH4
        while((LPC_ADC->ADGDR & (1<<31)) == 0);      // Wait for conversion
        adc_ch4 = (LPC_ADC->ADGDR >> 4) & 0xFFF;     // Get 12-bit result
        
        // 5. Read ADC channel 5
        LPC_ADC->ADCR = (1<<5) | (1<<21) | (1<<24);  // Start CH5
        while((LPC_ADC->ADGDR & (1<<31)) == 0);
        adc_ch5 = (LPC_ADC->ADGDR >> 4) & 0xFFF;
        
        // 6. Calculate difference (absolute value)
        diff = (adc_ch5 > adc_ch4) ? (adc_ch5 - adc_ch4) : (adc_ch4 - adc_ch5);
        
        // 7. Display on LCD
        lcd_gotoxy(0, 0);
        sprintf(buffer, "CH4:%04d", adc_ch4);
        lcd_puts(buffer);
        
        lcd_gotoxy(0, 1);
        sprintf(buffer, "CH5:%04d DIFF:%04d", adc_ch5, diff);
        lcd_puts(buffer);
        
        // 8. Delay before next reading
        for(int i=0; i<1000000; i++);
    }
    
    return 0;
}

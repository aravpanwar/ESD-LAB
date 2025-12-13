#include <LPC17xx.h>

// Function prototypes
void delay_ms(unsigned int ms);
void display_BCD(unsigned int count);
void init_timer0(void);

// 7-segment display patterns for BCD (0-9, common cathode)
const unsigned char seg_pattern[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

int main(void) {
    unsigned int counter = 9999;  // Start from 9999 (4-digit BCD max)
    unsigned int digit1, digit2, digit3, digit4;
    
    // Initialize GPIO for 7-segment display
    // Assuming segment pins on PORT1 (P1.0 to P1.6 for segments a-g, P1.7 for decimal point)
    // Assuming digit select pins on PORT2 (P2.0 to P2.3 for digits 1-4)
    LPC_GPIO1->FIODIR = 0xFF;  // Set P1.0-P1.7 as output for segments
    LPC_GPIO2->FIODIR = 0x0F;  // Set P2.0-P2.3 as output for digit selection
    
    // Initialize Timer0 for delay
    init_timer0();
    
    SystemInit();  // Initialize system clock
    
    while(1) {
        // Extract BCD digits
        digit1 = (counter / 1000) % 10;    // Thousands digit
        digit2 = (counter / 100) % 10;     // Hundreds digit
        digit3 = (counter / 10) % 10;      // Tens digit
        digit4 = counter % 10;             // Units digit
        
        // Display counter value (multiplexed display)
        display_BCD(counter);
        
        // Delay for 1 second between count updates
        delay_ms(1000);
        
        // Decrement counter
        if(counter > 0) {
            counter--;
        } else {
            counter = 9999;  // Reset to max when reaching 0
        }
    }
}

void init_timer0(void) {
    // Enable Timer0 clock
    LPC_SC->PCONP |= (1 << 1);
    
    // Reset Timer0
    LPC_TIM0->TCR = 0x02;
    
    // Set prescaler for 1ms tick (assuming 72MHz system clock)
    // Prescaler = System Clock / (1000 * desired frequency in Hz)
    // For 1ms: 72000000 / (1000 * 1) = 72000
    LPC_TIM0->PR = 72000 - 1;  // Timer increments every 1ms
    
    // Reset timer
    LPC_TIM0->TCR = 0x01;
}

void delay_ms(unsigned int ms) {
    unsigned int i;
    
    for(i = 0; i < ms; i++) {
        // Reset timer counter
        LPC_TIM0->TC = 0;
        
        // Wait until TC reaches 1 (1ms)
        while(LPC_TIM0->TC < 1);
    }
}

void display_BCD(unsigned int count) {
    unsigned int i, j;
    unsigned int digits[4];
    unsigned char digit_select = 0x01;
    
    // Extract individual digits
    digits[0] = (count / 1000) % 10;    // Thousands
    digits[1] = (count / 100) % 10;     // Hundreds
    digits[2] = (count / 10) % 10;      // Tens
    digits[3] = count % 10;             // Units
    
    // Multiplex the display - show each digit briefly in sequence
    for(i = 0; i < 4; i++) {
        // Select digit
        LPC_GPIO2->FIOCLR = 0x0F;           // Turn off all digits
        LPC_GPIO2->FIOSET = digit_select;   // Turn on current digit
        
        // Display digit pattern
        LPC_GPIO1->FIOPIN = seg_pattern[digits[i]];
        
        // Small delay for multiplexing (5ms per digit = 20ms total refresh)
        for(j = 0; j < 1000; j++);  // Simple software delay
        
        // Move to next digit
        digit_select <<= 1;
    }
}

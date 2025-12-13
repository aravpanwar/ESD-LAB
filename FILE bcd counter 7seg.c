/******************************************************************************
 * FILE: bcd_counter_7seg.c
 * DESCRIPTION: 4-digit BCD up/down counter on 7-segment display with switch
 *              control and 1-second timer delay
 * MICROCONTROLLER: LPC1768 (Cortex-M3)
 * AUTHOR: Embedded Systems Lab
 * DATE: Created for Lab Exam
 ******************************************************************************/

#include <LPC17xx.h>

/*=============================================================================
 * HARDWARE PIN DEFINITIONS
 * Based on ALS-SDA-ARMCTXM3-01 Board
 *============================================================================*/

/* 7-SEGMENT DATA LINES: Connected to P0.4 through P0.11
 * These 8 pins control the segments (a,b,c,d,e,f,g,decimal point)
 * P0.4 = Segment a, P0.5 = Segment b, ..., P0.11 = Segment h (decimal point)
 */
#define DATA_PORT       LPC_GPIO0           // GPIO Port 0 for segment data
#define DATA_MASK       0x00000FF0          // Mask for P0.4-P0.11 (bits 4-11)
                                            // Binary: 0000 0000 0000 0000 0000 1111 1111 0000

/* 7-SEGMENT ENABLE LINES: Connected to P1.23 through P1.26
 * Each pin enables one of the four 7-segment digits
 * P1.23 = Digit 1 (leftmost), P1.24 = Digit 2, P1.25 = Digit 3, P1.26 = Digit 4 (rightmost)
 */
#define ENABLE_PORT     LPC_GPIO1           // GPIO Port 1 for digit enables
#define ENABLE_ALL      0x07800000          // Mask for P1.23-P1.26 (bits 23-26)
                                            // Binary: 0000 0111 1000 0000 0000 0000 0000 0000

/* Individual digit enable masks for precise control */
#define DIGIT_1         0x00800000          // P1.23 - Enable first digit
#define DIGIT_2         0x01000000          // P1.24 - Enable second digit
#define DIGIT_3         0x02000000          // P1.25 - Enable third digit
#define DIGIT_4         0x04000000          // P1.26 - Enable fourth digit

/* CONTROL SWITCH (SW2): Connected to P2.12
 * This switch controls counting direction:
 *   - When SW2 is NOT pressed (logic HIGH): Count UP
 *   - When SW2 is PRESSED (logic LOW): Count DOWN
 * The switch has external pull-up resistor, so pressed = 0, released = 1
 */
#define SWITCH_PORT     LPC_GPIO2           // GPIO Port 2 for switch
#define SWITCH_PIN      (1 << 12)           // Bit 12 corresponds to P2.12
#define SWITCH_PRESSED  0                   // Logic level when switch is pressed
#define SWITCH_RELEASED 1                   // Logic level when switch is released

/*=============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/

/* BCD COUNTER VARIABLE
 * This is a 16-bit variable that stores the 4-digit BCD value
 * Each nibble (4 bits) represents one decimal digit:
 *   - Bits 15-12: Thousands digit (0-9)
 *   - Bits 11-8:  Hundreds digit (0-9)
 *   - Bits 7-4:   Tens digit (0-9)
 *   - Bits 3-0:   Units digit (0-9)
 * Example: 0x1234 means 1234 in decimal
 */
unsigned int bcd_counter = 0x0000;          // Initialize to 0000

/* COUNTING DIRECTION
 * 1 = Counting UP (increment)
 * 0 = Counting DOWN (decrement)
 * Default direction is UP
 */
unsigned char counting_direction = 1;       // 1 = UP, 0 = DOWN

/* DISPLAY MULTIPLEXING VARIABLE
 * Tracks which digit is currently being displayed
 * Range: 0 (Digit 1) to 3 (Digit 4)
 * This variable cycles continuously for multiplexing
 */
unsigned char current_digit = 0;            // Start with first digit

/* TIMER VARIABLE for 1-second delay
 * Incremented in main loop, used to approximate 1-second intervals
 * For precise timing, a hardware timer should be used
 */
unsigned int timer_accumulator = 0;         // Accumulates time for 1-second check

/* 7-SEGMENT LOOKUP TABLE for BCD digits 0-9
 * Each entry defines which segments to light for that digit
 * Segment mapping: bit 0 = segment a, bit 1 = segment b, ..., bit 7 = segment h (decimal point)
 * Common cathode display: Segment lights when corresponding pin is HIGH
 * Values are for active-high segments:
 *   0x3F = 00111111 = segments a,b,c,d,e,f ON for digit '0'
 *   0x06 = 00000110 = segments b,c ON for digit '1'
 *   etc.
 */
const unsigned char bcd_seg_table[10] = {
    0x3F,   // 0: segments a,b,c,d,e,f
    0x06,   // 1: segments b,c
    0x5B,   // 2: segments a,b,d,e,g
    0x4F,   // 3: segments a,b,c,d,g
    0x66,   // 4: segments b,c,f,g
    0x6D,   // 5: segments a,c,d,f,g
    0x7D,   // 6: segments a,c,d,e,f,g
    0x07,   // 7: segments a,b,c
    0x7F,   // 8: all segments a,b,c,d,e,f,g
    0x6F    // 9: segments a,b,c,d,f,g
};

/*=============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/
void initialize_gpio(void);                 // Initialize all GPIO pins
void initialize_timer0(void);               // Initialize Timer0 for 1-second interrupts
void display_digit(unsigned char digit_position, unsigned char bcd_value); // Display one digit
unsigned char extract_bcd_digit(unsigned int bcd_number, unsigned char position); // Get digit from BCD
void update_bcd_counter(void);              // Increment/decrement BCD counter
void delay_microseconds(unsigned int us);   // Simple delay function
void delay_milliseconds(unsigned int ms);   // Millisecond delay

/*=============================================================================
 * TIMER0 INTERRUPT HANDLER
 * This function is called automatically by hardware every 1 second
 * when Timer0 match occurs. It updates the BCD counter.
 *============================================================================*/
void TIMER0_IRQHandler(void) {
    /* Check if interrupt is from MR0 (Match Register 0) */
    if (LPC_TIM0->IR & (1 << 0)) {
        /* Clear the interrupt flag - IMPORTANT: Must clear to prevent infinite interrupts */
        LPC_TIM0->IR = (1 << 0);
        
        /* Read switch state to determine counting direction */
        if (SWITCH_PORT->FIOPIN & SWITCH_PIN) {
            counting_direction = 1;         // Switch NOT pressed = count UP
        } else {
            counting_direction = 0;         // Switch PRESSED = count DOWN
        }
        
        /* Update BCD counter based on direction */
        update_bcd_counter();
    }
}

/*=============================================================================
 * MAIN FUNCTION - Program entry point
 *============================================================================*/
int main(void) {
    unsigned char i;
    unsigned char digit_value;
    
    /* Step 1: SYSTEM INITIALIZATION
     * Configure system clocks and peripherals
     */
    SystemInit();                           // Initialize system clock
    SystemCoreClockUpdate();                // Update system core clock variable
    
    /* Step 2: GPIO INITIALIZATION
     * Configure data lines, enable lines, and switch
     */
    initialize_gpio();
    
    /* Step 3: TIMER INITIALIZATION
     * Configure Timer0 for 1-second interrupts
     */
    initialize_timer0();
    
    /* Step 4: INITIAL DISPLAY CLEAR
     * Turn off all segments and digits initially
     */
    DATA_PORT->FIOCLR = DATA_MASK;          // Clear all segment data lines
    ENABLE_PORT->FIOCLR = ENABLE_ALL;       // Disable all digits
    
    /* Step 5: MAIN SUPERVISORY LOOP
     * This loop runs forever, handling display multiplexing
     * Timer interrupts handle the 1-second counter updates
     */
    while (1) {
        /* DISPLAY MULTIPLEXING LOOP
         * Display each digit one at a time with short persistence
         * Human eye persistence creates illusion of all digits lit simultaneously
         */
        for (i = 0; i < 4; i++) {
            /* Extract BCD digit from appropriate position
             * Position: 0 = thousands, 1 = hundreds, 2 = tens, 3 = units
             */
            digit_value = extract_bcd_digit(bcd_counter, i);
            
            /* Display this digit on the corresponding 7-segment display */
            display_digit(i, digit_value);
            
            /* PERSISTENCE DELAY
             * Each digit displayed for 2ms, total cycle = 8ms
             * Refresh rate = 1/0.008 = 125Hz (flicker-free)
             * Adjust this delay if display appears dim or flickers
             */
            delay_milliseconds(2);
        }
        
        /* OPTIONAL: Add small delay between multiplexing cycles
         * Helps with display stability
         */
        delay_microseconds(100);
    }
    
    return 0;  // Never reached, but included for completeness
}

/*=============================================================================
 * GPIO INITIALIZATION FUNCTION
 * Configures all pins for 7-segment display and switch
 *============================================================================*/
void initialize_gpio(void) {
    /* PART A: CONFIGURE 7-SEGMENT DATA LINES (P0.4-P0.11)
     * These pins need to be outputs to control segment illumination
     */
    LPC_GPIO0->FIODIR |= DATA_MASK;         // Set P0.4-P0.11 as outputs
    /* Note: By default, these pins are GPIO. If they were used for other
     * functions, we would need to clear PINSEL0 bits 9-22 */
    
    /* PART B: CONFIGURE 7-SEGMENT ENABLE LINES (P1.23-P1.26)
     * These pins need to be outputs to enable individual digits
     */
    LPC_GPIO1->FIODIR |= ENABLE_ALL;        // Set P1.23-P1.26 as outputs
    /* Clear any alternate function selection for these pins */
    LPC_PINCON->PINSEL3 &= ~(0xFF << 14);   // Clear bits 15:14, 17:16, 19:18, 21:20
    
    /* PART C: CONFIGURE CONTROL SWITCH (P2.12)
     * This pin needs to be input to read switch state
     */
    /* First ensure P2.12 is configured as GPIO (not alternate function)
     * P2.12 uses PINSEL4 bits 25:24 (binary position: 24+1=25, 24+0=24)
     * We need to write 00 to these bits for GPIO function
     */
    LPC_PINCON->PINSEL4 &= ~(3 << 24);      // Clear bits 25:24 -> sets to GPIO
    
    /* Now configure as input by clearing the direction bit */
    LPC_GPIO2->FIODIR &= ~SWITCH_PIN;       // Set P2.12 as input
}

/*=============================================================================
 * TIMER0 INITIALIZATION FUNCTION
 * Configures Timer0 to generate interrupts every 1 second
 * System clock = 72MHz, we want 1Hz interrupt
 *============================================================================*/
void initialize_timer0(void) {
    /* Step 1: POWER UP TIMER0
     * Timer peripherals are disabled by default to save power
     * Need to enable Timer0 in Power Control for Peripherals register
     */
    LPC_SC->PCONP |= (1 << 1);              // Bit 1 = Timer0 power control
    
    /* Step 2: CONFIGURE TIMER MODE
     * CTCR = Counter/Timer Control Register
     * 0x00 = Timer mode (counts using peripheral clock)
     */
    LPC_TIM0->CTCR = 0x00;                  // Timer mode (not counter mode)
    
    /* Step 3: SET PRESCALER FOR 1ms TICK
     * PR = Prescale Register
     * Formula: PR = (SystemClock / DesiredTickRate) - 1
     * SystemClock = 72MHz = 72,000,000 Hz
     * Desired tick = 1ms = 1000Hz
     * PR = (72,000,000 / 1000) - 1 = 71999
     * This gives us 1ms interrupts
     */
    LPC_TIM0->PR = 71999;                   // 72MHz/72000 = 1KHz = 1ms
    
    /* Step 4: SET MATCH REGISTER FOR 1 SECOND
     * MR0 = Match Register 0
     * We want interrupt every 1000ms = 1000 * 1ms ticks
     */
    LPC_TIM0->MR0 = 1000;                   // Match every 1000ms = 1 second
    
    /* Step 5: CONFIGURE MATCH CONTROL
     * MCR = Match Control Register
     * Bit 0: Interrupt on MR0 match
     * Bit 1: Reset timer on MR0 match
     * Bit 2: Stop timer on MR0 match (not used here)
     * We want interrupt + reset to create periodic interrupts
     */
    LPC_TIM0->MCR = (1 << 0) | (1 << 1);    // Interrupt + Reset on MR0 match
    
    /* Step 6: RESET AND START TIMER
     * TCR = Timer Control Register
     * First reset the timer, then enable it
     */
    LPC_TIM0->TCR = 0x02;                   // Reset timer (bit 1 = 1)
    LPC_TIM0->TCR = 0x01;                   // Enable timer (bit 0 = 1)
    
    /* Step 7: ENABLE TIMER INTERRUPT IN NVIC
     * NVIC = Nested Vectored Interrupt Controller
     * TIMER0_IRQn = IRQ number for Timer0 (defined in LPC17xx.h)
     */
    NVIC_EnableIRQ(TIMER0_IRQn);            // Enable Timer0 interrupts in NVIC
    
    /* Step 8: SET INTERRUPT PRIORITY (Optional)
     * Lower number = higher priority
     * Useful when multiple interrupts are used
     */
    NVIC_SetPriority(TIMER0_IRQn, 3);       // Medium priority
}

/*=============================================================================
 * DISPLAY DIGIT FUNCTION
 * Displays a single BCD digit on specified 7-segment display
 * Parameters:
 *   digit_position: 0=thousands, 1=hundreds, 2=tens, 3=units
 *   bcd_value: 0-9 to display
 *============================================================================*/
void display_digit(unsigned char digit_position, unsigned char bcd_value) {
    unsigned long enable_mask = 0;
    
    /* STEP 1: DETERMINE WHICH DIGIT TO ENABLE
     * Based on digit_position, select the appropriate enable mask
     */
    switch (digit_position) {
        case 0:  // First digit (thousands, leftmost)
            enable_mask = DIGIT_1;          // P1.23
            break;
        case 1:  // Second digit (hundreds)
            enable_mask = DIGIT_2;          // P1.24
            break;
        case 2:  // Third digit (tens)
            enable_mask = DIGIT_3;          // P1.25
            break;
        case 3:  // Fourth digit (units, rightmost)
            enable_mask = DIGIT_4;          // P1.26
            break;
        default: // Should never happen, but for safety
            enable_mask = 0;
            break;
    }
    
    /* STEP 2: DISABLE ALL DIGITS FIRST
     * Important for multiplexing: Only one digit should be active at a time
     * Otherwise, same segment data would appear on multiple digits
     */
    ENABLE_PORT->FIOCLR = ENABLE_ALL;       // Turn off all digits
    
    /* STEP 3: SEND SEGMENT DATA FOR THIS DIGIT
     * First clear all segment data lines
     * Then set appropriate segments based on lookup table
     */
    DATA_PORT->FIOCLR = DATA_MASK;          // Clear all segments
    
    /* Get segment pattern from lookup table
     * Shift left by 4 because segments start at P0.4 (not P0.0)
     * Mask with DATA_MASK to ensure only P0.4-P0.11 are affected
     */
    DATA_PORT->FIOSET = (bcd_seg_table[bcd_value] << 4) & DATA_MASK;
    
    /* STEP 4: ENABLE THE SELECTED DIGIT
     * This turns on the transistor that connects the common cathode to ground
     * Only this digit will now display the segment pattern
     */
    ENABLE_PORT->FIOSET = enable_mask;      // Enable this digit only
}

/*=============================================================================
 * EXTRACT BCD DIGIT FUNCTION
 * Extracts a specific digit from a 4-digit BCD number
 * Parameters:
 *   bcd_number: 16-bit BCD number (each nibble = one digit)
 *   position: 0=thousands, 1=hundreds, 2=tens, 3=units
 * Returns: Digit value 0-9
 *============================================================================*/
unsigned char extract_bcd_digit(unsigned int bcd_number, unsigned char position) {
    unsigned char shift_amount;
    
    /* CALCULATE SHIFT AMOUNT
     * Thousands digit: shift right 12 bits (3 nibbles * 4 bits/nibble)
     * Hundreds digit:  shift right 8 bits (2 nibbles * 4 bits/nibble)
     * Tens digit:      shift right 4 bits (1 nibble * 4 bits/nibble)
     * Units digit:     shift right 0 bits
     */
    shift_amount = (3 - position) * 4;      // position 0→12, 1→8, 2→4, 3→0
    
    /* EXTRACT AND RETURN THE DIGIT
     * 1. Shift right to bring desired digit to bits 3-0
     * 2. Mask with 0x0F to isolate lowest 4 bits
     */
    return ((bcd_number >> shift_amount) & 0x0F);
}

/*=============================================================================
 * UPDATE BCD COUNTER FUNCTION
 * Increments or decrements BCD counter with proper BCD arithmetic
 * Handles carry/borrow between digits and wrap-around
 *============================================================================*/
void update_bcd_counter(void) {
    unsigned char units, tens, hundreds, thousands;
    
    if (counting_direction == 1) {
        /* COUNTING UP (INCREMENT) */
        
        /* Extract individual BCD digits */
        units = bcd_counter & 0x000F;       // Isolate units digit
        tens = (bcd_counter >> 4) & 0x000F; // Isolate tens digit
        hundreds = (bcd_counter >> 8) & 0x000F; // Isolate hundreds digit
        thousands = (bcd_counter >> 12) & 0x000F; // Isolate thousands digit
        
        /* Increment units digit with BCD carry */
        units++;
        if (units > 9) {                    // BCD carry from units to tens
            units = 0;
            tens++;
            if (tens > 9) {                 // BCD carry from tens to hundreds
                tens = 0;
                hundreds++;
                if (hundreds > 9) {         // BCD carry from hundreds to thousands
                    hundreds = 0;
                    thousands++;
                    if (thousands > 9) {    // Thousands overflow, wrap to 0000
                        thousands = 0;
                    }
                }
            }
        }
        
        /* Recombine digits into BCD counter */
        bcd_counter = (thousands << 12) | (hundreds << 8) | (tens << 4) | units;
        
    } else {
        /* COUNTING DOWN (DECREMENT) */
        
        /* Extract individual BCD digits */
        units = bcd_counter & 0x000F;
        tens = (bcd_counter >> 4) & 0x000F;
        hundreds = (bcd_counter >> 8) & 0x000F;
        thousands = (bcd_counter >> 12) & 0x000F;
        
        /* Decrement units digit with BCD borrow */
        if (units == 0) {                   // Need to borrow from tens
            units = 9;
            if (tens == 0) {                // Need to borrow from hundreds
                tens = 9;
                if (hundreds == 0) {        // Need to borrow from thousands
                    hundreds = 9;
                    if (thousands == 0) {   // Underflow, wrap to 9999
                        thousands = 9;
                        hundreds = 9;
                        tens = 9;
                        units = 9;
                    } else {
                        thousands--;
                    }
                } else {
                    hundreds--;
                }
            } else {
                tens--;
            }
        } else {
            units--;
        }
        
        /* Recombine digits into BCD counter */
        bcd_counter = (thousands << 12) | (hundreds << 8) | (tens << 4) | units;
    }
}

/*=============================================================================
 * DELAY FUNCTIONS
 * Simple software delays for timing control
 * Note: These are approximate and affected by compiler optimization
 * For precise timing, use hardware timers
 *============================================================================*/

/* MICROSECOND DELAY - Approximate */
void delay_microseconds(unsigned int us) {
    unsigned int i;
    /* Loop count needs calibration based on clock speed
     * Approximate: 72MHz → ~72 cycles per microsecond
     */
    for (i = 0; i < (us * 24); i++) {
        __asm("nop");                       // No operation assembly instruction
    }
}

/* MILLISECOND DELAY - Approximate */
void delay_milliseconds(unsigned int ms) {
    unsigned int i, j;
    /* Nested loops for longer delays
     * Inner loop calibrated for approximately 1ms
     */
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 10000; j++) {
            // Empty loop - compiler may optimize away
            // Using volatile to prevent optimization
            volatile int k = 0;
            k = k + 1;
        }
    }
}

/*=============================================================================
 * ALTERNATIVE: POLLING VERSION (Without Timer Interrupt)
 * Uncomment this main() function and comment out the timer interrupt
 * code if you want a simpler polling-based solution
 *============================================================================*/
/*
int main_polling_version(void) {
    unsigned int i;
    unsigned char digit_value;
    unsigned int loop_counter = 0;
    
    SystemInit();
    initialize_gpio();  // Same GPIO setup
    
    // No timer interrupt setup
    
    while (1) {
       

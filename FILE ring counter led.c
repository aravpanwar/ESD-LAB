/******************************************************************************
 * FILE: ring_counter_led.c
 * DESCRIPTION: 8-bit Ring Counter on LEDs controlled by key press (SW2)
 * MICROCONTROLLER: LPC1768 (Cortex-M3)
 * HARDWARE: 8 LEDs on P0.4-P0.11, SW2 on P2.12
 * OPERATION: Press SW2 to shift the single lit LED in ring pattern
 ******************************************************************************/

#include <LPC17xx.h>

// ==================== HARDWARE DEFINITIONS ====================

/* LED CONNECTIONS: 8 LEDs connected to P0.4 through P0.11
 * LED0 (LSB) on P0.4, LED1 on P0.5, ..., LED7 (MSB) on P0.11
 * When pin is HIGH (1), LED turns ON (assuming active-high LEDs)
 */
#define LED_PORT    LPC_GPIO0          // Port 0 for LEDs
#define LED_MASK    0x00000FF0         // Mask for P0.4-P0.11 (bits 4-11)
                                       // Binary: 0000 0000 0000 0000 0000 1111 1111 0000

/* SWITCH DEFINITION: SW2 connected to P2.12
 * When pressed: P2.12 = 0 (LOW) - switch connects to ground
 * When released: P2.12 = 1 (HIGH) - internal pull-up keeps it high
 */
#define SWITCH_PORT LPC_GPIO2          // Port 2 for switch
#define SWITCH_PIN  (1 << 12)          // Bit 12 = P2.12

// ==================== GLOBAL VARIABLES ====================

/* RING COUNTER STATE
 * Only one bit is '1' at any time, and it shifts circularly
 * Start with LSB (LED0) lit: 00000001 binary = 0x01
 * After 1st press: 00000010 = 0x02
 * After 2nd press: 00000100 = 0x04, etc.
 */
unsigned char ring_counter = 0x01;     // Initial: Only LED0 ON (binary 00000001)

/* PREVIOUS SWITCH STATE
 * Used for detecting button press (edge detection)
 * We detect FALLING edge: 1→0 transition = button press
 */
unsigned char prev_switch_state = 1;   // Start assuming button NOT pressed (HIGH)

// ==================== FUNCTION PROTOTYPES ====================
void init_gpio(void);
void update_leds(void);
void delay_ms(unsigned int ms);
unsigned char is_button_pressed(void);

// ==================== MAIN FUNCTION ====================
int main(void)
{
    SystemInit();                      // Initialize system clock
    
    init_gpio();                       // Setup LED and switch pins
    
    update_leds();                     // Display initial ring counter state
    
    while(1)                           // Infinite loop
    {
        /* CHECK IF BUTTON WAS PRESSED (edge detection) */
        if(is_button_pressed())
        {
            /* SHIFT THE RING COUNTER
             * Example: 00000001 → 00000010 → 00000100 → ... → 10000000 → 00000001
             */
            if(ring_counter == 0x80)   // If MSB is lit (10000000 binary)
            {
                ring_counter = 0x01;   // Wrap around to LSB (00000001)
            }
            else
            {
                ring_counter = ring_counter << 1;  // Shift left by 1 bit
            }
            
            update_leds();             // Update LEDs to show new pattern
            
            /* WAIT UNTIL BUTTON IS RELEASED
             * Prevents multiple triggers from single press
             */
            while(!(SWITCH_PORT->FIOPIN & SWITCH_PIN))
            {
                delay_ms(10);          // Short delay while button held
            }
            
            delay_ms(100);             // Debounce delay after release
        }
        
        delay_ms(10);                  // Small delay in main loop
    }
    
    return 0;                          // Never reached
}

// ==================== GPIO INITIALIZATION ====================
void init_gpio(void)
{
    /* 1. SETUP LED PINS AS OUTPUTS */
    LED_PORT->FIODIR |= LED_MASK;      // Set P0.4-P0.11 as outputs
    
    /* 2. SETUP SWITCH PIN AS INPUT */
    // First: Configure P2.12 as GPIO (not alternate function)
    LPC_PINCON->PINSEL4 &= ~(3 << 24); // Clear bits 25:24 → GPIO mode
    
    // Second: Set as input direction
    SWITCH_PORT->FIODIR &= ~SWITCH_PIN; // Clear bit 12 → input mode
    
    /* 3. INITIALIZE ALL LEDS OFF */
    LED_PORT->FIOCLR = LED_MASK;       // Clear all LED pins (turn OFF)
}

// ==================== UPDATE LED FUNCTION ====================
void update_leds(void)
{
    /* TURN OFF ALL LEDS FIRST */
    LED_PORT->FIOCLR = LED_MASK;       // Clear all bits (LEDs OFF)
    
    /* TURN ON LEDS BASED ON RING COUNTER
     * ring_counter has only one '1' bit, which shifts each press
     * Shift left by 4 because LEDs start at P0.4 (not P0.0)
     * Example: ring_counter = 0x04 (00000100 binary = LED2)
     *          0x04 << 4 = 0x40 (01000000 binary = P0.10)
     */
    LED_PORT->FIOSET = (ring_counter << 4) & LED_MASK;
}

// ==================== BUTTON DETECTION FUNCTION ====================
unsigned char is_button_pressed(void)
{
    unsigned char current_state;
    unsigned char pressed = 0;
    
    /* READ CURRENT SWITCH STATE
     * If bit is 1: switch NOT pressed (pulled high)
     * If bit is 0: switch IS pressed (connected to ground)
     */
    current_state = (SWITCH_PORT->FIOPIN >> 12) & 0x01;
    
    /* DETECT FALLING EDGE (1→0 transition)
     * Previous was HIGH (1) AND Current is LOW (0) = Button just pressed
     */
    if((prev_switch_state == 1) && (current_state == 0))
    {
        pressed = 1;                   // Button was pressed
        
        /* DEBOUNCE: Wait a bit and check again
         * Mechanical switches "bounce" - make multiple contacts
         * We wait 20ms then re-check to confirm real press
         */
        delay_ms(20);                  // Debounce delay
        
        current_state = (SWITCH_PORT->FIOPIN >> 12) & 0x01;
        if(current_state != 0)         // If not still low, it was noise
        {
            pressed = 0;
        }
    }
    
    /* UPDATE PREVIOUS STATE FOR NEXT CHECK */
    prev_switch_state = current_state;
    
    return pressed;                    // Return 1 if pressed, 0 if not
}

// ==================== DELAY FUNCTION ====================
void delay_ms(unsigned int ms)
{
    unsigned int i, j;
    
    /* SIMPLE SOFTWARE DELAY
     * Not precise - varies with compiler and optimization
     * But works for basic timing needs
     */
    for(i = 0; i < ms; i++)            // Outer loop: milliseconds
    {
        for(j = 0; j < 10000; j++)     // Inner loop: creates delay
        {
            // Empty loop - waste CPU cycles
            // Using volatile prevents compiler optimization
            volatile int x = 0;
            x = x + 1;
        }
    }
}

// ==================== ALTERNATIVE: SIMPLER VERSION ====================
/* For very basic implementation without debouncing: */
/*
int main_simple(void)
{
    SystemInit();
    
    // Setup
    LPC_GPIO0->FIODIR |= 0xFF0;        // P0.4-P0.11 output
    LPC_GPIO2->FIODIR &= ~(1<<12);     // P2.12 input
    
    ring_counter = 0x01;               // Start with LED0 ON
    
    while(1)
    {
        // Display current ring pattern
        LPC_GPIO0->FIOPIN = (ring_counter << 4) & 0xFF0;
        
        // Wait for button press (simple polling)
        if((LPC_GPIO2->FIOPIN & (1<<12)) == 0)
        {
            delay_ms(200);             // Simple debounce
            
            // Shift ring counter
            if(ring_counter == 0x80)
                ring_counter = 0x01;
            else
                ring_counter = ring_counter << 1;
            
            // Wait for button release
            while((LPC_GPIO2->FIOPIN & (1<<12)) == 0);
            delay_ms(200);
        }
        
        delay_ms(10);
    }
}
*/

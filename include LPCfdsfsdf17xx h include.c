#include <LPC17xx.h>
#include <stdio.h>
#include <string.h>

// LCD Control Pins (Change according to your connection)
#define LCD_DATA_PORT LPC_GPIO0  // PORT0 for data pins D0-D7
#define LCD_CTRL_PORT LPC_GPIO1  // PORT1 for control pins
#define RS (1 << 16)             // P1.16
#define RW (1 << 17)             // P1.17
#define EN (1 << 18)             // P1.18

// Keypad definitions
#define KEYPAD_PORT LPC_GPIO2
#define ROW1 (1 << 19)  // P2.19
#define ROW2 (1 << 20)  // P2.20
#define ROW3 (1 << 21)  // P2.21
#define ROW4 (1 << 22)  // P2.22
#define COL1 (1 << 23)  // P2.23
#define COL2 (1 << 24)  // P2.24
#define COL3 (1 << 25)  // P2.25

// Function prototypes
void LCD_Init(void);
void LCD_Command(unsigned char cmd);
void LCD_Data(unsigned char data);
void LCD_String(char *str);
void LCD_Clear(void);
void LCD_SetCursor(unsigned char row, unsigned char col);
void delay_ms(unsigned int ms);
char Read_Keypad(void);
void Get_Expression(void);
int BCD_To_Decimal(unsigned char bcd);
unsigned char Decimal_To_BCD(int decimal);
void Display_Result(int result);

// Global variables
char expression[20];
unsigned char first_operand = 0;
unsigned char second_operand = 0;
char operator = 0;
int result = 0;

// Keypad matrix mapping
const char keypad[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

int main(void) {
    SystemInit();
    
    // Initialize LCD
    LCD_Init();
    
    // Initialize Keypad
    KEYPAD_PORT->FIODIR &= ~(COL1 | COL2 | COL3);  // Columns as input
    KEYPAD_PORT->FIODIR |= (ROW1 | ROW2 | ROW3 | ROW4); // Rows as output
    KEYPAD_PORT->FIOPIN |= (COL1 | COL2 | COL3);   // Enable pull-up
    
    LCD_String("Expression Calc");
    LCD_SetCursor(1, 0);
    LCD_String("A op B =");
    
    while(1) {
        Get_Expression();
        Display_Result(result);
        delay_ms(3000);  // Display result for 3 seconds
        LCD_Clear();
        LCD_String("Enter New Expr:");
        LCD_SetCursor(1, 0);
        LCD_String("A op B =");
    }
}

void LCD_Init(void) {
    // Set data and control pins as output
    LCD_DATA_PORT->FIODIR |= 0xFF;  // P0.0-P0.7 as output
    LCD_CTRL_PORT->FIODIR |= (RS | RW | EN);  // Control pins as output
    
    delay_ms(20);  // Wait for LCD power up
    
    // Initialization sequence
    LCD_Command(0x38);  // 8-bit mode, 2 lines, 5x7 font
    LCD_Command(0x0C);  // Display ON, cursor OFF
    LCD_Command(0x06);  // Entry mode - increment cursor
    LCD_Command(0x01);  // Clear display
    delay_ms(2);
}

void LCD_Command(unsigned char cmd) {
    LCD_DATA_PORT->FIOPIN = cmd;
    LCD_CTRL_PORT->FIOCLR = (RS | RW);  // RS=0, RW=0 (command mode)
    LCD_CTRL_PORT->FIOSET = EN;         // Enable high
    delay_ms(1);
    LCD_CTRL_PORT->FIOCLR = EN;         // Enable low
    delay_ms(1);
}

void LCD_Data(unsigned char data) {
    LCD_DATA_PORT->FIOPIN = data;
    LCD_CTRL_PORT->FIOSET = RS;         // RS=1 (data mode)
    LCD_CTRL_PORT->FIOCLR = RW;         // RW=0 (write)
    LCD_CTRL_PORT->FIOSET = EN;         // Enable high
    delay_ms(1);
    LCD_CTRL_PORT->FIOCLR = EN;         // Enable low
    delay_ms(1);
}

void LCD_String(char *str) {
    while(*str) {
        LCD_Data(*str++);
    }
}

void LCD_Clear(void) {
    LCD_Command(0x01);
    delay_ms(2);
}

void LCD_SetCursor(unsigned char row, unsigned char col) {
    unsigned char address;
    
    if(row == 0)
        address = 0x80 + col;  // First row
    else
        address = 0xC0 + col;  // Second row
    
    LCD_Command(address);
}

void delay_ms(unsigned int ms) {
    for(unsigned int i = 0; i < ms; i++)
        for(unsigned int j = 0; j < 10000; j++);
}

char Read_Keypad(void) {
    char key = 0;
    unsigned int row, col;
    
    // Scan each row
    for(row = 0; row < 4; row++) {
        // Set all rows high
        KEYPAD_PORT->FIOSET = (ROW1 | ROW2 | ROW3 | ROW4);
        
        // Set current row low
        switch(row) {
            case 0: KEYPAD_PORT->FIOCLR = ROW1; break;
            case 1: KEYPAD_PORT->FIOCLR = ROW2; break;
            case 2: KEYPAD_PORT->FIOCLR = ROW3; break;
            case 3: KEYPAD_PORT->FIOCLR = ROW4; break;
        }
        
        delay_ms(10);  // Debounce delay
        
        // Check columns
        if(!(KEYPAD_PORT->FIOPIN & COL1)) {
            col = 0;
            key = keypad[row][col];
            while(!(KEYPAD_PORT->FIOPIN & COL1));  // Wait for key release
            return key;
        }
        if(!(KEYPAD_PORT->FIOPIN & COL2)) {
            col = 1;
            key = keypad[row][col];
            while(!(KEYPAD_PORT->FIOPIN & COL2));  // Wait for key release
            return key;
        }
        if(!(KEYPAD_PORT->FIOPIN & COL3)) {
            col = 2;
            key = keypad[row][col];
            while(!(KEYPAD_PORT->FIOPIN & COL3));  // Wait for key release
            return key;
        }
    }
    
    return 0;  // No key pressed
}

void Get_Expression(void) {
    char key;
    char buffer[20];
    int index = 0;
    int state = 0;  // 0: waiting for first operand, 1: waiting for operator, 2: waiting for second operand
    
    LCD_SetCursor(1, 8);  // Set cursor to input position
    
    first_operand = 0;
    second_operand = 0;
    operator = 0;
    
    while(1) {
        key = Read_Keypad();
        
        if(key != 0) {
            if(key >= '0' && key <= '9') {
                // Numeric key pressed
                if(state == 0) {
                    first_operand = key - '0';
                    LCD_Data(key);
                    state = 1;
                }
                else if(state == 2) {
                    second_operand = key - '0';
                    LCD_Data(key);
                    state = 3;
                }
            }
            else if(key == '+' || key == '-') {
                // Operator key pressed
                if(state == 1) {
                    operator = key;
                    LCD_Data(key);
                    state = 2;
                }
            }
            else if(key == '=') {
                // Equals key pressed
                if(state == 3) {
                    LCD_Data('=');
                    
                    // Perform calculation
                    if(operator == '+') {
                        result = BCD_To_Decimal(first_operand) + BCD_To_Decimal(second_operand);
                    }
                    else if(operator == '-') {
                        result = BCD_To_Decimal(first_operand) - BCD_To_Decimal(second_operand);
                    }
                    
                    // Check for valid BCD result (0-9)
                    if(result >= 0 && result <= 9) {
                        break;  // Exit input loop
                    }
                    else {
                        // Invalid result (out of BCD range)
                        LCD_SetCursor(1, 0);
                        LCD_String("Error: Result>9");
                        delay_ms(2000);
                        LCD_SetCursor(1, 0);
                        LCD_String("A op B =       ");
                        LCD_SetCursor(1, 8);
                        first_operand = 0;
                        second_operand = 0;
                        operator = 0;
                        state = 0;
                    }
                }
            }
            else if(key == '*') {
                // Clear/Reset
                LCD_SetCursor(1, 0);
                LCD_String("                ");
                LCD_SetCursor(1, 0);
                LCD_String("A op B =");
                LCD_SetCursor(1, 8);
                first_operand = 0;
                second_operand = 0;
                operator = 0;
                state = 0;
            }
        }
    }
}

int BCD_To_Decimal(unsigned char bcd) {
    return bcd;  // Since we're using single digit, it's the same
}

unsigned char Decimal_To_BCD(int decimal) {
    if(decimal >= 0 && decimal <= 9)
        return decimal;
    else
        return 0xFF;  // Invalid BCD
}

void Display_Result(int result) {
    char buffer[20];
    
    LCD_SetCursor(1, 10);
    
    if(result < 0) {
        // Negative result
        LCD_Data('-');
        result = -result;
        if(result <= 9) {
            LCD_Data(result + '0');
        }
        else {
            LCD_String("ERR");
        }
    }
    else if(result <= 9) {
        // Positive single digit result
        LCD_Data(result + '0');
    }
    else {
        // Result > 9 (not valid for single digit BCD)
        LCD_String("ERR");
    }
    
    // Display the complete expression on first line
    LCD_SetCursor(0, 0);
    LCD_String("                ");  // Clear first line
    LCD_SetCursor(0, 0);
    
    sprintf(buffer, "%d %c %d = ", first_operand, operator, second_operand);
    LCD_String(buffer);
    
    if(result >= 0 && result <= 9) {
        LCD_Data(result + '0');
    }
    else if(result < 0 && result >= -9) {
        LCD_Data('-');
        LCD_Data((-result) + '0');
    }
    else {
        LCD_String("ERR");
    }
}

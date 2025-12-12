#define RS_CTRL (1 << 27)     // P0.27
#define EN_CTRL (1 << 28)     // P0.28
#define DT_CTRL (0xF << 23)   // P0.23–P0.26

#include <lpc17xx.h>

unsigned long temp1, temp2,i,r,d;
unsigned char flag1;
unsigned char msg[] = "WELCOME";

void lcd_send_nibble(unsigned int nib);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char data);
void delay_lcd(unsigned int r);

void lcd_init(void);

int main(void)
{
    SystemInit();
    SystemCoreClockUpdate();

    LPC_GPIO0->FIODIR |= (RS_CTRL | EN_CTRL | DT_CTRL);

    lcd_init();

    for (i = 0; msg[i] != '\0'; i++)
        lcd_data(msg[i]);

    while (1);
}

void lcd_init(void)
{
    flag1 = 0; // command mode

    delay_lcd(500000);

    // --- RAW INITIALIZATION SEQUENCE (DO NOT USE lcd_write HERE!) --
    lcd_send_nibble(0x03);
    delay_lcd(50000);

    lcd_send_nibble(0x03);
    delay_lcd(50000);

    lcd_send_nibble(0x03);
    delay_lcd(50000);

    // Now enter 4-bit mode
    lcd_send_nibble(0x02);
    delay_lcd(50000);

    // Now normal commands
    lcd_cmd(0x28); // 4-bit, 2 line, 5x7 font
    lcd_cmd(0x0C); // Display on, cursor off
    lcd_cmd(0x06); // Entry mode
    lcd_cmd(0x01); // Clear display
    delay_lcd(50000);
}

void lcd_cmd(unsigned char cmd)
{
    flag1 = 0;

    lcd_send_nibble(cmd >> 4);
    lcd_send_nibble(cmd & 0x0F);
}

void lcd_data(unsigned char data)
{
    flag1 = 1;

    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
}

void lcd_send_nibble(unsigned int nib)
{
    LPC_GPIO0->FIOCLR = DT_CTRL;

    temp2 = (nib & 0x0F) << 23;
    LPC_GPIO0->FIOSET = temp2;

    if (flag1 == 0)
        LPC_GPIO0->FIOCLR = RS_CTRL;
    else
        LPC_GPIO0->FIOSET = RS_CTRL;

    LPC_GPIO0->FIOSET = EN_CTRL;
    delay_lcd(200);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delay_lcd(200);
}

void delay_lcd(unsigned int r)
{
    for (d = 0; d < r; d++);
}
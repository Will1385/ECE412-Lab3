// Lab3P1.c
//
//
//
// Author : Eugene Rockey
//
#include <string.h>
#include <math.h>

const char MS1[] = "\r\nECE-412 ATMega328PB Tiny OS";
const char MS2[] = "\r\nby Eugene Rockey Copyright 2022, All Rights Reserved";
const char MS3[] = "\r\nMenu: (L)CD, (A)DC, (E)EPROM, (U)SART\r\n";
const char MS4[] = "\r\nReady: ";
const char MS5[] = "\r\nInvalid Command Try Again...";
const char MS6[] = "Volts\r";
const char MS7[] = "                  Group 6: Hussein, Will, Kang, Nakul                  ";

unsigned long counter = 0;
unsigned int offset = 0;
char message_view[17];

void LCD_Init(void);			//external Assembly functions
void UART_Init(void);
void UART_Clear(void);
void UART_Get(void);
void UART_Put(void);
void LCD_Write_Data(void);
void LCD_Write_Command(void);
void LCD_Read_Data(void);
void Mega328P_Init(void);
void ADC_Get(void);
void EEPROM_Read(void);
void EEPROM_Write(void);

unsigned char ASCII;			//shared I/O variable with Assembly
unsigned char DATA;				//shared internal variable with Assembly
char HADC;                      //shared ADC variable with Assembly
char LADC;						//shared ADC variable with Assembly

// Shared variables for EEPROM Assembly drivers
unsigned char EEARH_VAL;
unsigned char EEARL_VAL;
unsigned char EEDR_VAL;

char volts[5];					//string buffer for ADC output
int Acc;                        //Accumulator for ADC use

// Direct register access macros for Free-Running ADC and UART checking
#define ADCSRA_REG (*(volatile unsigned char *)0x7A)
#define ADCSRB_REG (*(volatile unsigned char *)0x7B)
#define ADCL_REG   (*(volatile unsigned char *)0x78)
#define ADCH_REG   (*(volatile unsigned char *)0x79)
#define UCSR0A_REG (*(volatile unsigned char *)0xC0)

void UART_Puts(const char *str)	//Display a string in the PC Terminal Program
{
	while (*str)
	{
		ASCII = *str++;
		UART_Put();
	}
}

void LCD_Puts(const char *str)	//Display a string on the LCD Module
{
	while (*str)
	{
		DATA = *str++;
		LCD_Write_Data();
	}
}

void Banner(void)				//Display Tiny OS Banner on Terminal
{
	UART_Puts(MS1);
	UART_Puts(MS2);
	UART_Puts(MS4);
}

void HELP(void)					//Display available Tiny OS Commands on Terminal
{
	UART_Puts(MS3);
}

void LCD(void)
{
	const unsigned long COUNTER_MAX = 25000;
	const unsigned char LCD_LINE_SIZE = 16;
	const unsigned int OFFSET_MAX = strlen(MS7) - LCD_LINE_SIZE;
	unsigned char i;

	DATA = 0x34;
	LCD_Write_Command();
	DATA = 0x08;
	LCD_Write_Command();
	DATA = 0x02;
	LCD_Write_Command();
	DATA = 0x06;
	LCD_Write_Command();
	DATA = 0x0F;
	LCD_Write_Command();
    counter = 0;
	while (counter <= 25052)
	{
		counter++;
		if (counter >= COUNTER_MAX)
		{

			for (i = 0; i < LCD_LINE_SIZE; i++)
			{
				message_view[i] = MS7[offset + i];
			}
			message_view[LCD_LINE_SIZE] = '\0';
			DATA = 0x01;
			LCD_Write_Command();

			DATA = 0x02;
			LCD_Write_Command();

			LCD_Puts(message_view);

			offset++;

			if (offset >= OFFSET_MAX)
			{
				offset = 0;
			}
		}

	}
}

void ADC(void)					
{
    UART_Puts("\r\nThermistor ADC in Free Running Mode");
    UART_Puts("\r\nPress any key to exit.\r\n\n");

    // Enable Free Running Mode
    ADCSRB_REG = 0x00; // Free running mode source
    ADCSRA_REG = 0xE7; // ADEN=1, ADSC=1, ADATE=1, prescaler=128

    while(1) {
        // Check if a key was pressed to exit (RXC0 is bit 7 of UCSR0A)
        if (UCSR0A_REG & (1 << 7)) {
            UART_Get(); // Consume the character to clear the buffer
            break;
        }

        // Read 10-bit ADC value (must read ADCL first to lock registers)
        unsigned char low = ADCL_REG;
        unsigned char high = ADCH_REG;
        int adc_val = (high << 8) | low;

        // Bounds protection for math to prevent divide-by-zero
        if (adc_val <= 0) adc_val = 1;
        if (adc_val >= 1024) adc_val = 1023;

        // Calculate Resistance of Thermistor
        // Schematic layout: +5V -> 10K -> NTC -> GND
        float R_th = 10000.0 * ((float)adc_val / (1024.0 - (float)adc_val));

        // Calculate Temperature using Steinhart-Hart (B-parameter)
        float inv_T = (1.0 / 298.15) + (1.0 / 3950.0) * log(R_th / 10000.0);
        float T_Celsius = (1.0 / inv_T) - 273.15;

        // Format output for static display
        int int_part = (int)T_Celsius;
        int frac_part = (int)((T_Celsius - int_part) * 10.0);
        if (frac_part < 0) frac_part = -frac_part; 

        // \r returns cursor to start of line to update statically
        UART_Puts("\rTemperature: "); 

        int temp_int = int_part;
        if (T_Celsius < 0 && temp_int == 0) UART_Puts("-");
        if (temp_int < 0) {
            UART_Puts("-");
            temp_int = -temp_int;
        }

        // Simple integer to string formatting (hundreds, tens, ones)
        char num_str[10];
        num_str[0] = (temp_int / 100) % 10 + '0';
        num_str[1] = (temp_int / 10) % 10 + '0';
        num_str[2] = temp_int % 10 + '0';
        num_str[3] = '.';
        num_str[4] = frac_part + '0';
        num_str[5] = ' ';
        num_str[6] = 'C';
        num_str[7] = '\0';

        // Remove leading zeros for a cleaner display
        char *p = num_str;
        if (*p == '0') {
            p++;
            if (*p == '0' && temp_int < 10) p++; 
        }

        UART_Puts(p);
        UART_Puts("   "); // Trailing spaces to clear any leftover characters

        // Slight artificial delay to smooth the terminal output
        for(volatile long d = 0; d < 30000; d++);
    }

    // Restore ADC back to normal single conversion mode on exit
    ADCSRA_REG = 0x87; 
}

// Helper to read a single hex character from the terminal
unsigned char Get_Hex_Digit(void) {
    while (1) {
        UART_Get();
        char c = ASCII;
        // Check if input is a valid hex character, echo it, and return its value
        if (c >= '0' && c <= '9') { UART_Put(); return c - '0'; }
        if (c >= 'A' && c <= 'F') { UART_Put(); return c - 'A' + 10; }
        if (c >= 'a' && c <= 'f') { UART_Put(); return c - 'a' + 10; }
    }
}

// Helper to read a full byte (2 hex digits) from the terminal
unsigned char Get_Hex_Byte(void) {
    unsigned char high = Get_Hex_Digit();
    unsigned char low = Get_Hex_Digit();
    return (high << 4) | low;
}

// Helper to print a byte as 2 hex digits to the terminal
void Print_Hex_Byte(unsigned char val) {
    const char hex[] = "0123456789ABCDEF";
    ASCII = hex[(val >> 4) & 0x0F];
    UART_Put();
    ASCII = hex[val & 0x0F];
    UART_Put();
}

void EEPROM(void)
{
    UART_Puts("\r\nEEPROM Menu");
    UART_Puts("\r\n(W)rite or (R)ead? ");

    // Wait for valid input
    while (1) {
        UART_Get();
        if (ASCII == 'W' || ASCII == 'w' || ASCII == 'R' || ASCII == 'r') break;
    }
    
    char choice = ASCII;
    UART_Put(); // Echo their choice back to the terminal

    if (choice == 'W' || choice == 'w') {
        UART_Puts("\r\nEnter Address (4 hex digits, e.g. 001F): ");
        EEARH_VAL = Get_Hex_Byte(); // First two digits (High Byte)
        EEARL_VAL = Get_Hex_Byte(); // Last two digits (Low Byte)

        UART_Puts("\r\nEnter Data (2 hex digits, e.g. A5): ");
        EEDR_VAL = Get_Hex_Byte();

        EEPROM_Write(); // Call Assembly driver
        UART_Puts("\r\nWrite Complete.");
    } 
    else if (choice == 'R' || choice == 'r') {
        UART_Puts("\r\nEnter Address (4 hex digits, e.g. 001F): ");
        EEARH_VAL = Get_Hex_Byte();
        EEARL_VAL = Get_Hex_Byte();

        EEPROM_Read(); // Call Assembly driver

        UART_Puts("\r\nData read: 0x");
        Print_Hex_Byte(EEDR_VAL); // EEDR_VAL is populated by Assembly EEPROM_Read
    }
    
    UART_Puts("\r\n");
}
// Direct register access for UART configuration
#define UBRR0H_REG (*(volatile unsigned char *)0xC5)
#define UBRR0L_REG (*(volatile unsigned char *)0xC4)
#define UCSR0B_REG (*(volatile unsigned char *)0xC1)
#define UCSR0C_REG (*(volatile unsigned char *)0xC2)

void UART_Init(void) {
    UART_Puts("\r\n--- UART Configuration Menu ---");
    
    // 1. Baud Rate Selection
    UART_Puts("\r\nSelect Baud Rate:\r\n(1) 9600\r\n(2) 19200\r\n(3) 38400\r\nChoice: ");
    while(1) { UART_Get(); if (ASCII >= '1' && ASCII <= '3') break; }
    char baud = ASCII;
    UART_Put(); // Echo

    // 2. Parity Selection
    UART_Puts("\r\nSelect Parity:\r\n(N)one\r\n(E)ven\r\n(O)dd\r\nChoice: ");
    while(1) { UART_Get(); if (ASCII=='N'||ASCII=='n'||ASCII=='E'||ASCII=='e'||ASCII=='O'||ASCII=='o') break; }
    char parity = ASCII;
    UART_Put(); // Echo

    // Calculate UBRR value (assuming 16MHz clock and U2X0 = 0 from Assembly init)
    unsigned int ubrr = 103; // Default 9600 (0x67)
    if (baud == '2') ubrr = 51;  // 19200 (0x33)
    if (baud == '3') ubrr = 25;  // 38400 (0x19)

    // Build UCSR0C register value (Default: 8 data bits, 1 stop bit)
    // UCSZ01=1, UCSZ00=1 sets 8 data bits. That's 0x06.
    unsigned char ucsr0c_val = 0x06; 

    // Add parity bits if requested
    if (parity == 'E' || parity == 'e') ucsr0c_val |= (1 << 5); // UPM01 = 1
    if (parity == 'O' || parity == 'o') {
        ucsr0c_val |= (1 << 5); // UPM01 = 1
        ucsr0c_val |= (1 << 4); // UPM00 = 1
    }

    // Apply the new settings to the hardware registers
    UBRR0H_REG = (unsigned char)(ubrr >> 8);
    UBRR0L_REG = (unsigned char)ubrr;
    UCSR0C_REG = ucsr0c_val;

    UART_Puts("\r\n\n[!] UART Reconfigured. Please adjust your PC terminal settings to match to continue talking to the OS!\r\n");
}
void USART(void) {
    UART_Puts("Reconfiguring UART");
    UART_Init();
}

void Command(void)				//command interpreter
{
	UART_Puts(MS3);

	ASCII = '\0';

	while (ASCII == '\0')
	{
		UART_Get();
	}

	switch (ASCII)
	{
		case 'L' | 'l': LCD();
		break;

		case 'A' | 'a': ADC();
		break;

		case 'E' | 'e': EEPROM();
		break;
        
		case 'U' | 'u': USART();
        break;

		default:
			UART_Puts(MS5);
			HELP();
		break;

		//Add a 'USART' command and subroutine to allow the user to reconfigure the
		//serial port parameters during runtime.
        //Modify baud rate, # of data bits, parity,
		//# of stop bits.
	}
}

int main(void)
{
	Mega328P_Init();

	Banner();

	while (1)
	{
		Command();				//infinite command loop
	}
}
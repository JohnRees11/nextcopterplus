//***********************************************************
//* menu_settings.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include "compiledefs.h"
#include <avr/pgmspace.h> 
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <util/delay.h>
#include "io_cfg.h"
#include "init.h"
#include "mugui.h"
#include "glcd_menu.h"
#include "menu_ext.h"
#include "glcd_driver.h"
#include "main.h"
#include "eeprom.h"
#include "mixer.h"
#include "imu.h"
#include "uart.h"
#include "i2c.h"
#include "isr.h"
#include "MPU6050.h"

//************************************************************
// Prototypes
//************************************************************

// Menu items
void menu_rc_setup(uint8_t section);

//************************************************************
// Defines
//************************************************************

#define RCSTART 149 	// Start of Menu text items
#define RCOFFSET 79		// LCD offsets

#define RCTEXT 62 		// Start of value text items
#define GENERALTEXT	124
#define RCITEMS 7 		// Number of menu items displayed
#define RCITEMSOFFSET 9 // Actual number of menu items
#define GENERALITEMS 9


//************************************************************
// RC menu items
//************************************************************
	 
const uint8_t RCMenuText[2][GENERALITEMS] PROGMEM = 
{
	{RCTEXT, 118, 105, 116, 105, 0, 0},				// RC setup
	{GENERALTEXT, 0, 53, 0, 0, 37, 37, 37, 0},		// General 
};

const menu_range_t rc_menu_ranges[2][GENERALITEMS] PROGMEM = 
{
	{
		// RC setup (7)					// Min, Max, Increment, Style, Default
		{CPPM_MODE,SPEKTRUM,1,1,PWM},	// Receiver type
		{LOW,FAST,1,1,LOW},				// Servo rate
		{THROTTLE,GEAR,1,1,GEAR},		// PWM sync channel
		{JRSEQ,FUTABASEQ,1,1,JRSEQ}, 	// Channel order
		{THROTTLE,AUX3,1,1,GEAR},		// Profile select channel
		{0,40,1,0,0},					// TransitionSpeed 0 to 40
		{1,99,1,0,50},					// Transition P1n point
	},
	{
		// General (9)
		{HORIZONTAL,PITCHUP,1,1,HORIZONTAL}, // Orientation
		// Limit contrast range for KK2 Mini
#ifdef KK2Mini
		{26,34,1,0,30}, 				// Contrast (KK2 Mini)
#else
		{28,50,1,0,36}, 				// Contrast (Everything else)
#endif			
		{ARMED,ARMABLE,1,1,ARMABLE},	// Arming mode Armable/Armed
		{0,127,1,0,30},					// Auto-disarm enable
		{0,8,1,1,0},					// Low battery cell voltage
		{HZ5,HZ260,1,1,HZ21},			// MPU6050 LPF. Default is 21Hz
		{HZ5,NOFILTER,1,1,HZ21},		// Acc. LPF 21Hz default	(5, 10, 21, 44, 94, 184, 260, None)
		{HZ5,NOFILTER,1,1,NOFILTER},	// Gyro LPF. No LPF default (5, 10, 21, 44, 94, 184, 260, None)
		{1,10,1,0,7},					// AL correction
	}
};
//************************************************************
// Main menu-specific setup
//************************************************************

void menu_rc_setup(uint8_t section)
{
	int8_t *value_ptr = &Config.RxMode;

	menu_range_t range;
	uint8_t text_link;
	uint8_t i;
	uint8_t offset = 0;			// Index into channel structure
	uint8_t	items= RCITEMS;		// Items in group
	
	// If submenu item has changed, reset submenu positions
	if (menu_flag)
	{
		sub_top = RCSTART;
		menu_flag = 0;
	}

	while(button != BACK)
	{
		// Get menu offsets and load values from eeprom
		// 1 = RC, 2 = General
		switch(section)
		{
			case 1:				// RC setup menu
				break;
			case 2:				// General menu
				offset = RCITEMSOFFSET;
				items = GENERALITEMS;
				value_ptr = &Config.Orientation;
				break;
			default:
				break;
		}

		// Print menu
		print_menu_items(sub_top + offset, RCSTART + offset, value_ptr, (const unsigned char*)rc_menu_ranges[section - 1], 0, RCOFFSET, (const unsigned char*)RCMenuText[section - 1], cursor);

		// Handle menu changes
		update_menu(items, RCSTART, offset, button, &cursor, &sub_top, &menu_temp);
		range = get_menu_range ((const unsigned char*)rc_menu_ranges[section - 1], (menu_temp - RCSTART - offset)); 

		if (button == ENTER)
		{
			text_link = pgm_read_byte(&RCMenuText[section - 1][menu_temp - RCSTART - offset]);
			do_menu_item(menu_temp, value_ptr + (menu_temp - RCSTART - offset), 1, range, 0, text_link, false, 0);
		}

		if (button == ENTER)
		{
			init_int();				// In case RC type has changed, reinitialise interrupts
			init_uart();			// and UART
			UpdateLimits();			// Update I-term limits and triggers based on percentages

			// Update MPU6050 LPF and reverse sense of menu items
			writeI2Cbyte(MPU60X0_DEFAULT_ADDRESS, MPU60X0_RA_CONFIG, (6 - Config.MPU6050_LPF));

			// Update channel sequence
			for (i = 0; i < MAX_RC_CHANNELS; i++)
			{
				if (Config.TxSeq == FUTABASEQ)
				{
					Config.ChannelOrder[i] = pgm_read_byte(&FUTABA[i]);
				}
				else
				{
					Config.ChannelOrder[i] = pgm_read_byte(&JR[i]);
				}
			}

			// Check validity of RX type and PWM speed selection
			// If illegal setting, drop down to RC Sync
			if ((!(Config.RxMode == SBUS) || (Config.RxMode == SPEKTRUM)) && (Config.Servo_rate == FAST))
			{
				Config.Servo_rate = SYNC;
			}
						
			if (Config.ArmMode == ARMABLE)
			{
				General_error |= (1 << DISARMED);	// Set flags to disarmed
				LED1 = 0;
			}

			Save_Config_to_EEPROM(); // Save value and return
			Wait_BUTTON4();			 // Wait for user's finger off the button
		}
	}
}


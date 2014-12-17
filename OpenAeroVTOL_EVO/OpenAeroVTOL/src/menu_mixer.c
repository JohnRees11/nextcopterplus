//***********************************************************
//* menu_mixer.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include "compiledefs.h"
#include <avr/pgmspace.h> 
#include <avr/io.h>
#include <stdbool.h>
#include <string.h>
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

//************************************************************
// Prototypes
//************************************************************

// Menu items
void menu_mixer(uint8_t i);

//************************************************************
// Defines
//************************************************************

#define MIXERITEMS 34	// Number of mixer menu items
#define MIXERSTART 190 	// Start of Menu text items
#define MIXOFFSET  89	// Value offsets

//************************************************************
// RC menu items
//************************************************************
	 
const uint8_t MixerMenuText[MIXERITEMS] PROGMEM = 
{
	46,0,0,0,0,0,0,56,						// Motor control and offsets (8)
	0,0,0,0,0,0,							// Flight controls (6)
	68,68,68,68,68,68,68,68,68,68,68,68,	// Mixer ranges (12)
	238,0,238,0,238,0,238,0					// Other sources (8)
};

const menu_range_t mixer_menu_ranges[MIXERITEMS] PROGMEM = 
{
		// Motor control and offsets (8)
		{SERVO,MOTOR,1,1,SERVO},		// Motor marker (0)
		{-125,125,1,0,0},				// P1 Offset (%)
		{1,99,1,0,50},					// P1.n Position (%)
		{-125,125,1,0,0},				// P1.n Offset (%)
		{-125,125,1,0,0},				// P2 Offset (%)
		{0,125,1,0,100},				// P1 throttle volume 
		{0,125,1,0,100},				// P2 throttle volume
		{LINEAR,SQRTSINE,1,1,LINEAR},	// Throttle curves

		// Flight controls (6)
		{-125,125,1,0,0},				// P1 Aileron volume (8)
		{-125,125,1,0,0},				// P2 Aileron volume
		{-125,125,1,0,0},				// P1 Elevator volume
		{-125,125,1,0,0},				// P2 Elevator volume
		{-125,125,1,0,0},				// P1 Rudder volume
		{-125,125,1,0,0},				// P2 Rudder volume

		// Mixer ranges (12)
		{OFF, REVERSESCALE,1,1,OFF},	// P1 roll_gyro (14)
		{OFF, REVERSESCALE,1,1,OFF},	// P2 roll_gyro
		{OFF, REVERSESCALE,1,1,OFF},	// P1 pitch_gyro
		{OFF, REVERSESCALE,1,1,OFF},	// P2 pitch_gyro
		{OFF, REVERSESCALE,1,1,OFF},	// P1 yaw_gyro
		{OFF, REVERSESCALE,1,1,OFF},	// P2 yaw_gyro
		{OFF, REVERSESCALE,1,1,OFF},	// P1 roll_acc
		{OFF, REVERSESCALE,1,1,OFF},	// P2 roll_acc
		{OFF, REVERSESCALE,1,1,OFF},	// P1 pitch_acc
		{OFF, REVERSESCALE,1,1,OFF},	// P2 pitch_acc
		{OFF, REVERSESCALE,1,1,OFF},	// P1 Z_delta_acc
		{OFF, REVERSESCALE,1,1,OFF},	// P2 Z_delta_acc

		// Sources (8)
		{SRC1,NOMIX,1,1,NOMIX},			// P1 Source A (26)
		{-125,125,1,0,0},				// P1 Source A volume
		{SRC1,NOMIX,1,1,NOMIX},			// P2 Source A
		{-125,125,1,0,0},				// P2 Source A volume
		{SRC1,NOMIX,1,1,NOMIX},			// P1 Source B
		{-125,125,1,0,0},				// P1 Source B volume
		{SRC1,NOMIX,1,1,NOMIX},			// P2 Source B
		{-125,125,1,0,0},				// P2 Source B volume
};

//************************************************************
// Main menu-specific setup
//************************************************************

void menu_mixer(uint8_t i)
{
	int8_t *value_ptr;
	menu_range_t range;
	uint8_t text_link = 0;

	// If sub-menu item has changed, reset sub-menu positions
	if (menu_flag)
	{
		sub_top = MIXERSTART;
		menu_flag = 0;
	}

	while(button != BACK)
	{
		value_ptr = &Config.Channel[i].Motor_marker;

		// Print menu
		print_menu_items(sub_top, MIXERSTART, value_ptr, (const unsigned char*)mixer_menu_ranges, 0, MIXOFFSET, (const unsigned char*)MixerMenuText, cursor);

		// Handle menu changes
		update_menu(MIXERITEMS, MIXERSTART, 0, button, &cursor, &sub_top, &menu_temp);
		range = get_menu_range ((const unsigned char*)mixer_menu_ranges, menu_temp - MIXERSTART);

		if (button == ENTER)
		{
			text_link = pgm_read_byte(&MixerMenuText[menu_temp - MIXERSTART]);
			do_menu_item(menu_temp, value_ptr + (menu_temp - MIXERSTART), 1, range, 0, text_link, false, 0);
		}

		// Update limits when exiting
		if (button == ENTER)
		{
			UpdateLimits();			 // Update travel limits based on percentages
			Save_Config_to_EEPROM(); // Save value and return
			Wait_BUTTON4();			 // Wait for user's finger off the button
		}
	}
}





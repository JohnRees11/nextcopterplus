//***********************************************************
//* eeprom.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include "compiledefs.h"
#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include "io_cfg.h"
#include <util/delay.h>
#include "mixer.h"
#include "menu_ext.h"
#include "MPU6050.h"

//************************************************************
// Prototypes
//************************************************************

bool Initial_EEPROM_Config_Load(void);
void Save_Config_to_EEPROM(void);
void Set_EEPROM_Default_Config(void);
void eeprom_write_byte_changed(uint8_t *addr, uint8_t value);
void eeprom_write_block_changes(uint8_t *src, uint8_t *dest, uint16_t size);
void Update_V1_0_to_V1_1(void);
void Update_V1_1_to_V1_2(void);

//************************************************************
// Defines
//************************************************************

#define EEPROM_DATA_START_POS 0	// Make sure Rolf's signature is over-written for safety

// eePROM signature - change for each eePROM structure change to force factory reset or upgrade
#define V1_0_SIGNATURE 0x35		// EEPROM signature for V1.0 (old version)
#define V1_1_SIGNATURE 0x36		// EEPROM signature for V1.1
#define V1_2_SIGNATURE 0x37		// EEPROM signature for V1.2

#define MAGIC_NUMBER V1_1_SIGNATURE // Set current signature to that of V1.1

//************************************************************
// Code
//************************************************************

const uint8_t	JR[MAX_RC_CHANNELS] PROGMEM 	= {0,1,2,3,4,5,6,7}; 	// JR/Spektrum channel sequence (TAERG123)
const uint8_t	FUTABA[MAX_RC_CHANNELS] PROGMEM = {1,2,0,3,4,5,6,7}; 	// Futaba channel sequence (AETRGF12)

void Save_Config_to_EEPROM(void)
{
	// Write to eeProm
	cli();
	eeprom_write_block_changes((uint8_t*)&Config, (uint8_t*)EEPROM_DATA_START_POS, sizeof(CONFIG_STRUCT));	
	sei();
}

// src is the address in RAM
// dest is the address in eeprom (hence const)
void eeprom_write_block_changes(uint8_t *src, uint8_t *dest, uint16_t size)
{ 
	uint16_t len;
	uint8_t value;

	for (len=0; len < size; len++)
	{
		// Get value at src
		value = *src;
		
		// Write the value at src to dest
		eeprom_write_byte_changed(dest, value);
		src++;
		dest++;
	}
}

// addr is the address in eeprom
// value is the value to be written
void eeprom_write_byte_changed(uint8_t *addr, uint8_t value)
{
	if (eeprom_read_byte(addr) != value)
	{
		// void eeprom_write_byte (uint8_t *__p, uint8_t __value);
		eeprom_write_byte(addr, value);
	}
}

bool Initial_EEPROM_Config_Load(void)
{
	bool	updated = false;
	
	// Read eeProm data into RAM
	// void eeprom_read_block (void *pointer_ram, const void *pointer_eeprom, size_t n)
	eeprom_read_block((void*)&Config, (const void*)EEPROM_DATA_START_POS, sizeof(CONFIG_STRUCT));
	
	// See if we know what to do with the current eeprom data
	// Config.setup holds the magic number from the current EEPROM
	switch(Config.setup)
	{
		case V1_0_SIGNATURE:				// V1.0 detected
			Update_V1_0_to_V1_1();
			updated = true;
			// Fall through...

		case V1_1_SIGNATURE:				// V1.1 detected
			//Update_V1_1_to_V1_2();		
			// Fall through...
			break;

		default:							// Unknown solution - restore to factory defaults
			// Load factory defaults
			Set_EEPROM_Default_Config();
			break;
	}
	
	// Save back to eeprom	
	Save_Config_to_EEPROM();
	
	// Return info regarding eeprom structure changes 
	return updated;
}

//************************************************************
// Config data restructure code
//************************************************************

// Upgrade V1.0 structure to V1.1 structure
void Update_V1_0_to_V1_1(void)
{
	#define		OLDSIZE 29				// Old channel_t was 29 bytes
	#define		NEWSIZE 38				// New channel_t is 38 bytes

	uint8_t		i, j, temp;
	uint8_t		*src;
	uint8_t		*dst;
	uint8_t		mixer_buffer[NEWSIZE * 8]; // 304 bytes
	
	int8_t		P1_sensors;				// Sensor switches (6), motor marker (1)
	int8_t		P2_sensors;				// Sensor switches (6)
	int8_t		P1_scale;				// P1 sensor scale flags (6)
	int8_t		P2_scale;				// P2 sensor scale flags (6)

	// Save old P2 Source B volume
	memcpy((void*)&temp,(void*)1836,1); // Ugly - fix this properly.
	 
	// Move data that exists after the channel mixer to new location
	// Hard-coded to V1.0 RAM location	
	memmove((void*)&Config.Servo_reverse, (void*)1837, 74);	// RAM location determined empirically
	
	// Copy the old channel[] structure into buffer, spaced out to match the new structure
	for (i = 0; i < MAX_OUTPUTS; i++)
	{
		src = (void*)Config.Channel;	// Same location as old one
		dst = (void*)mixer_buffer;
		src += (i * OLDSIZE);			// Step to next old data in (corrupted) config structure
		dst += (i * NEWSIZE);			// Step to next location for new data in the buffer
		memcpy(dst, src, OLDSIZE);		// Move only the old (smaller) data
	}

	// Rearrange one output at a time	
	for (i = 0; i < MAX_OUTPUTS; i++)
	{
		// Move all bytes from the OLD P1_offset [4] up by one to make space for the Motor_marker byte
		src = &mixer_buffer[4 + (i * NEWSIZE)];	// The old P1_offset byte
		dst = &mixer_buffer[5 + (i * NEWSIZE)];
		memmove(dst, src, (OLDSIZE - 4));// Move all but P1_value, P2_value

		// Save the old switches
		P1_sensors = mixer_buffer[18 + (i * NEWSIZE)];
		P2_sensors = mixer_buffer[19 + (i * NEWSIZE)];
		P1_scale = mixer_buffer[20 + (i * NEWSIZE)];
		P2_scale = mixer_buffer[21 + (i * NEWSIZE)];
		
		// Take old motor marker switch and convert
		if ((P1_sensors & (1 << MotorMarker)) != 0)
		{
			// Set the new value in the right place
			mixer_buffer[4 + (i * NEWSIZE)] = MOTOR;
		}
		else
		{
			mixer_buffer[4 + (i * NEWSIZE)] = ASERVO;
		}

		// Move the universal source bytes (8) up eight bytes
		src = &mixer_buffer[22 + (i * NEWSIZE)]; // 21 + 1
		dst = &mixer_buffer[30 + (i * NEWSIZE)];
		memmove(dst, src, 8);

		
		// Convert old "None" settings to new ones
		// Skip every second byte
		for (j = 0; j < 8; j += 2)
		{
			if (mixer_buffer[30 + (i * NEWSIZE) + j] == 13) // 13 was the old "None"
			{
				mixer_buffer[30 + (i * NEWSIZE) + j] = NOMIX;
			}			
		}

		// Expand the old switches into new bytes
		// P1 roll gyro
		if ((P1_sensors & (1 << RollGyro)) != 0)
		{
			if ((P1_scale & (1 << RollScale)) != 0)
			{
				mixer_buffer[18 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[18 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[18 + (i * NEWSIZE)] = OFF;
		}

		// P2 roll gyro
		if ((P2_sensors & (1 << RollGyro)) != 0)
		{
			if ((P2_scale & (1 << RollScale)) != 0)
			{
				mixer_buffer[19 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[19 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[19 + (i * NEWSIZE)] = OFF;
		}

		// P1 pitch gyro
		if ((P1_sensors & (1 << PitchGyro)) != 0)
		{
			if ((P1_scale & (1 << PitchScale)) != 0)
			{
				mixer_buffer[20 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[20 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[20 + (i * NEWSIZE)] = OFF;
		}

		// P2 pitch gyro
		if ((P2_sensors & (1 << PitchGyro)) != 0)
		{
			if ((P2_scale & (1 << PitchScale)) != 0)
			{
				mixer_buffer[21 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[21 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[21 + (i * NEWSIZE)] = OFF;
		}

		// P1 yaw_gyro
		if ((P1_sensors & (1 << YawGyro)) != 0)
		{
			if ((P1_scale & (1 << YawScale)) != 0)
			{
				mixer_buffer[22 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[22 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[22 + (i * NEWSIZE)] = OFF;
		}

		// P2 yaw gyro
		if ((P2_sensors & (1 << YawGyro)) != 0)
		{
			if ((P2_scale & (1 << YawScale)) != 0)
			{
				mixer_buffer[23 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[23 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[23 + (i * NEWSIZE)] = OFF;
		}

		// P1 roll acc
		if ((P1_sensors & (1 << RollAcc)) != 0)
		{
			if ((P1_scale & (1 << AccRollScale)) != 0)
			{
				mixer_buffer[24 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[24 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[24 + (i * NEWSIZE)] = OFF;
		}

		// P2 roll acc
		if ((P2_sensors & (1 << RollAcc)) != 0)
		{
			if ((P2_scale & (1 << AccRollScale)) != 0)
			{
				mixer_buffer[25 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[25 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[25 + (i * NEWSIZE)] = OFF;
		}

		// P1 pitch acc
		if ((P1_sensors & (1 << PitchAcc)) != 0)
		{
			if ((P1_scale & (1 << AccPitchScale)) != 0)
			{
				mixer_buffer[26 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[26 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[26 + (i * NEWSIZE)] = OFF;
		}

		// P2 pitch acc
		if ((P2_sensors & (1 << PitchAcc)) != 0)
		{
			if ((P2_scale & (1 << AccPitchScale)) != 0)
			{
				mixer_buffer[27 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[27 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[27 + (i * NEWSIZE)] = OFF;
		}

		// P1 Z delta acc
		if ((P1_sensors & (1 << ZDeltaAcc)) != 0)
		{
			if ((P1_scale & (1 << AccZScale)) != 0)
			{
				mixer_buffer[28 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[28 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[28 + (i * NEWSIZE)] = OFF;
		}

		// P2 Z delta acc
		if ((P2_sensors & (1 << ZDeltaAcc)) != 0)
		{
			if ((P2_scale & (1 << AccZScale)) != 0)
			{
				mixer_buffer[29 + (i * NEWSIZE)] = SCALE;
			}
			else
			{
				mixer_buffer[29 + (i * NEWSIZE)] = ON;
			}
		}
		else
		{
			mixer_buffer[29 + (i * NEWSIZE)] = OFF;
		}
	}
		
	// Copy buffer back into new structure
	src = (void*)mixer_buffer;
	dst = (void*)Config.Channel;
	memcpy(dst, src, sizeof(mixer_buffer) - 1); // This appears to be spot on.

	// Restore corrupted byte manually
	Config.Channel[7].P2_source_b_volume = temp; // Ugly - fix this properly.

	// Set magic number to V1.1 signature
	Config.setup = MAGIC_NUMBER;
}

// Upgrade V1.1 structure to V1.2 structure
void Update_V1_1_to_V1_2(void)
{
	// Set magic number to V1.0 signature
	Config.setup = MAGIC_NUMBER;
}

// Force a factory reset
void Set_EEPROM_Default_Config(void)
{
	uint8_t i;
	
	// Clear entire Config space first
	memset(&Config.setup,0,(sizeof(Config)));

	// Set magic number to current signature
	Config.setup = MAGIC_NUMBER;

	// Misc settings
	Config.RxMode = PWM;				// Default to PWM
	Config.PWM_Sync = GEAR;
	Config.Acc_LPF = 2;					// Acc LPF around 21Hz (5, 10, 21, 32, 44, 74, None)
	Config.Gyro_LPF = 6;				// Gyro LPF off "None" (5, 10, 21, 32, 44, 74, None)
	Config.CF_factor = 7;
	Config.FlightChan = GEAR;			// Channel GEAR switches flight mode by default
	Config.Disarm_timer = 30;			// Default to 30 seconds
	Config.Transition_P1n = 50;			// Set P1.n point to 50%

	// Servo defaults
	for (i = 0; i < MAX_RC_CHANNELS; i++)
	{
		Config.ChannelOrder[i] = pgm_read_byte(&JR[i]);
		Config.RxChannelZeroOffset[i] = 3750;
	}
	
	// Monopolar throttle is a special case. Set to -100% or -1000
	Config.RxChannelZeroOffset[THROTTLE] = 2750;

	// Preset mixers to safe values
	for (i = 0; i < MAX_OUTPUTS; i++)
	{
		Config.Channel[i].P1n_position	= 50;
		Config.Channel[i].P1_source_a 	= NOMIX;
		Config.Channel[i].P1_source_b 	= NOMIX;
		Config.Channel[i].P2_source_a 	= NOMIX;
		Config.Channel[i].P2_source_b 	= NOMIX;
		Config.min_travel[i] = -100;
		Config.max_travel[i] = 100;
	}

	// Preset simple mixing for primary channels - all models
	Config.Channel[OUT1].P1_throttle_volume = 100;
	Config.Channel[OUT2].P1_aileron_volume = 100;
	Config.Channel[OUT3].P1_elevator_volume = 100;
	Config.Channel[OUT4].P1_rudder_volume = 100;
	
	// Set up profile 1
	Config.FlightMode[P1].Roll_P_mult = 60;			// PID defaults
	Config.FlightMode[P1].A_Roll_P_mult = 5;
	Config.FlightMode[P1].Pitch_P_mult = 60;
	Config.FlightMode[P1].A_Pitch_P_mult = 5;
	Config.FlightMode[P1].Yaw_P_mult = 80;
	Config.FlightMode[P1].Roll_I_mult = 40;
	Config.FlightMode[P1].Roll_limit = 10;
	Config.FlightMode[P1].Pitch_I_mult = 40;
	Config.FlightMode[P1].Pitch_limit = 10;
	Config.FlightMode[P1].Roll_Rate = 2;
	Config.FlightMode[P1].Pitch_Rate = 2;
	Config.FlightMode[P1].Yaw_Rate = 1;

	// Set up profile 2
	Config.FlightMode[P2].Roll_Rate = 2;
	Config.FlightMode[P2].Pitch_Rate = 2;
	Config.FlightMode[P2].Yaw_Rate = 1;
	
	// Preset stick volumes
	Config.Channel[OUT1].P2_throttle_volume = 100;
	Config.Channel[OUT2].P2_aileron_volume = 100;
	Config.Channel[OUT3].P2_elevator_volume = 100;
	Config.Channel[OUT4].P2_rudder_volume = 100;

	// Preset basic axis gyros in P2
	Config.Channel[OUT2].P2_Roll_gyro = ON;
	Config.Channel[OUT3].P2_Pitch_gyro = ON;
	Config.Channel[OUT4].P2_Yaw_gyro = ON;

#ifdef QUADCOPTER
	//**************************************
	//* Quadcopter defaults for testing
	//**************************************
	
	// Profile 1
	Config.FlightMode[P1].Roll_P_mult = 60;
	Config.FlightMode[P1].A_Roll_P_mult = 5;
	Config.FlightMode[P1].Pitch_P_mult = 60;
	Config.FlightMode[P1].A_Pitch_P_mult = 5;
	Config.FlightMode[P1].Yaw_P_mult = 80;
	Config.FlightMode[P1].Roll_I_mult = 40;
	Config.FlightMode[P1].Roll_limit = 10;
	Config.FlightMode[P1].Pitch_I_mult = 40;
	Config.FlightMode[P1].Pitch_limit = 10;
	Config.FlightMode[P1].Roll_Rate = 2;
	Config.FlightMode[P1].Pitch_Rate = 2;
	Config.FlightMode[P1].Yaw_Rate = 1;	
	
	// Profile 2
	Config.FlightMode[P2].Roll_P_mult = 60;
	Config.FlightMode[P2].A_Roll_P_mult = 5;
	Config.FlightMode[P2].Pitch_P_mult = 60;
	Config.FlightMode[P2].A_Pitch_P_mult = 5;
	Config.FlightMode[P2].Yaw_P_mult = 80;
	Config.FlightMode[P2].Roll_I_mult = 0;
	Config.FlightMode[P2].Roll_limit = 0;
	Config.FlightMode[P2].Pitch_I_mult = 0;
	Config.FlightMode[P2].Pitch_limit = 0;
	Config.FlightMode[P2].Roll_Rate = 2;
	Config.FlightMode[P2].Pitch_Rate = 2;
	Config.FlightMode[P2].Yaw_Rate = 1;	
	
	for (i = 0; i <= OUT4; i++)
	{
		Config.Channel[i].P1_throttle_volume = 100;
		Config.Channel[i].P2_throttle_volume = 100;
		Config.Channel[i].Motor_marker = MOTOR;
	}

	// OUT1
	Config.Channel[OUT1].P1_elevator_volume = -20;
	Config.Channel[OUT1].P2_elevator_volume = -20;
	Config.Channel[OUT1].P1_rudder_volume = -20;
	Config.Channel[OUT1].P2_rudder_volume = -20;
	Config.Channel[OUT1].P1_Pitch_gyro = ON;
	Config.Channel[OUT1].P2_Pitch_gyro = ON;
	Config.Channel[OUT1].P1_Yaw_gyro = ON;
	Config.Channel[OUT1].P2_Yaw_gyro = ON;	
	
	// OUT2
	Config.Channel[OUT2].P1_aileron_volume = -20;
	Config.Channel[OUT2].P2_aileron_volume = -20;
	Config.Channel[OUT2].P1_rudder_volume = 20;
	Config.Channel[OUT2].P2_rudder_volume = 20;
	Config.Channel[OUT2].P1_Roll_gyro = ON;
	Config.Channel[OUT2].P2_Roll_gyro = ON;
	Config.Channel[OUT2].P1_Yaw_gyro = ON;
	Config.Channel[OUT2].P2_Yaw_gyro = ON;
	
	// OUT3
	Config.Channel[OUT3].P1_elevator_volume = 20;
	Config.Channel[OUT3].P2_elevator_volume = 20;
	Config.Channel[OUT3].P1_rudder_volume = -20;
	Config.Channel[OUT3].P2_rudder_volume = -20;
	Config.Channel[OUT3].P1_Pitch_gyro = ON;
	Config.Channel[OUT3].P2_Pitch_gyro = ON;
	Config.Channel[OUT3].P1_Yaw_gyro = ON;
	Config.Channel[OUT3].P2_Yaw_gyro = ON;
		
	// OUT4
	Config.Channel[OUT4].P1_aileron_volume = 20;
	Config.Channel[OUT4].P2_aileron_volume = 20;
	Config.Channel[OUT4].P1_rudder_volume = 20;
	Config.Channel[OUT4].P2_rudder_volume = 20;
	Config.Channel[OUT4].P1_Roll_gyro = ON;
	Config.Channel[OUT4].P2_Roll_gyro = ON;
	Config.Channel[OUT4].P1_Yaw_gyro = ON;
	Config.Channel[OUT4].P2_Yaw_gyro = ON;
#endif
	
	// Set default sensor LPF
	Config.MPU6050_LPF = 2;				// 6 - 2 = 4. MPU6050's internal LPF. Values are 0x06 = 5Hz, (5)10Hz, (4)21Hz*, (3)44Hz, (2)94Hz, (1)184Hz LPF, (0)260Hz

	// Preset AccZeroNormZ
	Config.AccZeroNormZ		= 128;

	#ifdef KK2Mini
	Config.Contrast = 30;				// Contrast (KK2 Mini)
	#else
	Config.Contrast = 36;				// Contrast (Everything else)
	#endif
}


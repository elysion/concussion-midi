// $Id: main.c 1060 2012-05-18 20:03:51Z tk $
/*
 * Display DIN SR
 *
 * See also the README.txt for additional informations
 *
 * ==========================================================================
 *
 *  Copyright <year> <your name>
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <cmios.h>
#include <pic18fregs.h>

MIOS_ENC_TABLE {
    //  sr pin mode
    MIOS_ENC_ENTRY( 1, 0, MIOS_ENC_MODE_DETENTED ), // V-Pot 1
    MIOS_ENC_ENTRY( 1, 2, MIOS_ENC_MODE_DETENTED ), // V-Pot 2
    MIOS_ENC_ENTRY( 1, 4, MIOS_ENC_MODE_DETENTED ), // V-Pot 3
    MIOS_ENC_ENTRY( 1, 6, MIOS_ENC_MODE_DETENTED ), // V-Pot 4
    MIOS_ENC_ENTRY( 2, 0, MIOS_ENC_MODE_DETENTED ), // V-Pot 5
    MIOS_ENC_ENTRY( 2, 2, MIOS_ENC_MODE_DETENTED ), // V-Pot 6
    MIOS_ENC_ENTRY( 2, 4, MIOS_ENC_MODE_DETENTED ), // V-Pot 7
    MIOS_ENC_ENTRY( 2, 6, MIOS_ENC_MODE_DETENTED ), // V-Pot 8
    MIOS_ENC_ENTRY( 3, 0, MIOS_ENC_MODE_DETENTED ), // V-Pot 9
    MIOS_ENC_ENTRY( 3, 2, MIOS_ENC_MODE_DETENTED ), // V-Pot 10
    MIOS_ENC_ENTRY( 3, 4, MIOS_ENC_MODE_DETENTED ), // V-Pot 11
    MIOS_ENC_ENTRY( 3, 6, MIOS_ENC_MODE_DETENTED ), // V-Pot 12
    MIOS_ENC_ENTRY( 4, 0, MIOS_ENC_MODE_DETENTED ), // V-Pot 13
    MIOS_ENC_ENTRY( 4, 2, MIOS_ENC_MODE_DETENTED ), // V-Pot 14
    MIOS_ENC_ENTRY( 4, 4, MIOS_ENC_MODE_DETENTED ), // V-Pot 15
    MIOS_ENC_ENTRY( 4, 6, MIOS_ENC_MODE_DETENTED ), // V-Pot 16
    
    MIOS_ENC_EOT
};

#include "main.h"

/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////

// status of application (see bitfield declaration in main.h)
app_flags_t app_flags;

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// last ain/din/dout
unsigned char last_din_pin = 0;
unsigned char last_dout_pin = 0;
unsigned char last_din_value = 0;

unsigned char cd_deck_selection_active = 0;

const char effect_names[42][8] = {
    "FLANGER",
    "PHASER ",
    "FILT:92",
    "DELAY  ",
    "BEATMSR",
    "RVGRAIN",
    "TT FX  ",
    "REVERB ",
    "D LOFI ",
    "FLANG P",
    "FLANG F",
    "PHASERP",
    "PHASERF",
    "FILTLFO",
    "FILTPLS",
    "FILTER ",
    "F:92LFO",
    "F:92PLS",
    "GATER  ",
    "ICEVERB",
    "RMODLTR",
    "MULHDRV",
    "TRANSST",
    "REVRBT3",
    "DELAYT3",
    "BEATSLI",
    "FORMFLT",
    "PEAKFLT",
    "TAPEDLY",
    "RAMPDLY",
    "AUTOBNC",
    "BOUNCER",
    "WRMHOLE",
    "ZZZURP ",
    "LASERSL",
    "GRPHASE",
    "STRTCHF",
    "B-O-M  ",
    "PLARWND",
    "STRTCHS",
    "DRKMTTR",
    "EVENTHZ",
    "FLIGHTT"
};

unsigned char selected_fxs[3] = {0,0,0};
unsigned char selected_fx_unit = 1;

int fx_unit_amount[4] = {0,0,0,0};

#define RED 0
#define BLUE 1
#define GREEN 2

typedef enum FX_LEDS {
    FX_LED_YELLOW = 0,
    FX_LED_RED,
    FX_LED_GREEN,
    FX_LED_COUNT
} fxled_t;

#define STATE_ON 1
#define STATE_OFF 0

#define FX_LED_PIN_START 37

#define DECK_CONTROL_DOUT_RANGE_START 48
#define DECK_CONTROL_DOUT_RANGE_STOP 63

unsigned char getPinStateForEvent(unsigned char evnt0, unsigned char evnt2) {
    return (evnt0 == 0x80 || evnt2 == 0x00) ? 0 : 1;
}

unsigned char getPinStateForDeckEvent(unsigned char evnt0, unsigned char evnt2) {
    unsigned char input = getPinStateForEvent(evnt0, evnt2);
    
    if ((!cd_deck_selection_active && input) || (cd_deck_selection_active && !input)) {
        return 1;
    } else {
        return 0;
    }
}

void setDoutRangeTo(unsigned char start, unsigned char end, unsigned char state) {
    unsigned char i = start;
    for (;i <= end; ++i) {
        MIOS_DOUT_PinSet(i, state);
    }
}

void setDeckDoutTo(unsigned char state) {
    setDoutRangeTo(DECK_CONTROL_DOUT_RANGE_START, DECK_CONTROL_DOUT_RANGE_STOP, state);
}

void setFxLedsTo(unsigned char state) {
    unsigned char i = 0;
    unsigned char inversedState = state ? 0 : 1;
    for (;i < FX_LED_COUNT; ++i) {
        MIOS_DOUT_PinSet(FX_LED_PIN_START + i, inversedState);
    }
}

unsigned char getPinFromEvent(unsigned char evnt1) {
    return ((evnt1 / 4) * 4) - (evnt1 % 4) + 3;
}

unsigned char getOffsetForRow(unsigned char row) {
    switch (row) {
        case 0: return 0x0;
        case 1: return 0x40;
        default: return 0x50;
    }
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS after startup to initialize the
// application
/////////////////////////////////////////////////////////////////////////////
void Init(void) __wparam
{
    unsigned char i = 0;
    
    MIOS_LCD_TypeSet(0x07, 0x37, 0x24);
    
    // set shift register update frequency
    MIOS_SRIO_UpdateFrqSet(1); // ms
    
    // we need to set at least one IO shift register pair
    MIOS_SRIO_NumberSet(NUMBER_OF_SRIO);
    
    // debouncing value for DINs
    MIOS_SRIO_DebounceSet(DIN_DEBOUNCE_VALUE);
    
    MIOS_SRIO_TS_SensitivitySet(DIN_TS_SENSITIVITY);
    
    // initialize the AIN driver
    MIOS_AIN_NumberSet(8);   // seven ain channels in useRE
    MIOS_AIN_UnMuxed();      // no AINX4 modules are used
    MIOS_AIN_DeadbandSet(7); // 7bit resolution is used
    
    // set speed mode for encoders
    for(i=0; i<15; ++i) {
        // available speed modes: SLOW, NORMAL and FAST
        MIOS_ENC_SpeedSet(i, MIOS_ENC_SPEED_NORMAL, 2); // encoder, speed mode, divider
    }
    
    // turn off the fx dry wet knob leds
    setFxLedsTo(STATE_OFF);
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS in the mainloop when nothing else is to do
/////////////////////////////////////////////////////////////////////////////
void Tick(void) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is periodically called by MIOS. The frequency has to be
// initialized with MIOS_Timer_Set
/////////////////////////////////////////////////////////////////////////////
void Timer(void) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when the display content should be
// initialized. Thats the case during startup and after a temporary message
// has been printed on the screen
/////////////////////////////////////////////////////////////////////////////
void DISPLAY_Init(void) __wparam
{
    // clear screen
    MIOS_LCD_Clear();
    
    // request display update
    app_flags.DISPLAY_UPDATE_REQ = 1;
}

/////////////////////////////////////////////////////////////////////////////
//  This function is called in the mainloop when no temporary message is shown
//  on screen. Print the realtime messages here
/////////////////////////////////////////////////////////////////////////////
void DISPLAY_Tick(void) __wparam
{
    unsigned char i = 0;
    unsigned char j = 0;
    unsigned char pin = last_din_pin != 0 ? last_din_pin : last_dout_pin;
    // do nothing if no update has been requested
    if( !app_flags.DISPLAY_UPDATE_REQ )
        return;
    
    // clear request
    app_flags.DISPLAY_UPDATE_REQ = 0;
    
    MIOS_LCD_CursorSet(0x0);
    
    for (i = 0; i < 2; ++i) {
        MIOS_LCD_CursorSet(getOffsetForRow(i));
        MIOS_LCD_PrintCString("FX");
        MIOS_LCD_PrintBCD1(i + 1);
        if (selected_fx_unit == i) {
            MIOS_LCD_PrintChar('*');
        } else {
            MIOS_LCD_PrintChar(' ');
        }
        
        for (j = 0; j <= 3; ++j) {
            if (fx_unit_amount[i] > 0 && fx_unit_amount[i] + 1 > j * 32) {
                MIOS_LCD_PrintChar('*');
            } else {
                break;
            }
        }
    }
    
    for (i = 0; i <= 3; ++i) {
        MIOS_LCD_CursorSet(getOffsetForRow(i) + 9);
        MIOS_LCD_PrintCString(effect_names[selected_fxs[i]]);
    }
}

/////////////////////////////////////////////////////////////////////////////
//  This function is called by MIOS when a complete MIDI event has been received
/////////////////////////////////////////////////////////////////////////////
void MPROC_NotifyReceivedEvnt(unsigned char evnt0, unsigned char evnt1, unsigned char evnt2) __wparam
{
    unsigned char pin = 0;
    unsigned char i = 0;
    unsigned char start = 0;
    unsigned char state = 0;
    unsigned char fx_number = 0;
    unsigned char selected_fx = 0;
    
    if (evnt1 == 34) {
        // All FX knob leds
        state = getPinStateForEvent(evnt0, evnt2);
        setFxLedsTo(STATE_OFF);
        MIOS_DOUT_PinSet(FX_LED_PIN_START + FX_LED_GREEN, state);
        last_dout_pin = evnt1;
        
    } else if (evnt1 == 35) {
        // C & D deck selection
        cd_deck_selection_active = getPinStateForEvent(evnt0, evnt2);
        MIOS_DOUT_PinSet(35, cd_deck_selection_active);
        
        last_dout_pin = evnt1;
        last_din_value = evnt2;
        
        // F*king din inversion! => cannot use FX_LED_PIN_START
    } else if (evnt1 >= 36 && evnt1 <= 38) {
        // FX leds
        pin = getPinFromEvent(evnt1);
        //setFxLedsTo(STATE_OFF);
        MIOS_DOUT_PinSet(pin, !getPinStateForEvent(evnt0, evnt2));
        last_dout_pin = evnt1;
        
    } else if (evnt1 >= 39 && evnt1 <= 43 && evnt1 != 40 /* Shift */) {
        // Non deck controls
        pin = getPinFromEvent(evnt1);
        MIOS_DOUT_PinSet(pin, getPinStateForEvent(evnt0, evnt2));
        last_dout_pin = evnt1;

    } else if ( evnt1 > 32 && (evnt0 == 0x80 || evnt0 == 0x90) ) {
        // Deck controls
        pin = getPinFromEvent(evnt1);
        MIOS_DOUT_PinSet(pin, getPinStateForDeckEvent(evnt0, evnt2));
        last_dout_pin = evnt1;
        
    } else if ( evnt0 == 0xb0 )
        if ( evnt1 == 3 ) {
            // Modifier #3 aka effect selection
            selected_fx_unit = evnt2 / 18;
            
        } else if ( evnt1 >= 24 && evnt1 <= 26 ) {
            // Selected effect
            fx_number = evnt1 - 24;
        
            // It is tedious to store the effect number to effect name mappings and
            // thus this code assumes that the 41 effects in Traktor are evenly spaced...
            selected_fx = evnt2 * 41 / 127;
        
            // ... and because they are not we need to increase the fx number after RingModulator
            if (evnt2 >= 64 ) {
                selected_fx++;
            }
        
            selected_fxs[fx_number] = selected_fx;
            
        } else if ( evnt1 == 27 ) {
            // Dry wet adjust
            fx_unit_amount[selected_fx_unit] = evnt2;
            
        }
    
        last_dout_pin = evnt2;

    } else {
        // Multicolor DIN control
        pin = evnt2 / 25 * 3;
        start = evnt1 == 17 ? 0 : 16;
        
        if ( evnt2 < 63) {
            for (i = 0; i < 5; ++i) {
                MIOS_DOUT_PinSet(start + i * 3 + GREEN, i >= evnt2 / 12  ? 0x01 : 0x00);
                MIOS_DOUT_PinSet(start + i * 3 + RED, 0x00);
                MIOS_DOUT_PinSet(start + i * 3 + BLUE, 0x00);
            }
        } else if (evnt2 > 63) {
            for (i = 0; i < 5; ++i) {
                MIOS_DOUT_PinSet(start + i * 3 + RED, i <= evnt2 / 12 - 6 ? 0x01 : 0x00);
                MIOS_DOUT_PinSet(start + i * 3 + BLUE, 0x00);
                MIOS_DOUT_PinSet(start + i * 3 + GREEN, 0x00);
            }
        } else {
            for (i = 0; i <= 15; i += 3) {
                MIOS_DOUT_PinSet(start + i + RED, 0x00);
                MIOS_DOUT_PinSet(start + i + BLUE, 0x00);
                MIOS_DOUT_PinSet(start + i + GREEN, 0x00);
            }
        }
        
        last_dout_pin = evnt0;
        last_din_value = evnt2;
    }
    
    app_flags.DISPLAY_UPDATE_REQ = 1;
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when a MIDI event has been received
// which has been specified in the MIOS_MPROC_EVENT_TABLE
/////////////////////////////////////////////////////////////////////////////
void MPROC_NotifyFoundEvent(unsigned entry, unsigned char evnt0, unsigned char evnt1, unsigned char evnt2) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when a MIDI event has not been completly
// received within 2 seconds
/////////////////////////////////////////////////////////////////////////////
void MPROC_NotifyTimeout(void) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when a MIDI byte has been received
/////////////////////////////////////////////////////////////////////////////
void MPROC_NotifyReceivedByte(unsigned char byte) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS before the shift register are loaded
/////////////////////////////////////////////////////////////////////////////
void SR_Service_Prepare(void) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS after the shift register have been loaded
/////////////////////////////////////////////////////////////////////////////
void SR_Service_Finish(void) __wparam
{
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when an button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void DIN_NotifyToggle(unsigned char pin, unsigned char pin_value) __wparam
{
    // a button has been pressed, send Note at channel 1
    MIOS_MIDI_BeginStream();
    MIOS_MIDI_TxBufferPut(0x90); // Note at channel 1
    MIOS_MIDI_TxBufferPut(pin);  // pin number corresponds to note number
    MIOS_MIDI_TxBufferPut(pin_value ? 0x00 : 0x7f); // buttons are high-active
    MIOS_MIDI_EndStream();
    
    // notify display handler in DISPLAY_Tick() that DIN value has changed
    last_din_pin = pin;
    app_flags.DISPLAY_UPDATE_REQ = 1;
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void ENC_NotifyChange(unsigned char encoder, char incrementer) __wparam
{
    MIOS_MIDI_TxBufferPut(0xb0);           // CC at MIDI Channel #1
    MIOS_MIDI_TxBufferPut(0x10 + encoder); // CC# is 0x10 (16) for first encoder
    MIOS_MIDI_TxBufferPut((0x40 + incrementer) & 0x7f);
    // this "40 +/- speed" format is used by NI software and some others
}

/////////////////////////////////////////////////////////////////////////////
// This function is called by MIOS when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void AIN_NotifyChange(unsigned char pin, unsigned int pin_value) __wparam
{
    // a pot has been moved, send CC# at channel 1
    MIOS_MIDI_TxBufferPut(0xb0); // CC at channel 1
    MIOS_MIDI_TxBufferPut(pin);  // pin number corresponds to CC number
    MIOS_MIDI_TxBufferPut(MIOS_AIN_Pin7bitGet(pin));   // don't send 10bit pin_value,
    // but 7bit value
}

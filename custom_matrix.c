/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "custom_matrix.h"
#include "custom_analog.h"
#include "custom_calibration.h"
#include "custom_scanning.h"
#include "eeconfig_set_defaults.h"
#include "letmesleepsplit75he.h"

// is_keyboard_left()
// returns a bool of whether it is left or right
// is_keyboard_master()
// returns a bool of whether it is connected by usb

// Create global variables storing the row and column pins
#if defined(MATRIX_ROW_PINS) && defined(MATRIX_COL_PINS)
#    ifdef MATRIX_ROW_PINS
SPLIT_MUTABLE_ROW pin_t row_pins[ROWS_PER_HAND] = MATRIX_ROW_PINS;
#    endif // MATRIX_ROW_PINS
#    ifdef MATRIX_COL_PINS
SPLIT_MUTABLE_COL pin_t col_pins[MATRIX_COLS]   = MATRIX_COL_PINS;
#    endif // MATRIX_COL_PINS
#endif

// row offset
static uint8_t row_offset = 0;
// number of loops
static uint8_t number_of_loops = MATRIX_COLS;

// Create array for custom matrix mask
static const matrix_row_t custom_matrix_mask[MATRIX_ROWS] = CUSTOM_MATRIX_MASK;

// External definitions
extern ADCManager adcManager;
extern uint8_t virtual_axes_toggle;

// Initialize everything to zero
analog_key_t analog_key[MATRIX_ROWS][MATRIX_COLS]       = { 0 };
analog_config_t analog_config[MATRIX_ROWS][MATRIX_COLS] = { 0 };
calibration_parameters_t calibration_parameters         = { 0 };

// Define lookup tables
static uint8_t lut_displacement[ANALOG_CAL_MAX_VALUE+1]    = { 0 };
static uint8_t lut_joystick[ANALOG_CAL_MAX_VALUE+1]        = { 0 };
static uint16_t lut_multiplier[ANALOG_MULTIPLIER_LUT_SIZE] = { 0 };

// Create global joystick variables
#ifdef ANALOG_KEY_VIRTUAL_AXES
uint8_t virtual_axes_from_self[4][4]  = { 0 };
uint8_t virtual_axes_from_slave[4][4] = { 0 };
# ifdef JOYSTICK_COORDINATES_LEFT
const uint8_t joystick_coordinates_left[4][2] = JOYSTICK_COORDINATES_LEFT;
# endif
# ifdef JOYSTICK_COORDINATES_RIGHT
const uint8_t joystick_coordinates_right[4][2] = JOYSTICK_COORDINATES_RIGHT;
# endif
# ifdef MOUSE_COORDINATES_LEFT
const uint8_t mouse_coordinates_left[4][2] = MOUSE_COORDINATES_LEFT;
# endif
# ifdef MOUSE_COORDINATES_RIGHT
const uint8_t mouse_coordinates_right[4][2] = MOUSE_COORDINATES_RIGHT;
# endif
# ifdef SCROLL_COORDINATES_LEFT
const uint8_t scroll_coordinates_left[4][2] = MOUSE_COORDINATES_RIGHT;
# endif
# ifdef SCROLL_COORDINATES_RIGHT
const uint8_t scroll_coordinates_right[4][2] = MOUSE_COORDINATES_RIGHT;
# endif
#endif



// Initialise matrix
void matrix_init_custom(void){
#ifdef SPLIT_KEYBOARD
    // Set pinout for right half if pinout for that half is defined
    if (!is_keyboard_left()) {
#    ifdef MATRIX_ROW_PINS_RIGHT
        const pin_t row_pins_right[ROWS_PER_HAND] = MATRIX_ROW_PINS_RIGHT;
        for (uint8_t i = 0; i < ROWS_PER_HAND; i++) {
            row_pins[i] = row_pins_right[i]; // row_pins is a global variable
        }
#    endif
#    ifdef MATRIX_COL_PINS_RIGHT
        const pin_t col_pins_right[MATRIX_COLS] = MATRIX_COL_PINS_RIGHT;
        for (uint8_t i = 0; i < MATRIX_COLS; i++) {
            col_pins[i] = col_pins_right[i]; // col_pins is a global variable
        }
#    endif
        // set row offset if right hand
        row_offset = ROWS_PER_HAND;

        /* thisHand and thatHand are already defined in "matrix_common.c"
        thisHand = isLeftHand ? 0 : (ROWS_PER_HAND);
        thatHand = ROWS_PER_HAND - thisHand; */
    }
#endif
    
    // Generate lookup tables
    for (uint16_t i = 0; i < ANALOG_CAL_MAX_VALUE+1; i++){
        lut_displacement[i] = analog_to_distance(i, &calibration_parameters.displacement);
        lut_joystick[i]     = analog_to_distance(i, &calibration_parameters.joystick);
    }
    for (uint16_t i = 0; i < ANALOG_MULTIPLIER_LUT_SIZE; i++){
        // rest -> fully pressed value
        lut_multiplier[i] = rest_to_absolute_change(i, &calibration_parameters.multiplier);
    }

    // modify number of times to loop
#ifdef MATRIX_DIRECT
    if (is_keyboard_left()){
        number_of_loops = MATRIX_DIRECT / MAX_MUXES_PER_ADC;
    }
#endif
#ifdef MATRIX_DIRECT_RIGHT
    if (!is_keyboard_left()) {
        number_of_loops = MATRIX_DIRECT_RIGHT / MAX_MUXES_PER_ADC;
    }
#endif
    
    // Load default values (before real values are loaded)
    set_default_calibration_parameters();
    set_default_analog_config();
    set_default_analog_key();

    // Initialize multiplexer GPIO pins
    multiplexer_init();
    // Initialize ADC pins
    initADCGroups(&adcManager);
    // Wait some time for ADCs to start
    wait_ms(100);
    return;
}

// create a previous matrix
static matrix_row_t previous_matrix[MATRIX_ROWS];
// do a "lite" custom matrix
bool matrix_scan_custom(matrix_row_t current_matrix[]){
    // update previous matrix
    memcpy(previous_matrix, current_matrix, sizeof(previous_matrix));

    // store virtual axes
    static uint8_t virtual_axes_temp[4][4] = { 0 };

    // store cols
    static uint8_t current_col = 0;
    // store channel
    static uint8_t current_direct_pin = 0;

    // create variables to track time
    static bool save_rest_values = false;
    static bool time_to_be_updated = false;
    static uint32_t time_current = 0;
    static uint32_t time_next_calibration = 0;
    time_current = timer_read32();
    
    // check if keyboard should be calibrated
    if (time_current > time_next_calibration){
        save_rest_values = true;
        time_to_be_updated = true;
        number_of_loops = MATRIX_COLS;
    }

    // loop through columns
    for (uint8_t i = 0; i < number_of_loops; i++){

        // switch multiplexer to current column
        select_multiplexer_channel(current_col);

        // scan all rows
        adcStartAllConversions(&adcManager, current_direct_pin);

        // run on every row
        for (uint8_t current_row = row_offset; current_row < row_offset + ROWS_PER_HAND; current_row++){

            // row, col that can be modified
            uint8_t this_row = current_row;
            uint8_t this_col = current_col;

#        if (MAX_MUXES_PER_ADC) > 1
            // how many ADC pins were read on this row
            uint8_t number_pins_read = 1;
            // check if the current row contains direct pins
#        ifdef MATRIX_DIRECT_ROW
            if (this_row == MATRIX_DIRECT_ROW){
                number_pins_read = MAX_MUXES_PER_ADC;
            }
#        endif
#        ifdef MATRIX_DIRECT_ROW_RIGHT
            if (this_row == MATRIX_DIRECT_ROW_RIGHT){
                number_pins_read = MAX_MUXES_PER_ADC;
            }
#        endif
            // run multiple times if multiple direct pins were read
            for (uint8_t j = 0; j < number_pins_read; j++){
#        endif

                // increment current direct pin
#            ifdef MATRIX_DIRECT_ROW
                if (this_row == MATRIX_DIRECT_ROW){
                    this_col = current_direct_pin;
                    current_direct_pin = (current_direct_pin + 1) % MATRIX_DIRECT_ROW;
                }
#            endif
#            ifdef MATRIX_DIRECT_ROW_RIGHT
                if (this_row == MATRIX_DIRECT_ROW_RIGHT){
                    this_col = current_direct_pin;
                    current_direct_pin = (current_direct_pin + 1) % MATRIX_DIRECT_ROW_RIGHT;
                }
#            endif

                // if the key should be scanned
                if (BIT_GET(custom_matrix_mask[this_row], this_col)){
                    
                    // get raw adc value
                    uint16_t raw = getADCSample(&adcManager, this_row, current_direct_pin);
                    // account for magnet polarity (bipolar sensor, 12-bit reading)
                    if (raw < ANALOG_RAW_MAX_VALUE + 1){
                        raw = ANALOG_RAW_MAX_VALUE - raw;
                    }
                    else { // raw > ANALOG_RAW_MAX_VALUE
                        raw = raw - ANALOG_RAW_MAX_VALUE - 1;
                    }

                    // run calibration (output 0-1023)
                    uint16_t calibrated = scale_raw_value(raw, analog_key[this_row][this_col].rest, lut_multiplier);

                    // run lookup table (output 0-200, where 200=4mm)
                    uint8_t displacement = lut_displacement[calibrated];
 
                    if (
                        // run actuation (output bool, always false if mode > 4)
                        actuation(
                            &analog_config[this_row][this_col], 
                            &analog_key[this_row][this_col], 
                            &current_matrix[this_row], 
                            this_col,
                            displacement, 
                            calibration_parameters.displacement.max_output
                        )
                    )
                    {
                        // update time
                        time_to_be_updated = true;
                    }

#                ifdef DKS_ENABLE
                    // handle DKS
                    if (analog_config[this_row][this_col].mode > 4){
                        this_row = row_offset + ROWS_PER_HAND - 1; // last row on current hand
                        for (uint8_t l = 0; l < 4; l++){ // run actuation on four keys
                            this_col = l + (4 * (analog_config[this_row][this_col].mode - 5));
                            
                            if (
                                // run actuation (output bool, always false if mode > 4)
                                actuation(
                                    &analog_config[this_row][this_col], 
                                    &analog_key[this_row][this_col], 
                                    &current_matrix[this_row], 
                                    this_col,
                                    displacement, 
                                    calibration_parameters.displacement.max_output
                                )
                            )
                            {
                                // update time
                                time_to_be_updated = true;
                            }
                        }
                    }
#                endif

                    // handle joystick
#                ifdef ANALOG_KEY_VIRTUAL_AXES
                    if (
                        BIT_GET(virtual_axes_toggle, 0) || 
                        BIT_GET(virtual_axes_toggle, 1) || 
                        BIT_GET(virtual_axes_toggle, 2)
                    )
                    {
                        for (uint8_t k = 0; k < 4; k++){
#                        ifdef JOYSTICK_COORDINATES_LEFT     
                            if (
                                BIT_GET(virtual_axes_toggle, 0)             && 
                                this_col == joystick_coordinates_left[k][1] && 
                                this_row == joystick_coordinates_left[k][0]
                            )
                            {
                                virtual_axes_temp[0][k] += lut_joystick[calibrated];
                            }
#                        endif
#                        ifdef JOYSTICK_COORDINATES_RIGHT
                            if (
                                BIT_GET(virtual_axes_toggle, 0)              && 
                                this_col == joystick_coordinates_right[k][1] && 
                                this_row == joystick_coordinates_right[k][0]
                            )
                            {
                                virtual_axes_temp[1][k] += lut_joystick[calibrated];
                            }
#                        endif
#                        ifdef MOUSE_COORDINATES_LEFT
                            if (
                                BIT_GET(virtual_axes_toggle, 1)          && 
                                this_col == mouse_coordinates_left[k][1] && 
                                this_row == mouse_coordinates_left[k][0]
                            )
                            {
                                virtual_axes_temp[2][k] += lut_joystick[calibrated];
                            }
#                        endif
#                        ifdef SCROLL_COORDINATES_LEFT
                            if (
                                BIT_GET(virtual_axes_toggle, 1)           && 
                                this_col == scroll_coordinates_left[k][1] && 
                                this_row == scroll_coordinates_left[k][0]
                            )
                            {
                                virtual_axes_temp[3][k] += lut_joystick[calibrated];
                            }
#                        endif
#                        ifdef MOUSE_COORDINATES_RIGHT
                            if (
                                BIT_GET(virtual_axes_toggle, 2)           && 
                                this_col == mouse_coordinates_right[k][1] && 
                                this_row == mouse_coordinates_right[k][0]
                            )
                            {
                                virtual_axes_temp[2][k] += lut_joystick[calibrated];
                            }
#                        endif
#                        ifdef SCROLL_COORDINATES_RIGHT
                            if (
                                BIT_GET(virtual_axes_toggle, 2)            && 
                                this_col == scroll_coordinates_right[k][1] && 
                                this_row == scroll_coordinates_right[k][0]
                            )
                            {
                                virtual_axes_temp[3][k] += lut_joystick[calibrated];
                            }
#                        endif
                        }
                    }
#                    endif

                    // save raw value
                    if (save_rest_values) {
                        analog_key[this_row][this_col].rest = MIN(raw, ANALOG_MULTIPLIER_LUT_SIZE - 1);
                    }
                }
#        if (MAX_MUXES_PER_ADC) > 1
            }
#        endif
        }
        // increment current col
        current_col = (current_col + 1) % MATRIX_COLS;
    }

    if (time_to_be_updated){
        // add 5 minutes to current time
        time_next_calibration = time_current + 300000;
        // reset booleans
        time_to_be_updated = false;
        save_rest_values = false;
    }

#ifdef ANALOG_KEY_VIRTUAL_AXES
    // copy over virtual axes
    memcpy(virtual_axes_from_self, virtual_axes_temp, sizeof(virtual_axes_temp));
    // clear virtual axes temp
    memset(virtual_axes_temp, 0, sizeof(virtual_axes_temp));
#endif

    // compare current matrix against previous matrix
    return memcmp(previous_matrix, current_matrix, sizeof(previous_matrix)) != 0;
}



//done: adc reading (good)
//done: split matrix stuff (might not work)
//done: lite matrix scanning (badly)
//done: lookup table generation (need new values tho)
//done: load default settings
//done: accessing settings from EEPROM
//done: split communication - custom sync for joystick
//done: DKS (uses mode instead of keycode)
//done: virtual axes for joystick https://docs.qmk.fm/features/joystick#virtual-axes
//done: use mouse report overrides https://docs.qmk.fm/features/pointing_device#manipulating-mouse-reports


//todo: get values for generating: predicted abs diff between rest and down

//todo: add method to modify settings & save to EEPROM

//todo: fork vial to add extra tabs: analog settings & DKS & mouse keys & joystick
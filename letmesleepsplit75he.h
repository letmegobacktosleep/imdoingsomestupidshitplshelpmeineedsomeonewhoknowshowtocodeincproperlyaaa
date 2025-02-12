/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

extern uint8_t virtual_axes_toggle;

#define BIT_SET(byte, nbit) ((byte) |=  (1 << (nbit)))
#define BIT_CLR(byte, nbit) ((byte) &= ~(1 << (nbit)))
#define BIT_FLP(byte, nbit) ((byte) ^=  (1 << (nbit)))
#define BIT_GET(byte, nbit) ((byte) &   (1 << (nbit)))

enum custom_keycodes {
    KC_JS_TG = QK_KB_0,
    KC_MS_TG,
    KC_MS_MO,
    KC_MS_TG1,
    KC_MS_MO1,
    KC_MS_TG2,
    KC_MS_MO2,
};
/* This goes in the vial.json
"customKeycodes": [
    {"name": "Analog Joystick Toggle",
    "title": "Toggle use of WASD & PL;' as a joystick",
    "shortName": "KC_JS_TG"
    },
    {"name": "Analog Mouse Toggle All",
    "title": "Toggle use of WASD & Arrow Keys to control your mouse",
    "shortName": "KC_MS_TG"
    },
    {"name": "Analog Mouse Momentary All",
    "title": "Momentarily use WASD & Arrow Keys to control your mouse",
    "shortName": "KC_MS_MO"
    }
    {"name": "Analog Mouse Toggle Left",
    "title": "Toggle use of WASD to control your mouse",
    "shortName": "KC_MS_TG1"
    },
    {"name": "Analog Mouse Momentary Right",
    "title": "Momentarily use WASD Keys to control your mouse",
    "shortName": "KC_MS_MO1"
    }
    {"name": "Analog Mouse Toggle Left",
    "title": "Toggle use of Arrow Keys to control your mouse",
    "shortName": "KC_MS_TG2"
    },
    {"name": "Analog Mouse Momentary Right",
    "title": "Momentarily use Arrow Keys to control your mouse",
    "shortName": "KC_MS_MO2"
    }
],
*/
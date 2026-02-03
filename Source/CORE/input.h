#ifndef INPUT_H
#define INPUT_H

#include "joystick.h"

#define JOY_BUTTON_ENTER 0x01
#define JOY_BUTTON_ESC 0x02

enum {
    IN_KEY_NONE = 0,
    IN_KEY_UP,
    IN_KEY_DOWN,
    IN_KEY_LEFT,
    IN_KEY_RIGHT,
    IN_KEY_ENTER,
    IN_KEY_SPACE,
    IN_KEY_ESC
};

#define KEY_P 0x19

int in_keyhit(void);
int in_poll(void);
void in_clear(void);
int in_any_down(void);
int in_joystick_available(void);
int in_joystick_state(JoystickState *state);
int in_joystick_direction(int *dir_x, int *dir_y, unsigned char *buttons);
int Input_Pressed(int key);

#endif

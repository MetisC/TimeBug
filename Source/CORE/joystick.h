#ifndef JOYSTICK_H
#define JOYSTICK_H

typedef struct {
    int x;
    int y;
    unsigned char buttons;
} JoystickState;

int joy_available(void);
int joy_read(JoystickState *state);
int joy_get_direction(const JoystickState *state, int *dir_x, int *dir_y);
void joy_reset_calibration(void);

#endif

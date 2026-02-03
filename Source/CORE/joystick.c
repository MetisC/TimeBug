#include "joystick.h"

#include <dos.h>
#include <conio.h>

#define JOY_PORT 0x201
#define JOY_TIMEOUT 8000
#define JOY_DEFAULT_DEADZONE 30

static int g_center_x = -1;
static int g_center_y = -1;
static int g_deadzone = JOY_DEFAULT_DEADZONE;

static void joy_calibrate_center(const JoystickState *state)
{
    if (!state) {
        return;
    }

    if (state->x >= JOY_TIMEOUT || state->y >= JOY_TIMEOUT) {
        return;
    }

    if (g_center_x < 0) {
        g_center_x = state->x;
    }

    if (g_center_y < 0) {
        g_center_y = state->y;
    }
}

void joy_reset_calibration(void)
{
    g_center_x = -1;
    g_center_y = -1;
}

int joy_read(JoystickState *state)
{
    int x = 0;
    int y = 0;
    int port;

    if (!state) {
        return 0;
    }

    outp(JOY_PORT, 0xFF);

    while (1) {
        port = inp(JOY_PORT);
        if (port & 0x01) {
            x++;
        }
        if (port & 0x02) {
            y++;
        }

        if ((port & 0x03) == 0) {
            break;
        }

        if (x >= JOY_TIMEOUT && y >= JOY_TIMEOUT) {
            break;
        }
    }

    state->x = x;
    state->y = y;

    port = inp(JOY_PORT);
    state->buttons = 0;
    if ((port & 0x10) == 0) {
        state->buttons |= 0x01;
    }
    if ((port & 0x20) == 0) {
        state->buttons |= 0x02;
    }

    joy_calibrate_center(state);
    return 1;
}

int joy_available(void)
{
    JoystickState state;

    if (!joy_read(&state)) {
        return 0;
    }

    if (state.x >= JOY_TIMEOUT && state.y >= JOY_TIMEOUT) {
        return 0;
    }

    return 1;
}

int joy_get_direction(const JoystickState *state, int *dir_x, int *dir_y)
{
    int dx = 0;
    int dy = 0;

    if (!state || !dir_x || !dir_y) {
        return 0;
    }

    if (g_center_x < 0 || g_center_y < 0) {
        *dir_x = 0;
        *dir_y = 0;
        return 1;
    }

    if (state->x < (g_center_x - g_deadzone)) {
        dx = -1;
    } else if (state->x > (g_center_x + g_deadzone)) {
        dx = 1;
    }

    if (state->y < (g_center_y - g_deadzone)) {
        dy = -1;
    } else if (state->y > (g_center_y + g_deadzone)) {
        dy = 1;
    }

    *dir_x = dx;
    *dir_y = dy;
    return 1;
}

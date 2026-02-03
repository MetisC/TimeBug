#include "input.h"
#include "keyboard.h"
#include "joystick.h"
#include "options.h"
#include "sound.h"

static int g_joy_pending = IN_KEY_NONE;
static int g_joy_last_dx = 0;
static int g_joy_last_dy = 0;
static unsigned char g_joy_last_buttons = 0;
static unsigned char g_prev_keys[128];

static int input_joystick_enabled(void)
{
    const GameOptions *options = options_get();
    return options && options->input_mode == INPUT_JOYSTICK && in_joystick_available();
}

static int input_joystick_event(void)
{
    JoystickState state;
    int dx = 0;
    int dy = 0;
    int event = IN_KEY_NONE;
    unsigned char buttons;

    if (!in_joystick_state(&state)) {
        return IN_KEY_NONE;
    }

    joy_get_direction(&state, &dx, &dy);
    buttons = state.buttons;

    if (dy != 0 && dy != g_joy_last_dy) {
        event = (dy < 0) ? IN_KEY_UP : IN_KEY_DOWN;
    } else if (dx != 0 && dx != g_joy_last_dx) {
        event = (dx < 0) ? IN_KEY_LEFT : IN_KEY_RIGHT;
    } else if ((buttons & JOY_BUTTON_ENTER) && !(g_joy_last_buttons & JOY_BUTTON_ENTER)) {
        event = IN_KEY_ENTER;
    } else if ((buttons & JOY_BUTTON_ESC) && !(g_joy_last_buttons & JOY_BUTTON_ESC)) {
        event = IN_KEY_ESC;
    }

    g_joy_last_dx = dx;
    g_joy_last_dy = dy;
    g_joy_last_buttons = buttons;

    return event;
}

int in_poll(void)
{
    int key;

    sound_update();

    key = kb_poll();
    if (key != IN_KEY_NONE) {
        return key;
    }

    if (!input_joystick_enabled()) {
        return IN_KEY_NONE;
    }

    if (g_joy_pending != IN_KEY_NONE) {
        key = g_joy_pending;
        g_joy_pending = IN_KEY_NONE;
        return key;
    }

    return input_joystick_event();
}

void in_clear(void)
{
    int i;

    while (kb_keyhit()) {
        kb_poll();
    }
    g_joy_pending = IN_KEY_NONE;
    for (i = 0; i < 128; ++i) {
        g_prev_keys[i] = 0;
    }
}

int in_any_down(void)
{
    if (kb_any_down()) {
        return 1;
    }

    if (input_joystick_enabled()) {
        JoystickState state;
        int dx = 0;
        int dy = 0;

        if (in_joystick_state(&state)) {
            joy_get_direction(&state, &dx, &dy);
            if (dx != 0 || dy != 0 || state.buttons != 0) {
                return 1;
            }
        }
    }

    return 0;
}

int in_joystick_available(void)
{
    return joy_available();
}

int in_joystick_state(JoystickState *state)
{
    return joy_read(state);
}

int in_joystick_direction(int *dir_x, int *dir_y, unsigned char *buttons)
{
    JoystickState state;

    if (!dir_x || !dir_y) {
        return 0;
    }

    if (!in_joystick_state(&state)) {
        return 0;
    }

    joy_get_direction(&state, dir_x, dir_y);
    if (buttons) {
        *buttons = state.buttons;
    }

    return 1;
}

int in_keyhit(void)
{
    sound_update();

    if (kb_keyhit()) {
        return 1;
    }

    if (!input_joystick_enabled()) {
        return 0;
    }

    if (g_joy_pending != IN_KEY_NONE) {
        return 1;
    }

    g_joy_pending = input_joystick_event();
    return g_joy_pending != IN_KEY_NONE;
}

int Input_Pressed(int key)
{
    int down;

    if (key < 0 || key >= 128) {
        return 0;
    }

    down = kb_down((unsigned char)key);
    if (down && !g_prev_keys[key]) {
        g_prev_keys[key] = 1;
        return 1;
    }

    g_prev_keys[key] = (unsigned char)down;
    return 0;
}

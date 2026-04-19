#include "inputManager/Input.h"
#include <algorithm>

const std::unordered_map<std::string, SDL_Scancode> keycode_to_scancode = {
    // Directional (arrow) Keys
	{"up", SDL_SCANCODE_UP},
	{"down", SDL_SCANCODE_DOWN},
	{"right", SDL_SCANCODE_RIGHT},
	{"left", SDL_SCANCODE_LEFT},

	// Misc Keys
	{"escape", SDL_SCANCODE_ESCAPE},

	// Modifier Keys
	{"lshift", SDL_SCANCODE_LSHIFT},
	{"rshift", SDL_SCANCODE_RSHIFT},
	{"lctrl", SDL_SCANCODE_LCTRL},
	{"rctrl", SDL_SCANCODE_RCTRL},
	{"lalt", SDL_SCANCODE_LALT},
	{"ralt", SDL_SCANCODE_RALT},

	// Editing Keys
	{"tab", SDL_SCANCODE_TAB},
	{"return", SDL_SCANCODE_RETURN},
	{"enter", SDL_SCANCODE_RETURN},
	{"backspace", SDL_SCANCODE_BACKSPACE},
	{"delete", SDL_SCANCODE_DELETE},
	{"insert", SDL_SCANCODE_INSERT},

	// Character Keys
	{"space", SDL_SCANCODE_SPACE},
	{"a", SDL_SCANCODE_A},
	{"b", SDL_SCANCODE_B},
	{"c", SDL_SCANCODE_C},
	{"d", SDL_SCANCODE_D},
	{"e", SDL_SCANCODE_E},
	{"f", SDL_SCANCODE_F},
	{"g", SDL_SCANCODE_G},
	{"h", SDL_SCANCODE_H},
	{"i", SDL_SCANCODE_I},
	{"j", SDL_SCANCODE_J},
	{"k", SDL_SCANCODE_K},
	{"l", SDL_SCANCODE_L},
	{"m", SDL_SCANCODE_M},
	{"n", SDL_SCANCODE_N},
	{"o", SDL_SCANCODE_O},
	{"p", SDL_SCANCODE_P},
	{"q", SDL_SCANCODE_Q},
	{"r", SDL_SCANCODE_R},
	{"s", SDL_SCANCODE_S},
	{"t", SDL_SCANCODE_T},
	{"u", SDL_SCANCODE_U},
	{"v", SDL_SCANCODE_V},
	{"w", SDL_SCANCODE_W},
	{"x", SDL_SCANCODE_X},
	{"y", SDL_SCANCODE_Y},
	{"z", SDL_SCANCODE_Z},
	{"0", SDL_SCANCODE_0},
	{"1", SDL_SCANCODE_1},
	{"2", SDL_SCANCODE_2},
	{"3", SDL_SCANCODE_3},
	{"4", SDL_SCANCODE_4},
	{"5", SDL_SCANCODE_5},
	{"6", SDL_SCANCODE_6},
	{"7", SDL_SCANCODE_7},
	{"8", SDL_SCANCODE_8},
	{"9", SDL_SCANCODE_9},
	{"/", SDL_SCANCODE_SLASH},
	{";", SDL_SCANCODE_SEMICOLON},
	{"=", SDL_SCANCODE_EQUALS},
	{"-", SDL_SCANCODE_MINUS},
	{".", SDL_SCANCODE_PERIOD},
	{",", SDL_SCANCODE_COMMA},
	{"[", SDL_SCANCODE_LEFTBRACKET},
	{"]", SDL_SCANCODE_RIGHTBRACKET},
	{"\\", SDL_SCANCODE_BACKSLASH},
	{"'", SDL_SCANCODE_APOSTROPHE}
};

namespace
{
    bool resolve_scancode(const std::string& keycode, SDL_Scancode& out_scancode)
    {
        auto it = keycode_to_scancode.find(keycode);
        if (it == keycode_to_scancode.end())
        {
            return false;
        }
        out_scancode = it->second;
        return true;
    }
    bool is_supported_mouse_button(const int button)
    {
        return button == SDL_BUTTON_LEFT || button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT;
    }
}

void InputManager::Init()
{
    just_became_down_scancodes.clear();
    just_became_up_scancodes.clear();
    keyboard_states.clear();
    just_became_down_buttons.clear();
    just_became_up_buttons.clear();
    mouse_button_states.clear();
    mouse_position = glm::vec2(0.0f, 0.0f);
    mouse_scroll_this_frame = 0.0f;
}

void InputManager::ProcessEvent(const SDL_Event & e)
{
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
    {
        SDL_Scancode scancode = e.key.keysym.scancode;
        bool is_down = (e.type == SDL_KEYDOWN);
        auto it = keyboard_states.find(scancode);
        INPUT_STATE prev_state = (it != keyboard_states.end()) ? it->second : INPUT_STATE_UP;
        INPUT_STATE new_state;
        if (is_down)
        {
            new_state = (prev_state == INPUT_STATE_UP || prev_state == INPUT_STATE_JUST_BECAME_UP) ? INPUT_STATE_JUST_BECAME_DOWN : INPUT_STATE_DOWN;
            just_became_up_scancodes.erase(
                std::remove(just_became_up_scancodes.begin(), just_became_up_scancodes.end(), scancode),
                just_became_up_scancodes.end()
            );
            if (std::find(just_became_down_scancodes.begin(), just_became_down_scancodes.end(), scancode) == just_became_down_scancodes.end())
            {
                just_became_down_scancodes.push_back(scancode);
            }
        }
        else
        {
            new_state = (prev_state == INPUT_STATE_DOWN || prev_state == INPUT_STATE_JUST_BECAME_DOWN) ? INPUT_STATE_JUST_BECAME_UP : INPUT_STATE_UP;
            just_became_down_scancodes.erase(
                std::remove(just_became_down_scancodes.begin(), just_became_down_scancodes.end(), scancode),
                just_became_down_scancodes.end()
            );
            if (std::find(just_became_up_scancodes.begin(), just_became_up_scancodes.end(), scancode) == just_became_up_scancodes.end())
            {
                just_became_up_scancodes.push_back(scancode);
            }
        }
        keyboard_states[scancode] = new_state;
    }
    else if (e.type == SDL_MOUSEMOTION)
    {
        mouse_position = glm::vec2(static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
    }
    else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
    {
        const int button = e.button.button;
        if (!is_supported_mouse_button(button))
        {
            return;
        }
        const bool is_down = (e.type == SDL_MOUSEBUTTONDOWN);
        auto it = mouse_button_states.find(button);
        INPUT_STATE prev_state = (it != mouse_button_states.end()) ? it->second : INPUT_STATE_UP;
        INPUT_STATE new_state = INPUT_STATE_UP;
        if (is_down)
        {
            new_state = (prev_state == INPUT_STATE_UP || prev_state == INPUT_STATE_JUST_BECAME_UP) ? INPUT_STATE_JUST_BECAME_DOWN : INPUT_STATE_DOWN;
            just_became_up_buttons.erase(
                std::remove(just_became_up_buttons.begin(), just_became_up_buttons.end(), button),
                just_became_up_buttons.end()
            );
            if (std::find(just_became_down_buttons.begin(), just_became_down_buttons.end(), button) == just_became_down_buttons.end())
            {
                just_became_down_buttons.push_back(button);
            }
        }
        else
        {
            new_state = (prev_state == INPUT_STATE_DOWN || prev_state == INPUT_STATE_JUST_BECAME_DOWN) ? INPUT_STATE_JUST_BECAME_UP : INPUT_STATE_UP;
            just_became_down_buttons.erase(
                std::remove(just_became_down_buttons.begin(), just_became_down_buttons.end(), button),
                just_became_down_buttons.end()
            );
            if (std::find(just_became_up_buttons.begin(), just_became_up_buttons.end(), button) == just_became_up_buttons.end())
            {
                just_became_up_buttons.push_back(button);
            }
        }
        mouse_button_states[button] = new_state;
    }
    else if (e.type == SDL_MOUSEWHEEL)
    {
        mouse_scroll_this_frame += e.wheel.preciseY;
    }
}

void InputManager::LateUpdate()
{
    for (SDL_Scancode scancode : just_became_down_scancodes)
    {
        keyboard_states[scancode] = INPUT_STATE_DOWN;
    }
    for (SDL_Scancode scancode : just_became_up_scancodes)
    {
        keyboard_states[scancode] = INPUT_STATE_UP;
    }
    just_became_down_scancodes.clear();
    just_became_up_scancodes.clear();

    for (int button : just_became_down_buttons)
    {
        mouse_button_states[button] = INPUT_STATE_DOWN;
    }
    for (int button : just_became_up_buttons)
    {
        mouse_button_states[button] = INPUT_STATE_UP;
    }
    just_became_down_buttons.clear();
    just_became_up_buttons.clear();

    mouse_scroll_this_frame = 0.0f;
}

bool InputManager::GetKey(const std::string& keycode)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    if (!resolve_scancode(keycode, scancode))
    {
        return false;
    }
    auto it = keyboard_states.find(scancode);
    if (it != keyboard_states.end())
    {
        INPUT_STATE state = it->second;
        return state == INPUT_STATE_DOWN || state == INPUT_STATE_JUST_BECAME_DOWN;
    }
    return false;
}

bool InputManager::GetKeyDown(const std::string& keycode)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    if (!resolve_scancode(keycode, scancode))
    {
        return false;
    }
    return std::find(just_became_down_scancodes.begin(), just_became_down_scancodes.end(), scancode) != just_became_down_scancodes.end();
}

bool InputManager::GetKeyUp(const std::string& keycode)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    if (!resolve_scancode(keycode, scancode))
    {
        return false;
    }
    return std::find(just_became_up_scancodes.begin(), just_became_up_scancodes.end(), scancode) != just_became_up_scancodes.end();
}

bool InputManager::GetMouseButton(int button)
{
    if (!is_supported_mouse_button(button))
    {
        return false;
    }
    auto it = mouse_button_states.find(button);
    if (it == mouse_button_states.end())
    {
        return false;
    }
    const INPUT_STATE state = it->second;
    return state == INPUT_STATE_DOWN || state == INPUT_STATE_JUST_BECAME_DOWN;
}

bool InputManager::GetMouseButtonDown(int button)
{
    if (!is_supported_mouse_button(button))
    {
        return false;
    }
    return std::find(just_became_down_buttons.begin(), just_became_down_buttons.end(), button) != just_became_down_buttons.end();
}

bool InputManager::GetMouseButtonUp(int button)
{
    if (!is_supported_mouse_button(button))
    {
        return false;
    }
    return std::find(just_became_up_buttons.begin(), just_became_up_buttons.end(), button) != just_became_up_buttons.end();
}

glm::vec2 InputManager::GetMousePosition()
{
    return mouse_position;
}

float InputManager::GetMouseScrollDelta()
{
    return mouse_scroll_this_frame;
}

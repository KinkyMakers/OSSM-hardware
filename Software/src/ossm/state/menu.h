#ifndef OSSM_STATE_MENU_H
#define OSSM_STATE_MENU_H

#include "constants/Menu.h"

/**
 * Menu state - tracks current menu selection
 */
struct MenuState {
    Menu currentOption = Menu::SimplePenetration;
};

extern MenuState menuState;

#endif  // OSSM_STATE_MENU_H

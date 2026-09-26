#pragma once

enum UiScreen
{
    UI_MAIN_MENU,
    UI_EDIT_SEQUENCE,
    UI_CV_MENU,
    UI_SAVE_MENU,
    UI_LOAD_MENU,
    UI_NEW_MENU,
    UI_PATTERNS_MENU,
    UI_PATTERN_ORDER_MENU,
    UI_SETTINGS_MENU,
};

enum AssignMode
{
    ASSIGN_LENGTH = 0,
    ASSIGN_HITS,
};

extern UiScreen uiScreen;
extern int      activeCvIndex;

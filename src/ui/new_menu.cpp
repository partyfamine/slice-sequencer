#include "new_menu.h"
#include "app.h"
#include "oled_ui.h"
#include "storage.h"

static bool cursorOnYes;

void NewMenuEnter()
{
    cursorOnYes = false;
}

void NewMenuProcessEncoder()
{
    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        cursorOnYes = true;
    }
    else if(inc < 0)
    {
        cursorOnYes = false;
    }

    if(patch.encoder.RisingEdge())
    {
        if(cursorOnYes)
        {
            ResetToNewSequence();
        }
        uiScreen = UI_MAIN_MENU;
    }
}

void NewMenuDraw()
{
    DrawChars(0, 0, "Create a new", true);
    DrawChars(0, 10, "sequence?", true);
    DrawChars(0, 30, "No", cursorOnYes);
    DrawChars(0, 40, "Yes", !cursorOnYes);
}

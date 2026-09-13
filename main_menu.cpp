#include "main_menu.h"
#include "app.h"
#include "oled_ui.h"

static const int kMainMenuCount = 8;

static const char* kMainMenuLabels[kMainMenuCount] = {
    "Edit Sequence",
    "CV1 (TODO)",
    "CV2 (TODO)",
    "CV3 (TODO)",
    "CV4 (TODO)",
    "Save (TODO)",
    "Load (TODO)",
    "Settings (TODO)",
};

static int mainMenuIndex;

void MainMenuInit()
{
    mainMenuIndex = 0;
}

void MainMenuProcessEncoder()
{
    int inc = patch.encoder.Increment();
    mainMenuIndex += inc;
    mainMenuIndex = (mainMenuIndex % kMainMenuCount + kMainMenuCount)
                    % kMainMenuCount;

    if(patch.encoder.RisingEdge() && mainMenuIndex == 0)
    {
        uiScreen = UI_EDIT_SEQUENCE;
    }
}

void MainMenuDraw()
{
    DrawChars(0, 0, kMainMenuLabels[mainMenuIndex], true);
    DrawSequence(20, stepNumber);
}

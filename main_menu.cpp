#include "main_menu.h"
#include "app.h"
#include "oled_ui.h"
#include "cv_menu.h"
#include "save_menu.h"
#include "load_menu.h"
#include "new_menu.h"

static const int kMainMenuCount = 9;

static const char* kMainMenuLabels[kMainMenuCount] = {
    "Edit Sequence",
    "CV1",
    "CV2",
    "CV3",
    "CV4",
    "Save",
    "Load",
    "New",
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

    if(patch.encoder.RisingEdge())
    {
        if(mainMenuIndex == 0)
        {
            uiScreen = UI_EDIT_SEQUENCE;
        }
        else if(mainMenuIndex >= 1 && mainMenuIndex <= 4)
        {
            CvMenuEnter(mainMenuIndex - 1);
            uiScreen = UI_CV_MENU;
        }
        else if(mainMenuIndex == 5)
        {
            SaveMenuEnter();
            uiScreen = UI_SAVE_MENU;
        }
        else if(mainMenuIndex == 6)
        {
            LoadMenuEnter();
            uiScreen = UI_LOAD_MENU;
        }
        else if(mainMenuIndex == 7)
        {
            NewMenuEnter();
            uiScreen = UI_NEW_MENU;
        }
    }
}

void MainMenuDraw()
{
    DrawChars(0, 0, kMainMenuLabels[mainMenuIndex], true);

    char orig[kSeqLength + 2];
    char mod[kSeqLength + 2];
    BuildSequenceString(orig);
    BuildModifiedSequenceString(mod);

    DrawSequenceString(20, orig, -1, true);
    DrawCenteredSeparator(30);
    DrawSequenceString(40, mod, stepNumber, true);
}

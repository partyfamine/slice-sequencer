#include "settings_menu.h"
#include "app.h"
#include "oled_ui.h"
#include "storage.h"

enum SettingsCursor
{
    SETTINGS_CURSOR_SLICE_OUT = 0,
    SETTINGS_CURSOR_BACK,
};

static SettingsCursor cursor;
static bool           editing;
static SliceOutMode   draftSliceOut;

static const char* SliceOutName(SliceOutMode mode)
{
    switch(mode)
    {
        case SLICE_OUT_STEP: return "Step";
        case SLICE_OUT_NOTE:
        default: return "Note";
    }
}

void SettingsMenuEnter()
{
    cursor         = SETTINGS_CURSOR_SLICE_OUT;
    editing        = false;
    draftSliceOut  = sliceOutMode;
}

void SettingsMenuProcessEncoder()
{
    if(editing)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            int m = (int)draftSliceOut + inc;
            m     = (m % SLICE_OUT_LAST + SLICE_OUT_LAST) % SLICE_OUT_LAST;
            draftSliceOut = (SliceOutMode)m;
        }
        if(patch.encoder.RisingEdge())
        {
            sliceOutMode = draftSliceOut;
            SaveGlobalSettings();
            RefreshCvsFromCurrentStep();
            editing = false;
        }
        return;
    }

    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        cursor = (cursor == SETTINGS_CURSOR_SLICE_OUT) ? SETTINGS_CURSOR_BACK
                                                       : SETTINGS_CURSOR_SLICE_OUT;
    }
    else if(inc < 0)
    {
        cursor = (cursor == SETTINGS_CURSOR_SLICE_OUT) ? SETTINGS_CURSOR_BACK
                                                       : SETTINGS_CURSOR_SLICE_OUT;
    }

    if(patch.encoder.RisingEdge())
    {
        if(cursor == SETTINGS_CURSOR_BACK)
        {
            uiScreen = UI_MAIN_MENU;
            return;
        }
        draftSliceOut = sliceOutMode;
        editing       = true;
    }
}

void SettingsMenuDraw()
{
    SliceOutMode mode = editing ? draftSliceOut : sliceOutMode;
    bool selSlice = cursor == SETTINGS_CURSOR_SLICE_OUT && !editing;
    bool selBack  = cursor == SETTINGS_CURSOR_BACK;

    DrawChars(0, 0, "Slice Out", !selSlice);
    DrawChars(9 * kFontWidth, 0, ": ", true);
    DrawChars(11 * kFontWidth,
              0,
              SliceOutName(mode),
              !(editing && cursor == SETTINGS_CURSOR_SLICE_OUT));

    DrawChars(0, 50, "Back", !selBack);
}

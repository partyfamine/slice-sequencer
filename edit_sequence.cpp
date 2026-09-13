#include "edit_sequence.h"
#include "app.h"
#include "oled_ui.h"

enum EditCursor
{
    EDIT_CURSOR_TOTAL_STEPS,
    EDIT_CURSOR_SEQUENCE,
    EDIT_CURSOR_BACK,
};

static int        menuPos;
static bool       inSubMenu;
static bool       editingTotalSteps;
static EditCursor editCursor;

static void MoveMenu(int dir)
{
    if(editCursor == EDIT_CURSOR_TOTAL_STEPS)
    {
        if(dir > 0)
        {
            editCursor = EDIT_CURSOR_SEQUENCE;
            menuPos    = 0;
        }
        else
        {
            editCursor = EDIT_CURSOR_BACK;
        }
        return;
    }
    if(editCursor == EDIT_CURSOR_BACK)
    {
        if(dir > 0)
        {
            editCursor = EDIT_CURSOR_TOTAL_STEPS;
        }
        else
        {
            editCursor = EDIT_CURSOR_SEQUENCE;
            menuPos    = totalSteps - 1;
        }
        return;
    }
    if(dir > 0 && menuPos == totalSteps - 1)
    {
        editCursor = EDIT_CURSOR_BACK;
        return;
    }
    if(dir < 0 && menuPos == 0)
    {
        editCursor = EDIT_CURSOR_TOTAL_STEPS;
        return;
    }
    menuPos += dir;
}

void EditSequenceInit()
{
    menuPos           = 0;
    inSubMenu         = false;
    editingTotalSteps = false;
    editCursor        = EDIT_CURSOR_SEQUENCE;
}

void EditSequenceProcessEncoder()
{
    if(editingTotalSteps)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            ApplyTotalSteps(totalSteps + inc);
        }
        editingTotalSteps = patch.encoder.RisingEdge() ? false : true;
        return;
    }

    if(!inSubMenu)
    {
        int inc = patch.encoder.Increment();
        if(inc > 0)
        {
            for(int i = 0; i < inc; i++)
            {
                MoveMenu(1);
            }
        }
        else if(inc < 0)
        {
            for(int i = 0; i < -inc; i++)
            {
                MoveMenu(-1);
            }
        }

        if(patch.encoder.RisingEdge())
        {
            if(editCursor == EDIT_CURSOR_TOTAL_STEPS)
            {
                SnapshotSequence();
                editingTotalSteps = true;
            }
            else if(editCursor == EDIT_CURSOR_BACK)
            {
                uiScreen = UI_MAIN_MENU;
            }
            else
            {
                inSubMenu = true;
                menuPos   = NoteStartBeat(BeatToNote(menuPos));
            }
        }
        return;
    }

    int note = BeatToNote(menuPos);
    int inc  = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            IncreaseNoteLength(note);
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            DecreaseNoteLength(note);
        }
    }

    inSubMenu = patch.encoder.RisingEdge() ? false : true;
}

void EditSequenceDraw()
{
    const char* label        = "Total Steps";
    bool        invertLabel  = editCursor == EDIT_CURSOR_TOTAL_STEPS
                       && !editingTotalSteps;
    DrawChars(0, 0, label, !invertLabel);
    DrawChars(11 * kFontWidth, 0, ": ", true);

    char num[3];
    if(totalSteps >= 10)
    {
        num[0] = '0' + (totalSteps / 10);
        num[1] = '0' + (totalSteps % 10);
        num[2] = '\0';
    }
    else
    {
        num[0] = '0' + totalSteps;
        num[1] = '\0';
    }
    DrawChars(13 * kFontWidth, 0, num, !editingTotalSteps);

    int invertBeat = -1;
    if(editCursor == EDIT_CURSOR_SEQUENCE)
    {
        invertBeat = menuPos;
    }
    DrawSequence(20, invertBeat);

    bool invertBack = editCursor == EDIT_CURSOR_BACK;
    DrawChars(0, 40, "Back", !invertBack);
}

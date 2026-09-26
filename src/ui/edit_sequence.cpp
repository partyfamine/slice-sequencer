#include "edit_sequence.h"
#include "app.h"
#include "oled_ui.h"

enum EditCursor
{
    EDIT_CURSOR_TOTAL_STEPS,
    EDIT_CURSOR_ASSIGN,
    EDIT_CURSOR_SEQUENCE,
    EDIT_CURSOR_BACK,
};

static int        menuPos;
static bool       inSubMenu;
static bool       editingTotalSteps;
static bool       editingAssign;
static bool       editingHit;
static uint8_t    editHitValue;
static AssignMode assignMode;
static EditCursor editCursor;

static int NextNoteStart(int beat, int dir)
{
    if(numNotes <= 0)
    {
        return 0;
    }
    int note = BeatToNote(beat);
    if(dir > 0)
    {
        note++;
        if(note >= numNotes)
        {
            return -1; // past end
        }
    }
    else
    {
        if(beat > NoteStartBeat(note))
        {
            // Move to start of current note first.
            return NoteStartBeat(note);
        }
        note--;
        if(note < 0)
        {
            return -2; // before start
        }
    }
    return NoteStartBeat(note);
}

static void MoveMenu(int dir)
{
    if(editCursor == EDIT_CURSOR_TOTAL_STEPS)
    {
        editCursor = (dir > 0) ? EDIT_CURSOR_ASSIGN : EDIT_CURSOR_BACK;
        return;
    }
    if(editCursor == EDIT_CURSOR_ASSIGN)
    {
        if(dir > 0)
        {
            editCursor = EDIT_CURSOR_SEQUENCE;
            menuPos    = 0;
        }
        else
        {
            editCursor = EDIT_CURSOR_TOTAL_STEPS;
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
            if(assignMode == ASSIGN_HITS)
            {
                menuPos = NoteStartBeat(numNotes - 1);
            }
            else
            {
                menuPos = totalSteps - 1;
            }
        }
        return;
    }

    // SEQUENCE
    if(assignMode == ASSIGN_HITS)
    {
        int next = NextNoteStart(menuPos, dir);
        if(dir > 0)
        {
            if(next < 0)
            {
                editCursor = EDIT_CURSOR_BACK;
            }
            else
            {
                menuPos = next;
            }
        }
        else
        {
            if(next == -2)
            {
                editCursor = EDIT_CURSOR_ASSIGN;
            }
            else
            {
                menuPos = next;
            }
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
        editCursor = EDIT_CURSOR_ASSIGN;
        return;
    }
    menuPos += dir;
}

static char HitToChar(uint8_t hit)
{
    if(hit == HIT_KICK)
    {
        return 'k';
    }
    if(hit == HIT_SNARE)
    {
        return 's';
    }
    return ' ';
}

static uint8_t NextHit(uint8_t hit, int delta)
{
    int v = (int)hit + delta;
    v     = (v % 3 + 3) % 3;
    return (uint8_t)v;
}

void EditSequenceInit()
{
    menuPos           = 0;
    inSubMenu         = false;
    editingTotalSteps = false;
    editingAssign     = false;
    editingHit        = false;
    editHitValue      = HIT_NONE;
    assignMode        = ASSIGN_LENGTH;
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

    if(editingAssign)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            assignMode
                = (assignMode == ASSIGN_LENGTH) ? ASSIGN_HITS : ASSIGN_LENGTH;
        }
        if(patch.encoder.RisingEdge())
        {
            editingAssign = false;
            if(assignMode == ASSIGN_HITS)
            {
                menuPos = NoteStartBeat(BeatToNote(menuPos));
            }
        }
        return;
    }

    if(editingHit)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            editHitValue = NextHit(editHitValue, inc);
        }
        if(patch.encoder.RisingEdge())
        {
            int note         = BeatToNote(menuPos);
            noteHit[note]    = editHitValue;
            editingHit       = false;
            inSubMenu        = false;
        }
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
            else if(editCursor == EDIT_CURSOR_ASSIGN)
            {
                editingAssign = true;
            }
            else if(editCursor == EDIT_CURSOR_BACK)
            {
                uiScreen = UI_MAIN_MENU;
            }
            else
            {
                menuPos = NoteStartBeat(BeatToNote(menuPos));
                if(assignMode == ASSIGN_HITS)
                {
                    editingHit   = true;
                    editHitValue = noteHit[BeatToNote(menuPos)];
                }
                else
                {
                    inSubMenu = true;
                }
            }
        }
        return;
    }

    // Length editing
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
    bool invertTotal = editCursor == EDIT_CURSOR_TOTAL_STEPS && !editingTotalSteps;
    DrawChars(0, 0, "Total Steps", !invertTotal);
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

    bool invertAssign
        = editCursor == EDIT_CURSOR_ASSIGN && !editingAssign;
    DrawChars(0, 10, "Assign", !invertAssign);
    DrawChars(6 * kFontWidth, 10, ": ", true);
    const char* modeStr
        = (assignMode == ASSIGN_HITS) ? "Hits" : "Length";
    DrawChars(8 * kFontWidth, 10, modeStr, !editingAssign);

    int invertBeat = -1;
    if(editCursor == EDIT_CURSOR_SEQUENCE || editingHit)
    {
        invertBeat = menuPos;
    }
    DrawSequence(30, invertBeat);

    // Hit markers under note starts only.
    for(int n = 0; n < numNotes; n++)
    {
        int  beat = NoteStartBeat(n);
        char c    = HitToChar(noteHit[n]);
        if(editingHit && BeatToNote(menuPos) == n)
        {
            c = HitToChar(editHitValue);
        }
        if(c == ' ' && !(editingHit && BeatToNote(menuPos) == n))
        {
            continue;
        }
        char cstr[2]  = {c, '\0'};
        bool selected = editingHit && BeatToNote(menuPos) == n;
        DrawChars((beat + 1) * kFontWidth, 40, cstr, !selected);
    }

    bool invertBack = editCursor == EDIT_CURSOR_BACK;
    DrawChars(0, 50, "Back", !invertBack);
}

#include "patterns_menu.h"
#include "patterns.h"
#include "app.h"
#include "oled_ui.h"

enum PatternsCursor
{
    PAT_CURSOR_ADD = 0,
    PAT_CURSOR_DELETE,
    PAT_CURSOR_ORDER,
    PAT_CURSOR_BACK,
};

static PatternsCursor patCursor;

static const int kVisiblePatterns = 4;
static int       orderCursor; // 0 = Back, 1.. = slots
static int       orderWindow;
static bool      orderMoving;

void PatternsMenuEnter()
{
    patCursor = PAT_CURSOR_ADD;
    StoreWorkingCvToSlot(selectedPattern);
    RebuildModifiedSequenceForSlot(selectedPattern);
}

void PatternsMenuProcessEncoder()
{
    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            int c = (int)patCursor + 1;
            if(c > PAT_CURSOR_BACK)
            {
                c = PAT_CURSOR_ADD;
            }
            patCursor = (PatternsCursor)c;
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            int c = (int)patCursor - 1;
            if(c < PAT_CURSOR_ADD)
            {
                c = PAT_CURSOR_BACK;
            }
            patCursor = (PatternsCursor)c;
        }
    }

    if(patch.encoder.RisingEdge())
    {
        if(patCursor == PAT_CURSOR_BACK)
        {
            uiScreen = UI_MAIN_MENU;
            return;
        }
        if(patCursor == PAT_CURSOR_ADD)
        {
            AddPatternFromCurrent();
            return;
        }
        if(patCursor == PAT_CURSOR_DELETE)
        {
            DeleteSelectedPattern();
            return;
        }
        if(patCursor == PAT_CURSOR_ORDER)
        {
            PatternOrderEnter();
            uiScreen = UI_PATTERN_ORDER_MENU;
        }
    }
}

void PatternsMenuDraw()
{
    bool selAdd    = patCursor == PAT_CURSOR_ADD;
    bool selDelete = patCursor == PAT_CURSOR_DELETE;
    bool selOrder  = patCursor == PAT_CURSOR_ORDER;
    bool selBack   = patCursor == PAT_CURSOR_BACK;

    DrawChars(0, 0, "Add Pattern", !selAdd);
    DrawChars(0, 10, "Delete Pattern", !selDelete);
    DrawChars(0, 20, "Order Patterns", !selOrder);

    if(selAdd)
    {
        char mod[kSeqLength + 2];
        BuildModifiedSequenceString(mod);
        DrawSequenceString(40, mod, -1, true);
    }
    else if(selDelete)
    {
        char base[kSeqLength + 2];
        BuildBaselineSequenceString(base);
        DrawSequenceString(40, base, -1, true);
    }

    DrawChars(0, 50, "Back", !selBack);
}

void PatternOrderEnter()
{
    orderCursor = 0;
    orderWindow = 0;
    orderMoving = false;
}

void PatternOrderProcessEncoder()
{
    int count     = PatternSlotCount();
    int maxCursor = count; // 0=Back, 1..count = slots

    if(orderMoving)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0 && orderCursor >= 2)
        {
            int userFrom = orderCursor - 2; // slot 2 = userPatterns[0]
            int userTo   = userFrom + inc;
            if(userTo < 0)
            {
                userTo = 0;
            }
            if(userTo >= numUserPatterns)
            {
                userTo = numUserPatterns - 1;
            }
            if(userTo != userFrom)
            {
                MoveUserPattern(userFrom, userTo);
                orderCursor = userTo + 2;
                if(orderCursor - 1 >= orderWindow + kVisiblePatterns)
                {
                    orderWindow = orderCursor - kVisiblePatterns;
                }
                if(orderCursor - 1 < orderWindow)
                {
                    orderWindow = orderCursor - 1;
                }
                if(orderWindow < 0)
                {
                    orderWindow = 0;
                }
            }
        }

        if(patch.encoder.RisingEdge())
        {
            orderMoving = false;
        }
        return;
    }

    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            if(orderCursor >= maxCursor)
            {
                break;
            }
            orderCursor++;
            if(orderCursor > 0)
            {
                int idx = orderCursor - 1;
                if(idx >= orderWindow + kVisiblePatterns)
                {
                    orderWindow = idx - kVisiblePatterns + 1;
                }
            }
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            if(orderCursor <= 0)
            {
                break;
            }
            orderCursor--;
            if(orderCursor > 0)
            {
                int idx = orderCursor - 1;
                if(idx < orderWindow)
                {
                    orderWindow = idx;
                }
            }
            else
            {
                orderWindow = 0;
            }
        }
    }

    if(patch.encoder.RisingEdge())
    {
        if(orderCursor == 0)
        {
            uiScreen = UI_PATTERNS_MENU;
            return;
        }
        // Slot 1 is original — cannot move.
        if(orderCursor == 1)
        {
            return;
        }
        if(orderCursor >= 2 && orderCursor <= count)
        {
            orderMoving = true;
        }
    }
}

void PatternOrderDraw()
{
    int count = PatternSlotCount();
    DrawChars(0, 0, "Back", !(orderCursor == 0 && !orderMoving));

    if(orderWindow < 0)
    {
        orderWindow = 0;
    }
    if(count > kVisiblePatterns && orderWindow > count - kVisiblePatterns)
    {
        orderWindow = count - kVisiblePatterns;
    }
    if(count <= kVisiblePatterns)
    {
        orderWindow = 0;
    }

    for(int row = 0; row < kVisiblePatterns; row++)
    {
        int slot = orderWindow + row;
        if(slot >= count)
        {
            break;
        }
        bool selected = (orderCursor == slot + 1);
        bool invert   = selected;
        if(orderMoving && selected)
        {
            invert = true; // keep highlight while moving
        }

        char seq[kSeqLength + 2];
        BuildSlotBaselineString(slot, seq);
        // Skip leading space from BuildSequenceStringFrom
        const char* text = seq[0] == ' ' ? seq + 1 : seq;
        DrawChars(0, 20 + row * 10, text, !invert);
    }
}

#include "load_menu.h"
#include "app.h"
#include "oled_ui.h"
#include "storage.h"

static const int kVisibleSeqs = 4;

static int  cursor;      // 0 = Back, 1..count = sequences
static int  windowStart; // index into saved list for first visible row

void LoadMenuEnter()
{
    RefreshSavedSequenceList();
    cursor      = 0;
    windowStart = 0;
}

void LoadMenuProcessEncoder()
{
    int count = GetSavedSequenceCount();
    int maxCursor = count; // 0=Back, 1..count = items

    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            if(cursor >= maxCursor)
            {
                break;
            }
            cursor++;
            if(cursor > 0)
            {
                int seqIndex = cursor - 1;
                if(seqIndex >= windowStart + kVisibleSeqs)
                {
                    windowStart = seqIndex - kVisibleSeqs + 1;
                }
            }
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            if(cursor <= 0)
            {
                break;
            }
            cursor--;
            if(cursor > 0)
            {
                int seqIndex = cursor - 1;
                if(seqIndex < windowStart)
                {
                    windowStart = seqIndex;
                }
            }
            else
            {
                windowStart = 0;
            }
        }
    }

    if(patch.encoder.RisingEdge())
    {
        if(cursor == 0)
        {
            uiScreen = UI_MAIN_MENU;
            return;
        }
        int seqIndex = cursor - 1;
        if(seqIndex >= 0 && seqIndex < count)
        {
            LoadSequence(GetSavedSequenceName(seqIndex));
        }
        uiScreen = UI_MAIN_MENU;
    }
}

void LoadMenuDraw()
{
    int count = GetSavedSequenceCount();
    DrawChars(0, 0, "Back", !(cursor == 0));

    if(windowStart < 0)
    {
        windowStart = 0;
    }
    if(count > kVisibleSeqs && windowStart > count - kVisibleSeqs)
    {
        windowStart = count - kVisibleSeqs;
    }
    if(count <= kVisibleSeqs)
    {
        windowStart = 0;
    }

    for(int row = 0; row < kVisibleSeqs; row++)
    {
        int seqIndex = windowStart + row;
        if(seqIndex >= count)
        {
            break;
        }
        bool selected = (cursor == seqIndex + 1);
        DrawChars(0, 20 + row * 10, GetSavedSequenceName(seqIndex), !selected);
    }
}

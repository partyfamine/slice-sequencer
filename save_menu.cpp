#include "save_menu.h"
#include "app.h"
#include "oled_ui.h"
#include "storage.h"
#include <cstring>

enum SaveCursor
{
    SAVE_CURSOR_NAME = 0,
    SAVE_CURSOR_SAVE,
    SAVE_CURSOR_BACK,
};

static char       saveName[kMaxSeqNameLen + 1];
static int        saveNameLen;
static int        nameCursor;
static SaveCursor saveCursor;
static bool       editingChar;

// space, a-z, 0-9, _
static const char* kChars     = " abcdefghijklmnopqrstuvwxyz0123456789_";
static const int   kCharCount = 38;

static int CharToIndex(char c)
{
    for(int i = 0; i < kCharCount; i++)
    {
        if(kChars[i] == c)
        {
            return i;
        }
    }
    return 1; // 'a'
}

static int WrapIndex(int idx, int delta, int minIdx)
{
    int count = kCharCount - minIdx;
    int rel   = idx - minIdx + delta;
    rel       = (rel % count + count) % count;
    return rel + minIdx;
}

static bool IsAppendSlot()
{
    return nameCursor == saveNameLen && saveNameLen < kMaxSeqNameLen;
}

static bool AllowSpaceOption()
{
    // Empty space only for the last character when name length > 1.
    return !IsAppendSlot() && saveNameLen > 1 && nameCursor == saveNameLen - 1;
}

static int MaxNameCursor()
{
    if(saveNameLen < kMaxSeqNameLen)
    {
        return saveNameLen;
    }
    return saveNameLen > 0 ? saveNameLen - 1 : 0;
}

static void MoveSaveMenu(int dir)
{
    if(saveCursor == SAVE_CURSOR_NAME)
    {
        int next = nameCursor + dir;
        if(dir > 0)
        {
            if(next > MaxNameCursor())
            {
                saveCursor = SAVE_CURSOR_SAVE;
            }
            else
            {
                nameCursor = next;
            }
        }
        else if(next < 0)
        {
            saveCursor = SAVE_CURSOR_BACK;
        }
        else
        {
            nameCursor = next;
        }
        return;
    }

    if(saveCursor == SAVE_CURSOR_SAVE)
    {
        saveCursor = (dir > 0) ? SAVE_CURSOR_BACK : SAVE_CURSOR_NAME;
        if(saveCursor == SAVE_CURSOR_NAME)
        {
            nameCursor = MaxNameCursor();
        }
        return;
    }

    // BACK
    if(dir > 0)
    {
        saveCursor = SAVE_CURSOR_NAME;
        nameCursor = 0;
    }
    else
    {
        saveCursor = SAVE_CURSOR_SAVE;
    }
}

void SaveMenuEnter()
{
    if(HasSequenceName())
    {
        strncpy(saveName, sequenceName, kMaxSeqNameLen);
        saveName[kMaxSeqNameLen] = '\0';
        saveNameLen             = (int)strlen(saveName);
    }
    else
    {
        saveName[0]             = 'a';
        saveName[1]             = '\0';
        saveNameLen             = 1;
    }
    nameCursor  = 0;
    saveCursor  = SAVE_CURSOR_NAME;
    editingChar = false;
}

void SaveMenuProcessEncoder()
{
    if(editingChar)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            int minIdx = AllowSpaceOption() ? 0 : 1;
            int idx    = CharToIndex(saveName[nameCursor]);
            if(idx < minIdx)
            {
                idx = minIdx;
            }
            idx                      = WrapIndex(idx, inc, minIdx);
            char c                   = kChars[idx];
            saveName[nameCursor]     = c;
            saveName[nameCursor + 1] = '\0';

            if(c == ' ')
            {
                // Deleting the last character.
                saveName[nameCursor] = '\0';
                saveNameLen          = nameCursor;
                if(nameCursor > 0)
                {
                    nameCursor--;
                }
                editingChar = false;
            }
        }

        if(patch.encoder.RisingEdge())
        {
            if(saveName[nameCursor] != ' ' && saveName[nameCursor] != '\0')
            {
                if(nameCursor == saveNameLen)
                {
                    saveNameLen++;
                }
                saveName[saveNameLen] = '\0';
            }
            editingChar = false;
        }
        return;
    }

    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            MoveSaveMenu(1);
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            MoveSaveMenu(-1);
        }
    }

    if(patch.encoder.RisingEdge())
    {
        if(saveCursor == SAVE_CURSOR_BACK)
        {
            uiScreen = UI_MAIN_MENU;
            return;
        }
        if(saveCursor == SAVE_CURSOR_SAVE)
        {
            if(saveNameLen > 0)
            {
                saveName[saveNameLen] = '\0';
                SaveSequence(saveName);
            }
            uiScreen = UI_MAIN_MENU;
            return;
        }

        if(IsAppendSlot())
        {
            saveName[saveNameLen]     = 'a';
            saveName[saveNameLen + 1] = '\0';
        }
        editingChar = true;
    }
}

void SaveMenuDraw()
{
    int visibleLen = saveNameLen;
    if(editingChar && nameCursor == saveNameLen)
    {
        visibleLen = saveNameLen + 1;
    }

    for(int i = 0; i < visibleLen; i++)
    {
        char cstr[2] = {saveName[i], '\0'};
        bool selected
            = (saveCursor == SAVE_CURSOR_NAME && nameCursor == i);
        DrawChars(i * kFontWidth, 0, cstr, !selected);
    }

    if(!editingChar && saveNameLen < kMaxSeqNameLen)
    {
        bool selected
            = (saveCursor == SAVE_CURSOR_NAME && nameCursor == saveNameLen);
        if(selected)
        {
            DrawChars(saveNameLen * kFontWidth, 0, " ", false);
        }
    }

    DrawChars(0, 20, "Save", !(saveCursor == SAVE_CURSOR_SAVE));
    DrawChars(0, 30, "Back", !(saveCursor == SAVE_CURSOR_BACK));
}

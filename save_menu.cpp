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
static char       editChar; // character shown while editing (never stored as space)

// a-z, 0-9, _  (space is a delete action, not a storable character)
static const char* kChars     = "abcdefghijklmnopqrstuvwxyz0123456789_";
static const int   kCharCount = 37;

static int CharToIndex(char c)
{
    for(int i = 0; i < kCharCount; i++)
    {
        if(kChars[i] == c)
        {
            return i;
        }
    }
    return 0; // 'a'
}

static int WrapIndex(int idx, int delta, int count)
{
    int rel = idx + delta;
    rel     = (rel % count + count) % count;
    return rel;
}

static void TruncateName(int newLen)
{
    if(newLen < 0)
    {
        newLen = 0;
    }
    if(newLen > kMaxSeqNameLen)
    {
        newLen = kMaxSeqNameLen;
    }
    saveNameLen = newLen;
    saveName[saveNameLen] = '\0';
    // Clear any leftover bytes so append never resumes after stale characters.
    for(int i = saveNameLen + 1; i <= kMaxSeqNameLen; i++)
    {
        saveName[i] = '\0';
    }
}

static bool IsAppendSlot()
{
    return nameCursor == saveNameLen && saveNameLen < kMaxSeqNameLen;
}

static bool CanDeleteLastChar()
{
    return !IsAppendSlot() && saveNameLen > 1 && nameCursor == saveNameLen - 1;
}

static int MaxNameCursor()
{
    if(saveNameLen < kMaxSeqNameLen)
    {
        return saveNameLen; // one trailing append placeholder
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
        TruncateName((int)strlen(saveName));
    }
    else
    {
        saveName[0] = 'a';
        TruncateName(1);
    }
    nameCursor  = 0;
    saveCursor  = SAVE_CURSOR_NAME;
    editingChar = false;
    editChar    = saveName[0];
}

void SaveMenuProcessEncoder()
{
    if(editingChar)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            if(CanDeleteLastChar())
            {
                // Include a synthetic "space" option before 'a' for delete.
                // Indices: 0 = delete/space, 1..kCharCount = kChars[0..]
                int idx = CharToIndex(editChar) + 1;
                idx     = WrapIndex(idx, inc, kCharCount + 1);
                if(idx == 0)
                {
                    TruncateName(saveNameLen - 1);
                    nameCursor  = saveNameLen > 0 ? saveNameLen - 1 : 0;
                    editingChar = false;
                    return;
                }
                editChar = kChars[idx - 1];
            }
            else
            {
                int idx  = CharToIndex(editChar);
                idx      = WrapIndex(idx, inc, kCharCount);
                editChar = kChars[idx];
            }
        }

        if(patch.encoder.RisingEdge())
        {
            if(IsAppendSlot())
            {
                saveName[saveNameLen] = editChar;
                TruncateName(saveNameLen + 1);
            }
            else
            {
                saveName[nameCursor] = editChar;
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
                TruncateName(saveNameLen);
                SaveSequence(saveName);
            }
            uiScreen = UI_MAIN_MENU;
            return;
        }

        if(IsAppendSlot())
        {
            editChar = 'a';
        }
        else
        {
            editChar = saveName[nameCursor];
        }
        editingChar = true;
    }
}

void SaveMenuDraw()
{
    // Draw the real name characters (never includes spaces).
    for(int i = 0; i < saveNameLen; i++)
    {
        char c = saveName[i];
        if(editingChar && nameCursor == i)
        {
            c = editChar;
        }
        char cstr[2]  = {c, '\0'};
        bool selected = (saveCursor == SAVE_CURSOR_NAME && nameCursor == i);
        DrawChars(i * kFontWidth, 0, cstr, !selected);
    }

    // Single trailing append placeholder, or the char being edited there.
    if(saveNameLen < kMaxSeqNameLen)
    {
        bool onAppend
            = (saveCursor == SAVE_CURSOR_NAME && nameCursor == saveNameLen);
        if(editingChar && onAppend)
        {
            char cstr[2] = {editChar, '\0'};
            DrawChars(saveNameLen * kFontWidth, 0, cstr, false);
        }
        else if(onAppend)
        {
            DrawChars(saveNameLen * kFontWidth, 0, " ", false);
        }
    }

    DrawChars(0, 20, "Save", !(saveCursor == SAVE_CURSOR_SAVE));
    DrawChars(0, 30, "Back", !(saveCursor == SAVE_CURSOR_BACK));
}

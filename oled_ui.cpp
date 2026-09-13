#include "oled_ui.h"
#include "app.h"

void DrawChars(int x, int y, const char* str, bool on)
{
    char cstr[2];
    cstr[1] = '\0';
    for(int i = 0; str[i] != '\0'; i++)
    {
        cstr[0] = str[i];
        patch.display.SetCursor(x, y);
        patch.display.WriteString(cstr, Font_7x10, on);
        x += kFontWidth;
    }
}

int SequenceDrawX(bool center)
{
    if(!center)
    {
        return 0;
    }
    return ((kSeqLength - totalSteps) / 2) * kFontWidth;
}

void DrawSequenceString(int y, const char* seq, int invertBeat, bool center)
{
    int  xBase = SequenceDrawX(center);
    char cstr[2];
    cstr[1] = '\0';
    for(int b = 0; b < totalSteps + 1; b++)
    {
        cstr[0] = seq[b];
        patch.display.SetCursor(xBase + b * kFontWidth, y);
        bool on = (b == 0) ? true : (invertBeat != b - 1);
        patch.display.WriteString(cstr, Font_7x10, on);
    }
}

void DrawSequenceStringMasked(int y,
                              const char* seq,
                              int         invStartBeat,
                              int         invEndBeat,
                              bool        center)
{
    int  xBase = SequenceDrawX(center);
    char cstr[2];
    cstr[1] = '\0';
    for(int b = 0; b < totalSteps + 1; b++)
    {
        cstr[0] = seq[b];
        patch.display.SetCursor(xBase + b * kFontWidth, y);
        bool invert
            = (b > 0) && (b - 1 >= invStartBeat) && (b - 1 < invEndBeat);
        patch.display.WriteString(cstr, Font_7x10, !invert);
    }
}

void DrawSequence(int y, int invertBeat)
{
    char seq[kSeqLength + 2];
    BuildSequenceString(seq);
    DrawSequenceString(y, seq, invertBeat, false);
}

void DrawCenteredSeparator(int y)
{
    DrawChars(0, y, " ----------------", true);
}

#include "oled_ui.h"
#include "app.h"
#include "storage.h"
#include <cstring>

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
    char line[kSeqLength + 2];
    line[0] = ' ';

    if(!HasSequenceName())
    {
        for(int i = 0; i < kSeqLength; i++)
        {
            line[i + 1] = '-';
        }
        line[kSeqLength + 1] = '\0';
        DrawChars(0, y, line, true);
        return;
    }

    // 16-char centered name: spaces around name, then dashes to fill.
    char   body[kSeqLength + 1];
    int    nameLen = (int)strlen(sequenceName);
    if(nameLen > kSeqLength - 2)
    {
        nameLen = kSeqLength - 2;
    }

    // content with one space on each side of the name
    int contentLen = nameLen + 2;
    int remaining  = kSeqLength - contentLen;
    if(remaining < 0)
    {
        remaining = 0;
    }
    int leftDashes  = remaining / 2;
    int rightDashes = remaining - leftDashes;

    int pos = 0;
    for(int i = 0; i < leftDashes; i++)
    {
        body[pos++] = '-';
    }
    body[pos++] = ' ';
    for(int i = 0; i < nameLen; i++)
    {
        body[pos++] = sequenceName[i];
    }
    body[pos++] = ' ';
    for(int i = 0; i < rightDashes; i++)
    {
        body[pos++] = '-';
    }
    body[pos] = '\0';

    line[0] = ' ';
    for(int i = 0; i < kSeqLength; i++)
    {
        line[i + 1] = body[i];
    }
    line[kSeqLength + 1] = '\0';
    DrawChars(0, y, line, true);
}

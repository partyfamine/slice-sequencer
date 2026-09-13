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

void DrawSequence(int y, int invertBeat)
{
    char seq[kSeqLength + 2];
    BuildSequenceString(seq);

    char cstr[2];
    cstr[1] = '\0';
    for(int b = 0; b < totalSteps + 1; b++)
    {
        cstr[0] = seq[b];
        patch.display.SetCursor(b * kFontWidth, y);
        bool on = (b == 0) ? true : (invertBeat != b - 1);
        patch.display.WriteString(cstr, Font_7x10, on);
    }
}

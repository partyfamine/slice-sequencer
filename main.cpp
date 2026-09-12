#include "daisysp.h"
#include "daisy_patch.h"
#include <cmath>

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;

static const int  kSeqLength = 16;
static const int  kFontWidth = 7;
static const char kHex[]     = "0123456789ABCDEF";

int      noteLength[kSeqLength];
int      numNotes;
int      stepNumber;
bool     trigOut;
bool     pendingReset;
uint16_t cvValue;
uint16_t lengthCv;

int  menuPos;
bool inSubMenu;

void UpdateControls();
void UpdateOled();
void UpdateOutputs();

int NoteStartBeat(int note)
{
    int pos = 0;
    for(int n = 0; n < note; n++)
    {
        pos += noteLength[n];
    }
    return pos;
}

int BeatToNote(int beat)
{
    int pos = 0;
    for(int n = 0; n < numNotes; n++)
    {
        pos += noteLength[n];
        if(beat < pos)
        {
            return n;
        }
    }
    return numNotes - 1;
}

bool IsNoteStart(int beat)
{
    return beat == NoteStartBeat(BeatToNote(beat));
}

uint16_t CvForNote(int note)
{
    return (uint16_t)round(note * (4096.0 / (double)numNotes));
}

uint16_t CvForLength(int length)
{
    return (uint16_t)round((length - 1) * (4096.0 / 15.0));
}

void IncreaseNoteLength(int note)
{
    if(note >= numNotes - 1)
    {
        return;
    }

    int last = numNotes - 1;
    if(noteLength[last] > 1)
    {
        noteLength[last]--;
    }
    else
    {
        numNotes--;
    }
    noteLength[note]++;
}

void DecreaseNoteLength(int note)
{
    if(noteLength[note] <= 1)
    {
        return;
    }

    noteLength[note]--;
    noteLength[numNotes] = 1;
    numNotes++;
}

void TriggerNoteAtStep()
{
    trigOut  = true;
    int note = BeatToNote(stepNumber);
    cvValue  = CvForNote(note);
    lengthCv = CvForLength(noteLength[note]);
}

void ResetToFirstStep()
{
    stepNumber   = 0;
    pendingReset = false;
    TriggerNoteAtStep();
}

int main(void)
{
    patch.Init(); // Initialize hardware (daisy seed, and patch)

    stepNumber   = 0;
    trigOut      = false;
    pendingReset = false;
    cvValue    = 0;
    lengthCv   = 0;
    menuPos    = 0;
    inSubMenu  = false;
    numNotes   = kSeqLength;

    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = 1;
    }

    patch.StartAdc();
    while(1)
    {
        UpdateControls();
        UpdateOled();
        UpdateOutputs();
    }
}

void UpdateControls()
{
    patch.ProcessAnalogControls();
    patch.ProcessDigitalControls();

    if(!inSubMenu)
    {
        menuPos += patch.encoder.Increment();
        menuPos = (menuPos % kSeqLength + kSeqLength) % kSeqLength;

        if(patch.encoder.RisingEdge())
        {
            inSubMenu = true;
            menuPos   = NoteStartBeat(BeatToNote(menuPos));
        }
    }
    else
    {
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

    bool clock = patch.gate_input[0].Trig();
    bool reset = patch.gate_input[1].Trig();

    if(reset && clock)
    {
        ResetToFirstStep();
    }
    else if(reset)
    {
        pendingReset = true;
    }
    else if(clock)
    {
        if(pendingReset)
        {
            ResetToFirstStep();
        }
        else
        {
            stepNumber++;
            stepNumber %= kSeqLength;
            if(IsNoteStart(stepNumber))
            {
                TriggerNoteAtStep();
            }
        }
    }
}

void UpdateOled()
{
    patch.display.Fill(false);

    char seq[kSeqLength + 2];
    seq[0] = ' ';
    int  i = 1;
    for(int n = 0; n < numNotes; n++)
    {
        seq[i++] = kHex[n];
        for(int r = 1; r < noteLength[n]; r++)
        {
            seq[i++] = '-';
        }
    }
    seq[i] = '\0';

    char cstr[2];
    cstr[1] = '\0';
    for(int b = 0; b < kSeqLength + 1; b++)
    {
        cstr[0] = seq[b];
        patch.display.SetCursor(b * kFontWidth, 10);
        bool on = (b == 0) ? true : (menuPos != b - 1);
        patch.display.WriteString(cstr, Font_7x10, on);
    }

    cstr[0] = '!';
    patch.display.SetCursor((stepNumber + 1) * kFontWidth, 45);
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.Update();
}

void UpdateOutputs()
{
    patch.seed.dac.WriteValue(DacHandle::Channel::ONE, cvValue);
    patch.seed.dac.WriteValue(DacHandle::Channel::TWO, lengthCv);

    patch.gate_output.Write(trigOut);
    trigOut = false;
}

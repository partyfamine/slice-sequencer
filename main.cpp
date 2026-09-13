#include "daisysp.h"
#include "daisy_patch.h"
#include "app.h"
#include "main_menu.h"
#include "edit_sequence.h"
#include "cv_menu.h"

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;
UiScreen   uiScreen;
int        activeCvIndex;

bool     trigOut;
bool     pendingReset;
uint16_t cvValue;
uint16_t lengthCv;

void UpdateControls();
void UpdateOled();
void UpdateOutputs();

int main(void)
{
    patch.Init();

    uiScreen      = UI_MAIN_MENU;
    activeCvIndex = 0;
    trigOut       = false;
    pendingReset  = false;
    cvValue       = 0;
    lengthCv      = 0;

    InitSequence();
    MainMenuInit();
    EditSequenceInit();
    CvMenuInit();

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

    if(uiScreen == UI_EDIT_SEQUENCE)
    {
        EditSequenceProcessEncoder();
    }
    else if(uiScreen == UI_CV_MENU)
    {
        CvMenuProcessEncoder();
    }
    else
    {
        MainMenuProcessEncoder();
    }

    // Keep the modified view / playback in sync with live CV amounts.
    RebuildModifiedSequence();
    if(modNumNotes > 0 && totalSteps > 0)
    {
        int modNote = ModBeatToNote(stepNumber);
        int origId  = modNoteOrder[modNote];
        cvValue     = CvForNote(origId);
        lengthCv    = CvForLength(modNoteLength[modNote]);
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
            stepNumber %= totalSteps;
            if(ModIsNoteStart(stepNumber))
            {
                TriggerNoteAtStep();
            }
        }
    }
}

void UpdateOled()
{
    patch.display.Fill(false);

    if(uiScreen == UI_EDIT_SEQUENCE)
    {
        EditSequenceDraw();
    }
    else if(uiScreen == UI_CV_MENU)
    {
        CvMenuDraw();
    }
    else
    {
        MainMenuDraw();
    }

    patch.display.Update();
}

void UpdateOutputs()
{
    patch.seed.dac.WriteValue(DacHandle::Channel::ONE, cvValue);
    patch.seed.dac.WriteValue(DacHandle::Channel::TWO, lengthCv);

    patch.gate_output.Write(trigOut);
    trigOut = false;
}

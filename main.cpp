#include "daisysp.h"
#include "daisy_patch.h"
#include "app.h"
#include "main_menu.h"
#include "edit_sequence.h"
#include "cv_menu.h"
#include "save_menu.h"
#include "load_menu.h"
#include "new_menu.h"
#include "storage.h"

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

    InitStorage();
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
    else if(uiScreen == UI_SAVE_MENU)
    {
        SaveMenuProcessEncoder();
    }
    else if(uiScreen == UI_LOAD_MENU)
    {
        LoadMenuProcessEncoder();
    }
    else if(uiScreen == UI_NEW_MENU)
    {
        NewMenuProcessEncoder();
    }
    else
    {
        MainMenuProcessEncoder();
    }

    // Update the modified sequence for the display immediately.
    // CV/gate outputs only change on the next clocked step below.
    RebuildModifiedSequence();

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
            TriggerNoteAtStep(ModIsNoteStart(stepNumber));
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
    else if(uiScreen == UI_SAVE_MENU)
    {
        SaveMenuDraw();
    }
    else if(uiScreen == UI_LOAD_MENU)
    {
        LoadMenuDraw();
    }
    else if(uiScreen == UI_NEW_MENU)
    {
        NewMenuDraw();
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

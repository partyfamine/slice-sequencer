#include "daisysp.h"
#include "daisy_patch.h"
#include "app.h"
#include "main_menu.h"
#include "edit_sequence.h"

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;
UiScreen   uiScreen;

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

    uiScreen     = UI_MAIN_MENU;
    trigOut      = false;
    pendingReset = false;
    cvValue      = 0;
    lengthCv     = 0;

    InitSequence();
    MainMenuInit();
    EditSequenceInit();

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
    else
    {
        MainMenuProcessEncoder();
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

    if(uiScreen == UI_EDIT_SEQUENCE)
    {
        EditSequenceDraw();
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

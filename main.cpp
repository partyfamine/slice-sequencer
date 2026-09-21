#include "daisysp.h"
#include "daisy_patch.h"
#include "app.h"
#include "main_menu.h"
#include "edit_sequence.h"
#include "cv_menu.h"
#include "save_menu.h"
#include "load_menu.h"
#include "new_menu.h"
#include "patterns_menu.h"
#include "patterns.h"
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

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float kick  = 0.f;
        float snare = 0.f;
        if(kickGateSamples > 0)
        {
            kick = 1.f;
            kickGateSamples--;
        }
        if(snareGateSamples > 0)
        {
            snare = 1.f;
            snareGateSamples--;
        }
        out[0][i] = kick;
        out[1][i] = snare;
        out[2][i] = 0.f;
        out[3][i] = 0.f;
    }
}

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
    InitPatterns();
    LoadLastSequence();
    MainMenuInit();
    EditSequenceInit();
    CvMenuInit();

    patch.StartAdc();
    patch.StartAudio(AudioCallback);
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
    else if(uiScreen == UI_PATTERNS_MENU)
    {
        PatternsMenuProcessEncoder();
    }
    else if(uiScreen == UI_PATTERN_ORDER_MENU)
    {
        PatternOrderProcessEncoder();
    }
    else
    {
        MainMenuProcessEncoder();
    }

    UpdatePatternSelectionFromCv();

    // Display uses the selected pattern; playback may lag until next step.
    StoreWorkingCvToSlot(selectedPattern);
    RebuildModifiedSequenceForSlot(selectedPattern);

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
            bool midNote   = false;
            int  holdId    = 0;
            int  holdSteps = 0;
            if(PatternSwitchPending())
            {
                CapturePatternSwitchHoldState(&midNote, &holdId, &holdSteps);
            }

            stepNumber++;
            stepNumber %= totalSteps;

            if(PatternSwitchPending())
            {
                ApplyPendingPatternSwitch(midNote, holdId, holdSteps);
            }

            RebuildModifiedSequenceForSlot(playPattern);
            TriggerNoteAtStep(ModIsNoteStart(stepNumber));

            // Restore selected pattern for display.
            RebuildModifiedSequenceForSlot(selectedPattern);
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
    else if(uiScreen == UI_PATTERNS_MENU)
    {
        PatternsMenuDraw();
    }
    else if(uiScreen == UI_PATTERN_ORDER_MENU)
    {
        PatternOrderDraw();
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

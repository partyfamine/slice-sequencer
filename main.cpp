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
#include "settings_menu.h"
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

// Invariant: display follows selectedPattern; audio follows playPattern until
// the next clock step (when a pending switch may apply). UpdateDisplayModel
// keeps the OLED/CV-edit view in sync with selectedPattern; AdvanceClock
// advances playback from playPattern.
static void UpdateDisplayModel();
static void AdvanceClock();

struct ScreenHandlers
{
    void (*process)();
    void (*draw)();
};

// Indexed by UiScreen — keep in sync with the enum in ui.h.
static const ScreenHandlers kScreenHandlers[] = {
    {MainMenuProcessEncoder, MainMenuDraw},             // UI_MAIN_MENU
    {EditSequenceProcessEncoder, EditSequenceDraw},     // UI_EDIT_SEQUENCE
    {CvMenuProcessEncoder, CvMenuDraw},                 // UI_CV_MENU
    {SaveMenuProcessEncoder, SaveMenuDraw},             // UI_SAVE_MENU
    {LoadMenuProcessEncoder, LoadMenuDraw},             // UI_LOAD_MENU
    {NewMenuProcessEncoder, NewMenuDraw},               // UI_NEW_MENU
    {PatternsMenuProcessEncoder, PatternsMenuDraw},     // UI_PATTERNS_MENU
    {PatternOrderProcessEncoder, PatternOrderDraw},     // UI_PATTERN_ORDER_MENU
    {SettingsMenuProcessEncoder, SettingsMenuDraw},     // UI_SETTINGS_MENU
};

static void UpdateDisplayModel()
{
    UpdatePatternSelectionFromCv();
    StoreWorkingCvToSlot(selectedPattern);
    RebuildModifiedSequenceForSlot(selectedPattern);
}

static void AdvanceClock()
{
    if(pendingReset)
    {
        ResetToFirstStep();
        return;
    }

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

    kScreenHandlers[uiScreen].process();

    UpdateDisplayModel();

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
        AdvanceClock();
    }
}

void UpdateOled()
{
    patch.display.Fill(false);
    kScreenHandlers[uiScreen].draw();
    patch.display.Update();
}

void UpdateOutputs()
{
    patch.seed.dac.WriteValue(DacHandle::Channel::ONE, cvValue);
    patch.seed.dac.WriteValue(DacHandle::Channel::TWO, lengthCv);

    patch.gate_output.Write(trigOut);
    trigOut = false;
}

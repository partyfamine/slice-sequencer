#include "sequence.h"
#include "patterns.h"
#include <cmath>
#include <stdint.h>

static const int kAudioGateSamples = 480; // ~10ms at 48kHz

// Mid-note hold across a pattern switch — owned here, not by patterns.
static int  holdNoteId;
static int  holdStepsLeft;
static bool holdActive;
static bool holdSilence;

uint16_t CvForNote(int note)
{
    if(sliceOutMode == SLICE_OUT_STEP)
    {
        if(totalSteps <= 0)
        {
            return 0;
        }
        int start = NoteStartBeat(note);
        return (uint16_t)round(start * (4096.0 / (double)totalSteps));
    }

    if(numNotes <= 0)
    {
        return 0;
    }
    return (uint16_t)round(note * (4096.0 / (double)numNotes));
}

uint16_t CvForLength(int length)
{
    return (uint16_t)round((length - 1) * (4096.0 / 15.0));
}

void ClearPlaybackHold()
{
    holdActive    = false;
    holdSilence   = false;
    holdNoteId    = 0;
    holdStepsLeft = 0;
}

void CapturePatternSwitchHoldState(bool* midNote, int* noteId, int* stepsLeft)
{
    *midNote   = false;
    *noteId    = 0;
    *stepsLeft = 0;

    if(!PatternSwitchPending())
    {
        return;
    }

    RebuildModifiedSequenceForSlot(playPattern);
    if(modNumNotes <= 0 || totalSteps <= 0)
    {
        return;
    }

    int note    = ModBeatToNote(stepNumber);
    int noteEnd = ModNoteStartBeat(note) + modNoteLength[note];
    int next    = (stepNumber + 1) % totalSteps;
    if(next != 0 && next < noteEnd)
    {
        *midNote   = true;
        *noteId    = modNoteOrder[note];
        *stepsLeft = noteEnd - next;
    }
}

void ApplyHoldAfterPatternSwitch(bool midNote, int noteId, int stepsLeft)
{
    if(!ModIsNoteStart(stepNumber))
    {
        if(midNote && stepsLeft > 0)
        {
            holdActive    = true;
            holdSilence   = false;
            holdNoteId    = noteId;
            holdStepsLeft = stepsLeft;
        }
        else
        {
            holdActive    = false;
            holdSilence   = true;
            holdNoteId    = 0;
            holdStepsLeft = 0;
        }
    }
    else
    {
        ClearPlaybackHold();
    }
}

void RefreshCvsFromCurrentStep()
{
    if(totalSteps <= 0)
    {
        return;
    }
    if(stepNumber >= totalSteps)
    {
        stepNumber %= totalSteps;
    }
    RebuildModifiedSequence();
    if(modNumNotes <= 0)
    {
        return;
    }
    int modNote = ModBeatToNote(stepNumber);
    int origId  = modNoteOrder[modNote];
    cvValue     = CvForNote(origId);
    lengthCv    = CvForLength(modNoteLength[modNote]);
}

void TriggerHitGates(int noteId)
{
    if(noteId < 0 || noteId >= numNotes)
    {
        return;
    }
    if(noteHit[noteId] == HIT_KICK)
    {
        kickGateSamples = kAudioGateSamples;
    }
    else if(noteHit[noteId] == HIT_SNARE)
    {
        snareGateSamples = kAudioGateSamples;
    }
}

void TriggerNoteAtStep()
{
    TriggerNoteAtStep(true);
}

void TriggerNoteAtStep(bool fireGate)
{
    RebuildModifiedSequence();
    if(modNumNotes <= 0)
    {
        return;
    }

    if(holdActive)
    {
        if(ModIsNoteStart(stepNumber))
        {
            holdActive    = false;
            holdSilence   = false;
            holdStepsLeft = 0;
        }
        else if(holdStepsLeft <= 0)
        {
            holdActive    = false;
            holdSilence   = true;
            holdStepsLeft = 0;
            return;
        }
        else
        {
            cvValue  = CvForNote(holdNoteId);
            lengthCv = CvForLength(holdStepsLeft);
            holdStepsLeft--;
            return;
        }
    }

    if(holdSilence)
    {
        if(!ModIsNoteStart(stepNumber))
        {
            return;
        }
        holdSilence = false;
    }

    int modNote = ModBeatToNote(stepNumber);
    int origId  = modNoteOrder[modNote];
    cvValue     = CvForNote(origId);
    lengthCv    = CvForLength(modNoteLength[modNote]);
    if(fireGate && ModIsNoteStart(stepNumber))
    {
        trigOut = true;
        TriggerHitGates(origId);
    }
}

void ResetToFirstStep()
{
    stepNumber   = 0;
    pendingReset = false;
    ClearPlaybackHold();
    if(PatternSwitchPending())
    {
        ApplyPendingPatternSwitch();
        ApplyHoldAfterPatternSwitch(false, 0, 0);
    }
    RebuildModifiedSequenceForSlot(playPattern);
    TriggerNoteAtStep();
}

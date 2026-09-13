#pragma once
#include "daisy_patch.h"
#include <stdint.h>

extern daisy::DaisyPatch patch;

static const int  kSeqLength = 16;
static const int  kMinSteps  = 2;
static const int  kFontWidth = 7;
static const char kHex[]     = "0123456789ABCDEF";

enum UiScreen
{
    UI_MAIN_MENU,
    UI_EDIT_SEQUENCE,
};

extern UiScreen uiScreen;

extern int      noteLength[kSeqLength];
extern int      numNotes;
extern int      totalSteps;
extern int      stepNumber;
extern bool     trigOut;
extern bool     pendingReset;
extern uint16_t cvValue;
extern uint16_t lengthCv;

void InitSequence();
int  NoteStartBeat(int note);
int  BeatToNote(int beat);
bool IsNoteStart(int beat);
void BuildSequenceString(char* seq);
uint16_t CvForNote(int note);
uint16_t CvForLength(int length);
void RefreshCvsFromCurrentStep();
void SnapshotSequence();
void ApplyTotalSteps(int newTotal);
void IncreaseNoteLength(int note);
void DecreaseNoteLength(int note);
void TriggerNoteAtStep();
void ResetToFirstStep();

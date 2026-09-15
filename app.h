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
    UI_CV_MENU,
    UI_SAVE_MENU,
    UI_LOAD_MENU,
    UI_NEW_MENU,
};

enum CvType
{
    CV_TYPE_SHIFT = 0,
    CV_TYPE_TRANSPOSE,
    CV_TYPE_REPEAT,
    CV_TYPE_LAST,
};

struct CvChannel
{
    CvType type;
    int    position; // note index into the original sequence
    int    size;     // segment length in steps (1-8)
};

extern UiScreen uiScreen;
extern int      activeCvIndex;

extern int      noteLength[kSeqLength];
extern int      numNotes;
extern int      totalSteps;
extern int      stepNumber;
extern bool     trigOut;
extern bool     pendingReset;
extern uint16_t cvValue;
extern uint16_t lengthCv;

extern CvChannel cvChannels[4];

extern int modNoteOrder[kSeqLength];
extern int modNoteLength[kSeqLength];
extern int modNumNotes;

void InitSequence();
int  NoteStartBeat(int note);
int  BeatToNote(int beat);
bool IsNoteStart(int beat);
void BuildSequenceString(char* seq);
void BuildSequenceStringFrom(const int* lengths,
                             int        noteCount,
                             const int* noteIds,
                             char*      seq);
uint16_t CvForNote(int note);
uint16_t CvForLength(int length);
void RefreshCvsFromCurrentStep();
void SnapshotSequence();
void ApplyTotalSteps(int newTotal);
void IncreaseNoteLength(int note);
void DecreaseNoteLength(int note);
void TriggerNoteAtStep();
void TriggerNoteAtStep(bool fireGate);
void ResetToFirstStep();

void InitCvChannels();
void ClampCvPositions();
void GetSegmentRange(int position, int size, int* startBeat, int* endBeat);
bool NoteIntersectsSegment(int note, int startBeat, int endBeat);
void RebuildModifiedSequence();
int  ModNoteStartBeat(int modNote);
int  ModBeatToNote(int beat);
bool ModIsNoteStart(int beat);
void BuildModifiedSequenceString(char* seq);

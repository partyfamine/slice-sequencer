#pragma once
#include "daisy_patch.h"
#include <stdint.h>

extern daisy::DaisyPatch patch;

static const int  kSeqLength = 16;
static const int  kMinSteps  = 2;
static const int  kFontWidth = 7;
static const int  kNumCvControls = 3;
static const char kHex[]     = "0123456789ABCDEF";

enum UiScreen
{
    UI_MAIN_MENU,
    UI_EDIT_SEQUENCE,
    UI_CV_MENU,
    UI_SAVE_MENU,
    UI_LOAD_MENU,
    UI_NEW_MENU,
    UI_PATTERNS_MENU,
    UI_PATTERN_ORDER_MENU,
};

enum CvType
{
    CV_TYPE_SHIFT = 0,
    CV_TYPE_TRANSPOSE,
    CV_TYPE_REPEAT,
    CV_TYPE_DISABLED,
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

enum NoteHit : uint8_t
{
    HIT_NONE  = 0,
    HIT_KICK  = 1,
    HIT_SNARE = 2,
};

enum AssignMode
{
    ASSIGN_LENGTH = 0,
    ASSIGN_HITS,
};

extern int      noteLength[kSeqLength];
extern uint8_t  noteHit[kSeqLength];
extern int      numNotes;
extern int      totalSteps;
extern int      stepNumber;
extern bool     trigOut;
extern bool     pendingReset;
extern uint16_t cvValue;
extern uint16_t lengthCv;

extern CvChannel cvChannels[kNumCvControls];

extern int modNoteOrder[kSeqLength];
extern int modNoteLength[kSeqLength];
extern int modNumNotes;

extern volatile int kickGateSamples;
extern volatile int snareGateSamples;

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
void TriggerHitGates(int noteId);

void InitCvChannels();
void ClampCvPositions();
void GetSegmentRange(int position, int size, int* startBeat, int* endBeat);
bool NoteIntersectsSegment(int note, int startBeat, int endBeat);
void RebuildModifiedSequence();
int  ModNoteStartBeat(int modNote);
int  ModBeatToNote(int beat);
bool ModIsNoteStart(int beat);
void BuildModifiedSequenceString(char* seq);
void BuildBaselineSequenceString(char* seq);
int  RemapBaselinePosition(int oldPosition);
void SetRebuildBaseline(const int* order, const int* lengths, int count);
void ClearRebuildBaseline(); // use original identity sequence
int  ActiveNoteCount();
int  ActiveNoteStartBeat(int note);
int  ActiveBeatToNote(int beat);
bool ActiveNoteIntersectsSegment(int note, int startBeat, int endBeat);

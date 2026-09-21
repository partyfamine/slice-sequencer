#pragma once
#include "app.h"

static const int kMaxUserPatterns = 15;

struct UserPattern
{
    int       noteOrder[kSeqLength];
    int       noteLength[kSeqLength];
    int       numNotes;
    CvChannel cv[kNumCvControls];
};

extern int         selectedPattern; // display / CV edit (0 = original)
extern int         playPattern;     // playback slot
extern int         numUserPatterns;
extern bool        patternSelectLocked;
extern UserPattern userPatterns[kMaxUserPatterns];
extern CvChannel   originalCv[kNumCvControls];

extern int  holdNoteId;
extern int  holdStepsLeft;
extern bool holdActive;
extern bool holdSilence;

void InitPatterns();
void SyncWorkingCvFromSlot(int slot);
void StoreWorkingCvToSlot(int slot);
int  PatternSlotCount(); // 1 + numUserPatterns
int  PatternIndexFromCv(float amount);
void UpdatePatternSelectionFromCv();
void CapturePatternSwitchHoldState(bool* midNote, int* noteId, int* stepsLeft);
void ApplyPendingPatternSwitch(bool midNote, int noteId, int stepsLeft);
bool AddPatternFromCurrent();
bool DeleteSelectedPattern();
void MoveUserPattern(int fromUserIndex, int toUserIndex);
void BuildSlotBaselineString(int slot, char* seq);
void GetSlotBaseline(int slot, int* order, int* lengths, int* count);
CvChannel* GetSlotCv(int slot);
void RebuildModifiedSequenceForSlot(int slot);
bool PatternSwitchPending();
void ClearPatternHold();


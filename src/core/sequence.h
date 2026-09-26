#pragma once
#include "types.h"

extern int      noteLength[kSeqLength];
extern uint8_t  noteHit[kSeqLength];
extern int      numNotes;
extern int      totalSteps;
extern int      stepNumber;
extern bool     trigOut;
extern bool     pendingReset;
extern uint16_t cvValue;
extern uint16_t lengthCv;

extern SliceOutMode sliceOutMode;

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

// Pattern-switch hold (owned by playback; patterns only signals the switch).
void ClearPlaybackHold();
void CapturePatternSwitchHoldState(bool* midNote, int* noteId, int* stepsLeft);
void ApplyHoldAfterPatternSwitch(bool midNote, int noteId, int stepsLeft);

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

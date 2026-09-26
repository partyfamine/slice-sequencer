#include "sequence.h"
#include "cv.h"
#include <stdint.h>

int noteLength[kSeqLength];
uint8_t noteHit[kSeqLength];
int numNotes;
int totalSteps;
int stepNumber;

SliceOutMode sliceOutMode = SLICE_OUT_NOTE;

volatile int kickGateSamples;
volatile int snareGateSamples;

static int     savedNoteLength[kSeqLength];
static uint8_t savedNoteHit[kSeqLength];
static int     savedNumNotes;

void InitSequence()
{
    stepNumber       = 0;
    totalSteps       = kSeqLength;
    numNotes         = kSeqLength;
    kickGateSamples  = 0;
    snareGateSamples = 0;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = 1;
        noteHit[i]    = HIT_NONE;
    }
    InitCvChannels();
    RebuildModifiedSequence();
}

int NoteStartBeat(int note)
{
    int pos = 0;
    for(int n = 0; n < note; n++)
    {
        pos += noteLength[n];
    }
    return pos;
}

int BeatToNote(int beat)
{
    int pos = 0;
    for(int n = 0; n < numNotes; n++)
    {
        pos += noteLength[n];
        if(beat < pos)
        {
            return n;
        }
    }
    return numNotes - 1;
}

bool IsNoteStart(int beat)
{
    return beat == NoteStartBeat(BeatToNote(beat));
}

void BuildSequenceStringFrom(const int* lengths,
                             int        noteCount,
                             const int* noteIds,
                             char*      seq)
{
    seq[0] = ' ';
    int s  = 1;
    for(int n = 0; n < noteCount; n++)
    {
        int id   = noteIds ? noteIds[n] : n;
        seq[s++] = kHex[id];
        for(int r = 1; r < lengths[n]; r++)
        {
            seq[s++] = '-';
        }
    }
    seq[s] = '\0';
}

void BuildSequenceString(char* seq)
{
    BuildSequenceStringFrom(noteLength, numNotes, nullptr, seq);
}

void SnapshotSequence()
{
    savedNumNotes = numNotes;
    for(int i = 0; i < kSeqLength; i++)
    {
        savedNoteLength[i] = noteLength[i];
        savedNoteHit[i]    = noteHit[i];
    }
}

void ApplyTotalSteps(int newTotal)
{
    if(newTotal < kMinSteps)
    {
        newTotal = kMinSteps;
    }
    if(newTotal > kSeqLength)
    {
        newTotal = kSeqLength;
    }

    int beats = 0;
    numNotes  = 0;
    for(int n = 0; n < savedNumNotes && beats < newTotal; n++)
    {
        int len       = savedNoteLength[n];
        int remaining = newTotal - beats;
        if(len > remaining)
        {
            len = remaining;
        }
        noteLength[numNotes] = len;
        noteHit[numNotes]    = savedNoteHit[n];
        numNotes++;
        beats += len;
    }
    while(beats < newTotal)
    {
        noteLength[numNotes] = 1;
        noteHit[numNotes]    = HIT_NONE;
        numNotes++;
        beats++;
    }

    totalSteps = newTotal;
    ClampCvPositions();
    RefreshCvsFromCurrentStep();
}

void IncreaseNoteLength(int note)
{
    if(note >= numNotes - 1)
    {
        return;
    }

    int last = numNotes - 1;
    if(noteLength[last] > 1)
    {
        noteLength[last]--;
    }
    else
    {
        numNotes--;
        noteHit[numNotes] = HIT_NONE;
    }
    noteLength[note]++;
    ClampCvPositions();
    RebuildModifiedSequence();
}

void DecreaseNoteLength(int note)
{
    if(noteLength[note] <= 1)
    {
        return;
    }

    noteLength[note]--;
    noteLength[numNotes] = 1;
    noteHit[numNotes]    = HIT_NONE;
    numNotes++;
    RebuildModifiedSequence();
}

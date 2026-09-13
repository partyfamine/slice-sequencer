#include "app.h"
#include <cmath>

int noteLength[kSeqLength];
int numNotes;
int totalSteps;
int stepNumber;

static int savedNoteLength[kSeqLength];
static int savedNumNotes;

void InitSequence()
{
    stepNumber = 0;
    totalSteps = kSeqLength;
    numNotes   = kSeqLength;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = 1;
    }
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

void BuildSequenceString(char* seq)
{
    seq[0] = ' ';
    int s  = 1;
    for(int n = 0; n < numNotes; n++)
    {
        seq[s++] = kHex[n];
        for(int r = 1; r < noteLength[n]; r++)
        {
            seq[s++] = '-';
        }
    }
    seq[s] = '\0';
}

uint16_t CvForNote(int note)
{
    return (uint16_t)round(note * (4096.0 / (double)numNotes));
}

uint16_t CvForLength(int length)
{
    return (uint16_t)round((length - 1) * (4096.0 / 15.0));
}

void RefreshCvsFromCurrentStep()
{
    if(stepNumber >= totalSteps)
    {
        stepNumber %= totalSteps;
    }
    int note = BeatToNote(stepNumber);
    cvValue  = CvForNote(note);
    lengthCv = CvForLength(noteLength[note]);
}

void SnapshotSequence()
{
    savedNumNotes = numNotes;
    for(int i = 0; i < kSeqLength; i++)
    {
        savedNoteLength[i] = noteLength[i];
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
        numNotes++;
        beats += len;
    }
    while(beats < newTotal)
    {
        noteLength[numNotes] = 1;
        numNotes++;
        beats++;
    }

    totalSteps = newTotal;
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
    }
    noteLength[note]++;
}

void DecreaseNoteLength(int note)
{
    if(noteLength[note] <= 1)
    {
        return;
    }

    noteLength[note]--;
    noteLength[numNotes] = 1;
    numNotes++;
}

void TriggerNoteAtStep()
{
    trigOut  = true;
    int note = BeatToNote(stepNumber);
    cvValue  = CvForNote(note);
    lengthCv = CvForLength(noteLength[note]);
}

void ResetToFirstStep()
{
    stepNumber   = 0;
    pendingReset = false;
    TriggerNoteAtStep();
}

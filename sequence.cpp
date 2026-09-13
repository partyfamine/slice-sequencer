#include "app.h"
#include <cmath>

int noteLength[kSeqLength];
int numNotes;
int totalSteps;
int stepNumber;

int modNoteOrder[kSeqLength];
int modNoteLength[kSeqLength];
int modNumNotes;

CvChannel cvChannels[4];

static int savedNoteLength[kSeqLength];
static int savedNumNotes;

void InitCvChannels()
{
    for(int i = 0; i < 4; i++)
    {
        cvChannels[i].type     = CV_TYPE_SHIFT;
        cvChannels[i].position = 0;
        cvChannels[i].size     = 1;
    }
}

void ClampCvPositions()
{
    for(int i = 0; i < 4; i++)
    {
        if(numNotes <= 0)
        {
            cvChannels[i].position = 0;
            continue;
        }
        if(cvChannels[i].position >= numNotes)
        {
            cvChannels[i].position = numNotes - 1;
        }
        if(cvChannels[i].position < 0)
        {
            cvChannels[i].position = 0;
        }
    }
}

void InitSequence()
{
    stepNumber = 0;
    totalSteps = kSeqLength;
    numNotes   = kSeqLength;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = 1;
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

void BuildModifiedSequenceString(char* seq)
{
    BuildSequenceStringFrom(modNoteLength, modNumNotes, modNoteOrder, seq);
}

uint16_t CvForNote(int note)
{
    return (uint16_t)round(note * (4096.0 / (double)numNotes));
}

uint16_t CvForLength(int length)
{
    return (uint16_t)round((length - 1) * (4096.0 / 15.0));
}

void GetSegmentRange(int position, int size, int* startBeat, int* endBeat)
{
    if(numNotes <= 0 || totalSteps <= 0)
    {
        *startBeat = 0;
        *endBeat   = 0;
        return;
    }
    if(position < 0)
    {
        position = 0;
    }
    if(position >= numNotes)
    {
        position = numNotes - 1;
    }
    if(size < 1)
    {
        size = 1;
    }
    if(size > 8)
    {
        size = 8;
    }

    if(size >= totalSteps)
    {
        *startBeat = 0;
        *endBeat   = totalSteps;
        return;
    }

    *startBeat = NoteStartBeat(position);
    *endBeat   = *startBeat + size;
    if(*endBeat > totalSteps)
    {
        *endBeat = totalSteps;
    }
}

bool NoteIntersectsSegment(int note, int startBeat, int endBeat)
{
    int nStart = NoteStartBeat(note);
    int nEnd   = nStart + noteLength[note];
    return nStart < endBeat && nEnd > startBeat;
}

int ModNoteStartBeat(int modNote)
{
    int pos = 0;
    for(int n = 0; n < modNote; n++)
    {
        pos += modNoteLength[n];
    }
    return pos;
}

int ModBeatToNote(int beat)
{
    int pos = 0;
    for(int n = 0; n < modNumNotes; n++)
    {
        pos += modNoteLength[n];
        if(beat < pos)
        {
            return n;
        }
    }
    return modNumNotes > 0 ? modNumNotes - 1 : 0;
}

bool ModIsNoteStart(int beat)
{
    if(modNumNotes <= 0)
    {
        return false;
    }
    return beat == ModNoteStartBeat(ModBeatToNote(beat));
}

static void MarkSegmentNotes(int position, int size, bool* inSeg)
{
    int startBeat, endBeat;
    GetSegmentRange(position, size, &startBeat, &endBeat);
    for(int n = 0; n < numNotes; n++)
    {
        inSeg[n] = NoteIntersectsSegment(n, startBeat, endBeat);
    }
    for(int n = numNotes; n < kSeqLength; n++)
    {
        inSeg[n] = false;
    }
}

static int OtherSegmentId(int noteId, const bool segNotes[4][kSeqLength], int selfCv)
{
    for(int c = 0; c < 4; c++)
    {
        if(c == selfCv)
        {
            continue;
        }
        if(cvChannels[c].type == CV_TYPE_SHIFT && segNotes[c][noteId])
        {
            return c;
        }
    }
    return -1;
}

void RebuildModifiedSequence()
{
    int order[kSeqLength];
    int lengths[kSeqLength];
    int count = numNotes;

    for(int i = 0; i < numNotes; i++)
    {
        order[i]   = i;
        lengths[i] = noteLength[i];
    }

    bool segNotes[4][kSeqLength];
    for(int c = 0; c < 4; c++)
    {
        for(int n = 0; n < kSeqLength; n++)
        {
            segNotes[c][n] = false;
        }
        if(cvChannels[c].type == CV_TYPE_SHIFT)
        {
            MarkSegmentNotes(
                cvChannels[c].position, cvChannels[c].size, segNotes[c]);
        }
    }

    for(int c = 0; c < 4; c++)
    {
        if(cvChannels[c].type != CV_TYPE_SHIFT || count <= 0)
        {
            continue;
        }

        bool* inSeg      = segNotes[c];
        int   blockStart = -1;
        int   blockEnd   = -1;
        for(int i = 0; i < count; i++)
        {
            if(inSeg[order[i]])
            {
                if(blockStart < 0)
                {
                    blockStart = i;
                }
                blockEnd = i;
            }
        }
        if(blockStart < 0)
        {
            continue;
        }

        // Unit boundary ends for notes before / after the block.
        int leftEnds[kSeqLength];
        int leftCount = 0;
        for(int i = 0; i < blockStart;)
        {
            int segId = OtherSegmentId(order[i], segNotes, c);
            int j     = i + 1;
            if(segId >= 0)
            {
                while(j < blockStart && segNotes[segId][order[j]])
                {
                    j++;
                }
            }
            leftEnds[leftCount++] = j;
            i                     = j;
        }

        int rightEnds[kSeqLength];
        int rightCount = 0;
        for(int i = blockEnd + 1; i < count;)
        {
            int segId = OtherSegmentId(order[i], segNotes, c);
            int j     = i + 1;
            if(segId >= 0)
            {
                while(j < count && segNotes[segId][order[j]])
                {
                    j++;
                }
            }
            rightEnds[rightCount++] = j;
            i                       = j;
        }

        float amount = patch.GetKnobValue((daisy::DaisyPatch::Ctrl)c);
        if(amount < 0.f)
        {
            amount = 0.f;
        }
        if(amount > 1.f)
        {
            amount = 1.f;
        }

        int moves = 0;
        if(amount < 0.5f && leftCount > 0)
        {
            float t = (0.5f - amount) / 0.5f;
            moves   = -(int)roundf(t * (float)leftCount);
        }
        else if(amount > 0.5f && rightCount > 0)
        {
            float t = (amount - 0.5f) / 0.5f;
            moves   = (int)roundf(t * (float)rightCount);
        }
        if(moves == 0)
        {
            continue;
        }

        int blockLen = blockEnd - blockStart + 1;
        int blockNotes[kSeqLength];
        int blockLens[kSeqLength];
        for(int b = 0; b < blockLen; b++)
        {
            blockNotes[b] = order[blockStart + b];
            blockLens[b]  = lengths[blockStart + b];
        }

        int destUnit = leftCount + moves;
        if(destUnit < 0)
        {
            destUnit = 0;
        }
        if(destUnit > leftCount + rightCount)
        {
            destUnit = leftCount + rightCount;
        }

        int newOrder[kSeqLength];
        int newLens[kSeqLength];
        int out      = 0;
        int unitIdx  = 0;
        int inserted = 0;

        int leftCursor = 0;
        for(int u = 0; u < leftCount; u++)
        {
            if(unitIdx == destUnit)
            {
                for(int b = 0; b < blockLen; b++)
                {
                    newOrder[out] = blockNotes[b];
                    newLens[out]  = blockLens[b];
                    out++;
                }
                inserted = 1;
            }
            for(int k = leftCursor; k < leftEnds[u]; k++)
            {
                newOrder[out] = order[k];
                newLens[out]  = lengths[k];
                out++;
            }
            leftCursor = leftEnds[u];
            unitIdx++;
        }

        int rightCursor = blockEnd + 1;
        for(int u = 0; u < rightCount; u++)
        {
            if(unitIdx == destUnit)
            {
                for(int b = 0; b < blockLen; b++)
                {
                    newOrder[out] = blockNotes[b];
                    newLens[out]  = blockLens[b];
                    out++;
                }
                inserted = 1;
            }
            for(int k = rightCursor; k < rightEnds[u]; k++)
            {
                newOrder[out] = order[k];
                newLens[out]  = lengths[k];
                out++;
            }
            rightCursor = rightEnds[u];
            unitIdx++;
        }

        if(!inserted)
        {
            for(int b = 0; b < blockLen; b++)
            {
                newOrder[out] = blockNotes[b];
                newLens[out]  = blockLens[b];
                out++;
            }
        }

        for(int k = 0; k < count; k++)
        {
            order[k]   = newOrder[k];
            lengths[k] = newLens[k];
        }
    }

    modNumNotes = count;
    for(int i = 0; i < count; i++)
    {
        modNoteOrder[i]  = order[i];
        modNoteLength[i] = lengths[i];
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
    numNotes++;
    RebuildModifiedSequence();
}

void TriggerNoteAtStep()
{
    RebuildModifiedSequence();
    trigOut = true;
    if(modNumNotes <= 0)
    {
        return;
    }
    int modNote = ModBeatToNote(stepNumber);
    int origId  = modNoteOrder[modNote];
    cvValue     = CvForNote(origId);
    lengthCv    = CvForLength(modNoteLength[modNote]);
}

void ResetToFirstStep()
{
    stepNumber   = 0;
    pendingReset = false;
    TriggerNoteAtStep();
}

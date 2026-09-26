#include "app.h"
#include "patterns.h"
#include <cmath>
#include <stdint.h>

int noteLength[kSeqLength];
uint8_t noteHit[kSeqLength];
int numNotes;
int totalSteps;
int stepNumber;

int modNoteOrder[kSeqLength];
int modNoteLength[kSeqLength];
int modNumNotes;
int modSrcTag[kSeqLength];

CvChannel cvChannels[kNumCvControls];

SliceOutMode sliceOutMode = SLICE_OUT_NOTE;

volatile int kickGateSamples;
volatile int snareGateSamples;

static const int kAudioGateSamples = 480; // ~10ms at 48kHz

static int     savedNoteLength[kSeqLength];
static uint8_t savedNoteHit[kSeqLength];
static int     savedNumNotes;

void InitCvChannels()
{
    for(int i = 0; i < kNumCvControls; i++)
    {
        cvChannels[i].type     = CV_TYPE_DISABLED;
        cvChannels[i].position = 0;
        cvChannels[i].size     = 1;
    }
}

void ClampCvPositions()
{
    int count = ActiveNoteCount();
    for(int i = 0; i < kNumCvControls; i++)
    {
        if(count <= 0)
        {
            cvChannels[i].position = 0;
            continue;
        }
        if(cvChannels[i].position >= count)
        {
            cvChannels[i].position = count - 1;
        }
        if(cvChannels[i].position < 0)
        {
            cvChannels[i].position = 0;
        }
    }
}

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

static const int* rebuildOrder = nullptr;
static const int* rebuildLens  = nullptr;
static int        rebuildCount = 0;

void SetRebuildBaseline(const int* order, const int* lengths, int count)
{
    rebuildOrder = order;
    rebuildLens  = lengths;
    rebuildCount = count;
}

void ClearRebuildBaseline()
{
    rebuildOrder = nullptr;
    rebuildLens  = nullptr;
    rebuildCount = 0;
}

int ActiveNoteCount()
{
    return rebuildLens ? rebuildCount : numNotes;
}

int ActiveNoteStartBeat(int note)
{
    const int* lens = rebuildLens ? rebuildLens : noteLength;
    int        pos  = 0;
    for(int n = 0; n < note; n++)
    {
        pos += lens[n];
    }
    return pos;
}

int ActiveBeatToNote(int beat)
{
    const int* lens  = rebuildLens ? rebuildLens : noteLength;
    int        count = ActiveNoteCount();
    int        pos   = 0;
    for(int n = 0; n < count; n++)
    {
        pos += lens[n];
        if(beat < pos)
        {
            return n;
        }
    }
    return count > 0 ? count - 1 : 0;
}

bool ActiveNoteIntersectsSegment(int note, int startBeat, int endBeat)
{
    const int* lens   = rebuildLens ? rebuildLens : noteLength;
    int        nStart = ActiveNoteStartBeat(note);
    int        nEnd   = nStart + lens[note];
    return nStart < endBeat && nEnd > startBeat;
}

void BuildModifiedSequenceString(char* seq)
{
    BuildSequenceStringFrom(modNoteLength, modNumNotes, modNoteOrder, seq);
}

void BuildBaselineSequenceString(char* seq)
{
    if(rebuildLens)
    {
        BuildSequenceStringFrom(rebuildLens, rebuildCount, rebuildOrder, seq);
    }
    else
    {
        BuildSequenceString(seq);
    }
}

int RemapBaselinePosition(int oldPosition)
{
    if(modNumNotes <= 0)
    {
        return 0;
    }
    for(int i = 0; i < modNumNotes; i++)
    {
        if(modSrcTag[i] == oldPosition)
        {
            return i;
        }
    }
    if(oldPosition < 0)
    {
        return 0;
    }
    if(oldPosition >= modNumNotes)
    {
        return modNumNotes - 1;
    }
    return oldPosition;
}

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

void GetSegmentRange(int position, int size, int* startBeat, int* endBeat)
{
    int count = ActiveNoteCount();
    if(count <= 0 || totalSteps <= 0)
    {
        *startBeat = 0;
        *endBeat   = 0;
        return;
    }
    if(position < 0)
    {
        position = 0;
    }
    if(position >= count)
    {
        position = count - 1;
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

    *startBeat = ActiveNoteStartBeat(position);
    *endBeat   = *startBeat + size;
    if(*endBeat > totalSteps)
    {
        *endBeat = totalSteps;
    }
}

bool NoteIntersectsSegment(int note, int startBeat, int endBeat)
{
    return ActiveNoteIntersectsSegment(note, startBeat, endBeat);
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
    int count = ActiveNoteCount();
    for(int n = 0; n < count; n++)
    {
        inSeg[n] = ActiveNoteIntersectsSegment(n, startBeat, endBeat);
    }
    for(int n = count; n < kSeqLength; n++)
    {
        inSeg[n] = false;
    }
}

static int StepsAvailableFromNote(int note)
{
    int steps = 0;
    for(int n = note; n < numNotes; n++)
    {
        steps += noteLength[n];
    }
    return steps;
}

static int ClampCvAmountMoves(float amount, int maxDown, int maxUp)
{
    if(amount < 0.f)
    {
        amount = 0.f;
    }
    if(amount > 1.f)
    {
        amount = 1.f;
    }

    if(amount < 0.5f && maxDown > 0)
    {
        float t = (0.5f - amount) / 0.5f;
        return -(int)roundf(t * (float)maxDown);
    }
    if(amount > 0.5f && maxUp > 0)
    {
        float t = (amount - 0.5f) / 0.5f;
        return (int)roundf(t * (float)maxUp);
    }
    return 0;
}

static int TransposeOffset(int position, int size, float amount)
{
    int startBeat, endBeat;
    GetSegmentRange(position, size, &startBeat, &endBeat);
    int segmentSteps = endBeat - startBeat;
    if(segmentSteps <= 0)
    {
        return 0;
    }

    int maxDown = position;
    int maxUp   = 0;
    for(int d = 0; position + d < numNotes; d++)
    {
        if(StepsAvailableFromNote(position + d) >= segmentSteps)
        {
            maxUp = d;
        }
        else
        {
            break;
        }
    }

    return ClampCvAmountMoves(amount, maxDown, maxUp);
}

static int FillFromOriginal(int sourceNote,
                            int steps,
                            int* outIds,
                            int* outLens)
{
    if(steps <= 0 || numNotes <= 0)
    {
        return 0;
    }
    if(sourceNote < 0)
    {
        sourceNote = 0;
    }
    if(sourceNote >= numNotes)
    {
        sourceNote = numNotes - 1;
    }

    // If this start can't supply enough steps, walk back until it can (or to 0).
    while(sourceNote > 0 && StepsAvailableFromNote(sourceNote) < steps)
    {
        sourceNote--;
    }

    int outCount  = 0;
    int remaining = steps;
    int n         = sourceNote;
    while(remaining > 0 && n < numNotes)
    {
        int take = noteLength[n];
        if(take > remaining)
        {
            take = remaining;
        }
        outIds[outCount]  = n;
        outLens[outCount] = take;
        outCount++;
        remaining -= take;
        n++;
    }
    return outCount;
}

static int OtherSegmentId(int noteId, const bool segNotes[kNumCvControls][kSeqLength], int selfCv)
{
    for(int c = 0; c < kNumCvControls; c++)
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

static bool cvModified[kNumCvControls];

static void ApplyShifts(int*     order,
                        int*     lengths,
                        uint8_t* segFlags,
                        int*     srcTag,
                        int*     countInOut)
{
    int count = *countInOut;

    bool segNotes[kNumCvControls][kSeqLength];
    for(int c = 0; c < kNumCvControls; c++)
    {
        cvModified[c] = false;
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

    for(int c = 0; c < kNumCvControls; c++)
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
            int tag = srcTag[i];
            if(tag >= 0 && inSeg[tag])
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

        int leftEnds[kSeqLength];
        int leftCount = 0;
        for(int i = 0; i < blockStart;)
        {
            int segId = OtherSegmentId(srcTag[i], segNotes, c);
            int j     = i + 1;
            if(segId >= 0)
            {
                while(j < blockStart && srcTag[j] >= 0
                      && segNotes[segId][srcTag[j]])
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
            int segId = OtherSegmentId(srcTag[i], segNotes, c);
            int j     = i + 1;
            if(segId >= 0)
            {
                while(j < count && srcTag[j] >= 0
                      && segNotes[segId][srcTag[j]])
                {
                    j++;
                }
            }
            rightEnds[rightCount++] = j;
            i                       = j;
        }

        float amount = patch.GetKnobValue((daisy::DaisyPatch::Ctrl)c);
        int   moves  = ClampCvAmountMoves(amount, leftCount, rightCount);
        if(moves == 0)
        {
            continue;
        }
        cvModified[c] = true;

        int blockLen = blockEnd - blockStart + 1;
        int blockNotes[kSeqLength];
        int blockLens[kSeqLength];
        int blockTags[kSeqLength];
        uint8_t blockFlags[kSeqLength];
        for(int b = 0; b < blockLen; b++)
        {
            blockNotes[b] = order[blockStart + b];
            blockLens[b]  = lengths[blockStart + b];
            blockTags[b]  = srcTag[blockStart + b];
            blockFlags[b] = segFlags[blockStart + b];
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

        int     newOrder[kSeqLength];
        int     newLens[kSeqLength];
        int     newTags[kSeqLength];
        uint8_t newFlags[kSeqLength];
        int     out      = 0;
        int     unitIdx  = 0;
        int     inserted = 0;

        int leftCursor = 0;
        for(int u = 0; u < leftCount; u++)
        {
            if(unitIdx == destUnit)
            {
                for(int b = 0; b < blockLen; b++)
                {
                    newOrder[out] = blockNotes[b];
                    newLens[out]  = blockLens[b];
                    newTags[out]  = blockTags[b];
                    newFlags[out] = blockFlags[b];
                    out++;
                }
                inserted = 1;
            }
            for(int k = leftCursor; k < leftEnds[u]; k++)
            {
                newOrder[out] = order[k];
                newLens[out]  = lengths[k];
                newTags[out]  = srcTag[k];
                newFlags[out] = segFlags[k];
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
                    newTags[out]  = blockTags[b];
                    newFlags[out] = blockFlags[b];
                    out++;
                }
                inserted = 1;
            }
            for(int k = rightCursor; k < rightEnds[u]; k++)
            {
                newOrder[out] = order[k];
                newLens[out]  = lengths[k];
                newTags[out]  = srcTag[k];
                newFlags[out] = segFlags[k];
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
                newTags[out]  = blockTags[b];
                newFlags[out] = blockFlags[b];
                out++;
            }
        }

        for(int k = 0; k < count; k++)
        {
            order[k]    = newOrder[k];
            lengths[k]  = newLens[k];
            srcTag[k]   = newTags[k];
            segFlags[k] = newFlags[k];
        }
    }

    *countInOut = count;
}

static void ApplyTransposes(int*     order,
                            int*     lengths,
                            uint8_t* segFlags,
                            int*     srcTag,
                            int*     countInOut)
{
    int count = *countInOut;

    for(int c = 0; c < kNumCvControls; c++)
    {
        if(cvChannels[c].type != CV_TYPE_TRANSPOSE || count <= 0)
        {
            continue;
        }

        bool inSeg[kSeqLength];
        MarkSegmentNotes(
            cvChannels[c].position, cvChannels[c].size, inSeg);

        float amount = patch.GetKnobValue((daisy::DaisyPatch::Ctrl)c);
        int   offset = TransposeOffset(
            cvChannels[c].position, cvChannels[c].size, amount);
        if(offset == 0)
        {
            continue;
        }
        cvModified[c] = true;

        int     newOrder[kSeqLength];
        int     newLens[kSeqLength];
        int     newTags[kSeqLength];
        uint8_t newFlags[kSeqLength];
        int     out = 0;
        int     i   = 0;
        while(i < count)
        {
            int tag = srcTag[i];
            if(tag < 0 || !inSeg[tag])
            {
                newOrder[out] = order[i];
                newLens[out]  = lengths[i];
                newTags[out]  = srcTag[i];
                newFlags[out] = segFlags[i];
                out++;
                i++;
                continue;
            }

            // Transpose relative to the original sequence note ID.
            int     runFirst = order[i];
            int     runSteps = 0;
            uint8_t runFlags = 0;
            while(i < count)
            {
                tag = srcTag[i];
                if(tag < 0 || !inSeg[tag])
                {
                    break;
                }
                runSteps += lengths[i];
                runFlags |= segFlags[i];
                i++;
            }

            int fillIds[kSeqLength];
            int fillLens[kSeqLength];
            int source = runFirst + offset;
            int filled = FillFromOriginal(source, runSteps, fillIds, fillLens);
            for(int f = 0; f < filled; f++)
            {
                newOrder[out] = fillIds[f];
                newLens[out]  = fillLens[f];
                newTags[out]  = -1;
                // Preserve nested segment membership (e.g. Repeat inside Transpose).
                newFlags[out] = runFlags;
                out++;
            }
        }

        count = out;
        for(int k = 0; k < count; k++)
        {
            order[k]    = newOrder[k];
            lengths[k]  = newLens[k];
            srcTag[k]   = newTags[k];
            segFlags[k] = newFlags[k];
        }
    }

    *countInOut = count;
}

static void ExpandToSteps(const int*     order,
                          const int*     lengths,
                          const uint8_t* segFlags,
                          const int*     srcTag,
                          int            count,
                          int*           stepNote,
                          bool*          stepStart,
                          uint8_t*       stepFlags,
                          int*           stepTag,
                          int*           nStepsOut)
{
    int nSteps = 0;
    for(int i = 0; i < count; i++)
    {
        for(int t = 0; t < lengths[i]; t++)
        {
            if(nSteps >= kSeqLength)
            {
                break;
            }
            stepNote[nSteps]  = order[i];
            stepStart[nSteps] = (t == 0);
            stepFlags[nSteps] = segFlags[i];
            stepTag[nSteps]   = srcTag[i];
            nSteps++;
        }
    }
    *nStepsOut = nSteps;
}

static void CollapseFromSteps(const int*  stepNote,
                              const bool* stepStart,
                              const int*  stepTag,
                              int         nSteps,
                              int*        order,
                              int*        lengths,
                              int*        srcTag,
                              int*        countOut)
{
    int count = 0;
    int i     = 0;
    while(i < nSteps)
    {
        int id  = stepNote[i];
        int tag = stepTag[i];
        int len = 1;
        i++;
        while(i < nSteps && !stepStart[i])
        {
            len++;
            i++;
        }
        order[count]   = id;
        lengths[count] = len;
        srcTag[count]  = tag;
        count++;
    }
    *countOut = count;
}

static void ApplyRepeats(int*     order,
                         int*     lengths,
                         uint8_t* segFlags,
                         int*     srcTag,
                         int*     countInOut)
{
    int count = *countInOut;
    if(count <= 0 || totalSteps <= 0)
    {
        return;
    }

    int     stepNote[kSeqLength];
    bool    stepStart[kSeqLength];
    uint8_t stepFlags[kSeqLength];
    int     stepTag[kSeqLength];
    int     nSteps = 0;
    ExpandToSteps(order,
                  lengths,
                  segFlags,
                  srcTag,
                  count,
                  stepNote,
                  stepStart,
                  stepFlags,
                  stepTag,
                  &nSteps);
    if(nSteps <= 0)
    {
        return;
    }

    bool stepProtected[kSeqLength];
    bool stepRepeatOwned[kSeqLength];
    for(int s = 0; s < nSteps; s++)
    {
        stepProtected[s]   = false;
        stepRepeatOwned[s] = false;
        for(int c = 0; c < kNumCvControls; c++)
        {
            if(!cvModified[c])
            {
                continue;
            }
            if(cvChannels[c].type != CV_TYPE_SHIFT
               && cvChannels[c].type != CV_TYPE_TRANSPOSE)
            {
                continue;
            }
            if(stepFlags[s] & (1u << c))
            {
                stepProtected[s] = true;
            }
        }
    }

    for(int c = 0; c < kNumCvControls; c++)
    {
        if(cvChannels[c].type != CV_TYPE_REPEAT)
        {
            continue;
        }

        uint8_t mask = (uint8_t)(1u << c);

        // Locate contiguous runs belonging to this Repeat segment.
        int runStarts[kSeqLength];
        int runEnds[kSeqLength];
        int runCount = 0;
        int s        = 0;
        while(s < nSteps)
        {
            if(!(stepFlags[s] & mask))
            {
                s++;
                continue;
            }
            int start = s;
            while(s < nSteps && (stepFlags[s] & mask))
            {
                s++;
            }
            runStarts[runCount] = start;
            runEnds[runCount]   = s;
            runCount++;
        }
        if(runCount == 0)
        {
            continue;
        }

        float amount = patch.GetKnobValue((daisy::DaisyPatch::Ctrl)c);

        for(int r = 0; r < runCount; r++)
        {
            int patternStart = runStarts[r];
            int patternEnd   = runEnds[r];
            int patternLen   = patternEnd - patternStart;
            if(patternLen <= 0)
            {
                continue;
            }

            int  patNote[kSeqLength];
            bool patStart[kSeqLength];
            int  patTag[kSeqLength];
            for(int p = 0; p < patternLen; p++)
            {
                patNote[p]  = stepNote[patternStart + p];
                patStart[p] = stepStart[patternStart + p];
                patTag[p]   = stepTag[patternStart + p];
            }

            int maxBefore = patternStart;
            int maxAfter  = nSteps - patternEnd;
            int moves     = ClampCvAmountMoves(amount, maxBefore, maxAfter);
            if(moves == 0)
            {
                continue;
            }

            if(moves < 0)
            {
                int  fillCount = -moves;
                int  destStart = patternStart - fillCount;
                int  phase
                    = (patternLen - (fillCount % patternLen)) % patternLen;
                bool forceStart = true;
                for(int i = 0; i < fillCount; i++)
                {
                    int dest = destStart + i;
                    int src  = (phase + i) % patternLen;
                    if(stepProtected[dest] || stepRepeatOwned[dest])
                    {
                        forceStart = true;
                        continue;
                    }
                    stepNote[dest]        = patNote[src];
                    stepStart[dest]       = patStart[src] || forceStart;
                    stepTag[dest]         = patTag[src];
                    stepRepeatOwned[dest] = true;
                    forceStart            = false;
                }
            }
            else
            {
                int  fillCount  = moves;
                int  destStart  = patternEnd;
                bool forceStart = true;
                for(int i = 0; i < fillCount; i++)
                {
                    int dest = destStart + i;
                    int src  = i % patternLen;
                    if(stepProtected[dest] || stepRepeatOwned[dest])
                    {
                        forceStart = true;
                        continue;
                    }
                    stepNote[dest]        = patNote[src];
                    stepStart[dest]       = patStart[src] || forceStart;
                    stepTag[dest]         = patTag[src];
                    stepRepeatOwned[dest] = true;
                    forceStart            = false;
                }
            }
        }
    }

    CollapseFromSteps(
        stepNote, stepStart, stepTag, nSteps, order, lengths, srcTag, countInOut);
}

void RebuildModifiedSequence()
{
    int     order[kSeqLength];
    int     lengths[kSeqLength];
    int     srcTag[kSeqLength];
    uint8_t segFlags[kSeqLength];
    int     count = ActiveNoteCount();

    for(int c = 0; c < kNumCvControls; c++)
    {
        cvModified[c] = false;
    }

    bool segNotes[kNumCvControls][kSeqLength];
    for(int c = 0; c < kNumCvControls; c++)
    {
        for(int n = 0; n < kSeqLength; n++)
        {
            segNotes[c][n] = false;
        }
        if(cvChannels[c].type != CV_TYPE_DISABLED)
        {
            MarkSegmentNotes(
                cvChannels[c].position, cvChannels[c].size, segNotes[c]);
        }
    }

    for(int i = 0; i < count; i++)
    {
        order[i]   = rebuildOrder ? rebuildOrder[i] : i;
        lengths[i] = rebuildLens ? rebuildLens[i] : noteLength[i];
        srcTag[i]  = i;
        segFlags[i] = 0;
        for(int c = 0; c < kNumCvControls; c++)
        {
            if(segNotes[c][i])
            {
                segFlags[i] |= (uint8_t)(1u << c);
            }
        }
    }

    ApplyShifts(order, lengths, segFlags, srcTag, &count);
    ApplyTransposes(order, lengths, segFlags, srcTag, &count);
    ApplyRepeats(order, lengths, segFlags, srcTag, &count);

    modNumNotes = count;
    for(int i = 0; i < count; i++)
    {
        modNoteOrder[i]  = order[i];
        modNoteLength[i] = lengths[i];
        modSrcTag[i]     = srcTag[i];
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
    ClearPatternHold();
    if(PatternSwitchPending())
    {
        ApplyPendingPatternSwitch(false, 0, 0);
    }
    RebuildModifiedSequenceForSlot(playPattern);
    TriggerNoteAtStep();
}

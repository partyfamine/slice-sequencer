#include "patterns.h"
#include "sequence.h"
#include "cv.h"

int         selectedPattern;
int         playPattern;
int         numUserPatterns;
bool        patternSelectLocked;
UserPattern userPatterns[kMaxUserPatterns];
CvChannel   originalCv[kNumCvControls];

static int  pendingPattern = -1;
static bool pendingSwitch  = false;

static void CopyCv(CvChannel* dst, const CvChannel* src)
{
    for(int c = 0; c < kNumCvControls; c++)
    {
        dst[c] = src[c];
    }
}

static int MapUserIndexAfterMove(int u, int fromUserIndex, int toUserIndex)
{
    if(u < 0)
    {
        return u;
    }
    if(u == fromUserIndex)
    {
        return toUserIndex;
    }
    if(fromUserIndex < toUserIndex)
    {
        if(u > fromUserIndex && u <= toUserIndex)
        {
            return u - 1;
        }
    }
    else if(u >= toUserIndex && u < fromUserIndex)
    {
        return u + 1;
    }
    return u;
}

void InitPatterns()
{
    selectedPattern     = 0;
    playPattern         = 0;
    numUserPatterns     = 0;
    patternSelectLocked = false;
    pendingPattern      = -1;
    pendingSwitch       = false;
    ClearPlaybackHold();
    for(int c = 0; c < kNumCvControls; c++)
    {
        originalCv[c] = cvChannels[c];
    }
    ClearRebuildBaseline();
}

void SyncWorkingCvFromSlot(int slot)
{
    CopyCv(cvChannels, GetSlotCv(slot));
    ClampCvPositions();
}

void StoreWorkingCvToSlot(int slot)
{
    CopyCv(GetSlotCv(slot), cvChannels);
}

int PatternSlotCount()
{
    return 1 + numUserPatterns;
}

void GetSlotBaseline(int slot, int* order, int* lengths, int* count)
{
    if(slot <= 0 || slot > numUserPatterns)
    {
        *count = numNotes;
        for(int i = 0; i < numNotes; i++)
        {
            order[i]   = i;
            lengths[i] = noteLength[i];
        }
        return;
    }

    UserPattern& p = userPatterns[slot - 1];
    *count         = p.numNotes;
    for(int i = 0; i < p.numNotes; i++)
    {
        order[i]   = p.noteOrder[i];
        lengths[i] = p.noteLength[i];
    }
}

CvChannel* GetSlotCv(int slot)
{
    if(slot <= 0 || slot > numUserPatterns)
    {
        return originalCv;
    }
    return userPatterns[slot - 1].cv;
}

void BuildSlotBaselineString(int slot, char* seq)
{
    int order[kSeqLength];
    int lengths[kSeqLength];
    int count = 0;
    GetSlotBaseline(slot, order, lengths, &count);
    BuildSequenceStringFrom(lengths, count, order, seq);
}

void RebuildModifiedSequenceForSlot(int slot)
{
    if(slot <= 0 || slot > numUserPatterns)
    {
        ClearRebuildBaseline();
    }
    else
    {
        UserPattern& p = userPatterns[slot - 1];
        SetRebuildBaseline(p.noteOrder, p.noteLength, p.numNotes);
    }
    SyncWorkingCvFromSlot(slot);
    RebuildModifiedSequence();
}

int PatternIndexFromCv(float amount)
{
    if(amount < 0.f)
    {
        amount = 0.f;
    }
    if(amount > 1.f)
    {
        amount = 1.f;
    }

    int slots = PatternSlotCount();
    if(slots <= 1)
    {
        return 0;
    }

    int idx = (int)(amount * (float)slots);
    if(idx >= slots)
    {
        idx = slots - 1;
    }
    if(idx < 0)
    {
        idx = 0;
    }
    return idx;
}

void UpdatePatternSelectionFromCv()
{
    if(patternSelectLocked)
    {
        return;
    }

    float amount = patch.GetKnobValue(daisy::DaisyPatch::CTRL_4);
    int   idx    = PatternIndexFromCv(amount);
    if(idx == selectedPattern)
    {
        return;
    }

    StoreWorkingCvToSlot(selectedPattern);
    selectedPattern = idx;
    RebuildModifiedSequenceForSlot(selectedPattern);
    if(playPattern != selectedPattern)
    {
        pendingPattern = selectedPattern;
        pendingSwitch  = true;
    }
    else
    {
        pendingPattern = -1;
        pendingSwitch  = false;
    }
}

bool PatternSwitchPending()
{
    return pendingSwitch || (pendingPattern >= 0 && pendingPattern != playPattern);
}

void ApplyPendingPatternSwitch()
{
    if(!pendingSwitch && pendingPattern < 0)
    {
        return;
    }

    int target = (pendingPattern >= 0) ? pendingPattern : selectedPattern;
    if(target == playPattern)
    {
        pendingPattern = -1;
        pendingSwitch  = false;
        return;
    }

    playPattern     = target;
    selectedPattern = playPattern;
    pendingPattern  = -1;
    pendingSwitch   = false;

    RebuildModifiedSequenceForSlot(playPattern);
}

bool AddPatternFromCurrent()
{
    if(numUserPatterns >= kMaxUserPatterns)
    {
        return false;
    }

    StoreWorkingCvToSlot(selectedPattern);
    RebuildModifiedSequenceForSlot(selectedPattern);

    CvChannel remapped[kNumCvControls];
    for(int c = 0; c < kNumCvControls; c++)
    {
        remapped[c]          = cvChannels[c];
        remapped[c].position = RemapBaselinePosition(cvChannels[c].position);
    }

    UserPattern& p = userPatterns[numUserPatterns];
    p.numNotes     = modNumNotes;
    for(int i = 0; i < modNumNotes; i++)
    {
        p.noteOrder[i]  = modNoteOrder[i];
        p.noteLength[i] = modNoteLength[i];
    }
    for(int i = modNumNotes; i < kSeqLength; i++)
    {
        p.noteOrder[i]  = 0;
        p.noteLength[i] = 1;
    }
    CopyCv(p.cv, remapped);

    numUserPatterns++;
    selectedPattern = numUserPatterns;
    playPattern     = selectedPattern;
    pendingPattern  = -1;
    pendingSwitch   = false;
    ClearPlaybackHold();
    RebuildModifiedSequenceForSlot(selectedPattern);
    return true;
}

bool DeleteSelectedPattern()
{
    if(selectedPattern <= 0 || selectedPattern > numUserPatterns)
    {
        return false;
    }

    int userIdx = selectedPattern - 1;
    for(int i = userIdx; i < numUserPatterns - 1; i++)
    {
        userPatterns[i] = userPatterns[i + 1];
    }
    numUserPatterns--;

    if(selectedPattern > numUserPatterns)
    {
        selectedPattern = numUserPatterns;
    }
    if(playPattern > userIdx + 1)
    {
        playPattern--;
    }
    else if(playPattern == userIdx + 1)
    {
        playPattern = selectedPattern;
    }
    if(playPattern > numUserPatterns)
    {
        playPattern = numUserPatterns;
    }

    pendingPattern = -1;
    pendingSwitch  = false;
    ClearPlaybackHold();
    RebuildModifiedSequenceForSlot(selectedPattern);
    return true;
}

void MoveUserPattern(int fromUserIndex, int toUserIndex)
{
    if(fromUserIndex < 0 || fromUserIndex >= numUserPatterns
       || toUserIndex < 0 || toUserIndex >= numUserPatterns
       || fromUserIndex == toUserIndex)
    {
        return;
    }

    UserPattern tmp = userPatterns[fromUserIndex];
    if(fromUserIndex < toUserIndex)
    {
        for(int i = fromUserIndex; i < toUserIndex; i++)
        {
            userPatterns[i] = userPatterns[i + 1];
        }
    }
    else
    {
        for(int i = fromUserIndex; i > toUserIndex; i--)
        {
            userPatterns[i] = userPatterns[i - 1];
        }
    }
    userPatterns[toUserIndex] = tmp;

    int oldSelectedUser = selectedPattern > 0 ? selectedPattern - 1 : -1;
    int oldPlayUser     = playPattern > 0 ? playPattern - 1 : -1;
    if(oldSelectedUser >= 0)
    {
        selectedPattern = MapUserIndexAfterMove(
                              oldSelectedUser, fromUserIndex, toUserIndex)
                          + 1;
    }
    if(oldPlayUser >= 0)
    {
        playPattern
            = MapUserIndexAfterMove(oldPlayUser, fromUserIndex, toUserIndex)
              + 1;
    }

    RebuildModifiedSequenceForSlot(selectedPattern);
}

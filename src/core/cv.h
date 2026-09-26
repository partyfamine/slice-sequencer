#pragma once
#include "types.h"

extern CvChannel cvChannels[kNumCvControls];

void InitCvChannels();
void ClampCvPositions();
void GetSegmentRange(int position, int size, int* startBeat, int* endBeat);
bool NoteIntersectsSegment(int note, int startBeat, int endBeat);

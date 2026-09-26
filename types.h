#pragma once
#include "daisy_patch.h"
#include <stdint.h>

extern daisy::DaisyPatch patch;

static const int  kSeqLength     = 16;
static const int  kMinSteps      = 2;
static const int  kFontWidth     = 7;
static const int  kNumCvControls = 3;
static const char kHex[]         = "0123456789ABCDEF";

enum CvType
{
    CV_TYPE_SHIFT = 0,
    CV_TYPE_TRANSPOSE,
    CV_TYPE_REPEAT,
    CV_TYPE_DISABLED,
    CV_TYPE_LAST,
};

enum SliceOutMode
{
    SLICE_OUT_NOTE = 0,
    SLICE_OUT_STEP,
    SLICE_OUT_LAST,
};

enum NoteHit : uint8_t
{
    HIT_NONE  = 0,
    HIT_KICK  = 1,
    HIT_SNARE = 2,
};

struct CvChannel
{
    CvType type;
    int    position; // note index into the original sequence
    int    size;     // segment length in steps (1-8)
};

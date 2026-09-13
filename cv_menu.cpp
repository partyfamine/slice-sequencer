#include "cv_menu.h"
#include "app.h"
#include "oled_ui.h"

enum CvCursor
{
    CV_CURSOR_TYPE = 0,
    CV_CURSOR_POSITION,
    CV_CURSOR_SIZE,
    CV_CURSOR_BACK,
};

struct CvMenuState
{
    CvCursor cursor;
    bool     editing;
    CvType   draftType;
    int      draftPosition;
    int      draftSize;
};

static CvMenuState menus[4];
static int         currentCv;

static const char* TypeName(CvType t)
{
    switch(t)
    {
        case CV_TYPE_TRANSPOSE: return "Transpose";
        case CV_TYPE_REPEAT: return "Repeat";
        case CV_TYPE_SHIFT:
        default: return "Shift";
    }
}

static void LoadDraft(int idx)
{
    menus[idx].draftType     = cvChannels[idx].type;
    menus[idx].draftPosition = cvChannels[idx].position;
    menus[idx].draftSize     = cvChannels[idx].size;
}

static void SaveDraft(int idx)
{
    cvChannels[idx].type     = menus[idx].draftType;
    cvChannels[idx].position = menus[idx].draftPosition;
    cvChannels[idx].size     = menus[idx].draftSize;
    ClampCvPositions();
    menus[idx].draftPosition = cvChannels[idx].position;
    RebuildModifiedSequence();
}

static void MoveCursor(int dir)
{
    int c = (int)menus[currentCv].cursor + dir;
    if(c < CV_CURSOR_TYPE)
    {
        c = CV_CURSOR_BACK;
    }
    if(c > CV_CURSOR_BACK)
    {
        c = CV_CURSOR_TYPE;
    }
    menus[currentCv].cursor = (CvCursor)c;
}

static void EditValue(int inc)
{
    CvMenuState& m = menus[currentCv];
    if(m.cursor == CV_CURSOR_TYPE)
    {
        int t = (int)m.draftType + inc;
        t     = (t % CV_TYPE_LAST + CV_TYPE_LAST) % CV_TYPE_LAST;
        m.draftType = (CvType)t;
        return;
    }
    if(m.cursor == CV_CURSOR_POSITION)
    {
        if(numNotes <= 0)
        {
            return;
        }
        m.draftPosition += inc;
        m.draftPosition
            = (m.draftPosition % numNotes + numNotes) % numNotes;
        return;
    }
    if(m.cursor == CV_CURSOR_SIZE)
    {
        m.draftSize += inc;
        if(m.draftSize < 1)
        {
            m.draftSize = 1;
        }
        if(m.draftSize > 8)
        {
            m.draftSize = 8;
        }
    }
}

void CvMenuInit()
{
    currentCv = 0;
    for(int i = 0; i < 4; i++)
    {
        menus[i].cursor  = CV_CURSOR_TYPE;
        menus[i].editing = false;
        LoadDraft(i);
    }
}

void CvMenuEnter(int cvIndex)
{
    if(cvIndex < 0)
    {
        cvIndex = 0;
    }
    if(cvIndex > 3)
    {
        cvIndex = 3;
    }
    currentCv                = cvIndex;
    activeCvIndex            = cvIndex;
    menus[currentCv].cursor  = CV_CURSOR_TYPE;
    menus[currentCv].editing = false;
    LoadDraft(currentCv);
    ClampCvPositions();
    LoadDraft(currentCv);
}

void CvMenuProcessEncoder()
{
    CvMenuState& m = menus[currentCv];

    if(m.editing)
    {
        int inc = patch.encoder.Increment();
        if(inc != 0)
        {
            EditValue(inc);
        }
        if(patch.encoder.RisingEdge())
        {
            SaveDraft(currentCv);
            m.editing = false;
        }
        return;
    }

    int inc = patch.encoder.Increment();
    if(inc > 0)
    {
        for(int i = 0; i < inc; i++)
        {
            MoveCursor(1);
        }
    }
    else if(inc < 0)
    {
        for(int i = 0; i < -inc; i++)
        {
            MoveCursor(-1);
        }
    }

    if(patch.encoder.RisingEdge())
    {
        if(m.cursor == CV_CURSOR_BACK)
        {
            uiScreen = UI_MAIN_MENU;
            return;
        }
        LoadDraft(currentCv);
        m.editing = true;
    }
}

void CvMenuDraw()
{
    CvMenuState& m = menus[currentCv];

    CvType type     = m.editing ? m.draftType : cvChannels[currentCv].type;
    int    position = m.editing ? m.draftPosition : cvChannels[currentCv].position;
    int    size     = m.editing ? m.draftSize : cvChannels[currentCv].size;

    bool selType = m.cursor == CV_CURSOR_TYPE && !m.editing;
    bool selPos  = m.cursor == CV_CURSOR_POSITION && !m.editing;
    bool selSize = m.cursor == CV_CURSOR_SIZE && !m.editing;

    DrawChars(0, 0, "Type", !selType);
    DrawChars(4 * kFontWidth, 0, ": ", true);
    DrawChars(6 * kFontWidth,
              0,
              TypeName(type),
              !(m.editing && m.cursor == CV_CURSOR_TYPE));

    DrawChars(0, 10, "Position", !selPos);
    DrawChars(8 * kFontWidth, 10, ": ", true);
    char posStr[2] = {kHex[position < numNotes ? position : 0], '\0'};
    DrawChars(10 * kFontWidth,
              10,
              posStr,
              !(m.editing && m.cursor == CV_CURSOR_POSITION));

    DrawChars(0, 20, "Size", !selSize);
    DrawChars(4 * kFontWidth, 20, ": ", true);
    char sizeStr[2] = {(char)('0' + size), '\0'};
    DrawChars(6 * kFontWidth,
              20,
              sizeStr,
              !(m.editing && m.cursor == CV_CURSOR_SIZE));

    char seq[kSeqLength + 2];
    BuildSequenceString(seq);
    int startBeat, endBeat;
    GetSegmentRange(position, size, &startBeat, &endBeat);
    DrawSequenceStringMasked(40, seq, startBeat, endBeat, false);

    bool selBack = m.cursor == CV_CURSOR_BACK;
    DrawChars(0, 50, "Back", !selBack);
}

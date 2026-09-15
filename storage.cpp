#include "storage.h"
#include "app.h"
#include "sys/fatfs.h"
#include "ff.h"
#include <cstring>
#include <cstdio>

using namespace daisy;

char sequenceName[kMaxSeqNameLen + 1];

static SdmmcHandler   sdmmc;
static FatFSInterface fsi;
static bool           storageReady;

static char savedNames[kMaxSavedSeqs][kMaxSeqNameLen + 1];
static int  savedCount;

#pragma pack(push, 1)
struct SeqFileData
{
    char    magic[4];
    uint8_t version;
    uint8_t totalSteps;
    uint8_t numNotes;
    uint8_t reserved;
    uint8_t lengths[kSeqLength];
    struct
    {
        uint8_t type;
        uint8_t position;
        uint8_t size;
    } cv[4];
};
#pragma pack(pop)

static const char* kSeqDir = "seq";

static bool IsValidNameChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static bool IsValidName(const char* name)
{
    if(name == nullptr || name[0] == '\0')
    {
        return false;
    }
    int len = 0;
    while(name[len] != '\0')
    {
        if(len >= kMaxSeqNameLen || !IsValidNameChar(name[len]))
        {
            return false;
        }
        len++;
    }
    return true;
}

static void BuildDirPath(char* path, size_t pathSize)
{
    snprintf(path, pathSize, "%s%s", fsi.GetSDPath(), kSeqDir);
}

static void BuildPath(char* path, size_t pathSize, const char* name)
{
    snprintf(path, pathSize, "%s%s/%s.seq", fsi.GetSDPath(), kSeqDir, name);
}

static void SortSavedNames()
{
    for(int i = 0; i < savedCount; i++)
    {
        for(int j = i + 1; j < savedCount; j++)
        {
            if(strcmp(savedNames[j], savedNames[i]) < 0)
            {
                char tmp[kMaxSeqNameLen + 1];
                strcpy(tmp, savedNames[i]);
                strcpy(savedNames[i], savedNames[j]);
                strcpy(savedNames[j], tmp);
            }
        }
    }
}

void ClearSequenceName()
{
    sequenceName[0] = '\0';
}

void SetSequenceName(const char* name)
{
    if(!IsValidName(name))
    {
        ClearSequenceName();
        return;
    }
    strncpy(sequenceName, name, kMaxSeqNameLen);
    sequenceName[kMaxSeqNameLen] = '\0';
}

bool HasSequenceName()
{
    return sequenceName[0] != '\0';
}

bool InitStorage()
{
    storageReady = false;
    ClearSequenceName();
    savedCount = 0;

    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SdmmcHandler::Speed::STANDARD;
    sdmmc.Init(sd_cfg);

    if(fsi.Init(FatFSInterface::Config::MEDIA_SD) != FatFSInterface::Result::OK)
    {
        return false;
    }

    if(f_mount(&fsi.GetSDFileSystem(), fsi.GetSDPath(), 1) != FR_OK)
    {
        return false;
    }

    char dirPath[32];
    BuildDirPath(dirPath, sizeof(dirPath));
    f_mkdir(dirPath);
    storageReady = true;
    RefreshSavedSequenceList();
    return true;
}

bool StorageReady()
{
    return storageReady;
}

void RefreshSavedSequenceList()
{
    savedCount = 0;
    if(!storageReady)
    {
        return;
    }

    DIR     dir;
    FILINFO info;
    char    dirPath[32];
    BuildDirPath(dirPath, sizeof(dirPath));
    if(f_opendir(&dir, dirPath) != FR_OK)
    {
        return;
    }

    while(savedCount < kMaxSavedSeqs && f_readdir(&dir, &info) == FR_OK)
    {
        if(info.fname[0] == '\0')
        {
            break;
        }
        if(info.fattrib & AM_DIR)
        {
            continue;
        }

        // Expect "<name>.seq"
        const char* fname = info.fname;
        int         len   = (int)strlen(fname);
        if(len < 5)
        {
            continue;
        }
        if(strcmp(fname + len - 4, ".seq") != 0
           && strcmp(fname + len - 4, ".SEQ") != 0)
        {
            continue;
        }

        int nameLen = len - 4;
        if(nameLen <= 0 || nameLen > kMaxSeqNameLen)
        {
            continue;
        }

        bool ok = true;
        for(int i = 0; i < nameLen; i++)
        {
            char c = fname[i];
            if(c >= 'A' && c <= 'Z')
            {
                c = (char)(c - 'A' + 'a');
            }
            if(!IsValidNameChar(c))
            {
                ok = false;
                break;
            }
            savedNames[savedCount][i] = c;
        }
        if(!ok)
        {
            continue;
        }
        savedNames[savedCount][nameLen] = '\0';
        savedCount++;
    }

    f_closedir(&dir);
    SortSavedNames();
}

int GetSavedSequenceCount()
{
    return savedCount;
}

const char* GetSavedSequenceName(int index)
{
    if(index < 0 || index >= savedCount)
    {
        return "";
    }
    return savedNames[index];
}

int ListSavedSequences(char names[][kMaxSeqNameLen + 1], int maxNames)
{
    RefreshSavedSequenceList();
    int n = savedCount;
    if(n > maxNames)
    {
        n = maxNames;
    }
    for(int i = 0; i < n; i++)
    {
        strcpy(names[i], savedNames[i]);
    }
    return n;
}

bool SaveSequence(const char* name)
{
    if(!storageReady || !IsValidName(name))
    {
        return false;
    }

    char dirPath[32];
    BuildDirPath(dirPath, sizeof(dirPath));
    f_mkdir(dirPath);

    char path[64];
    BuildPath(path, sizeof(path), name);

    SeqFileData data;
    memset(&data, 0, sizeof(data));
    data.magic[0]    = 'S';
    data.magic[1]    = 'L';
    data.magic[2]    = 'S';
    data.magic[3]    = 'Q';
    data.version     = 1;
    data.totalSteps  = (uint8_t)totalSteps;
    data.numNotes    = (uint8_t)numNotes;
    data.reserved    = 0;
    for(int i = 0; i < kSeqLength; i++)
    {
        data.lengths[i] = (i < numNotes) ? (uint8_t)noteLength[i] : 0;
    }
    for(int c = 0; c < 4; c++)
    {
        data.cv[c].type     = (uint8_t)cvChannels[c].type;
        data.cv[c].position = (uint8_t)cvChannels[c].position;
        data.cv[c].size     = (uint8_t)cvChannels[c].size;
    }

    FIL     file;
    FRESULT openRes
        = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    if(openRes != FR_OK)
    {
        return false;
    }

    UINT    written = 0;
    FRESULT writeRes = f_write(&file, &data, sizeof(data), &written);
    f_close(&file);

    if(writeRes != FR_OK || written != sizeof(data))
    {
        return false;
    }

    SetSequenceName(name);
    RefreshSavedSequenceList();
    return true;
}

bool LoadSequence(const char* name)
{
    if(!storageReady || !IsValidName(name))
    {
        return false;
    }

    char path[64];
    BuildPath(path, sizeof(path), name);

    FIL file;
    if(f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }

    SeqFileData data;
    UINT        read = 0;
    FRESULT     res  = f_read(&file, &data, sizeof(data), &read);
    f_close(&file);

    if(res != FR_OK || read != sizeof(data))
    {
        return false;
    }
    if(data.magic[0] != 'S' || data.magic[1] != 'L' || data.magic[2] != 'S'
       || data.magic[3] != 'Q' || data.version != 1)
    {
        return false;
    }
    if(data.totalSteps < kMinSteps || data.totalSteps > kSeqLength
       || data.numNotes < 1 || data.numNotes > kSeqLength)
    {
        return false;
    }

    int beatSum = 0;
    for(int i = 0; i < data.numNotes; i++)
    {
        if(data.lengths[i] < 1)
        {
            return false;
        }
        beatSum += data.lengths[i];
    }
    if(beatSum != data.totalSteps)
    {
        return false;
    }

    totalSteps = data.totalSteps;
    numNotes   = data.numNotes;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = (i < numNotes) ? data.lengths[i] : 1;
    }
    for(int c = 0; c < 4; c++)
    {
        if(data.cv[c].type >= CV_TYPE_LAST)
        {
            cvChannels[c].type = CV_TYPE_SHIFT;
        }
        else
        {
            cvChannels[c].type = (CvType)data.cv[c].type;
        }
        cvChannels[c].position = data.cv[c].position;
        cvChannels[c].size     = data.cv[c].size;
        if(cvChannels[c].size < 1)
        {
            cvChannels[c].size = 1;
        }
        if(cvChannels[c].size > 8)
        {
            cvChannels[c].size = 8;
        }
    }

    ClampCvPositions();
    stepNumber = 0;
    SetSequenceName(name);
    RebuildModifiedSequence();
    RefreshCvsFromCurrentStep();
    return true;
}

void ResetToNewSequence()
{
    ClearSequenceName();
    InitSequence();
    stepNumber = 0;
    RefreshCvsFromCurrentStep();
}

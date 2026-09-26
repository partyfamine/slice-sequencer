#include "storage.h"
#include "app.h"
#include "patterns.h"
#include "sys/fatfs.h"
#include "ff.h"
#include <cstring>
#include <cstdio>

using namespace daisy;

char sequenceName[kMaxSeqNameLen + 1];

static SdmmcHandler   sdmmc;
static FatFSInterface fsi;
static FIL            file;
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
    } cv[4]; // v1/v2 used 4; v3 uses first 3 for original sequence
    uint8_t hits[kSeqLength]; // version >= 2
};

struct PatternFileEntry
{
    uint8_t numNotes;
    uint8_t pad;
    uint8_t order[kSeqLength];
    uint8_t lengths[kSeqLength];
    struct
    {
        uint8_t type;
        uint8_t position;
        uint8_t size;
    } cv[kNumCvControls];
};
#pragma pack(pop)

static SeqFileData fileData;

static const char* kSeqDir      = "seq";
static const char* kLastPath    = "seq/last.txt";
static const char* kSettingsPath = "seq/settings.bin";

#pragma pack(push, 1)
struct GlobalSettingsFile
{
    char    magic[4];
    uint8_t version;
    uint8_t sliceOut;
    uint8_t reserved[2];
};
#pragma pack(pop)

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

static void BuildPath(char* path, size_t pathSize, const char* name)
{
    snprintf(path, pathSize, "%s/%s.seq", kSeqDir, name);
}

static bool RememberLastSequence(const char* name)
{
    if(!storageReady || !IsValidName(name))
    {
        return false;
    }

    f_mkdir(kSeqDir);
    if(f_open(&file, kLastPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return false;
    }

    UINT    bytes_written = 0;
    size_t  len           = strlen(name);
    FRESULT write_res     = f_write(&file, name, len, &bytes_written);
    FRESULT close_res     = f_close(&file);
    return write_res == FR_OK && close_res == FR_OK && bytes_written == len;
}

static void ClearLastSequence()
{
    if(!storageReady)
    {
        return;
    }
    f_unlink(kLastPath);
}

static bool ReadLastSequenceName(char* nameOut, size_t nameOutSize)
{
    if(!storageReady || nameOut == nullptr || nameOutSize == 0)
    {
        return false;
    }

    if(f_open(&file, kLastPath, FA_READ) != FR_OK)
    {
        return false;
    }

    char buf[kMaxSeqNameLen + 1];
    memset(buf, 0, sizeof(buf));
    UINT    bytes_read = 0;
    FRESULT read_res   = f_read(&file, buf, kMaxSeqNameLen, &bytes_read);
    f_close(&file);

    if(read_res != FR_OK || bytes_read == 0)
    {
        return false;
    }
    buf[bytes_read] = '\0';

    while(bytes_read > 0
          && (buf[bytes_read - 1] == '\n' || buf[bytes_read - 1] == '\r'
              || buf[bytes_read - 1] == ' '))
    {
        buf[--bytes_read] = '\0';
    }

    if(!IsValidName(buf))
    {
        return false;
    }

    strncpy(nameOut, buf, nameOutSize - 1);
    nameOut[nameOutSize - 1] = '\0';
    return true;
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

static void LoadCvChannel(CvChannel* dst, uint8_t type, uint8_t position, uint8_t size)
{
    if(type >= CV_TYPE_LAST)
    {
        dst->type = CV_TYPE_DISABLED;
    }
    else
    {
        dst->type = (CvType)type;
    }
    dst->position = position;
    dst->size     = size;
    if(dst->size < 1)
    {
        dst->size = 1;
    }
    if(dst->size > 8)
    {
        dst->size = 8;
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

    FatFSInterface::Config fsi_config;
    fsi_config.media = FatFSInterface::Config::MEDIA_SD;
    if(fsi.Init(fsi_config) != FatFSInterface::Result::OK)
    {
        return false;
    }

    FATFS& fs = fsi.GetSDFileSystem();
    if(f_mount(&fs, "/", 1) != FR_OK)
    {
        return false;
    }

    f_mkdir(kSeqDir);
    storageReady = true;
    LoadGlobalSettings();
    RefreshSavedSequenceList();
    return true;
}

bool StorageReady()
{
    return storageReady;
}

bool LoadGlobalSettings()
{
    sliceOutMode = SLICE_OUT_NOTE;
    if(!storageReady)
    {
        return false;
    }

    if(f_open(&file, kSettingsPath, FA_READ) != FR_OK)
    {
        return false;
    }

    GlobalSettingsFile data;
    memset(&data, 0, sizeof(data));
    UINT    bytes_read = 0;
    FRESULT read_res
        = f_read(&file, &data, sizeof(data), &bytes_read);
    f_close(&file);

    if(read_res != FR_OK || bytes_read < sizeof(data))
    {
        return false;
    }
    if(data.magic[0] != 'S' || data.magic[1] != 'L'
       || data.magic[2] != 'S' || data.magic[3] != 'T')
    {
        return false;
    }
    if(data.version != 1)
    {
        return false;
    }
    if(data.sliceOut >= SLICE_OUT_LAST)
    {
        sliceOutMode = SLICE_OUT_NOTE;
    }
    else
    {
        sliceOutMode = (SliceOutMode)data.sliceOut;
    }
    return true;
}

bool SaveGlobalSettings()
{
    if(!storageReady)
    {
        return false;
    }

    f_mkdir(kSeqDir);

    GlobalSettingsFile data;
    memset(&data, 0, sizeof(data));
    data.magic[0]  = 'S';
    data.magic[1]  = 'L';
    data.magic[2]  = 'S';
    data.magic[3]  = 'T';
    data.version   = 1;
    data.sliceOut  = (uint8_t)sliceOutMode;

    if(f_open(&file, kSettingsPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return false;
    }

    UINT    bytes_written = 0;
    FRESULT write_res
        = f_write(&file, &data, sizeof(data), &bytes_written);
    FRESULT close_res = f_close(&file);
    return write_res == FR_OK && close_res == FR_OK
           && bytes_written == sizeof(data);
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
    if(f_opendir(&dir, kSeqDir) != FR_OK)
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

    StoreWorkingCvToSlot(selectedPattern);

    f_mkdir(kSeqDir);

    char path[64];
    BuildPath(path, sizeof(path), name);

    memset(&fileData, 0, sizeof(fileData));
    fileData.magic[0]   = 'S';
    fileData.magic[1]   = 'L';
    fileData.magic[2]   = 'S';
    fileData.magic[3]   = 'Q';
    fileData.version    = 3;
    fileData.totalSteps = (uint8_t)totalSteps;
    fileData.numNotes   = (uint8_t)numNotes;
    fileData.reserved   = 0;
    for(int i = 0; i < kSeqLength; i++)
    {
        fileData.lengths[i] = (i < numNotes) ? (uint8_t)noteLength[i] : 0;
        fileData.hits[i]    = (i < numNotes) ? noteHit[i] : HIT_NONE;
    }
    for(int c = 0; c < kNumCvControls; c++)
    {
        fileData.cv[c].type     = (uint8_t)originalCv[c].type;
        fileData.cv[c].position = (uint8_t)originalCv[c].position;
        fileData.cv[c].size     = (uint8_t)originalCv[c].size;
    }
    fileData.cv[3].type     = 0;
    fileData.cv[3].position = 0;
    fileData.cv[3].size     = 1;

    if(f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return false;
    }

    UINT    bytes_written = 0;
    FRESULT write_res
        = f_write(&file, &fileData, sizeof(fileData), &bytes_written);
    if(write_res != FR_OK || bytes_written != sizeof(fileData))
    {
        f_close(&file);
        return false;
    }

    uint8_t nPatterns = (uint8_t)numUserPatterns;
    bytes_written     = 0;
    write_res = f_write(&file, &nPatterns, 1, &bytes_written);
    if(write_res != FR_OK || bytes_written != 1)
    {
        f_close(&file);
        return false;
    }

    for(int p = 0; p < numUserPatterns; p++)
    {
        PatternFileEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.numNotes = (uint8_t)userPatterns[p].numNotes;
        for(int i = 0; i < kSeqLength; i++)
        {
            entry.order[i]
                = (i < userPatterns[p].numNotes)
                      ? (uint8_t)userPatterns[p].noteOrder[i]
                      : 0;
            entry.lengths[i]
                = (i < userPatterns[p].numNotes)
                      ? (uint8_t)userPatterns[p].noteLength[i]
                      : 0;
        }
        for(int c = 0; c < kNumCvControls; c++)
        {
            entry.cv[c].type     = (uint8_t)userPatterns[p].cv[c].type;
            entry.cv[c].position = (uint8_t)userPatterns[p].cv[c].position;
            entry.cv[c].size     = (uint8_t)userPatterns[p].cv[c].size;
        }

        bytes_written = 0;
        write_res = f_write(&file, &entry, sizeof(entry), &bytes_written);
        if(write_res != FR_OK || bytes_written != sizeof(entry))
        {
            f_close(&file);
            return false;
        }
    }

    FRESULT close_res = f_close(&file);
    if(close_res != FR_OK)
    {
        return false;
    }

    SetSequenceName(name);
    RememberLastSequence(name);
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

    if(f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }

    UINT    bytes_read = 0;
    FRESULT read_res
        = f_read(&file, &fileData, sizeof(fileData), &bytes_read);

    const UINT kV1Size = sizeof(fileData) - sizeof(fileData.hits);
    bool       ok      = read_res == FR_OK && bytes_read >= kV1Size;
    if(ok
       && (fileData.magic[0] != 'S' || fileData.magic[1] != 'L'
           || fileData.magic[2] != 'S' || fileData.magic[3] != 'Q'))
    {
        ok = false;
    }

    uint8_t version = fileData.version;
    if(ok && version == 1)
    {
        memset(fileData.hits, HIT_NONE, sizeof(fileData.hits));
    }
    else if(ok && (version == 2 || version == 3))
    {
        if(bytes_read < sizeof(fileData))
        {
            ok = false;
        }
    }
    else if(ok)
    {
        ok = false;
    }

    if(ok
       && (fileData.totalSteps < kMinSteps || fileData.totalSteps > kSeqLength
           || fileData.numNotes < 1 || fileData.numNotes > kSeqLength))
    {
        ok = false;
    }

    int beatSum = 0;
    if(ok)
    {
        for(int i = 0; i < fileData.numNotes; i++)
        {
            if(fileData.lengths[i] < 1)
            {
                ok = false;
                break;
            }
            beatSum += fileData.lengths[i];
        }
        if(ok && beatSum != fileData.totalSteps)
        {
            ok = false;
        }
    }

    if(!ok)
    {
        f_close(&file);
        return false;
    }

    totalSteps = fileData.totalSteps;
    numNotes   = fileData.numNotes;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = (i < numNotes) ? fileData.lengths[i] : 1;
        uint8_t hit   = (i < numNotes) ? fileData.hits[i] : HIT_NONE;
        if(hit > HIT_SNARE)
        {
            hit = HIT_NONE;
        }
        noteHit[i] = hit;
    }

    InitPatterns();
    for(int c = 0; c < kNumCvControls; c++)
    {
        LoadCvChannel(&originalCv[c],
                      fileData.cv[c].type,
                      fileData.cv[c].position,
                      fileData.cv[c].size);
    }

    if(version == 3)
    {
        uint8_t nPatterns = 0;
        bytes_read        = 0;
        read_res          = f_read(&file, &nPatterns, 1, &bytes_read);
        if(read_res != FR_OK || bytes_read != 1 || nPatterns > kMaxUserPatterns)
        {
            f_close(&file);
            return false;
        }

        numUserPatterns = nPatterns;
        for(int p = 0; p < numUserPatterns; p++)
        {
            PatternFileEntry entry;
            bytes_read = 0;
            read_res = f_read(&file, &entry, sizeof(entry), &bytes_read);
            if(read_res != FR_OK || bytes_read != sizeof(entry)
               || entry.numNotes < 1 || entry.numNotes > kSeqLength)
            {
                f_close(&file);
                InitPatterns();
                return false;
            }

            int pBeats = 0;
            for(int i = 0; i < entry.numNotes; i++)
            {
                if(entry.lengths[i] < 1
                   || entry.order[i] >= (uint8_t)numNotes)
                {
                    f_close(&file);
                    InitPatterns();
                    return false;
                }
                pBeats += entry.lengths[i];
            }
            if(pBeats != totalSteps)
            {
                f_close(&file);
                InitPatterns();
                return false;
            }

            userPatterns[p].numNotes = entry.numNotes;
            for(int i = 0; i < kSeqLength; i++)
            {
                userPatterns[p].noteOrder[i]
                    = (i < entry.numNotes) ? entry.order[i] : 0;
                userPatterns[p].noteLength[i]
                    = (i < entry.numNotes) ? entry.lengths[i] : 1;
            }
            for(int c = 0; c < kNumCvControls; c++)
            {
                LoadCvChannel(&userPatterns[p].cv[c],
                              entry.cv[c].type,
                              entry.cv[c].position,
                              entry.cv[c].size);
            }
        }
    }

    f_close(&file);

    selectedPattern = 0;
    playPattern     = 0;
    ClearPlaybackHold();
    SyncWorkingCvFromSlot(0);
    ClampCvPositions();
    stepNumber = 0;
    SetSequenceName(name);
    RememberLastSequence(name);
    RebuildModifiedSequenceForSlot(0);
    RefreshCvsFromCurrentStep();
    return true;
}

bool LoadLastSequence()
{
    char name[kMaxSeqNameLen + 1];
    if(!ReadLastSequenceName(name, sizeof(name)))
    {
        return false;
    }
    return LoadSequence(name);
}

void ResetToNewSequence()
{
    ClearLastSequence();
    ClearSequenceName();
    InitSequence();
    InitPatterns();
    stepNumber = 0;
    RefreshCvsFromCurrentStep();
}

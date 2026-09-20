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
    } cv[4];
};
#pragma pack(pop)

static SeqFileData fileData;

static const char* kSeqDir   = "seq";
static const char* kLastPath = "seq/last.txt";

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

    f_mkdir(kSeqDir);

    char path[64];
    BuildPath(path, sizeof(path), name);

    memset(&fileData, 0, sizeof(fileData));
    fileData.magic[0]   = 'S';
    fileData.magic[1]   = 'L';
    fileData.magic[2]   = 'S';
    fileData.magic[3]   = 'Q';
    fileData.version    = 1;
    fileData.totalSteps = (uint8_t)totalSteps;
    fileData.numNotes   = (uint8_t)numNotes;
    fileData.reserved   = 0;
    for(int i = 0; i < kSeqLength; i++)
    {
        fileData.lengths[i] = (i < numNotes) ? (uint8_t)noteLength[i] : 0;
    }
    for(int c = 0; c < 4; c++)
    {
        fileData.cv[c].type     = (uint8_t)cvChannels[c].type;
        fileData.cv[c].position = (uint8_t)cvChannels[c].position;
        fileData.cv[c].size     = (uint8_t)cvChannels[c].size;
    }

    if(f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return false;
    }

    UINT    bytes_written = 0;
    FRESULT write_res
        = f_write(&file, &fileData, sizeof(fileData), &bytes_written);
    FRESULT close_res = f_close(&file);

    if(write_res != FR_OK || close_res != FR_OK
       || bytes_written != sizeof(fileData))
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
    f_close(&file);

    if(read_res != FR_OK || bytes_read != sizeof(fileData))
    {
        return false;
    }
    if(fileData.magic[0] != 'S' || fileData.magic[1] != 'L'
       || fileData.magic[2] != 'S' || fileData.magic[3] != 'Q'
       || fileData.version != 1)
    {
        return false;
    }
    if(fileData.totalSteps < kMinSteps || fileData.totalSteps > kSeqLength
       || fileData.numNotes < 1 || fileData.numNotes > kSeqLength)
    {
        return false;
    }

    int beatSum = 0;
    for(int i = 0; i < fileData.numNotes; i++)
    {
        if(fileData.lengths[i] < 1)
        {
            return false;
        }
        beatSum += fileData.lengths[i];
    }
    if(beatSum != fileData.totalSteps)
    {
        return false;
    }

    totalSteps = fileData.totalSteps;
    numNotes   = fileData.numNotes;
    for(int i = 0; i < kSeqLength; i++)
    {
        noteLength[i] = (i < numNotes) ? fileData.lengths[i] : 1;
    }
    for(int c = 0; c < 4; c++)
    {
        if(fileData.cv[c].type >= CV_TYPE_LAST)
        {
            cvChannels[c].type = CV_TYPE_SHIFT;
        }
        else
        {
            cvChannels[c].type = (CvType)fileData.cv[c].type;
        }
        cvChannels[c].position = fileData.cv[c].position;
        cvChannels[c].size     = fileData.cv[c].size;
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
    RememberLastSequence(name);
    RebuildModifiedSequence();
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
    stepNumber = 0;
    RefreshCvsFromCurrentStep();
}

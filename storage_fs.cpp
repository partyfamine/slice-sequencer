#include "storage_internal.h"
#include "app.h"
#include <cstring>
#include <cstdio>

using namespace daisy;

char sequenceName[kMaxSeqNameLen + 1];

static SdmmcHandler   sdmmc;
static FatFSInterface fsi;
FIL                   file;
bool                  storageReady;

static char savedNames[kMaxSavedSeqs][kMaxSeqNameLen + 1];
static int  savedCount;

const char* kSeqDir       = "seq";
static const char* kLastPath     = "seq/last.txt";
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

bool IsValidName(const char* name)
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

void BuildPath(char* path, size_t pathSize, const char* name)
{
    snprintf(path, pathSize, "%s/%s.seq", kSeqDir, name);
}

bool RememberLastSequence(const char* name)
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

void ClearLastSequence()
{
    if(!storageReady)
    {
        return;
    }
    f_unlink(kLastPath);
}

bool ReadLastSequenceName(char* nameOut, size_t nameOutSize)
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

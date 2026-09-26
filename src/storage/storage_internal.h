#pragma once

#include "storage.h"
#include "sys/fatfs.h"
#include "ff.h"
#include <cstddef>

// Shared between storage_fs.cpp and storage_seq.cpp (not part of the public API).

extern FIL  file;
extern bool storageReady;

extern const char* kSeqDir;

bool IsValidName(const char* name);
void BuildPath(char* path, size_t pathSize, const char* name);
bool RememberLastSequence(const char* name);
void ClearLastSequence();
bool ReadLastSequenceName(char* nameOut, size_t nameOutSize);

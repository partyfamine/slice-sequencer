#pragma once

static const int kMaxSeqNameLen  = 12;
static const int kMaxSavedSeqs   = 64;

extern char sequenceName[kMaxSeqNameLen + 1];

bool InitStorage();
bool StorageReady();
void ClearSequenceName();
void SetSequenceName(const char* name);
bool HasSequenceName();

bool SaveSequence(const char* name);
bool LoadSequence(const char* name);
bool LoadLastSequence();
void ResetToNewSequence();

int  ListSavedSequences(char names[][kMaxSeqNameLen + 1], int maxNames);
void RefreshSavedSequenceList();
int  GetSavedSequenceCount();
const char* GetSavedSequenceName(int index);

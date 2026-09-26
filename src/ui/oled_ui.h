#pragma once

void DrawChars(int x, int y, const char* str, bool on);
void DrawSequenceString(int y, const char* seq, int invertBeat, bool center);
void DrawSequenceStringMasked(int y,
                              const char* seq,
                              int         invStartBeat,
                              int         invEndBeat,
                              bool        center);
void DrawSequence(int y, int invertBeat);
void DrawCenteredSeparator(int y);
int  SequenceDrawX(bool center);

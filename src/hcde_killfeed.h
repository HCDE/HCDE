#pragma once

#include "zstring.h"

struct FHCDEKillfeedEntry
{
	FString Text;
	int Tic = 0;
};

void HCDEKillfeed_Push(const char* text, int tic);
int HCDEKillfeed_CopyRecent(FHCDEKillfeedEntry* out, int maxEntries, int nowTic, int lifetimeTics);
void HCDEKillfeed_Clear();

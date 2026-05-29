#include "hcde_killfeed.h"

namespace
{
constexpr int MaxKillfeedEntries = 6;

FHCDEKillfeedEntry Entries[MaxKillfeedEntries];
int NextEntry = 0;
int EntryCount = 0;
}

void HCDEKillfeed_Push(const char* text, int tic)
{
	if (text == nullptr || text[0] == '\0')
		return;

	Entries[NextEntry].Text = text;
	Entries[NextEntry].Tic = tic;
	NextEntry = (NextEntry + 1) % MaxKillfeedEntries;
	if (EntryCount < MaxKillfeedEntries)
	{
		++EntryCount;
	}
}

int HCDEKillfeed_CopyRecent(FHCDEKillfeedEntry* out, int maxEntries, int nowTic, int lifetimeTics)
{
	if (out == nullptr || maxEntries <= 0)
		return 0;

	int written = 0;
	for (int i = 0; i < EntryCount && written < maxEntries; ++i)
	{
		const int index = (NextEntry - 1 - i + MaxKillfeedEntries) % MaxKillfeedEntries;
		const FHCDEKillfeedEntry& entry = Entries[index];
		if (lifetimeTics > 0 && nowTic - entry.Tic > lifetimeTics)
			continue;

		out[written++] = entry;
	}
	return written;
}

void HCDEKillfeed_Clear()
{
	for (auto& entry : Entries)
	{
		entry.Text = "";
		entry.Tic = 0;
	}
	NextEntry = 0;
	EntryCount = 0;
}

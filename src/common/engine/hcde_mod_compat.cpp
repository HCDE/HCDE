/*
** hcde_mod_compat.cpp
**
** HCDE-managed compatibility resources for known gameplay mods.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#include "hcde_mod_compat.h"

#include "cmdlib.h"
#include "findfile.h"
#include "printf.h"

struct HCDEModCompatEntry
{
	const char* Label;
	const char* ResourceFile;
	const char* const* Patterns;
	unsigned int Flags;
};

static const char* const BrutalDoomRailgunPatterns[] =
{
	"brutal*.pk3",
	"brutaldoom*.pk3",
	nullptr
};

static const char* const AliensEradicationPatterns[] =
{
	"ALIENS_ERADICATION_TC*",
	"ALIENS_ERADICATION_TC*.pk3",
	nullptr
};

static const char* const AliensEradicationMapsetPatterns[] =
{
	"ERADICATION_MAPSET*",
	"ERADICATION_MAPSET*.wad",
	nullptr
};

static unsigned int ActiveCompatFlags = 0u;

static const HCDEModCompatEntry ModCompatEntries[] =
{
	// Keep mod-specific resources in one PK3 for now; split when dependencies
	// between different mods make one shared archive too fragile.
	{
		"Brutal Doom railgun server compatibility",
		"hcde_mod_compat.pk3",
		BrutalDoomRailgunPatterns,
		0u
	},
	{
		"Aliens Eradication dedicated player input compatibility",
		nullptr,
		AliensEradicationPatterns,
		HCDE_MODCOMPAT_ALIENS_PLAYER0_INPUT
	}
};

static bool HCDE_ModCompat_FileAlreadyListed(const std::vector<FileSys::ResourceName>& wadfiles, const char* file)
{
	FString targetBase = ExtractFileBase(file, true);

	for (const auto& wad : wadfiles)
	{
		if (!stricmp(wad.Name.c_str(), file))
		{
			return true;
		}

		FString wadBase = ExtractFileBase(wad.Name.c_str(), true);
		if (!stricmp(wadBase.GetChars(), targetBase.GetChars()))
		{
			return true;
		}
	}

	return false;
}

static bool HCDE_ModCompat_FileMatchesPattern(const FileSys::ResourceName& wad, const char* pattern)
{
	FString base = ExtractFileBase(wad.Name.c_str(), true);
	return CheckWildcards(pattern, base.GetChars()) || CheckWildcards(pattern, wad.Name.c_str());
}

static bool HCDE_ModCompat_EntryMatches(const std::vector<FileSys::ResourceName>& pwads, const HCDEModCompatEntry& entry)
{
	for (const auto& wad : pwads)
	{
		for (const char* const* pattern = entry.Patterns; *pattern != nullptr; ++pattern)
		{
			if (HCDE_ModCompat_FileMatchesPattern(wad, *pattern))
			{
				return true;
			}
		}
	}

	return false;
}

static int HCDE_ModCompat_FindFirstMatch(const std::vector<FileSys::ResourceName>& pwads, const char* const* patterns)
{
	for (size_t i = 0; i < pwads.size(); ++i)
	{
		for (const char* const* pattern = patterns; *pattern != nullptr; ++pattern)
		{
			if (HCDE_ModCompat_FileMatchesPattern(pwads[i], *pattern))
			{
				return static_cast<int>(i);
			}
		}
	}

	return -1;
}

static void HCDE_ModCompat_NormalizeAliensEradicationOrder(std::vector<FileSys::ResourceName>& pwads)
{
	const int mapsetIndex = HCDE_ModCompat_FindFirstMatch(pwads, AliensEradicationMapsetPatterns);
	if (mapsetIndex < 0)
	{
		return;
	}

	const int tcIndex = HCDE_ModCompat_FindFirstMatch(pwads, AliensEradicationPatterns);
	if (tcIndex < 0)
	{
		Printf("HCDE: Aliens Eradication mapset detected without ALIENS_ERADICATION_TC; load the TC before the mapset.\n");
		return;
	}

	if (mapsetIndex < tcIndex)
	{
		// The mapset replaces actors defined by the TC. Some launchers preserve UI order,
		// others sort paths; normalize here so the known pair parses in dedicated servers.
		FileSys::ResourceName tc = pwads[tcIndex];
		pwads.erase(pwads.begin() + tcIndex);
		pwads.insert(pwads.begin() + mapsetIndex, tc);
		Printf("HCDE: reordered Aliens Eradication add-ons so the TC loads before the mapset.\n");
	}
}

void HCDE_ModCompat_AppendFiles(std::vector<FileSys::ResourceName>& pwads, FConfigFile* config)
{
	ActiveCompatFlags = 0u;

	if (pwads.empty())
	{
		return;
	}

	HCDE_ModCompat_NormalizeAliensEradicationOrder(pwads);

	for (const auto& entry : ModCompatEntries)
	{
		if (!HCDE_ModCompat_EntryMatches(pwads, entry))
		{
			continue;
		}

		if (entry.Flags != 0u)
		{
			ActiveCompatFlags |= entry.Flags;
			Printf("HCDE: enabled mod compatibility '%s'.\n", entry.Label);
		}

		if (entry.ResourceFile == nullptr || entry.ResourceFile[0] == '\0')
		{
			continue;
		}

		const char* compatFile = BaseFileSearch(entry.ResourceFile, nullptr, true, config);
		if (compatFile == nullptr)
		{
			Printf("HCDE: mod compatibility '%s' matched, but '%s' was not found.\n", entry.Label, entry.ResourceFile);
			continue;
		}

		if (HCDE_ModCompat_FileAlreadyListed(pwads, compatFile))
		{
			continue;
		}

		if (D_AddFile(pwads, compatFile, true, -1, config, false))
		{
			Printf("HCDE: loaded mod compatibility '%s'.\n", entry.Label);
		}
	}
}

bool HCDE_ModCompat_IsActive(unsigned int flags)
{
	return (ActiveCompatFlags & flags) == flags;
}

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

// Keep compatibility resources discoverable even when users install them from the
// optional compat zip in separate folders, instead of the bundled base package.

struct HCDEModCompatEntry
{
	const char* Label;
	const char* ResourceFile;
	const char* StartupMapOverride;
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

static const char* const TheIslandPatterns[] =
{
	"theisland*.pk3",
	"the_island*.pk3",
	nullptr
};

static const char* const PinkValleyPatterns[] =
{
	"THE_PINK_VALLEY - ENG*",
	"the_pink_valley*",
	nullptr
};

static const char* const Armageddon2Patterns[] =
{
	"armageddon2*",
	"Armageddon2*",
	nullptr
};

static const char* const BeheadedKamikaziPatterns[] =
{
	"armageddon2*",
	"Armageddon2*",
	"BeheadedKamikazi*",
	"beheadedkamikazi*",
	nullptr
};

static const char* const SkulltagContentPatterns[] =
{
	"armageddon2*",
	"Armageddon2*",
	"skulltag_content*",
	"skulltagcontent*",
	nullptr
};

static const char* const SkulltagFlamePatchPatterns[] =
{
	"armageddon2*",
	"Armageddon2*",
	"skulltag_content*",
	"skulltagcontent*",
	"skulltag_flame_patches*",
	nullptr
};

static const char* const MonstersAndAddonsPatterns[] =
{
	"Monstersandaddons*.pk3",
	"Monsters_and_addons*.pk3",
	nullptr
};

static unsigned int ActiveCompatFlags = 0u;
static const char* ActiveStartupMapOverride = nullptr;

static const char* const HCDEModCompatSearchFolders[] =
{
	"",
	"compat",
	"compat-mods",
	"compatibility",
	"mods",
	"mod_compat",
	nullptr
};

static FString HCDE_ModCompat_NormalizedProgDir()
{
	FString base = progdir;
	FixPathSeperator(base);
	if (base.IsNotEmpty() && (base.Back() == '/' || base.Back() == '\\'))
	{
		base.DeleteLastCharacter();
	}
	return base;
}

static FString HCDE_ModCompat_ParentDir(const FString& path)
{
	FString parent = path;
	if (parent.IsEmpty())
	{
		return parent;
	}
	if (parent.Back() == '/' || parent.Back() == '\\')
	{
		parent.DeleteLastCharacter();
	}

	ptrdiff_t slash = parent.LastIndexOfAny("/\\");
	if (slash < 0)
	{
		return parent;
	}

	return parent.Left(static_cast<size_t>(slash + 1));
}

static const char* HCDE_ModCompat_ResolveCompatFile(const char* resourceFile, FConfigFile* config)
{
	if (resourceFile == nullptr || resourceFile[0] == '\0')
	{
		return nullptr;
	}

	const char* found = BaseFileSearch(resourceFile, nullptr, true, config);
	if (found != nullptr)
	{
		return found;
	}

	static FString fallback;
	fallback = "";

	const FString normalizedProgDir = HCDE_ModCompat_NormalizedProgDir();
	const FString parentProgDir = HCDE_ModCompat_ParentDir(normalizedProgDir);

	const char* const additionalRoots[] =
	{
		normalizedProgDir.GetChars(),
		parentProgDir.GetChars(),
		"",
		nullptr
	};

	for (size_t i = 0; additionalRoots[i] != nullptr; ++i)
	{
		const char* root = additionalRoots[i];
		if (root == nullptr)
		{
			continue;
		}

		for (size_t j = 0; HCDEModCompatSearchFolders[j] != nullptr; ++j)
		{
			FString basePath = root;
			if (basePath.IsNotEmpty() && basePath.Back() != '/' && basePath.Back() != '\\')
			{
				basePath << '/';
			}

			const char* folder = HCDEModCompatSearchFolders[j];
			if (*folder != '\0')
			{
				basePath << folder;
				basePath << '/';
			}

			FString candidate = basePath;
			candidate << resourceFile;
			if (DirEntryExists(candidate.GetChars()))
			{
				fallback = std::move(candidate);
				Printf("HCDE: compatibility resource '%s' resolved from fallback path '%s'.\n", resourceFile, fallback.GetChars());
				return fallback.GetChars();
			}
		}
	}

	if (!parentProgDir.IsEmpty())
	{
		FString recursiveSearchRoot = parentProgDir;
		if (recursiveSearchRoot.Back() == '/' || recursiveSearchRoot.Back() == '\\')
		{
			recursiveSearchRoot.DeleteLastCharacter();
		}

		FString recursiveMatch = RecursiveFileExists(recursiveSearchRoot, resourceFile);
		if (recursiveMatch.IsNotEmpty())
		{
			fallback = std::move(recursiveMatch);
			Printf("HCDE: compatibility resource '%s' resolved from recursive search in '%s'.\n", resourceFile, fallback.GetChars());
			return fallback.GetChars();
		}
	}

	return nullptr;
}

static const HCDEModCompatEntry ModCompatEntries[] =
{
	{
		"Brutal Doom railgun server compatibility",
		"hcde_mod_compat_combined.pk3",
		nullptr,
		BrutalDoomRailgunPatterns,
		0u
	},
	{
		"Aliens Eradication dedicated player input compatibility",
		nullptr,
		nullptr,
		AliensEradicationPatterns,
		HCDE_MODCOMPAT_ALIENS_PLAYER0_INPUT
	},
	{
		"The Island MAPINFO and sound compatibility",
		"hcde_mod_compat_combined.pk3",
		nullptr,
		TheIslandPatterns,
		HCDE_MODCOMPAT_MAPINFO_TRAILING_TEXT_COMMA
	},
	{
		"Pink Valley map compatibility",
		"hcde_mod_compat_pink_valley_eng.pk3",
		"A_NEW_DAY",
		PinkValleyPatterns,
		HCDE_MODCOMPAT_MAPINFO_SKY_SPEED_NO_COMMA
	},
	{
		"Armageddon2 compatibility",
		"hcde_mod_compat_armageddon2_test.pk3",
		nullptr,
		Armageddon2Patterns,
		0u
	},
	{
		"Beheaded Kamikazi monster pack",
		"BeheadedKamikazi.pk3",
		nullptr,
		BeheadedKamikaziPatterns,
		0u
	},
	{
		"Skulltag content resources",
		"skulltag_content-4.0.pk3",
		nullptr,
		SkulltagContentPatterns,
		0u
	},
	{
		"Skulltag flame patch sprites",
		"skulltag_flame_patches.pk3",
		nullptr,
		SkulltagFlamePatchPatterns,
		0u
	},
	{
		"Monsters and Addons settings controller script compatibility",
		nullptr,
		nullptr,
		MonstersAndAddonsPatterns,
		HCDE_MODCOMPAT_SETTINGS_CONTROLLER_NONNET_SCRIPTS
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
	ActiveStartupMapOverride = nullptr;

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
		if (ActiveStartupMapOverride == nullptr && entry.StartupMapOverride != nullptr && entry.StartupMapOverride[0] != '\0')
		{
			ActiveStartupMapOverride = entry.StartupMapOverride;
			Printf("HCDE: startup map compatibility override is '%s'.\n", ActiveStartupMapOverride);
		}

		if (entry.ResourceFile == nullptr || entry.ResourceFile[0] == '\0')
		{
			continue;
		}

		const char* compatFile = HCDE_ModCompat_ResolveCompatFile(entry.ResourceFile, config);
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

const char* HCDE_ModCompat_ResolveStartupMapOverride(const char* requestedMap)
{
	if (ActiveStartupMapOverride == nullptr || requestedMap == nullptr || requestedMap[0] == '\0')
	{
		return nullptr;
	}

	// Doom Connector commonly passes MAP01/E1M1 as a default map value. For mods
	// with a custom first map, treat those two values as a launcher default and
	// remap them to the known compatibility entrypoint.
	if (stricmp(requestedMap, "MAP01") != 0 && stricmp(requestedMap, "E1M1") != 0)
	{
		return nullptr;
	}

	return ActiveStartupMapOverride;
}

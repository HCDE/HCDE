/*
** hcde_eternity_compat.cpp
**
** HCDE-managed compatibility detection for Eternity Engine mod resources.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#include "hcde_eternity_compat.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "filesystem.h"
#include "printf.h"

namespace
{
	struct EternityResourceCounts
	{
		int EMapInfo = 0;
		int Edf = 0;
		int ExtraData = 0;
	};

	unsigned int ActiveEternityCompatFlags = 0u;
	EternityResourceCounts ActiveEternityResourceCounts;
	std::vector<std::string> ActiveEternitySamples;

	std::string LowerCopy(const char* value)
	{
		if (value == nullptr)
		{
			return {};
		}

		std::string lowered(value);
		std::replace(lowered.begin(), lowered.end(), '\\', '/');
		std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return lowered;
	}

	std::string BaseName(const std::string& value)
	{
		const size_t slash = value.find_last_of('/');
		return slash == std::string::npos ? value : value.substr(slash + 1);
	}

	bool EndsWith(const std::string& value, const char* suffix)
	{
		const std::string tail(suffix);
		return value.size() >= tail.size() && value.compare(value.size() - tail.size(), tail.size(), tail) == 0;
	}

	bool HasPathSegment(const std::string& value, const char* segment)
	{
		const std::string needle = std::string("/") + segment + "/";
		return value.find(needle) != std::string::npos || value.rfind(std::string(segment) + "/", 0) == 0;
	}

	bool IsUserLoadedLump(int lump)
	{
		return fileSystem.GetFileContainer(lump) > fileSystem.GetMaxIwadNum();
	}

	void RememberSample(int lump)
	{
		if (ActiveEternitySamples.size() >= 8)
		{
			return;
		}

		const std::string sample = fileSystem.GetFileFullPath(lump);
		if (std::find(ActiveEternitySamples.begin(), ActiveEternitySamples.end(), sample) == ActiveEternitySamples.end())
		{
			ActiveEternitySamples.push_back(sample);
		}
	}

	void RecordEMapInfo(int lump)
	{
		ActiveEternityCompatFlags |= HCDE_ETERNITYCOMPAT_EMAPINFO;
		ActiveEternityResourceCounts.EMapInfo++;
		RememberSample(lump);
	}

	void RecordEdf(int lump)
	{
		ActiveEternityCompatFlags |= HCDE_ETERNITYCOMPAT_EDF;
		ActiveEternityResourceCounts.Edf++;
		RememberSample(lump);
	}

	void RecordExtraData(int lump)
	{
		ActiveEternityCompatFlags |= HCDE_ETERNITYCOMPAT_EXTRADATA;
		ActiveEternityResourceCounts.ExtraData++;
		RememberSample(lump);
	}

	void ReportDetectedResources()
	{
		if (ActiveEternityCompatFlags == 0u)
		{
			return;
		}

		Printf(
			"HCDE: Eternity compatibility resources detected: EMAPINFO=%d EDF=%d ExtraData=%d.\n",
			ActiveEternityResourceCounts.EMapInfo,
			ActiveEternityResourceCounts.Edf,
			ActiveEternityResourceCounts.ExtraData
		);
		Printf("HCDE: Eternity compatibility will parse supported EMAPINFO, ExtraData, XLAT, and EDF manifest properties; unsupported behavior still needs compat patches.\n");

		for (const auto& sample : ActiveEternitySamples)
		{
			Printf("HCDE: Eternity resource: %s\n", sample.c_str());
		}
	}
}

void HCDE_EternityCompat_DetectLoadedResources()
{
	ActiveEternityCompatFlags = 0u;
	ActiveEternityResourceCounts = {};
	ActiveEternitySamples.clear();

	for (int lump = 0; lump < fileSystem.GetNumEntries(); ++lump)
	{
		if (!IsUserLoadedLump(lump))
		{
			continue;
		}

		const std::string shortName = LowerCopy(fileSystem.GetFileShortName(lump));
		const std::string fullName = LowerCopy(fileSystem.GetFileFullName(lump, false));
		const std::string path = fullName.empty() ? shortName : fullName;
		const std::string base = BaseName(path);

		if (base == "emapinfo")
		{
			RecordEMapInfo(lump);
		}

		if (base == "edfroot" || base == "edf" || EndsWith(base, ".edf") || HasPathSegment(path, "edf"))
		{
			RecordEdf(lump);
		}

		if (base == "extradata" || base == "extradata.txt" || EndsWith(path, "/extradata") || EndsWith(path, "/extradata.txt"))
		{
			RecordExtraData(lump);
		}
	}

	ReportDetectedResources();
}

bool HCDE_EternityCompat_IsActive(unsigned int flags)
{
	return (ActiveEternityCompatFlags & flags) == flags;
}

unsigned int HCDE_EternityCompat_GetFlags()
{
	return ActiveEternityCompatFlags;
}

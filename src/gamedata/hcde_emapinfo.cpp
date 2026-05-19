/*
** hcde_emapinfo.cpp
**
** HCDE-owned parser for a supported subset of Eternity EMAPINFO.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#include "hcde_emapinfo.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "cmdlib.h"
#include "filesystem.h"
#include "g_mapinfo.h"
#include "gi.h"
#include "printf.h"

namespace
{
	struct EMapInfoUnsupportedSample
	{
		std::string MapName;
		std::string Property;
		int Line = 0;
	};

	struct EMapInfoStats
	{
		int Maps = 0;
		int Properties = 0;
		int Applied = 0;
		int ExtraDataResolved = 0;
		int ExtraDataMissing = 0;
		int Unsupported = 0;
		int Invalid = 0;
		std::vector<EMapInfoUnsupportedSample> UnsupportedSamples;
	};

	TMap<FName, bool> EMapInfoMaps;
	TMap<FName, int> EMapInfoExtraDataLumps;

	static std::string TrimCopy(const std::string& value)
	{
		size_t first = 0;
		while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
		{
			first++;
		}

		size_t last = value.size();
		while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
		{
			last--;
		}

		return value.substr(first, last - first);
	}

	static std::string StripOuterQuotes(const std::string& value)
	{
		if (value.size() >= 2)
		{
			const char first = value.front();
			const char last = value.back();
			if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
			{
				return value.substr(1, value.size() - 2);
			}
		}
		return value;
	}

	static std::string LowerCopy(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return value;
	}

	static std::string UpperCopy(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::toupper(ch));
		});
		return value;
	}

	static std::string NormalizeResourceName(std::string value)
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		while (!value.empty() && value.front() == '/')
		{
			value.erase(value.begin());
		}
		return value;
	}

	static bool HasPathOrExtension(const std::string& value)
	{
		return value.find('/') != std::string::npos || value.find('.') != std::string::npos;
	}

	static void RememberEMapInfoMap(const char* mapname)
	{
		EMapInfoMaps[FName(mapname)] = true;
	}

	static std::string StripLineComment(const std::string& line)
	{
		bool inDoubleQuote = false;
		for (size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '"')
			{
				inDoubleQuote = !inDoubleQuote;
				continue;
			}

			if (!inDoubleQuote)
			{
				if (line[i] == '#' || line[i] == ';')
				{
					return line.substr(0, i);
				}
				if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')
				{
					return line.substr(0, i);
				}
			}
		}
		return line;
	}

	static bool SplitPropertyLine(const std::string& line, std::string& key, std::string& value)
	{
		const size_t equals = line.find('=');
		if (equals != std::string::npos)
		{
			key = TrimCopy(line.substr(0, equals));
			value = TrimCopy(line.substr(equals + 1));
		}
		else
		{
			size_t split = 0;
			while (split < line.size() && !std::isspace(static_cast<unsigned char>(line[split])))
			{
				split++;
			}
			key = TrimCopy(line.substr(0, split));
			value = TrimCopy(split < line.size() ? line.substr(split + 1) : std::string());
		}

		key = LowerCopy(key);
		value = StripOuterQuotes(TrimCopy(value));
		return !key.empty();
	}

	static std::vector<std::string> SplitValueList(const std::string& value)
	{
		std::vector<std::string> result;
		std::string current;

		for (char ch : value)
		{
			if (std::isspace(static_cast<unsigned char>(ch)) || ch == ',' || ch == '|' || ch == '+')
			{
				if (!current.empty())
				{
					result.push_back(current);
					current.clear();
				}
			}
			else
			{
				current.push_back(ch);
			}
		}

		if (!current.empty())
		{
			result.push_back(current);
		}

		return result;
	}

	static bool ParseBool(const std::string& value, bool& result)
	{
		if (value.empty())
		{
			result = true;
			return true;
		}

		const std::string lowered = LowerCopy(value);
		if (lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "1")
		{
			result = true;
			return true;
		}
		if (lowered == "false" || lowered == "no" || lowered == "off" || lowered == "0")
		{
			result = false;
			return true;
		}
		return false;
	}

	static bool ParseInt(const std::string& value, int& result)
	{
		if (value.empty())
		{
			return false;
		}

		char* end = nullptr;
		errno = 0;
		const long parsed = std::strtol(value.c_str(), &end, 0);
		if (errno != 0 || end == value.c_str() || !TrimCopy(end).empty())
		{
			return false;
		}

		result = static_cast<int>(parsed);
		return true;
	}

	static bool ParseDouble(const std::string& value, double& result)
	{
		if (value.empty())
		{
			return false;
		}

		char* end = nullptr;
		errno = 0;
		const double parsed = std::strtod(value.c_str(), &end);
		if (errno != 0 || end == value.c_str() || !TrimCopy(end).empty())
		{
			return false;
		}

		result = parsed;
		return true;
	}

	static void SetFlag(int32_t& flags, unsigned int flag, bool enabled)
	{
		if (enabled)
		{
			flags |= static_cast<int32_t>(flag);
		}
		else
		{
			flags &= ~static_cast<int32_t>(flag);
		}
	}

	static void SetFlag(uint32_t& flags, unsigned int flag, bool enabled)
	{
		if (enabled)
		{
			flags |= static_cast<uint32_t>(flag);
		}
		else
		{
			flags &= ~static_cast<uint32_t>(flag);
		}
	}

	static int GetDefaultLevelNum(const char* mapname, int& id24num)
	{
		if (!strnicmp(mapname, "MAP", 3) && strlen(mapname) <= 5)
		{
			const int mapnum = atoi(mapname + 3);
			if (mapnum >= 1 && mapnum <= 99)
			{
				id24num = mapnum;
				return mapnum;
			}
		}
		else if (mapname[0] == 'E' &&
			mapname[1] >= '0' && mapname[1] <= '9' &&
			mapname[2] == 'M' &&
			mapname[3] >= '0' && mapname[3] <= '9' && strlen(mapname) <= 5)
		{
			const int epinum = mapname[1] - '1';
			const int mapnum = atoi(mapname + 3);
			id24num = mapnum;
			if (strlen(mapname) == 4)
			{
				return epinum * 10 + mapnum;
			}
		}
		return 0;
	}

	static void SetLevelNum(level_info_t* info, int levelnum)
	{
		for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
		{
			if (&wadlevelinfos[i] != info && wadlevelinfos[i].levelnum == levelnum)
			{
				wadlevelinfos[i].levelnum = 0;
			}
		}
		info->levelnum = levelnum;
		info->id24_levelnum = 0;
	}

	static level_info_t* GetOrCreateLevel(const std::string& rawMapName, level_info_t* defaultinfo)
	{
		std::string upperMapName = UpperCopy(rawMapName);
		level_info_t* info = FindLevelInfo(upperMapName.c_str(), false);
		if (info == nullptr)
		{
			const unsigned int levelindex = wadlevelinfos.Reserve(1);
			info = &wadlevelinfos[levelindex];
			*info = *defaultinfo;
			info->MapName = upperMapName.c_str();
			info->levelnum = GetDefaultLevelNum(info->MapName.GetChars(), info->id24_levelnum);
			if (info->levelnum != 0)
			{
				SetLevelNum(info, info->levelnum);
			}
		}
		return info;
	}

	static FString NormalizeMusicName(const std::string& value)
	{
		if (value.empty())
		{
			return "";
		}

		const std::string upper = UpperCopy(value);
		const std::string lower = LowerCopy(value);
		if (value[0] == '$' || value.find('/') != std::string::npos || value.find('\\') != std::string::npos ||
			value.find('.') != std::string::npos || lower.rfind("d_", 0) == 0 || lower.rfind("mus_", 0) == 0)
		{
			return value.c_str();
		}

		if (gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Chex)
		{
			return FStringf("D_%s", upper.c_str());
		}
		if (gameinfo.gametype == GAME_Heretic)
		{
			return FStringf("MUS_%s", upper.c_str());
		}
		return value.c_str();
	}

	static void RecordUnsupported(EMapInfoStats& stats, const std::string& mapName, const std::string& property, int line)
	{
		stats.Unsupported++;
		if (stats.UnsupportedSamples.size() < 8)
		{
			stats.UnsupportedSamples.push_back({ mapName, property, line });
		}
	}

	static void RecordInvalid(EMapInfoStats& stats, const std::string& mapName, const std::string& property, int line)
	{
		stats.Invalid++;
		if (stats.UnsupportedSamples.size() < 8)
		{
			stats.UnsupportedSamples.push_back({ mapName, property + " (invalid value)", line });
		}
	}

	static bool ApplyBoolProperty(EMapInfoStats& stats, const std::string& mapName, const std::string& key, const std::string& value, int line, bool& result)
	{
		if (ParseBool(value, result))
		{
			return true;
		}
		RecordInvalid(stats, mapName, key, line);
		return false;
	}

	static bool ApplyIntProperty(EMapInfoStats& stats, const std::string& mapName, const std::string& key, const std::string& value, int line, int& result)
	{
		if (ParseInt(value, result))
		{
			return true;
		}
		RecordInvalid(stats, mapName, key, line);
		return false;
	}

	static bool ApplyDoubleProperty(EMapInfoStats& stats, const std::string& mapName, const std::string& key, const std::string& value, int line, double& result)
	{
		if (ParseDouble(value, result))
		{
			return true;
		}
		RecordInvalid(stats, mapName, key, line);
		return false;
	}

	static bool ApplyBossSpecials(level_info_t* info, const std::string& value)
	{
		int32_t flags = 0;
		uint32_t flags3 = 0;
		for (const auto& raw : SplitValueList(value))
		{
			const std::string flag = UpperCopy(raw);
			if (flag == "MAP07_1" || flag == "MAP07_2")
			{
				flags |= LEVEL_MAP07SPECIAL;
			}
			else if (flag == "E1M8")
			{
				flags3 |= LEVEL3_E1M8SPECIAL;
			}
			else if (flag == "E2M8")
			{
				flags3 |= LEVEL3_E2M8SPECIAL;
			}
			else if (flag == "E3M8")
			{
				flags3 |= LEVEL3_E3M8SPECIAL;
			}
			else if (flag == "E4M6")
			{
				flags3 |= LEVEL3_E4M6SPECIAL;
			}
			else if (flag == "E4M8")
			{
				flags3 |= LEVEL3_E4M8SPECIAL;
			}
			else
			{
				return false;
			}
		}
		info->flags |= flags;
		info->flags3 |= flags3;
		return flags != 0 || flags3 != 0;
	}

	static bool ApplyDefaultEnvironment(EMapInfoStats& stats, level_info_t* info, const std::string& mapName, const std::string& key, const std::string& value, int line)
	{
		const auto parts = SplitValueList(value);
		if (parts.empty() || parts.size() > 2)
		{
			RecordInvalid(stats, mapName, key, line);
			return false;
		}

		int first = 0;
		int second = 0;
		if (!ParseInt(parts[0], first) || (parts.size() == 2 && !ParseInt(parts[1], second)))
		{
			RecordInvalid(stats, mapName, key, line);
			return false;
		}

		info->DefaultEnvironment = (first << 8) | second;
		return true;
	}

	static int ResolveSiblingLump(int emapinfoLump, const std::string& value)
	{
		const int wad = fileSystem.GetFileContainer(emapinfoLump);
		std::string name = NormalizeResourceName(value);
		if (name.empty())
		{
			return -1;
		}

		int lump = fileSystem.CheckNumForFullName(name.c_str(), wad);
		if (lump >= 0)
		{
			return lump;
		}

		const char* emapinfoName = fileSystem.GetFileFullName(emapinfoLump, false);
		if (emapinfoName != nullptr)
		{
			std::string sourcePath = NormalizeResourceName(emapinfoName);
			const size_t slash = sourcePath.find_last_of('/');
			if (slash != std::string::npos)
			{
				std::string relative = sourcePath.substr(0, slash + 1) + name;
				lump = fileSystem.CheckNumForFullName(relative.c_str(), wad);
				if (lump >= 0)
				{
					return lump;
				}
			}
		}

		if (name.size() <= 8 && !HasPathOrExtension(name))
		{
			lump = fileSystem.CheckNumForName(name.c_str(), FileSys::ns_global, wad, true);
			if (lump >= 0)
			{
				return lump;
			}
		}

		return -1;
	}

	static int ResolveExtraDataLump(int emapinfoLump, const std::string& value)
	{
		int lump = ResolveSiblingLump(emapinfoLump, value);
		if (lump >= 0)
		{
			return lump;
		}

		const std::string name = NormalizeResourceName(value);
		return fileSystem.CheckNumForFullName(name.c_str(), true, FileSys::ns_global);
	}

	static bool ApplyEMapInfoProperty(EMapInfoStats& stats, int emapinfoLump, level_info_t* info, const std::string& mapName, const std::string& key, const std::string& value, int line)
	{
		if (key == "levelname" || key == "inter-levelname")
		{
			info->LevelName = value.c_str();
			info->PName = "";
			info->flags &= ~LEVEL_LOOKUPLEVELNAME;
		}
		else if (key == "creator")
		{
			info->AuthorName = value.c_str();
		}
		else if (key == "levelnum")
		{
			int number = 0;
			if (!ApplyIntProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			SetLevelNum(info, number);
		}
		else if (key == "levelpic")
		{
			info->PName = UpperCopy(value).c_str();
		}
		else if (key == "nextlevel")
		{
			info->NextMap = UpperCopy(value).c_str();
		}
		else if (key == "nextsecret")
		{
			info->NextSecretMap = UpperCopy(value).c_str();
		}
		else if (key == "partime")
		{
			int number = 0;
			if (!ApplyIntProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			info->partime = number;
		}
		else if (key == "enterpic")
		{
			info->EnterPic = UpperCopy(value).c_str();
		}
		else if (key == "interpic")
		{
			info->ExitPic = UpperCopy(value).c_str();
		}
		else if (key == "extradata")
		{
			const int lump = ResolveExtraDataLump(emapinfoLump, value);
			if (lump < 0)
			{
				info->EDName = value.c_str();
				stats.ExtraDataMissing++;
				RecordInvalid(stats, mapName, key, line);
				return false;
			}
			info->EDName = fileSystem.GetFileFullName(lump);
			EMapInfoExtraDataLumps[FName(mapName.c_str())] = lump;
			stats.ExtraDataResolved++;
		}
		else if (key == "acsscript")
		{
			info->acsName = value.c_str();
		}
		else if (key == "disable-jump")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			SetFlag(info->flags, LEVEL_JUMP_NO, enabled);
		}
		else if (key == "aircontrol")
		{
			double number = 0;
			if (!ApplyDoubleProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			info->aircontrol = number == 0 ? std::numeric_limits<double>::min() : number / 65536.0;
		}
		else if (key == "gravity")
		{
			double number = 0;
			if (!ApplyDoubleProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			info->gravity = number == 0 ? DBL_MAX : number / 65536.0;
		}
		else if (key == "colormap")
		{
			info->CustomColorMap = UpperCopy(value).c_str();
		}
		else if (key == "defaultenvironment")
		{
			if (!ApplyDefaultEnvironment(stats, info, mapName, key, value, line))
			{
				return false;
			}
		}
		else if (key == "doublesky")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			SetFlag(info->flags, LEVEL_DOUBLESKY, enabled);
		}
		else if (key == "lightning")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			SetFlag(info->flags, LEVEL_STARTLIGHTNING, enabled);
		}
		else if (key == "noautosequences")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			SetFlag(info->flags, LEVEL_SNDSEQTOTALCTRL, enabled);
		}
		else if (key == "killstats")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			SetFlag(info->flags, LEVEL_NOINTERMISSION, enabled);
		}
		else if (key == "music")
		{
			info->Music = NormalizeMusicName(value);
			info->musicorder = 0;
		}
		else if (key == "intermusic")
		{
			info->InterMusic = NormalizeMusicName(value);
			info->intermusicorder = 0;
		}
		else if (key == "skyname")
		{
			info->SkyPic1 = UpperCopy(value).c_str();
		}
		else if (key == "sky2name")
		{
			info->SkyPic2 = UpperCopy(value).c_str();
		}
		else if (key == "skydelta")
		{
			double number = 0;
			if (!ApplyDoubleProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			info->skyspeed1 = static_cast<float>(number * TICRATE / 1000.0);
		}
		else if (key == "sky2delta")
		{
			double number = 0;
			if (!ApplyDoubleProperty(stats, mapName, key, value, line, number))
			{
				return false;
			}
			info->skyspeed2 = static_cast<float>(number * TICRATE / 1000.0);
		}
		else if (key == "unevenlight")
		{
			bool enabled = false;
			if (!ApplyBoolProperty(stats, mapName, key, value, line, enabled))
			{
				return false;
			}
			if (!enabled)
			{
				info->WallVertLight = info->WallHorizLight = 0;
			}
		}
		else if (key == "boss-specials")
		{
			if (!ApplyBossSpecials(info, value))
			{
				RecordUnsupported(stats, mapName, key, line);
				return false;
			}
		}
		else
		{
			RecordUnsupported(stats, mapName, key, line);
			return false;
		}

		stats.Applied++;
		return true;
	}

	static void ReportStats(int lumpnum, const EMapInfoStats& stats)
	{
		Printf(
			"HCDE: parsed EMAPINFO %s: maps=%d properties=%d applied=%d extradata=%d missing=%d unsupported=%d invalid=%d.\n",
			fileSystem.GetFileFullPath(lumpnum).c_str(),
			stats.Maps,
			stats.Properties,
			stats.Applied,
			stats.ExtraDataResolved,
			stats.ExtraDataMissing,
			stats.Unsupported,
			stats.Invalid
		);

		for (const auto& sample : stats.UnsupportedSamples)
		{
			Printf(
				"HCDE: EMAPINFO note: %s property '%s' at line %d is not handled by the current compatibility layer.\n",
				sample.MapName.empty() ? "<global>" : sample.MapName.c_str(),
				sample.Property.c_str(),
				sample.Line
			);
		}
	}
}

int HCDE_ParseEMapInfo(int lumpnum, level_info_t* defaultinfo)
{
	auto data = fileSystem.ReadFile(lumpnum);
	const std::string text(data.string(), data.size());
	EMapInfoStats stats;
	level_info_t* currentLevel = nullptr;
	std::string currentMap;

	size_t start = 0;
	int lineNumber = 0;
	while (start <= text.size())
	{
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
		{
			end = text.size();
		}

		lineNumber++;
		std::string line = text.substr(start, end - start);
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		line = TrimCopy(StripLineComment(line));

		if (!line.empty())
		{
			if (line.front() == '[')
			{
				const size_t close = line.find(']');
				if (close == std::string::npos)
				{
					RecordInvalid(stats, currentMap, "section", lineNumber);
					currentLevel = nullptr;
					currentMap.clear();
				}
				else
				{
					currentMap = UpperCopy(TrimCopy(line.substr(1, close - 1)));
					currentLevel = currentMap.empty() ? nullptr : GetOrCreateLevel(currentMap, defaultinfo);
					if (currentLevel != nullptr)
					{
						RememberEMapInfoMap(currentLevel->MapName.GetChars());
						stats.Maps++;
					}
				}
			}
			else if (currentLevel == nullptr)
			{
				RecordUnsupported(stats, currentMap, "<property outside map block>", lineNumber);
			}
			else
			{
				std::string key;
				std::string value;
				if (!SplitPropertyLine(line, key, value))
				{
					RecordInvalid(stats, currentMap, "<property>", lineNumber);
				}
				else
				{
					stats.Properties++;
					ApplyEMapInfoProperty(stats, lumpnum, currentLevel, currentMap, key, value, lineNumber);
					if (!currentLevel->SkyPic2.CompareNoCase("-NOFLAT-") || currentLevel->SkyPic2.IsEmpty())
					{
						currentLevel->SkyPic2 = currentLevel->SkyPic1;
					}
				}
			}
		}

		if (end == text.size())
		{
			break;
		}
		start = end + 1;
	}

	ReportStats(lumpnum, stats);
	return 1;
}

void HCDE_ClearEMapInfoCompat()
{
	EMapInfoMaps.Clear();
	EMapInfoExtraDataLumps.Clear();
}

bool HCDE_EMapInfo_IsEternityMap(const char* mapname)
{
	if (mapname == nullptr || mapname[0] == '\0')
	{
		return false;
	}

	return EMapInfoMaps.CheckKey(FName(mapname)) != nullptr;
}

int HCDE_EMapInfo_FindExtraDataLump(const char* mapname, const char* edname)
{
	if (mapname != nullptr && mapname[0] != '\0')
	{
		int* lump = EMapInfoExtraDataLumps.CheckKey(FName(mapname));
		if (lump != nullptr && *lump >= 0)
		{
			return *lump;
		}
	}

	if (edname == nullptr || edname[0] == '\0')
	{
		return -1;
	}

	return fileSystem.CheckNumForFullName(edname, true, FileSys::ns_global);
}

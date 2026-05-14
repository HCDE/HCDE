/*
** hcde_edf.cpp
**
** HCDE-owned parser for a supported subset of Eternity EDF.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#include "hcde_edf.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "filesystem.h"
#include "hcde_eternity_compat.h"
#include "info.h"
#include "printf.h"

namespace
{
	struct EdfIssueSample
	{
		std::string Source;
		std::string Context;
		std::string Detail;
		int Line = 0;
	};

	struct EdfThingDefinition
	{
		std::string Name;
		std::string CompatName;
		std::string BaseName;
		int DoomedNum = -1;
		int DehackedNum = -1;
		int SpawnId = -1;
		int SourceLump = -1;
		int Line = 0;
		bool IsDelta = false;
		bool HasUnsupportedBehavior = false;
	};

	struct EdfParseStats
	{
		int RootFiles = 0;
		int Includes = 0;
		int IncludeLoops = 0;
		int MissingIncludes = 0;
		int SkippedStdIncludes = 0;
		int ThingTypes = 0;
		int ThingDeltas = 0;
		int FrameBlocks = 0;
		int SoundBlocks = 0;
		int SpriteBlocks = 0;
		int KnownBlocks = 0;
		int UnknownBlocks = 0;
		int Properties = 0;
		int UnsupportedProperties = 0;
		int InvalidProperties = 0;
	};

	struct EdfActorStats
	{
		int Candidates = 0;
		int Applied = 0;
		int Existing = 0;
		int MissingActors = 0;
		int RegistryConflicts = 0;
		int EdfDoomedNumConflicts = 0;
		int DuplicateNames = 0;
		int UnsupportedBehavior = 0;
	};

	struct EdfSourceLine
	{
		std::string Text;
		int Line = 0;
	};

	EdfParseStats ParseStats;
	EdfActorStats ActorStats;
	std::vector<int> ParsedLumps;
	std::vector<EdfThingDefinition> ThingDefinitions;
	std::vector<EdfIssueSample> IssueSamples;
	size_t ReportedIssueSamples = 0;
	bool ParsedAnyEdf = false;
	bool ActorMappingsApplied = false;

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

	static std::string LowerCopy(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return value;
	}

	static bool EndsWith(const std::string& value, const char* suffix)
	{
		const std::string tail(suffix);
		return value.size() >= tail.size() && value.compare(value.size() - tail.size(), tail.size(), tail) == 0;
	}

	static std::string BaseName(const std::string& value)
	{
		const size_t slash = value.find_last_of('/');
		return slash == std::string::npos ? value : value.substr(slash + 1);
	}

	static bool HasPathSegment(const std::string& value, const char* segment)
	{
		const std::string needle = std::string("/") + segment + "/";
		return value.find(needle) != std::string::npos || value.rfind(std::string(segment) + "/", 0) == 0;
	}

	static bool HasPathOrExtension(const std::string& value)
	{
		return value.find('/') != std::string::npos || value.find('.') != std::string::npos;
	}

	static bool IsUserLoadedLump(int lump)
	{
		return fileSystem.GetFileContainer(lump) > fileSystem.GetMaxIwadNum();
	}

	static bool ContainsLump(const std::vector<int>& lumps, int lump)
	{
		return std::find(lumps.begin(), lumps.end(), lump) != lumps.end();
	}

	static void AddUniqueLump(std::vector<int>& lumps, int lump)
	{
		if (lump >= 0 && !ContainsLump(lumps, lump))
		{
			lumps.push_back(lump);
		}
	}

	static void RecordIssue(const char* context, const std::string& detail, int lump, int line)
	{
		if (IssueSamples.size() >= 20)
		{
			return;
		}

		const char* source = lump >= 0 ? fileSystem.GetFileFullName(lump, false) : nullptr;
		EdfIssueSample sample;
		sample.Source = source != nullptr ? source : "<unknown>";
		sample.Context = context != nullptr ? context : "<unknown>";
		sample.Detail = detail;
		sample.Line = line;
		IssueSamples.push_back(sample);
	}

	static std::string NormalizeResourceName(std::string value)
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		while (!value.empty() && value.front() == '/')
		{
			value.erase(value.begin());
		}

		std::vector<std::string> parts;
		size_t start = 0;
		while (start <= value.size())
		{
			size_t end = value.find('/', start);
			if (end == std::string::npos)
			{
				end = value.size();
			}

			std::string part = value.substr(start, end - start);
			if (part == "..")
			{
				if (!parts.empty())
				{
					parts.pop_back();
				}
			}
			else if (!part.empty() && part != ".")
			{
				parts.push_back(part);
			}

			if (end == value.size())
			{
				break;
			}
			start = end + 1;
		}

		std::string normalized;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0)
			{
				normalized += '/';
			}
			normalized += parts[i];
		}
		return normalized;
	}

	static std::vector<std::string> TokenizeSimple(const std::string& value)
	{
		std::vector<std::string> tokens;
		std::string current;
		bool inQuote = false;
		char quoteChar = 0;
		bool escape = false;

		for (char ch : value)
		{
			if (escape)
			{
				current.push_back(ch);
				escape = false;
				continue;
			}

			if (inQuote)
			{
				if (ch == '\\')
				{
					escape = true;
				}
				else if (ch == quoteChar)
				{
					inQuote = false;
				}
				else
				{
					current.push_back(ch);
				}
				continue;
			}

			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quoteChar = ch;
				continue;
			}

			const bool isDelimiter = std::isspace(static_cast<unsigned char>(ch)) ||
				ch == '{' || ch == '}' || ch == '(' || ch == ')' ||
				ch == ',' || ch == '=' || ch == ';';

			if (isDelimiter || ch == ':')
			{
				if (!current.empty())
				{
					tokens.push_back(current);
					current.clear();
				}
				if (ch == ':')
				{
					tokens.emplace_back(":");
				}
				continue;
			}

			current.push_back(ch);
		}

		if (!current.empty())
		{
			tokens.push_back(current);
		}
		return tokens;
	}

	static bool ParseIntStrict(const std::string& value, int& result)
	{
		if (value.empty())
		{
			return false;
		}

		char* end = nullptr;
		const long parsed = std::strtol(value.c_str(), &end, 0);
		if (end == value.c_str() || *end != '\0')
		{
			return false;
		}
		result = static_cast<int>(parsed);
		return true;
	}

	static bool ParseFirstIntAfter(const std::vector<std::string>& tokens, size_t start, int& result)
	{
		for (size_t i = start; i < tokens.size(); ++i)
		{
			if (ParseIntStrict(tokens[i], result))
			{
				return true;
			}
		}
		return false;
	}

	static std::string CleanEdfLine(const std::string& line, bool& inBlockComment)
	{
		std::string clean;
		bool inQuote = false;
		char quoteChar = 0;
		bool escape = false;

		for (size_t i = 0; i < line.size(); ++i)
		{
			const char ch = line[i];
			const char next = i + 1 < line.size() ? line[i + 1] : '\0';

			if (inBlockComment)
			{
				if (ch == '*' && next == '/')
				{
					inBlockComment = false;
					i++;
				}
				continue;
			}

			if (escape)
			{
				clean.push_back(ch);
				escape = false;
				continue;
			}

			if (inQuote)
			{
				clean.push_back(ch);
				if (ch == '\\')
				{
					escape = true;
				}
				else if (ch == quoteChar)
				{
					inQuote = false;
				}
				continue;
			}

			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quoteChar = ch;
				clean.push_back(ch);
				continue;
			}

			if (ch == '/' && next == '*')
			{
				inBlockComment = true;
				i++;
				continue;
			}
			if ((ch == '/' && next == '/') || ch == '#')
			{
				break;
			}
			if (ch == ';')
			{
				clean.push_back(' ');
				continue;
			}

			clean.push_back(ch);
		}

		return TrimCopy(clean);
	}

	static std::vector<EdfSourceLine> SplitCleanLines(const std::string& text)
	{
		std::vector<EdfSourceLine> lines;
		bool inBlockComment = false;
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

			line = CleanEdfLine(line, inBlockComment);
			if (!line.empty())
			{
				lines.push_back({ line, lineNumber });
			}

			if (end == text.size())
			{
				break;
			}
			start = end + 1;
		}

		return lines;
	}

	static bool ContainsBlockOpen(const std::string& line)
	{
		bool inQuote = false;
		char quoteChar = 0;
		bool escape = false;
		for (char ch : line)
		{
			if (inQuote)
			{
				if (escape)
				{
					escape = false;
				}
				else if (ch == '\\')
				{
					escape = true;
				}
				else if (ch == quoteChar)
				{
					inQuote = false;
				}
				continue;
			}
			if (ch == '"' || ch == '\'')
			{
				inQuote = true;
				quoteChar = ch;
				continue;
			}
			if (ch == '{')
			{
				return true;
			}
		}
		return false;
	}

	static size_t CollectBlockBody(const std::vector<EdfSourceLine>& lines, size_t start, std::vector<EdfSourceLine>& body)
	{
		int depth = 0;
		bool entered = false;

		for (size_t i = start; i < lines.size(); ++i)
		{
			const std::string& text = lines[i].Text;
			std::string fragment;
			bool inQuote = false;
			char quoteChar = 0;
			bool escape = false;

			for (char ch : text)
			{
				if (inQuote)
				{
					if (entered && depth >= 1)
					{
						fragment.push_back(ch);
					}
					if (escape)
					{
						escape = false;
					}
					else if (ch == '\\')
					{
						escape = true;
					}
					else if (ch == quoteChar)
					{
						inQuote = false;
					}
					continue;
				}

				if (ch == '"' || ch == '\'')
				{
					inQuote = true;
					quoteChar = ch;
					if (entered && depth >= 1)
					{
						fragment.push_back(ch);
					}
					continue;
				}

				if (ch == '{')
				{
					depth++;
					if (!entered)
					{
						entered = true;
						continue;
					}
					if (depth >= 1)
					{
						fragment.push_back(ch);
					}
					continue;
				}

				if (ch == '}')
				{
					if (entered && depth > 1)
					{
						fragment.push_back(ch);
					}
					depth--;
					if (entered && depth <= 0)
					{
						if (!TrimCopy(fragment).empty())
						{
							body.push_back({ fragment, lines[i].Line });
						}
						return i;
					}
					continue;
				}

				if (entered && depth >= 1)
				{
					fragment.push_back(ch);
				}
			}

			if (entered && !TrimCopy(fragment).empty())
			{
				body.push_back({ fragment, lines[i].Line });
			}
		}

		return lines.empty() ? 0 : lines.size() - 1;
	}

	static bool IsKnownTopLevelBlock(const std::string& key)
	{
		return key == "frame" || key == "sound" || key == "spritenames" ||
			key == "thinggroup" || key == "damagetype" || key == "morphtype" ||
			key == "castinfo" || key == "terrain" || key == "splash" ||
			key == "soundsequence" || key == "reverb" || key == "font" ||
			key == "string" || key == "menu" || key == "gamemodeinfo" ||
			key == "pickup" || key == "boss_spawner";
	}

	static bool IsBehaviorProperty(const std::string& key)
	{
		return key == "states" || key == "spawnstate" || key == "seestate" ||
			key == "painstate" || key == "meleestate" || key == "missilestate" ||
			key == "deathstate" || key == "xdeathstate" || key == "crashstate" ||
			key == "raisestate" || key == "healstate" || key == "firstdecoratestate" ||
			key == "action" || key == "cflags" || key == "addflags" ||
			key == "remflags" || key == "flags" || key == "basictype" ||
			key == "radius" || key == "height" || key == "correct_height" ||
			key == "speed" || key == "health" || key == "damage" ||
			key == "mass" || key == "painchance" || key == "reactiontime" ||
			key == "seesound" || key == "attacksound" || key == "painsound" ||
			key == "deathsound" || key == "activesound" || key == "obituary" ||
			key == "missiletype" || key == "explosiondamage" ||
			key == "explosionradius" || key == "translation" || key == "bloodcolor" ||
			key == "dropitem" || key == "particlefx";
	}

	static bool IsKnownIgnoredThingProperty(const std::string& key)
	{
		return key == "doomednum" || key == "dehackednum" || key == "spawnid" ||
			key == "inherits" || key == "compatname";
	}

	static void ParseThingPropertyLine(EdfThingDefinition& thing, const EdfSourceLine& sourceLine)
	{
		const auto tokens = TokenizeSimple(sourceLine.Text);
		if (tokens.empty())
		{
			return;
		}

		const std::string key = LowerCopy(tokens[0]);
		if (key.empty())
		{
			return;
		}

		ParseStats.Properties++;
		if (key == "doomednum")
		{
			if (!ParseFirstIntAfter(tokens, 1, thing.DoomedNum))
			{
				ParseStats.InvalidProperties++;
				RecordIssue("thingtype", thing.Name + " has an invalid doomednum", thing.SourceLump, sourceLine.Line);
			}
		}
		else if (key == "dehackednum")
		{
			if (!ParseFirstIntAfter(tokens, 1, thing.DehackedNum))
			{
				ParseStats.InvalidProperties++;
				RecordIssue("thingtype", thing.Name + " has an invalid dehackednum", thing.SourceLump, sourceLine.Line);
			}
		}
		else if (key == "spawnid")
		{
			if (!ParseFirstIntAfter(tokens, 1, thing.SpawnId))
			{
				ParseStats.InvalidProperties++;
				RecordIssue("thingtype", thing.Name + " has an invalid spawnid", thing.SourceLump, sourceLine.Line);
			}
		}
		else if (key == "inherits")
		{
			if (tokens.size() >= 2)
			{
				thing.BaseName = tokens[1];
			}
		}
		else if (key == "compatname")
		{
			if (tokens.size() >= 2)
			{
				thing.CompatName = tokens[1];
			}
		}
		else if (IsBehaviorProperty(key))
		{
			thing.HasUnsupportedBehavior = true;
			ParseStats.UnsupportedProperties++;
		}
		else if (!IsKnownIgnoredThingProperty(key))
		{
			thing.HasUnsupportedBehavior = true;
			ParseStats.UnsupportedProperties++;
			RecordIssue("thingtype", thing.Name + " property '" + key + "' needs a compat patch", thing.SourceLump, sourceLine.Line);
		}
	}

	static void ParseThingHeader(EdfThingDefinition& thing, const std::string& header)
	{
		const size_t brace = header.find('{');
		const std::string headerOnly = brace == std::string::npos ? header : header.substr(0, brace);
		const auto tokens = TokenizeSimple(headerOnly);
		if (tokens.size() < 2)
		{
			return;
		}

		thing.Name = tokens[1];
		for (size_t i = 2; i < tokens.size(); ++i)
		{
			if (tokens[i] != ":")
			{
				continue;
			}

			if (i + 1 < tokens.size())
			{
				thing.BaseName = tokens[i + 1];
			}

			int value = -1;
			if (ParseFirstIntAfter(tokens, i + 2, value))
			{
				thing.DoomedNum = value;
				for (size_t j = i + 2; j < tokens.size(); ++j)
				{
					if (ParseIntStrict(tokens[j], value))
					{
						for (size_t k = j + 1; k < tokens.size(); ++k)
						{
							if (ParseIntStrict(tokens[k], value))
							{
								thing.DehackedNum = value;
								return;
							}
						}
						return;
					}
				}
			}
			return;
		}
	}

	static void ParseThingBlock(const std::string& header, const std::vector<EdfSourceLine>& body, int lump, int headerLine, bool isDelta)
	{
		EdfThingDefinition thing;
		thing.SourceLump = lump;
		thing.Line = headerLine;
		thing.IsDelta = isDelta;
		ParseThingHeader(thing, header);

		if (thing.Name.empty())
		{
			ParseStats.InvalidProperties++;
			RecordIssue(isDelta ? "thingdelta" : "thingtype", "missing thing name", lump, thing.Line);
			return;
		}

		if (isDelta)
		{
			ParseStats.ThingDeltas++;
		}
		else
		{
			ParseStats.ThingTypes++;
		}

		int nestedDepth = 0;
		bool inHeredoc = false;
		for (const auto& line : body)
		{
			const std::string trimmed = TrimCopy(line.Text);
			if (trimmed.empty())
			{
				continue;
			}

			if (inHeredoc)
			{
				if (trimmed.find("\"@") != std::string::npos)
				{
					inHeredoc = false;
				}
				continue;
			}

			std::string propertyPart = trimmed;
			const size_t nestedOpen = propertyPart.find('{');
			if (nestedOpen != std::string::npos)
			{
				propertyPart = propertyPart.substr(0, nestedOpen);
			}

			if (nestedDepth == 0 && !TrimCopy(propertyPart).empty())
			{
				ParseThingPropertyLine(thing, { propertyPart, line.Line });
			}

			if (trimmed.find("@\"") != std::string::npos && trimmed.find("\"@") == std::string::npos)
			{
				inHeredoc = true;
			}

			for (char ch : trimmed)
			{
				if (ch == '{')
				{
					nestedDepth++;
				}
				else if (ch == '}' && nestedDepth > 0)
				{
					nestedDepth--;
				}
			}
		}

		ThingDefinitions.push_back(thing);
	}

	static int ResolveIncludeLump(int sourceLump, const std::string& includeName)
	{
		const int wad = fileSystem.GetFileContainer(sourceLump);
		const std::string name = NormalizeResourceName(includeName);
		if (name.empty())
		{
			return -1;
		}

		int lump = fileSystem.CheckNumForFullName(name.c_str(), wad);
		if (lump >= 0)
		{
			return lump;
		}

		const char* sourceName = fileSystem.GetFileFullName(sourceLump, false);
		if (sourceName != nullptr)
		{
			std::string sourcePath = NormalizeResourceName(sourceName);
			const size_t slash = sourcePath.find_last_of('/');
			if (slash != std::string::npos)
			{
				const std::string relative = NormalizeResourceName(sourcePath.substr(0, slash + 1) + name);
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

	static std::string ParseIncludeName(const std::string& line)
	{
		const auto tokens = TokenizeSimple(line);
		return tokens.size() >= 2 ? tokens[1] : std::string();
	}

	static void ParseEdfLump(int lump, bool isRoot);

	static void ParseIncludeLine(const std::string& key, const EdfSourceLine& line, int sourceLump)
	{
		const std::string includeName = ParseIncludeName(line.Text);
		if (includeName.empty())
		{
			ParseStats.InvalidProperties++;
			RecordIssue(key.c_str(), "missing include path", sourceLump, line.Line);
			return;
		}

		if (key == "stdinclude" || key == "userinclude")
		{
			ParseStats.SkippedStdIncludes++;
			RecordIssue(key.c_str(), includeName + " was not imported; HCDE uses native actors and compat patches instead", sourceLump, line.Line);
			return;
		}

		const int includeLump = ResolveIncludeLump(sourceLump, includeName);
		if (includeLump < 0)
		{
			ParseStats.MissingIncludes++;
			RecordIssue("include", includeName + " was not found beside its EDF source", sourceLump, line.Line);
			return;
		}

		ParseEdfLump(includeLump, false);
	}

	static void CountSkippedBlock(const std::string& key)
	{
		if (key == "frame")
		{
			ParseStats.FrameBlocks++;
		}
		else if (key == "sound")
		{
			ParseStats.SoundBlocks++;
		}
		else if (key == "spritenames")
		{
			ParseStats.SpriteBlocks++;
		}
		else if (IsKnownTopLevelBlock(key))
		{
			ParseStats.KnownBlocks++;
		}
		else
		{
			ParseStats.UnknownBlocks++;
		}
	}

	static void ParseEdfLump(int lump, bool isRoot)
	{
		if (ContainsLump(ParsedLumps, lump))
		{
			ParseStats.IncludeLoops++;
			return;
		}

		ParsedLumps.push_back(lump);
		ParsedAnyEdf = true;
		if (isRoot)
		{
			ParseStats.RootFiles++;
		}
		else
		{
			ParseStats.Includes++;
		}

		auto data = fileSystem.ReadFile(lump);
		const std::string text(data.string(), data.size());
		const std::vector<EdfSourceLine> lines = SplitCleanLines(text);

		for (size_t i = 0; i < lines.size(); ++i)
		{
			const auto tokens = TokenizeSimple(lines[i].Text);
			if (tokens.empty())
			{
				continue;
			}

			const std::string key = LowerCopy(tokens[0]);
			if (key == "include" || key == "stdinclude" || key == "userinclude")
			{
				ParseIncludeLine(key, lines[i], lump);
				continue;
			}

			if (key == "setdialect" || key == "enable" || key == "ifenabled" ||
				key == "ifdisabled" || key == "ifgametype" || key == "ifngametype" ||
				key == "else" || key == "endif")
			{
				continue;
			}

			if (key == "thingtype" || key == "thingdelta" || IsKnownTopLevelBlock(key))
			{
				std::string header = lines[i].Text;
				size_t blockStart = i;
				while (!ContainsBlockOpen(header) && blockStart + 1 < lines.size())
				{
					blockStart++;
					header += " ";
					header += lines[blockStart].Text;
				}

				if (!ContainsBlockOpen(header))
				{
					ParseStats.InvalidProperties++;
					RecordIssue(key.c_str(), "block missing opening brace", lump, lines[i].Line);
					i = blockStart;
					continue;
				}

				std::vector<EdfSourceLine> body;
				const size_t blockEnd = CollectBlockBody(lines, blockStart, body);
				if (key == "thingtype" || key == "thingdelta")
				{
					ParseThingBlock(header, body, lump, lines[i].Line, key == "thingdelta");
				}
				else
				{
					CountSkippedBlock(key);
				}
				i = blockEnd;
				continue;
			}

			ParseStats.UnknownBlocks++;
			RecordIssue("top-level", "unsupported EDF statement '" + key + "'", lump, lines[i].Line);
		}
	}

	static void DiscoverRootLumps(std::vector<int>& roots)
	{
		std::vector<int> looseCandidates;
		for (int lump = 0; lump < fileSystem.GetNumEntries(); ++lump)
		{
			if (!IsUserLoadedLump(lump))
			{
				continue;
			}

			const char* fullNameRaw = fileSystem.GetFileFullName(lump, false);
			const char* shortNameRaw = fileSystem.GetFileShortName(lump);
			std::string fullName = fullNameRaw != nullptr ? fullNameRaw : "";
			std::string shortName = shortNameRaw != nullptr ? shortNameRaw : "";
			std::replace(fullName.begin(), fullName.end(), '\\', '/');
			std::replace(shortName.begin(), shortName.end(), '\\', '/');
			fullName = LowerCopy(fullName);
			shortName = LowerCopy(shortName);
			const std::string path = fullName.empty() ? shortName : fullName;
			const std::string base = BaseName(path);

			if (base == "edfroot" || base == "edf" || base == "edfroot.edf")
			{
				AddUniqueLump(roots, lump);
			}
			else if (EndsWith(base, ".edf") || HasPathSegment(path, "edf"))
			{
				AddUniqueLump(looseCandidates, lump);
			}
		}

		if (roots.empty())
		{
			roots.swap(looseCandidates);
		}
	}

	static bool HasLaterDefinitionForName(size_t index)
	{
		const std::string name = LowerCopy(ThingDefinitions[index].Name);
		for (size_t i = index + 1; i < ThingDefinitions.size(); ++i)
		{
			if (LowerCopy(ThingDefinitions[i].Name) == name)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsDoomedNumConflict(size_t index)
	{
		const int doomedNum = ThingDefinitions[index].DoomedNum;
		if (doomedNum < 0)
		{
			return false;
		}

		for (size_t i = 0; i < ThingDefinitions.size(); ++i)
		{
			if (i == index || HasLaterDefinitionForName(i))
			{
				continue;
			}
			if (ThingDefinitions[i].DoomedNum == doomedNum &&
				LowerCopy(ThingDefinitions[i].Name) != LowerCopy(ThingDefinitions[index].Name))
			{
				return true;
			}
		}
		return false;
	}

	static PClassActor* FindActorForThing(const EdfThingDefinition& thing, std::string& matchedName)
	{
		PClassActor* actor = PClass::FindActor(thing.Name.c_str());
		if (actor != nullptr)
		{
			matchedName = thing.Name;
			return actor;
		}

		if (!thing.CompatName.empty())
		{
			actor = PClass::FindActor(thing.CompatName.c_str());
			if (actor != nullptr)
			{
				matchedName = thing.CompatName;
				return actor;
			}
		}

		return nullptr;
	}

	static std::string DescribeDoomEdEntry(const FDoomEdEntry* entry)
	{
		if (entry == nullptr)
		{
			return "<none>";
		}
		if (entry->Type != nullptr)
		{
			return entry->Type->TypeName.GetChars();
		}
		if (entry->Special > 0)
		{
			return "special " + std::to_string(entry->Special);
		}
		return "<reserved>";
	}

	static void ReportParseSummary()
	{
		if (!ParsedAnyEdf)
		{
			return;
		}

		int doomedNums = 0;
		int spawnIds = 0;
		int dehackedNums = 0;
		int compatNames = 0;
		int inheritedThings = 0;
		int behaviorThings = 0;
		for (const auto& thing : ThingDefinitions)
		{
			if (thing.DoomedNum >= 0) doomedNums++;
			if (thing.SpawnId >= 0) spawnIds++;
			if (thing.DehackedNum >= 0) dehackedNums++;
			if (!thing.CompatName.empty()) compatNames++;
			if (!thing.BaseName.empty()) inheritedThings++;
			if (thing.HasUnsupportedBehavior) behaviorThings++;
		}

		Printf(
			"HCDE: EDF compatibility parsed roots=%d includes=%d duplicate_includes=%d thingtypes=%d thingdeltas=%d doomednums=%d spawnids=%d dehacked=%d inherited=%d compatnames=%d behavior=%d frames=%d sounds=%d sprites=%d other_blocks=%d unknown_blocks=%d properties=%d unsupported_props=%d invalid=%d missing_includes=%d.\n",
			ParseStats.RootFiles,
			ParseStats.Includes,
			ParseStats.IncludeLoops,
			ParseStats.ThingTypes,
			ParseStats.ThingDeltas,
			doomedNums,
			spawnIds,
			dehackedNums,
			inheritedThings,
			compatNames,
			behaviorThings,
			ParseStats.FrameBlocks,
			ParseStats.SoundBlocks,
			ParseStats.SpriteBlocks,
			ParseStats.KnownBlocks,
			ParseStats.UnknownBlocks,
			ParseStats.Properties,
			ParseStats.UnsupportedProperties,
			ParseStats.InvalidProperties,
			ParseStats.MissingIncludes
		);

		if (ParseStats.SkippedStdIncludes > 0)
		{
			Printf("HCDE: EDF compatibility skipped %d standard include%s; HCDE uses native actors and explicit compat patches for that layer.\n",
				ParseStats.SkippedStdIncludes,
				ParseStats.SkippedStdIncludes == 1 ? "" : "s");
		}

		for (const auto& sample : IssueSamples)
		{
			Printf(
				"HCDE: EDF note: %s line %d (%s): %s.\n",
				sample.Source.c_str(),
				sample.Line,
				sample.Context.c_str(),
				sample.Detail.c_str()
			);
		}
		ReportedIssueSamples = IssueSamples.size();
	}

	static void ResetState()
	{
		ParseStats = {};
		ActorStats = {};
		ParsedLumps.clear();
		ThingDefinitions.clear();
		IssueSamples.clear();
		ReportedIssueSamples = 0;
		ParsedAnyEdf = false;
		ActorMappingsApplied = false;
	}
}

void HCDE_EdfCompat_ParseLoadedResources()
{
	ResetState();

	if (!HCDE_EternityCompat_IsActive(HCDE_ETERNITYCOMPAT_EDF))
	{
		return;
	}

	std::vector<int> roots;
	DiscoverRootLumps(roots);
	for (int lump : roots)
	{
		ParseEdfLump(lump, true);
	}

	ReportParseSummary();
}

void HCDE_EdfCompat_ApplyActorMappings()
{
	if (!ParsedAnyEdf || ActorMappingsApplied)
	{
		return;
	}

	ActorMappingsApplied = true;
	for (size_t i = 0; i < ThingDefinitions.size(); ++i)
	{
		const EdfThingDefinition& thing = ThingDefinitions[i];
		if (thing.DoomedNum < 0)
		{
			continue;
		}

		ActorStats.Candidates++;
		if (thing.HasUnsupportedBehavior)
		{
			ActorStats.UnsupportedBehavior++;
		}

		if (HasLaterDefinitionForName(i))
		{
			ActorStats.DuplicateNames++;
			RecordIssue("thingtype", thing.Name + " was redefined later; using the latest EDF definition only", thing.SourceLump, thing.Line);
			continue;
		}

		if (IsDoomedNumConflict(i))
		{
			ActorStats.EdfDoomedNumConflicts++;
			RecordIssue("thingtype", thing.Name + " shares doomednum " + std::to_string(thing.DoomedNum) + " with another EDF thing", thing.SourceLump, thing.Line);
			continue;
		}

		std::string matchedName;
		PClassActor* actor = FindActorForThing(thing, matchedName);
		if (actor == nullptr)
		{
			ActorStats.MissingActors++;
			RecordIssue("thingtype", thing.Name + " has no matching HCDE/ZScript actor", thing.SourceLump, thing.Line);
			continue;
		}

		FDoomEdEntry* existing = DoomEdMap.CheckKey(thing.DoomedNum);
		if (existing != nullptr)
		{
			if (existing->Type == actor)
			{
				ActorStats.Existing++;
			}
			else
			{
				ActorStats.RegistryConflicts++;
				RecordIssue("thingtype", thing.Name + " doomednum " + std::to_string(thing.DoomedNum) + " conflicts with " + DescribeDoomEdEntry(existing), thing.SourceLump, thing.Line);
			}
			continue;
		}

		FDoomEdEntry entry;
		memset(&entry, 0, sizeof(entry));
		entry.Type = actor;
		// EDF can add a numeric alias for an existing actor, but actor behavior
		// still belongs in an explicit HCDE compatibility patch.
		entry.Special = -2;
		DoomEdMap.Insert(thing.DoomedNum, entry);
		ActorStats.Applied++;
		Printf("HCDE: EDF mapped DoomEdNum %d to actor %s from thingtype %s.\n",
			thing.DoomedNum,
			matchedName.c_str(),
			thing.Name.c_str());
	}

	Printf(
		"HCDE: EDF actor binding candidates=%d applied=%d existing=%d missing_actors=%d registry_conflicts=%d edf_conflicts=%d duplicate_names=%d unsupported_behavior=%d.\n",
		ActorStats.Candidates,
		ActorStats.Applied,
		ActorStats.Existing,
		ActorStats.MissingActors,
		ActorStats.RegistryConflicts,
		ActorStats.EdfDoomedNumConflicts,
		ActorStats.DuplicateNames,
		ActorStats.UnsupportedBehavior
	);

	for (size_t i = ReportedIssueSamples; i < IssueSamples.size(); ++i)
	{
		const auto& sample = IssueSamples[i];
		Printf(
			"HCDE: EDF note: %s line %d (%s): %s.\n",
			sample.Source.c_str(),
			sample.Line,
			sample.Context.c_str(),
			sample.Detail.c_str()
		);
	}
}

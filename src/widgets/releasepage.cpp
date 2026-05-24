/*
** releasepage.cpp
**
** Changelog tab of launcher
**
**---------------------------------------------------------------------------
**
** Copyright 2025 Marcus Minhorst
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include <cstring>
#include <cstdio>
#include <string>

#include <rapidxml/rapidxml.hpp>
#include <string_view>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/textedit/textedit.h>

#include "filesystem.h"
#include "findfile.h"
#include "gameconfigfile.h"
#include "gstrings.h"
#include "i_interface.h"
#include "launcherwindow.h"
#include "name.h"
#include "releasepage.h"
#include "version.h"
#include "zstring.h"

constexpr unsigned NUMBER_OF_RELEASES_TO_DISPLAY = 3;

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
static std::wstring CreateTempScriptPath(const wchar_t* prefix)
{
	wchar_t tempPath[MAX_PATH] = {};
	wchar_t tempFile[MAX_PATH] = {};
	if (!GetTempPathW(_countof(tempPath), tempPath))
		return {};
	if (!GetTempFileNameW(tempPath, prefix, 0, tempFile))
		return {};

	std::wstring scriptPath = tempFile;
	size_t dot = scriptPath.find_last_of(L'.');
	if (dot != std::wstring::npos)
	{
		scriptPath = scriptPath.substr(0, dot) + L".ps1";
	}
	else
	{
		scriptPath += L".ps1";
	}

	if (!MoveFileExW(tempFile, scriptPath.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		return {};
	}
	return scriptPath;
}

static bool WriteUtf8ScriptFile(const std::wstring& path, const std::string& content)
{
	FILE* fp = _wfopen(path.c_str(), L"wb");
	if (!fp)
		return false;
	if (!content.empty())
	{
		const size_t written = fwrite(content.data(), 1, content.size(), fp);
		if (written != content.size())
		{
			fclose(fp);
			return false;
		}
	}
	fclose(fp);
	return true;
}

static FString RunPowerShellScriptCapture(const std::wstring& scriptPath)
{
	std::wstring command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"";
	command += scriptPath;
	command += L"\" 2>&1";

	FILE* pipe = _wpopen(command.c_str(), L"rt");
	if (!pipe)
	{
		return {};
	}

	FString output;
	char buffer[1024];
	while (fgets(buffer, _countof(buffer), pipe))
	{
		output += buffer;
	}
	_pclose(pipe);
	return output;
}

static FString FetchGitHubChangelog()
{
	// Match Doom Connector-style release body viewing, but keep this best-effort:
	// if network fetch fails, the launcher falls back to packaged changelog text.
	const std::wstring scriptPath = CreateTempScriptPath(L"hcl");
	if (scriptPath.empty())
	{
		return {};
	}

	const std::string script = R"PS(
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$api = 'https://api.github.com/repos/bokoxthexchocobo/HCDE/releases?per_page=8'
$headers = @{ 'User-Agent' = 'HCDE-Changelog' }
$sb = New-Object System.Text.StringBuilder
try {
	# Keep launcher UX responsive on slow/blocked networks.
	$releases = Invoke-RestMethod -Uri $api -Headers $headers -TimeoutSec 6
	$count = 0
	foreach ($release in $releases) {
		if ($release.draft -or $release.prerelease) { continue }
		if ($count -gt 0) {
			[void]$sb.AppendLine()
			[void]$sb.AppendLine('---')
			[void]$sb.AppendLine()
		}
		$tag = [string]$release.tag_name
		if ($tag.StartsWith('v') -or $tag.StartsWith('V')) { $tag = $tag.Substring(1) }
		$published = [string]$release.published_at
		[void]$sb.Append('HCDE v')
		[void]$sb.Append($tag)
		if (-not [string]::IsNullOrWhiteSpace($published)) {
			[void]$sb.Append(' [')
			[void]$sb.Append($published)
			[void]$sb.Append(']')
		}
		[void]$sb.AppendLine()
		[void]$sb.AppendLine()

		$body = [string]$release.body
		if ([string]::IsNullOrWhiteSpace($body)) {
			[void]$sb.AppendLine('- No changelog provided.')
		} else {
			[void]$sb.AppendLine($body.Trim())
		}

		$count++
		if ($count -ge 3) { break }
	}
	if ($count -gt 0) {
		[Console]::Out.Write($sb.ToString().TrimEnd())
	}
} catch {
	# Keep the launcher responsive and fall back to packaged changelog if this fails.
}
)PS";

	if (!WriteUtf8ScriptFile(scriptPath, script))
	{
		DeleteFileW(scriptPath.c_str());
		return {};
	}

	FString output = RunPowerShellScriptCapture(scriptPath);
	DeleteFileW(scriptPath.c_str());
	output.StripLeftRight();
	return output;
}
}
#endif

ReleasePage::ReleasePage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	ShowThis = new CheckboxLabel(this);
	Notes = new TextEdit(this);

	auto text = GetReleaseNotes();

	Notes->SetText(text.GetChars());
	Notes->SetReadOnly(true);

	ShowThis->SetChecked(info.notifyNewRelease);
}

void ReleasePage::SetValues(FStartupSelectionInfo& info) const
{
	info.notifyNewRelease = ShowThis->GetChecked();
}

void ReleasePage::UpdateLanguage()
{
	ShowThis->SetText(GStrings.GetString("PICKER_SHOWTHIS"));
}

void ReleasePage::OnGeometryChanged()
{
	double y = 0.0;
	double w = GetWidth();
	double h = GetHeight();

	Notes->SetFrameGeometry(0.0, y, w, h - ShowThis->GetPreferredHeight());
	y += h - ShowThis->GetPreferredHeight();

	ShowThis->SetFrameGeometry(0.0, y, w, ShowThis->GetPreferredHeight());
	y += ShowThis->GetPreferredHeight();

	Launcher->UpdatePlayButton();
}


FString ReleasePage::_ParseReleaseNotes(rapidxml::xml_node<char> * release)
{
	// braindead html to plaintext parser

	if (!release) return GStrings.GetString("NOTES_FAIL"); // "Unable to parse changelog";

	auto description = release->first_node("description");
	auto version = release->first_attribute("version");
	auto date = release->first_attribute("date");
	auto url = release->first_node("url");
	FString text;

	// https://docs.flathub.org/docs/for-app-authors/metainfo-guidelines#description
	//
	// Only the following child tags are supported:
	// p (paragraph), ol, ul (ordered and unordered list) with li (list items) child tags,
	// em for italicized emphasis and code for inline code in monospace. **bold** is also

	auto block = [&text](const char *prefix, rapidxml::xml_node<char> *node)
	{
		FString block;

		while (node)
		{
			rapidxml::xml_node<char> *text = nullptr;
			const char *l = "", *r = "";
			if (node->type() == rapidxml::node_element)
			{
				auto name = std::string_view(node->name());
				if (name == "em")
				{
					l = r = "*";
				}
				else if (name == "code")
				{
					l = r = "`";
				}
				text = node;
			}
			else if (node->type() == rapidxml::node_data)
			{
				text = node;
			}
			if (text)
			{
				block.AppendFormat("%s%s%s", l, node->value(), r);
			}
			node = node->next_sibling();
		}

		text.AppendFormat("%s%s\n", prefix, block.GetChars());
	};

	if (description)
	{
		auto blocknode = description->first_node();
		std::string_view prev = "";
		while (blocknode)
		{
			std::string_view name = blocknode->name();
			if (prev != "" && (name == "p" || name == prev))
				text.AppendCharacter('\n');
			prev = name;

			if (name == "p")
			{
				block("", blocknode->first_node());
			}
			else if (name == "ul")
			{
				auto node = blocknode->first_node();
				while (node)
				{
					block("-  ", node->first_node());
					node = node->next_sibling();
				}
			}
			else if (name == "ol")
			{
				auto count = 0;
				auto node = blocknode->first_node();
				while (node)
				{
					FString pfx = FStringf("%d. ", ++count);
					block(pfx.GetChars(), node->first_node());
					node = node->next_sibling();
				}
			}

			blocknode = blocknode->next_sibling();
		}
	}

	FString result;
	result.AppendFormat(
		"%s version %s, released %s",
		GAMENAME,
		version
			? version->value()
			: GStrings.GetString("NOTES_UNKNOWN"), // "Unknown"
		date
			? date->value()
			: GStrings.GetString("NOTES_UNKNOWN") // "Unknown"
	);
	result.AppendFormat(
		"\n\n%s",
		description
			? text.GetChars()
			: GStrings.GetString("NOTES_EMPTY") // "No description provided."
	);
	if (url)
	{
		result.AppendCharacter('\n');
		result.AppendFormat("For more details see: %s", url->value());
	}
	return result;
}

FString ReleasePage::_BuildReleaseNotes(rapidxml::xml_document<> &doc)
{
	// braindead html to plaintext parser

	// traverse to first (latest) release node
	auto release = doc.first_node("component");
	if (!release) return GStrings.GetString("NOTES_FAIL"); // "Unable to parse changelog";
	release = release->first_node("releases");
	if (!release) return GStrings.GetString("NOTES_FAIL"); // "Unable to parse changelog";
	release = release->first_node("release");
	if (!release) return GStrings.GetString("NOTES_FAIL"); // "Unable to parse changelog";

	FString text;

	for (unsigned i = 1; ; i++)
	{
		if (!release) break;
		text.AppendFormat("%s", _ParseReleaseNotes(release).GetChars());
		if (i >= NUMBER_OF_RELEASES_TO_DISPLAY) break;

		text.AppendFormat("\n\n---\n\n");
		release = release->next_sibling("release");
	}

	return text;
}

// Ensure you free returned pointer
char * ReleasePage::_OpenReleaseNotes()
{
	auto wad = BaseFileSearch(BASEWAD, NULL, true, GameConfig);
	if (!wad) return nullptr;

	// we need to be free
	auto resf = FResourceFile::OpenResourceFile(wad);
	if (!resf) return nullptr;

	char * notes = nullptr;
	auto lump = resf->FindEntry("meta.xml"); // created from org.hcde.HCDE.metainfo.xml during build

	if (lump >= 0)
	{
		auto data = resf->Read(lump);

		// needs to be freed by caller
		notes = (char *)calloc(data.size()+1, sizeof(char));

		if (notes) strncpy(notes, data.string(), data.size());
	}

	delete resf;

	return notes;
}

//==========================================================================
//
// Extract changelog text for the launcher tab
//
//==========================================================================
FString ReleasePage::GetReleaseNotes()
{
#ifdef _WIN32
	const FString liveChangelog = FetchGitHubChangelog();
	if (liveChangelog.IsNotEmpty())
	{
		return liveChangelog;
	}
#endif

	// Fallback: use packaged metadata changelog from hcde.pk3.
	char* text = _OpenReleaseNotes();

	rapidxml::xml_document<> doc;
	if (text) doc.parse<rapidxml::parse_default>(text);
	FString content = _BuildReleaseNotes(doc);

	free(text);

	return content;
}

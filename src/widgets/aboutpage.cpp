/*
** aboutpage.cpp
**
** About tab of launcher
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
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/textedit/textedit.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>

#include "aboutpage.h"
#include "cmdlib.h"
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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
struct UpdateCheckResult
{
	bool ok = false;
	FString error = {};
	FString latestTag = {};
	FString downloadUrl = {};
	FString assetName = {};
};

struct LastUpdateRunStatus
{
	bool hasStatus = false;
	bool success = false;
	FString version = {};
	FString message = {};
	FString logPath = {};
	FString timestampUtc = {};
	bool rollbackAttempted = false;
	bool rollbackSucceeded = false;
};

struct UpdaterLockState
{
	bool exists = false;
	bool stale = false;
	double ageMinutes = 0.0;
	std::wstring path = {};
};

static constexpr double UpdaterLockStaleMinutes = 120.0;

// Quoting and escaping logic for Windows command-line arguments.
// Follows the standard conventions used by the Microsoft C Runtime (CRT).
static std::wstring QuoteWindowsCommandArg(const std::wstring& value)
{
	// Quoting exactly once for Windows command-line parsing.
	// This preserves characters used in signed download URLs (e.g. ?, &, =, %)
	// while still preventing argument splitting.
	std::wstring quoted;
	quoted.reserve(value.length() + 2);
	quoted.push_back(L'"');

	size_t backslashCount = 0;
	for (wchar_t ch : value)
	{
		if (ch == L'\\')
		{
			backslashCount++;
			continue;
		}

		if (ch == L'"')
		{
			// Double the backslashes before a quote and add one to escape the quote.
			quoted.append(backslashCount * 2 + 1, L'\\');
			quoted.push_back(L'"');
			backslashCount = 0;
			continue;
		}

		if (backslashCount > 0)
		{
			// Backslashes followed by a non-quote are literal.
			quoted.append(backslashCount, L'\\');
			backslashCount = 0;
		}

		quoted.push_back(ch);
	}

	if (backslashCount > 0)
	{
		// Backslashes at the end of the string must be doubled because we add a quote.
		quoted.append(backslashCount * 2, L'\\');
	}

	quoted.push_back(L'"');
	return quoted;
}

static std::wstring GetInstallDirPath()
{
	std::wstring installDir = WideString(progdir.GetChars());
	if (!installDir.empty() && (installDir.back() == L'/' || installDir.back() == L'\\'))
	{
		installDir.pop_back();
	}
	return installDir;
}

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
		return "Failed to launch PowerShell.";
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

static FString ReadUtf8TextFile(const std::wstring& path)
{
	FILE* fp = _wfopen(path.c_str(), L"rb");
	if (!fp)
		return {};

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return {};
	}

	const long fileSize = ftell(fp);
	if (fileSize < 0)
	{
		fclose(fp);
		return {};
	}
	rewind(fp);

	if (fileSize == 0)
	{
		fclose(fp);
		return {};
	}

	std::string bytes;
	bytes.resize(static_cast<size_t>(fileSize));
	const size_t readCount = fread(bytes.data(), 1, bytes.size(), fp);
	fclose(fp);
	if (readCount != bytes.size())
	{
		return {};
	}

	const size_t bomOffset =
		(bytes.size() >= 3 &&
		 static_cast<unsigned char>(bytes[0]) == 0xEF &&
		 static_cast<unsigned char>(bytes[1]) == 0xBB &&
		 static_cast<unsigned char>(bytes[2]) == 0xBF) ? 3 : 0;

	FString result;
	result.AppendCStrPart(bytes.data() + bomOffset, bytes.size() - bomOffset);
	return result;
}

static void ParseKeyValueField(const FString& output, const char* key, FString& outValue)
{
	const size_t keyLen = strlen(key);
	TArray<FString> lines = output.Split("\n", FString::TOK_SKIPEMPTY);
	for (auto& line : lines)
	{
		line.StripLeftRight();
		const char* text = line.GetChars();
		if (_strnicmp(text, key, keyLen) == 0 && text[keyLen] == '=')
		{
			outValue = text + keyLen + 1;
			outValue.StripLeftRight();
			return;
		}
	}
}

static FString ToLowerAscii(FString value)
{
	value.ToLower();
	return value;
}

static bool IsAllowedUpdateHost(const FString& hostLower)
{
	return hostLower.Compare("github.com") == 0
		|| hostLower.Compare("api.github.com") == 0
		|| hostLower.Compare("objects.githubusercontent.com") == 0
		|| hostLower.Compare("release-assets.githubusercontent.com") == 0;
}

static LastUpdateRunStatus ReadLastUpdateRunStatus()
{
	LastUpdateRunStatus status;
	const std::wstring installDir = GetInstallDirPath();
	if (installDir.empty())
	{
		return status;
	}

	std::wstring statusPath = installDir + L"\\backups\\hcde-update-last-status.txt";
	const FString content = ReadUtf8TextFile(statusPath);
	if (content.IsEmpty())
	{
		return status;
	}

	FString state;
	ParseKeyValueField(content, "STATUS", state);
	if (state.IsEmpty())
	{
		return status;
	}

	status.hasStatus = true;
	status.success = ToLowerAscii(state).Compare("success") == 0;
	ParseKeyValueField(content, "VERSION", status.version);
	ParseKeyValueField(content, "MESSAGE", status.message);
	ParseKeyValueField(content, "LOG", status.logPath);
	ParseKeyValueField(content, "TIMESTAMP_UTC", status.timestampUtc);
	FString rollbackAttempted;
	FString rollbackSucceeded;
	ParseKeyValueField(content, "ROLLBACK_ATTEMPTED", rollbackAttempted);
	ParseKeyValueField(content, "ROLLBACK_SUCCEEDED", rollbackSucceeded);
	status.rollbackAttempted = ToLowerAscii(rollbackAttempted).Compare("true") == 0;
	status.rollbackSucceeded = ToLowerAscii(rollbackSucceeded).Compare("true") == 0;
	return status;
}

static bool FileExists(const std::wstring& path)
{
	const DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static double FileAgeMinutesUtc(const FILETIME& fileWriteTime)
{
	FILETIME nowFileTime = {};
	GetSystemTimeAsFileTime(&nowFileTime);

	ULARGE_INTEGER nowTicks = {};
	nowTicks.LowPart = nowFileTime.dwLowDateTime;
	nowTicks.HighPart = nowFileTime.dwHighDateTime;

	ULARGE_INTEGER writeTicks = {};
	writeTicks.LowPart = fileWriteTime.dwLowDateTime;
	writeTicks.HighPart = fileWriteTime.dwHighDateTime;

	if (nowTicks.QuadPart <= writeTicks.QuadPart)
	{
		return 0.0;
	}

	static constexpr double TicksPerMinute = 600000000.0;
	return (nowTicks.QuadPart - writeTicks.QuadPart) / TicksPerMinute;
}

static UpdaterLockState ReadUpdaterLockState()
{
	UpdaterLockState state;
	const std::wstring installDir = GetInstallDirPath();
	if (installDir.empty())
	{
		return state;
	}

	state.path = installDir + L"\\backups\\hcde-update.lock";
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if (!GetFileAttributesExW(state.path.c_str(), GetFileExInfoStandard, &data))
	{
		return state;
	}

	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return state;
	}

	state.exists = true;
	state.ageMinutes = FileAgeMinutesUtc(data.ftLastWriteTime);
	state.stale = state.ageMinutes >= UpdaterLockStaleMinutes;
	return state;
}

static bool TryClearStaleUpdaterLock(UpdaterLockState& state)
{
	if (!state.exists || !state.stale || state.path.empty())
	{
		return true;
	}

	if (DeleteFileW(state.path.c_str()))
	{
		state.exists = false;
		state.stale = false;
		state.ageMinutes = 0.0;
		return true;
	}
	return false;
}

static std::wstring ResolveLastUpdateLogPath(const LastUpdateRunStatus& status)
{
	std::wstring candidate = WideString(status.logPath.GetChars());
	if (!candidate.empty() && FileExists(candidate))
	{
		return candidate;
	}

	const std::wstring installDir = GetInstallDirPath();
	if (installDir.empty())
	{
		return {};
	}

	const std::wstring backupsDir = installDir + L"\\backups";
	const std::wstring pattern = backupsDir + L"\\hcde-update-*.log";
	WIN32_FIND_DATAW findData = {};
	HANDLE findHandle = FindFirstFileW(pattern.c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return {};
	}

	std::wstring newestPath;
	FILETIME newestTime = {};
	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}

		if (newestPath.empty() || CompareFileTime(&findData.ftLastWriteTime, &newestTime) > 0)
		{
			newestPath = backupsDir + L"\\" + findData.cFileName;
			newestTime = findData.ftLastWriteTime;
		}
	}
	while (FindNextFileW(findHandle, &findData));

	FindClose(findHandle);
	return newestPath;
}

static FString BuildRunDetail(const LastUpdateRunStatus& status, const char* fallback)
{
	FString detail = status.message.IsEmpty() ? fallback : status.message;
	if (status.timestampUtc.IsNotEmpty())
	{
		detail += FStringf(" [utc: %s]", status.timestampUtc.GetChars());
	}
	return detail;
}

static bool ValidateUpdateUrl(const FString& sourceUrl, FString& normalizedUrl, FString& error)
{
	normalizedUrl = sourceUrl;
	normalizedUrl.StripLeftRight();
	if (normalizedUrl.IsEmpty())
	{
		error = "No downloadable update archive was found in the latest release.";
		return false;
	}

	FString lower = ToLowerAscii(normalizedUrl);
	if (lower.IndexOf("https://") != 0)
	{
		error = "Update URL was rejected: only HTTPS is allowed.";
		return false;
	}

	const char* text = normalizedUrl.GetChars();
	const char* hostStart = strstr(text, "://");
	hostStart = hostStart ? hostStart + 3 : text;
	const char* slash = strchr(hostStart, '/');
	const char* hostEnd = slash ? slash : hostStart + strlen(hostStart);
	FString host(hostStart, hostEnd - hostStart);
	host.StripLeftRight();
	if (host.IsEmpty())
	{
		error = "Update URL was rejected: host is missing.";
		return false;
	}

	const int atIndex = host.LastIndexOf('@');
	if (atIndex >= 0)
	{
		host = host.Mid(atIndex + 1);
	}

	const int colonIndex = host.LastIndexOf(':');
	if (colonIndex >= 0)
	{
		host = host.Left(colonIndex);
	}

	host = ToLowerAscii(host);
	if (!IsAllowedUpdateHost(host))
	{
		error = FStringf("Update URL host is not trusted: %s", host.GetChars());
		return false;
	}

	// Validate the URL path itself (not query/fragment text) so we only accept
	// actual zip package targets.
	std::string pathPart = slash ? slash : "/";
	const size_t suffixMarker = pathPart.find_first_of("?#");
	if (suffixMarker != std::string::npos)
	{
		pathPart.resize(suffixMarker);
	}

	std::transform(pathPart.begin(), pathPart.end(), pathPart.begin(), [](unsigned char c)
	{
		return static_cast<char>(std::tolower(c));
	});

	if (pathPart.length() < 4 || pathPart.compare(pathPart.length() - 4, 4, ".zip") != 0)
	{
		error = "Update URL was rejected: expected a zip package.";
		return false;
	}

	return true;
}

// Robust version number parser. Extracts all numeric components from a string,
// ignoring leading 'v' or other non-digit separators.
// Example: "v0.4.5-hotfix1" -> {0, 4, 5, 1}
static bool ParseVersionNumbers(const FString& version, std::vector<int>& outParts)
{
	const std::string input = version.GetChars();
	std::string token;
	for (char c : input)
	{
		if (c >= '0' && c <= '9')
		{
			token.push_back(c);
		}
		else if (!token.empty())
		{
			outParts.push_back(atoi(token.c_str()));
			token.clear();
		}
	}
	if (!token.empty())
	{
		outParts.push_back(atoi(token.c_str()));
	}
	return !outParts.empty();
}

static int CompareVersionTags(const FString& left, const FString& right)
{
	std::vector<int> a;
	std::vector<int> b;
	if (!ParseVersionNumbers(left, a) || !ParseVersionNumbers(right, b))
		return 0;

	const size_t count = (std::max)(a.size(), b.size());
	for (size_t i = 0; i < count; i++)
	{
		const int av = (i < a.size()) ? a[i] : 0;
		const int bv = (i < b.size()) ? b[i] : 0;
		if (av < bv) return -1;
		if (av > bv) return 1;
	}
	return 0;
}

static UpdateCheckResult CheckForLauncherUpdate()
{
	UpdateCheckResult result;
	const std::wstring scriptPath = CreateTempScriptPath(L"hcu");
	if (scriptPath.empty())
	{
		result.error = "Could not create temporary updater script.";
		return result;
	}

	const std::string script = R"PS(
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$api = 'https://api.github.com/repos/bokoxthexchocobo/HCDE/releases/latest'
$headers = @{ 'User-Agent' = 'HCDE-Updater' }
$release = Invoke-RestMethod -Uri $api -Headers $headers
$tag = [string]$release.tag_name
$zipAssets = @($release.assets | Where-Object { $_.name -match '(?i)\.zip$' })
$runtimeZipAssets = @($zipAssets | Where-Object {
	$name = [string]$_.name
	($name -match '(?i)^HCDE-.*-windows-x64\.zip$' -or $name -match '(?i)windows.*x64|win.*x64|hcde.*x64') -and
	($name -notmatch '(?i)(symbols?|pdb|debug|source|src)')
})
$asset = $null
if ($runtimeZipAssets.Count -gt 0) {
	$asset = $runtimeZipAssets |
		Sort-Object `
			@{ Expression = {
				$name = [string]$_.name
				$score = 0
				if ($name -match '(?i)^HCDE-.*-windows-x64\.zip$') { $score += 100 }
				if ($name -match '(?i)windows.*x64|win.*x64|hcde.*x64') { $score += 40 }
				if ($name -match '(?i)^hcde') { $score += 10 }
				if ($name -match '(?i)(symbols?|pdb|debug|source|src)') { $score -= 120 }
				$score
			}; Descending = $true },
			@{ Expression = { [string]$_.name }; Descending = $false } |
		Select-Object -First 1
}
$url = if ($asset) { [string]$asset.browser_download_url } else { '' }
$name = if ($asset) { [string]$asset.name } else { '' }
Write-Output ('TAG=' + $tag)
Write-Output ('URL=' + $url)
Write-Output ('ASSET=' + $name)
)PS";

	if (!WriteUtf8ScriptFile(scriptPath, script))
	{
		result.error = "Could not write temporary updater script.";
		DeleteFileW(scriptPath.c_str());
		return result;
	}

	const FString output = RunPowerShellScriptCapture(scriptPath);
	DeleteFileW(scriptPath.c_str());

	ParseKeyValueField(output, "TAG", result.latestTag);
	ParseKeyValueField(output, "URL", result.downloadUrl);
	ParseKeyValueField(output, "ASSET", result.assetName);

	result.latestTag.StripLeftRight();
	result.downloadUrl.StripLeftRight();
	result.assetName.StripLeftRight();

	if (result.latestTag.IsEmpty())
	{
		result.error = output.IsEmpty() ? "No update data returned from release API." : output;
		return result;
	}

	FString normalizedUrl;
	FString validationError;
	if (!ValidateUpdateUrl(result.downloadUrl, normalizedUrl, validationError))
	{
		result.error = validationError;
		return result;
	}
	result.downloadUrl = normalizedUrl;

	result.ok = true;
	return result;
}

static bool LaunchInstallerScript(const FString& versionTag, const FString& downloadUrl)
{
	if (downloadUrl.IsEmpty())
		return false;

	const std::wstring scriptPath = CreateTempScriptPath(L"hcu");
	if (scriptPath.empty())
		return false;

	const std::string script = R"PS(
param(
	[Parameter(Mandatory=$true)][int]$TargetPid,
	[Parameter(Mandatory=$true)][string]$InstallDir,
	[Parameter(Mandatory=$true)][string]$DownloadUrl,
	[Parameter(Mandatory=$true)][string]$VersionTag,
	[Parameter(Mandatory=$true)][string]$ExePath
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$uri = [Uri]$DownloadUrl
if ($uri.Scheme -ne 'https')
{
	throw "Update URL was rejected: only HTTPS is allowed."
}

$allowedHosts = @(
	'github.com',
	'api.github.com',
	'objects.githubusercontent.com',
	'release-assets.githubusercontent.com'
)

if (-not ($allowedHosts -contains $uri.Host.ToLowerInvariant()))
{
	throw "Update URL host is not trusted: $($uri.Host)"
}
if (-not ($uri.AbsolutePath -match '(?i)\.zip$'))
{
	throw "Update URL was rejected: expected a zip package."
}

function Get-InstallScopedHcdeProcesses {
	param([Parameter(Mandatory=$true)][string]$Root)

	$results = @()
	$seen = @{}
	# Include the launcher so lock waits also account for secondary launchers
	# started from the same install directory.
	$candidates = Get-Process -Name hcde,hcdeserv,hcdelaunch -ErrorAction SilentlyContinue

	foreach ($proc in $candidates) {
		$path = $null
		try {
			$path = $proc.Path
		} catch {
			$path = $null
		}
		if (-not $path) {
			try {
				$path = $proc.MainModule.FileName
			} catch {
				$path = $null
			}
		}

		if ($path -and $path.StartsWith($Root, [System.StringComparison]::OrdinalIgnoreCase)) {
			if (-not $seen.ContainsKey($proc.Id)) {
				$results += $proc
				$seen[$proc.Id] = $true
			}
		}
	}

	return $results
}

function Wait-ForInstallProcessesToExit {
	param(
		[Parameter(Mandatory=$true)][string]$Root,
		[int]$TimeoutSeconds = 45
	)

	$deadlineUtc = (Get-Date).ToUniversalTime().AddSeconds([Math]::Max(1, $TimeoutSeconds))
	while ($true) {
		$running = @(Get-InstallScopedHcdeProcesses -Root $Root)
		if ($running.Count -eq 0) {
			return
		}

		foreach ($proc in $running) {
			try {
				$proc.WaitForExit(400)
			} catch {
				# Ignore transient wait errors.
			}
		}

		if ((Get-Date).ToUniversalTime() -ge $deadlineUtc) {
			$names = ($running | ForEach-Object { "{0} (PID {1})" -f $_.ProcessName, $_.Id } | Sort-Object -Unique) -join ', '
			throw "HCDE processes are still running and holding files: $names"
		}
		Start-Sleep -Milliseconds 250
	}
}

function Wait-ForFileUnlock {
	param(
		[Parameter(Mandatory=$true)][string]$Path,
		[Parameter(Mandatory=$true)][string]$Root,
		[int]$TimeoutSeconds = 60
	)

	if (-not (Test-Path -LiteralPath $Path)) {
		return
	}

	$deadlineUtc = (Get-Date).ToUniversalTime().AddSeconds([Math]::Max(1, $TimeoutSeconds))
	while ($true) {
		$stream = $null
		try {
			$stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
			return
		} catch {
			if ((Get-Date).ToUniversalTime() -ge $deadlineUtc) {
				throw "Timed out waiting for file lock release: $Path ($($_.Exception.Message))"
			}
			Wait-ForInstallProcessesToExit -Root $Root -TimeoutSeconds 2
			Start-Sleep -Milliseconds 250
		} finally {
			if ($stream) {
				$stream.Dispose()
			}
		}
	}
}

function Copy-TreeWithRetries {
	param(
		[Parameter(Mandatory=$true)][string]$SourcePattern,
		[Parameter(Mandatory=$true)][string]$Destination,
		[Parameter(Mandatory=$true)][string]$Root,
		[Parameter(Mandatory=$true)][string]$Phase,
		[int]$MaxAttempts = 40
	)

	$attemptLimit = [Math]::Max(1, $MaxAttempts)
	for ($attempt = 1; $attempt -le $attemptLimit; $attempt++) {
		try {
			Copy-Item -Path $SourcePattern -Destination $Destination -Recurse -Force
			return
		} catch {
			if ($attempt -ge $attemptLimit) {
				throw "$Phase failed after $attempt attempts: $($_.Exception.Message)"
			}

			$running = @(Get-InstallScopedHcdeProcesses -Root $Root)
			foreach ($proc in $running) {
				try {
					$proc.WaitForExit(500)
				} catch {
					# Ignore transient wait errors and keep retrying.
				}
			}

			$delay = [Math]::Min(1500, 200 + ($attempt * 50))
			Start-Sleep -Milliseconds $delay
		}
	}
}

	# Phase 1: Wait for parent process (launcher) and any game/server instances to exit.
	# This ensures we can overwrite the executables without 'Permission Denied' errors.
	while (Get-Process -Id $TargetPid -ErrorAction SilentlyContinue)
	{
		Start-Sleep -Milliseconds 350
	}

	if (-not (Test-Path -LiteralPath $InstallDir))
	{
		throw "Install directory was not found: $InstallDir"
	}

	$resolvedInstallDir = (Resolve-Path -LiteralPath $InstallDir).ProviderPath
	$installRoot = [System.IO.Path]::GetPathRoot($resolvedInstallDir)
	if ([string]::IsNullOrWhiteSpace($installRoot))
	{
		throw "Install directory root could not be resolved: $resolvedInstallDir"
	}
	if ($resolvedInstallDir.TrimEnd('\') -eq $installRoot.TrimEnd('\'))
	{
		throw "Refusing to update at filesystem root: $resolvedInstallDir"
	}

	# Ensure install-local HCDE processes are fully gone and key files are writable.
	Wait-ForInstallProcessesToExit -Root $resolvedInstallDir -TimeoutSeconds 60
	$lockSensitiveFiles = @('hcde.exe', 'hcdeserv.exe', 'hcdelaunch.exe')
	foreach ($fileName in $lockSensitiveFiles) {
		Wait-ForFileUnlock -Path (Join-Path $resolvedInstallDir $fileName) -Root $resolvedInstallDir -TimeoutSeconds 60
	}

	$safeVersionTag = ($VersionTag -replace '[^A-Za-z0-9._-]', '_')
	if ([string]::IsNullOrWhiteSpace($safeVersionTag))
	{
		$safeVersionTag = 'unknown'
	}

	$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
	$backupDir = Join-Path $resolvedInstallDir 'backups'
	New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
	$backupZip = Join-Path $backupDir ("hcde-backup-$safeVersionTag-$timestamp.zip")
	$updateLog = Join-Path $backupDir ("hcde-update-$safeVersionTag-$timestamp.log")
	$statusFile = Join-Path $backupDir 'hcde-update-last-status.txt'
	$lockFile = Join-Path $backupDir 'hcde-update.lock'
	$status = 'failed'
	$statusMessage = 'Updater did not complete.'
	$launchRequested = $false
	$rollbackAttempted = $false
	$rollbackSucceeded = $false

	$workRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("hcde-update-" + [Guid]::NewGuid().ToString("N"))
	$archivePath = Join-Path $workRoot "hcde-update.zip"
	$extractPath = Join-Path $workRoot "extract"
	New-Item -ItemType Directory -Force -Path $workRoot | Out-Null

	# Phase 2: Manage updater lock to prevent concurrent update attempts.
	if (Test-Path -LiteralPath $lockFile)
	{
		$existingLock = Get-Item -LiteralPath $lockFile -ErrorAction SilentlyContinue
		$lockAgeMinutes = if ($existingLock) { ((Get-Date).ToUniversalTime() - $existingLock.LastWriteTimeUtc).TotalMinutes } else { 0.0 }
		if ($existingLock -and $lockAgeMinutes -ge 120)
		{
			Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
			"Removed stale updater lock: $lockFile" | Out-File -LiteralPath $updateLog -Encoding utf8
		}
		else
		{
			throw "Another updater run is already in progress."
		}
	}

	$lockContent = @(
		"PID=$PID",
		"VERSION=$VersionTag",
		"START_UTC=$((Get-Date).ToUniversalTime().ToString('o'))"
	) -join [Environment]::NewLine

	$lockBytes = [System.Text.Encoding]::UTF8.GetBytes($lockContent + [Environment]::NewLine)

	try
	{
		# Create the lock with CreateNew for atomic acquisition.
		$lockStream = [System.IO.File]::Open($lockFile, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
		try
		{
			$lockStream.Write($lockBytes, 0, $lockBytes.Length)
		}
		finally
		{
			$lockStream.Dispose()
		}
	}
	catch [System.IO.IOException]
	{
		throw "Another updater run is already in progress."
	}

	try
	{
		# Phase 3: Create pre-update backup.
		$backupItems = Get-ChildItem -LiteralPath $resolvedInstallDir -Force | Where-Object { $_.Name -ne 'backups' }
		if ($backupItems.Count -gt 0)
		{
			Compress-Archive -Path ($backupItems.FullName) -DestinationPath $backupZip -Force
			"Backup created: $backupZip" | Out-File -LiteralPath $updateLog -Encoding utf8
		}
		else
		{
			"No backup archive was created because the install directory had no content to archive." | Out-File -LiteralPath $updateLog -Encoding utf8
		}

		# Phase 4: Download and extract update.
		Invoke-WebRequest -Uri $DownloadUrl -OutFile $archivePath -UseBasicParsing
		Expand-Archive -Path $archivePath -DestinationPath $extractPath -Force

		# Handle nested directory in ZIP if applicable.
		$payloadRoot = $extractPath
		$children = Get-ChildItem -LiteralPath $extractPath -Force
		if ($children.Count -eq 1 -and $children[0].PSIsContainer)
		{
			$payloadRoot = $children[0].FullName
		}

		# Validation check for core executable.
		$coreExe = Join-Path $payloadRoot 'hcde.exe'
		if (-not (Test-Path -LiteralPath $coreExe))
		{
			throw "Update archive did not contain hcde.exe at payload root: $payloadRoot"
		}

		# Phase 5: Apply payload by copying over existing files with lock-aware retries.
		$payloadPattern = Join-Path $payloadRoot '*'
		Copy-TreeWithRetries -SourcePattern $payloadPattern -Destination $resolvedInstallDir -Root $resolvedInstallDir -Phase 'Payload copy'
		"Payload copied from: $payloadRoot" | Add-Content -LiteralPath $updateLog -Encoding utf8

		# Prune old backups to save space (keep last 6).
		$pruned = Get-ChildItem -LiteralPath $backupDir -Filter 'hcde-backup-*.zip' -File |
			Sort-Object LastWriteTime -Descending |
			Select-Object -Skip 6
		foreach ($old in $pruned)
		{
			Remove-Item -LiteralPath $old.FullName -Force -ErrorAction SilentlyContinue
		}
		$status = 'success'
		$statusMessage = 'Update applied successfully.'
		$launchRequested = $true
	}
	catch
	{
		# Phase 6: Error handling and automatic rollback.
		$statusMessage = $_.Exception.Message
		if (Test-Path -LiteralPath $backupZip)
		{
			$rollbackAttempted = $true
			try
			{
				$rollbackExtractPath = Join-Path $workRoot 'rollback'
				New-Item -ItemType Directory -Force -Path $rollbackExtractPath | Out-Null
				Expand-Archive -LiteralPath $backupZip -DestinationPath $rollbackExtractPath -Force

				$rollbackItems = Get-ChildItem -LiteralPath $rollbackExtractPath -Force
				if ($rollbackItems.Count -gt 0)
				{
					$rollbackPattern = Join-Path $rollbackExtractPath '*'
					Copy-TreeWithRetries -SourcePattern $rollbackPattern -Destination $resolvedInstallDir -Root $resolvedInstallDir -Phase 'Rollback copy'
					$rollbackSucceeded = $true
					$statusMessage = "$statusMessage (rollback restored from backup)"
					"Rollback restored from backup: $backupZip" | Add-Content -LiteralPath $updateLog -Encoding utf8 -ErrorAction SilentlyContinue
				}
				else
				{
					$statusMessage = "$statusMessage (rollback archive was empty)"
					"Rollback archive was empty: $backupZip" | Add-Content -LiteralPath $updateLog -Encoding utf8 -ErrorAction SilentlyContinue
				}
			}
			catch
			{
				$rollbackError = $_.Exception.Message
				$statusMessage = "$statusMessage (rollback failed: $rollbackError)"
				"Rollback failed: $rollbackError" | Add-Content -LiteralPath $updateLog -Encoding utf8 -ErrorAction SilentlyContinue
			}
		}
		else
		{
			$statusMessage = "$statusMessage (no rollback archive was available)"
		}

		"Updater failed: $statusMessage" | Add-Content -LiteralPath $updateLog -Encoding utf8 -ErrorAction SilentlyContinue
		throw
	}
finally
{
	@(
		"STATUS=$status",
		"VERSION=$VersionTag",
		"MESSAGE=$statusMessage",
		"LOG=$updateLog",
		"ROLLBACK_ATTEMPTED=$rollbackAttempted",
		"ROLLBACK_SUCCEEDED=$rollbackSucceeded",
		"TIMESTAMP_UTC=$((Get-Date).ToUniversalTime().ToString('o'))"
	) | Out-File -LiteralPath $statusFile -Encoding utf8 -Force
	Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
	Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
	# Best-effort cleanup of the transient updater helper script.
	$scriptFile = $MyInvocation.MyCommand.Path
	if ($scriptFile -and (Test-Path -LiteralPath $scriptFile))
	{
		Remove-Item -LiteralPath $scriptFile -Force -ErrorAction SilentlyContinue
	}
}

if ($launchRequested)
{
	Start-Process -FilePath $ExePath -WorkingDirectory $resolvedInstallDir
}
)PS";

	if (!WriteUtf8ScriptFile(scriptPath, script))
	{
		DeleteFileW(scriptPath.c_str());
		return false;
	}

	wchar_t modulePath[MAX_PATH] = {};
	if (!GetModuleFileNameW(nullptr, modulePath, _countof(modulePath)))
	{
		DeleteFileW(scriptPath.c_str());
		return false;
	}

	const std::wstring installDir = GetInstallDirPath();
	if (installDir.empty())
	{
		DeleteFileW(scriptPath.c_str());
		return false;
	}

	std::wstring params = L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ";
	params += QuoteWindowsCommandArg(scriptPath);
	params += L" -TargetPid ";
	params += std::to_wstring(GetCurrentProcessId());
	params += L" -InstallDir ";
	params += QuoteWindowsCommandArg(installDir);
	params += L" -DownloadUrl ";
	params += QuoteWindowsCommandArg(WideString(downloadUrl.GetChars()));
	params += L" -VersionTag ";
	params += QuoteWindowsCommandArg(WideString(versionTag.GetChars()));
	params += L" -ExePath ";
	params += QuoteWindowsCommandArg(modulePath);

	const HINSTANCE handle = ShellExecuteW(nullptr, L"open", L"powershell.exe", params.c_str(), nullptr, SW_HIDE);
	return reinterpret_cast<uintptr_t>(handle) > 32;
}
}
#endif

AboutPage::AboutPage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	// [Marcus] TODO: Probably make this rich-text
	Text = new TextEdit(this);
	Notes = new PushButton(this);
#ifdef _WIN32
	UpdateStatus = new TextLabel(this);
	CheckUpdates = new PushButton(this);
	InstallUpdate = new PushButton(this);
	OpenUpdateLogs = new PushButton(this);
	OpenLastUpdateLog = new PushButton(this);
#endif

	auto wad = BaseFileSearch(BASEWAD, NULL, true, GameConfig);
	if (wad)
	{
		// we need to be free
		auto resf = FResourceFile::OpenResourceFile(wad);
		FString text;

		auto append = [&resf,&text](const char * name) {
			auto lump = resf->FindEntry(name);
			if (lump < 0) return;
			auto data = resf->Read(lump);
			text.AppendCStrPart(data.string(), data.size());
		};
		if (resf)
		{
			append("about.txt");

			// [Marcus] I would love to instead have this done at compile time and also
			// separate the entries by ' · ', but there's currently a bug in zwidget
			// that breaks how long a soft-wrapped line of text can be :(
			text.AppendCharacter('\n');
			append("contributors.txt");

			text.StripLeftRight();
		}

		delete resf;

		Text->SetText(text.GetChars());
	}

	Text->SetReadOnly(true);
	Notes->SetText(GStrings.GetString("PICKER_SHOWNOTES"));
#ifdef _WIN32
	const LastUpdateRunStatus lastRun = ReadLastUpdateRunStatus();
	UpdaterLockState lockState = ReadUpdaterLockState();
	bool staleLockCleared = false;
	if (lockState.exists && lockState.stale)
	{
		staleLockCleared = TryClearStaleUpdaterLock(lockState);
	}

	if (lastRun.hasStatus && !lastRun.success)
	{
		const FString detail = BuildRunDetail(lastRun, "check updater log in backups");
		if (lastRun.rollbackAttempted && lastRun.rollbackSucceeded)
		{
			UpdateStatus->SetText(FStringf("Update status: Last updater run failed, rollback restored previous files (%s).", detail.GetChars()).GetChars());
		}
		else if (lastRun.rollbackAttempted)
		{
			UpdateStatus->SetText(FStringf("Update status: Last updater run failed and rollback also failed (%s).", detail.GetChars()).GetChars());
		}
		else
		{
			UpdateStatus->SetText(FStringf("Update status: Last updater run failed (%s).", detail.GetChars()).GetChars());
		}
		AutoCheckOnStartup = false;
		Launcher->SetUpdateNotice("Updater needs attention", true);
	}
	else if (lastRun.hasStatus && lastRun.success)
	{
		FString detail = lastRun.version.IsEmpty() ? "latest package" : lastRun.version;
		if (lastRun.timestampUtc.IsNotEmpty())
		{
			detail += FStringf(" [utc: %s]", lastRun.timestampUtc.GetChars());
		}
		UpdateStatus->SetText(FStringf("Update status: Last updater run succeeded (%s).", detail.GetChars()).GetChars());
		Launcher->SetUpdateNotice("", false);
	}
	else
	{
		UpdateStatus->SetText("Update status: Not checked yet.");
		Launcher->SetUpdateNotice("", false);
	}

	CheckUpdates->SetText("Check for updates");
	InstallUpdate->SetText("Install update");
	OpenUpdateLogs->SetText("Open updater backups");
	OpenLastUpdateLog->SetText("Open last updater log");

	if (lockState.exists)
	{
		UpdateLockActive = true;
		UpdateStatus->SetText(FStringf("Update status: Another updater run appears active (lock age %.1f min).", lockState.ageMinutes).GetChars());
		AutoCheckOnStartup = false;
		Launcher->SetUpdateNotice("Update in progress", true);
	}
	else if (staleLockCleared && !lastRun.hasStatus)
	{
		UpdateStatus->SetText("Update status: Cleared stale updater lock. Ready to check for updates.");
	}

	UpdateInstallButtonState();
#endif

	Notes->OnClick = [=,this]()
	{
		if (!Launcher->Release)
		{
			Launcher->Release = new ReleasePage(launcher, info);
			Launcher->Pages->AddTab(Launcher->Release, "Changelog");
			Launcher->UpdateLanguage();
		}

		Launcher->Pages->SetCurrentIndex(Launcher->Pages->GetPageIndex(Launcher->Release));
		Launcher->Pages->GetCurrentWidget()->SetFocus();
	};
#ifdef _WIN32
	CheckUpdates->OnClick = [this]() { OnCheckUpdatesClicked(); };
	InstallUpdate->OnClick = [this]() { OnInstallUpdateClicked(); };
	OpenUpdateLogs->OnClick = [this]() { OnOpenUpdateLogsClicked(); };
	OpenLastUpdateLog->OnClick = [this]() { OnOpenLastUpdateLogClicked(); };
	if (AutoCheckOnStartup)
	{
		OnCheckUpdatesClicked();
	}
#endif
}

void AboutPage::SetValues(FStartupSelectionInfo& info) const
{
}

void AboutPage::UpdateLanguage()
{
	Notes->SetText(GStrings.GetString("PICKER_SHOWNOTES"));
#ifdef _WIN32
	CheckUpdates->SetText("Check for updates");
	InstallUpdate->SetText("Install update");
	OpenUpdateLogs->SetText("Open updater backups");
	OpenLastUpdateLog->SetText("Open last updater log");
#endif
}

void AboutPage::OnGeometryChanged()
{
	double y = 0.0;
	double w = GetWidth();
	double h = GetHeight();
	double tw, th;
	th = Notes->GetPreferredHeight();
	tw = Notes->GetPreferredWidth();

	double footerHeight = Notes->GetPreferredHeight() + 8.0;
#ifdef _WIN32
	footerHeight += UpdateStatus->GetPreferredHeight() + 8.0;
	footerHeight += (std::max)(CheckUpdates->GetPreferredHeight(), InstallUpdate->GetPreferredHeight()) + 8.0;
	footerHeight += (std::max)(OpenUpdateLogs->GetPreferredHeight(), OpenLastUpdateLog->GetPreferredHeight()) + 8.0;
#endif
	Text->SetFrameGeometry(0.0, y, w, (std::max)(h - footerHeight, 0.0));
	y += (std::max)(h - footerHeight, 0.0);

#ifdef _WIN32
	const double statusHeight = UpdateStatus->GetPreferredHeight();
	UpdateStatus->SetFrameGeometry(0.0, y, w, statusHeight);
	y += statusHeight + 8.0;

	const double buttonHeight = (std::max)(CheckUpdates->GetPreferredHeight(), InstallUpdate->GetPreferredHeight());
	const double gap = 8.0;
	const double buttonWidth = (std::max)((w - gap) * 0.5, 0.0);
	CheckUpdates->SetFrameGeometry(0.0, y, buttonWidth, buttonHeight);
	InstallUpdate->SetFrameGeometry(buttonWidth + gap, y, buttonWidth, buttonHeight);
	y += buttonHeight + 8.0;

	const double logsButtonHeight = (std::max)(OpenUpdateLogs->GetPreferredHeight(), OpenLastUpdateLog->GetPreferredHeight());
	OpenUpdateLogs->SetFrameGeometry(0.0, y, buttonWidth, logsButtonHeight);
	OpenLastUpdateLog->SetFrameGeometry(buttonWidth + gap, y, buttonWidth, logsButtonHeight);
	y += logsButtonHeight + 8.0;
#endif

	Notes->SetFrameGeometry(round((w-tw)/2), y, tw, th);

	Launcher->UpdatePlayButton();
}

#ifdef _WIN32
void AboutPage::SetUpdateActionBusy(bool busy)
{
	UpdateActionBusy = busy;
	UpdateInstallButtonState();
}

void AboutPage::UpdateInstallButtonState()
{
	const bool hasUpdate = PendingUpdateVersion.IsNotEmpty() && PendingUpdateUrl.IsNotEmpty();
	CheckUpdates->SetEnabled(!UpdateActionBusy && !UpdateLockActive);
	InstallUpdate->SetEnabled(hasUpdate && !UpdateActionBusy && !UpdateLockActive);
	OpenUpdateLogs->SetEnabled(!UpdateActionBusy);
	OpenLastUpdateLog->SetEnabled(!UpdateActionBusy);
}

void AboutPage::OnCheckUpdatesClicked()
{
	if (UpdateActionBusy)
	{
		return;
	}

	UpdaterLockState lockState = ReadUpdaterLockState();
	if (lockState.exists && lockState.stale)
	{
		(void)TryClearStaleUpdaterLock(lockState);
	}

	UpdateLockActive = lockState.exists;
	if (UpdateLockActive)
	{
		UpdateStatus->SetText(FStringf("Update status: Another updater run appears active (lock age %.1f min).", lockState.ageMinutes).GetChars());
		Launcher->SetUpdateNotice("Update in progress", true);
		UpdateInstallButtonState();
		return;
	}

	SetUpdateActionBusy(true);
	UpdateStatus->SetText("Update status: Checking latest release...");
	Update();

	const UpdateCheckResult result = CheckForLauncherUpdate();
	if (!result.ok)
	{
		UpdateStatus->SetText(FStringf("Update status: check failed (%s)", result.error.GetChars()).GetChars());
		PendingUpdateVersion = "";
		PendingUpdateUrl = "";
		PendingAssetName = "";
		Launcher->SetUpdateNotice("", false);
		SetUpdateActionBusy(false);
		return;
	}

	FString latest = result.latestTag;
	latest.StripLeftRight();
	if (latest.Len() > 1 && (latest[0] == 'v' || latest[0] == 'V'))
	{
		latest = latest.Mid(1);
	}

	if (CompareVersionTags(latest, VERSIONSTR) > 0)
	{
		PendingUpdateVersion = latest;
		PendingUpdateUrl = result.downloadUrl;
		PendingAssetName = result.assetName;
		UpdateStatus->SetText(FStringf("Update status: v%s is available (%s).", latest.GetChars(), PendingAssetName.IsEmpty() ? "release zip found" : PendingAssetName.GetChars()).GetChars());
		Launcher->SetUpdateNotice(FStringf("Update available: v%s", latest.GetChars()), true);
	}
	else
	{
		PendingUpdateVersion = "";
		PendingUpdateUrl = "";
		PendingAssetName = "";
		UpdateStatus->SetText(FStringf("Update status: You're up to date (v%s).", VERSIONSTR).GetChars());
		Launcher->SetUpdateNotice("", false);
	}

	SetUpdateActionBusy(false);
}

void AboutPage::OnInstallUpdateClicked()
{
	if (UpdateActionBusy)
	{
		return;
	}

	UpdaterLockState lockState = ReadUpdaterLockState();
	if (lockState.exists && lockState.stale)
	{
		(void)TryClearStaleUpdaterLock(lockState);
	}

	UpdateLockActive = lockState.exists;
	if (UpdateLockActive)
	{
		UpdateStatus->SetText(FStringf("Update status: Another updater run appears active (lock age %.1f min).", lockState.ageMinutes).GetChars());
		Launcher->SetUpdateNotice("Update in progress", true);
		UpdateInstallButtonState();
		return;
	}

	FString normalizedUrl;
	FString validationError;
	if (PendingUpdateVersion.IsEmpty() || !ValidateUpdateUrl(PendingUpdateUrl, normalizedUrl, validationError))
	{
		UpdateStatus->SetText(FStringf("Update status: %s", validationError.IsEmpty() ? "No update package is selected yet." : validationError.GetChars()).GetChars());
		UpdateInstallButtonState();
		return;
	}

	PendingUpdateUrl = normalizedUrl;

	SetUpdateActionBusy(true);
	UpdateStatus->SetText("Update status: Launching updater (backup + replace + restart)...");
	Update();

	const bool launched = LaunchInstallerScript(PendingUpdateVersion, PendingUpdateUrl);
	if (!launched)
	{
		UpdateStatus->SetText("Update status: Failed to launch updater helper.");
		SetUpdateActionBusy(false);
		return;
	}

	Launcher->SetUpdateNotice("", false);
	UpdateStatus->SetText("Update status: Updater started. HCDE will close to apply the update.");
	Launcher->Exit();
}

void AboutPage::OnOpenUpdateLogsClicked()
{
	std::wstring backupsDir = GetInstallDirPath();
	if (backupsDir.empty())
	{
		UpdateStatus->SetText("Update status: Could not resolve install path for updater logs.");
		return;
	}
	backupsDir += L"\\backups";
	CreateDirectoryW(backupsDir.c_str(), nullptr);
	const HINSTANCE handle = ShellExecuteW(nullptr, L"open", backupsDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<uintptr_t>(handle) <= 32)
	{
		UpdateStatus->SetText("Update status: Could not open updater backups folder.");
	}
}

void AboutPage::OnOpenLastUpdateLogClicked()
{
	const LastUpdateRunStatus lastRun = ReadLastUpdateRunStatus();
	const std::wstring logPath = ResolveLastUpdateLogPath(lastRun);
	if (logPath.empty())
	{
		UpdateStatus->SetText("Update status: No updater log was found yet.");
		return;
	}

	const HINSTANCE handle = ShellExecuteW(nullptr, L"open", logPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<uintptr_t>(handle) <= 32)
	{
		UpdateStatus->SetText("Update status: Could not open the last updater log.");
	}
}
#endif

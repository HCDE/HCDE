# Getting Started

Quick paths into running and hosting HCDE. For full detail, use the wiki pages linked below.

## Install or build

See [Building](Building) for CMake requirements and Windows/Linux build commands.

Download a release from [GitHub Releases](https://github.com/bokoxthexchocobo/HCDE/releases) if you do not need to compile from source.

## Run a private dedicated server

```powershell
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 +map MAP01
```

## Join that server

```powershell
hcde -join 127.0.0.1:10666 -dedicatedjoin -iwad C:\Games\doom2.wad
```

## Public listing (optional)

```powershell
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 -advertise -master hcde.servebeer.com:15000 +map MAP01
```

## Single-player Invasion

Invasion (`sv_gametype 4`) also runs in local single-player. The console
player counts as a participant, so the wave director moves through
`WAITING -> COUNTDOWN -> SPAWNING` without a remote authority.

```powershell
hcde -iwad C:\Games\doom2.wad +map MAP01 +set sv_gametype 4 +set sv_invasioncountdowntime 10 +set sv_invasionwaves 8
```

External launchers (Doom Connector etc.) can pass `+sv_gametype 4` or
`+set sv_gametype 4`; HCDE preserves an explicit invasion gametype even
if `-coop` is also present. Once in-game, `invasion_status` shows the
current state and wave progression. Use `invasion_preset` from the
console to apply the recommended baseline CVARs, or `sv_gametype 0` to
return to coop.

## Where to go next

| Topic | Wiki page |
| --- | --- |
| Dedicated launch args, master server, browser protocol | [Launcher Protocol](Launcher-Protocol) |
| Engine netcode lanes, diagnostics, debug traces | [Network Protocol](Network-Protocol) |
| Windows in-launcher updates | [Windows Updater](Windows-Updater) |
| Console variables | [CVAR Reference](CVAR-Reference) |

Repository docs under `docs/` (netcode overhaul, stage notes) complement the wiki for contributors.

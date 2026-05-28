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

## Where to go next

| Topic | Wiki page |
| --- | --- |
| Dedicated launch args, master server, browser protocol | [Launcher Protocol](Launcher-Protocol) |
| Engine netcode lanes, diagnostics, debug traces | [Network Protocol](Network-Protocol) |
| Windows in-launcher updates | [Windows Updater](Windows-Updater) |
| Console variables | [CVAR Reference](CVAR-Reference) |

Repository docs under `docs/` (netcode overhaul, stage notes) complement the wiki for contributors.

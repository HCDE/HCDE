<div align="center">

[<img src="branding/hcde-logo.svg" alt="HCDE logo" style="width: 100%; max-width: 1600px;" />](.)

</div>

# HCDE

HCDE is a Doom-engine project focused on multiplayer, mod compatibility, and dedicated-server workflows.

**Documentation:** [HCDE Wiki](https://github.com/bokoxthexchocobo/HCDE/wiki) — build, dedicated servers, protocols, CVARs, and Windows updater guides.

## What ships in this repo

| Binary | Role |
| --- | --- |
| `hcde` | Client / game executable |
| `hcdeserv` | Dedicated server |
| `hcdemaster` | Standalone public-server registry (master server) |

Master protocol constants live in `protocol/` so engine, launcher, and master stay separate (`protocol/hcde_master_protocol.json`, `protocol/hcde_master_protocol.h`).

## Quick build

**Windows (Visual Studio):**

```powershell
cmake -S C:\path\to\HCDE -B C:\path\to\HCDE\build -G "Visual Studio 17 2022" -A x64
cmake --build C:\path\to\HCDE\build --config Release
```

**Linux:**

```bash
cmake -S /path/to/HCDE -B /path/to/HCDE/build -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/HCDE/build -j
```

See the wiki [Building](https://github.com/bokoxthexchocobo/HCDE/wiki/Building) page for requirements and output paths. See [Getting Started](https://github.com/bokoxthexchocobo/HCDE/wiki/Getting-Started) to host or join a dedicated server.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/` | Engine and networking |
| `protocol/` | Master protocol schema |
| `tools/hcdemaster/` | Standalone master server source |
| `wadsrc*` | Game resources and compat packs |
| `wiki/` | Source for the GitHub Wiki (published by CI) |
| `docs/` | Contributor netcode and review notes |

## Licensing

HCDE is **GPL-3.0-or-later** ([`LICENSE`](LICENSE)). Branding and some asset trees have separate terms — see `branding/BRANDING-LICENSE.md` and license files under `wadsrc_bm`, `wadsrc_extra`, and `wadsrc_widepix`.

## Contributors

[`contributors.md`](contributors.md) — HCDE contributors · [`CONTRIBUTORS`](CONTRIBUTORS) — upstream history

## Tech stack

CMake (≥ 3.16), C++20, Python 3; bundled libraries include ZMusic, ZVulkan, ZWidget, Abseil, WebP, LZMA, and others under `libraries/`. Per-component licenses: [`docs/licenses/`](docs/licenses/).

# HCDE Master Protocol

HCDE uses a separate master server process for public discovery. The master
server does not link against HCDE engine code; it only speaks UDP with the
dedicated server and launcher/browser clients.

## Boundary Rules

This file is the approved shared contract for HCDE public-server discovery.
Doom Connector may implement these packets in its own code, and the standalone
master server may implement the matching server side, but neither program should
copy HCDE engine implementation files or link against HCDE engine libraries.

Allowed shared material is limited to protocol facts: packet marker values,
field layouts, byte order, ports, time-to-live rules, and neutral generated
constants or schemas. Socket loops, storage logic, launcher behavior, engine
headers, and gameplay state remain implementation details of their own program.

The neutral machine-readable protocol file is
`protocol/hcde_master_protocol.json`. The protocol-only C++ constants in
`protocol/hcde_master_protocol.h` mirror that JSON data and contain no socket,
storage, launcher, engine, or gameplay implementation.

## UDP Port

Default master port: `15000`.

## Dedicated Server Startup

Dedicated servers advertise only when `sv_usemasters` is enabled. Use
`-master host[:port]` for startup master addresses:

```powershell
hcdeserv -server 1 -port 10666 -iwad doom2.wad +set sv_usemasters 1 -master hcde.servebeer.com +map MAP01
```

`-master` may be repeated. If no startup or archived masters are configured,
HCDE falls back to the built-in default master list. The `addmaster` console
command is still available at runtime and in archived configs, but launchers
should prefer `-master` because dedicated-server startup does not run delayed
console commands.

## Server Heartbeat

`hcdeserv` sends this packet to each configured master when `sv_usemasters` is
enabled:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | heartbeat marker `5560020` |
| 4 | 2 | little | public game server UDP port |

The master records the sender IP plus the supplied game port. Entries expire
after the master's configured TTL.

## Launcher List Query

Doom Connector sends:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | list query marker `777123` |

The master replies:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | response marker `777123` |
| 4 | 2 | little | server count |
| 6 | 6 * count | mixed | repeated IPv4 bytes followed by little-endian port |

The per-server launcher query is a different protocol and uses network byte
order. Keep these two wire formats separate unless both sides are versioned.

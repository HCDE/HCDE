# HCDE Netcode Architecture & Stabilization

This document provides a comprehensive technical overview of the HCDE networking architecture, protocol taxonomy, debugging facilities, and state repair mechanics. It serves as the primary reference for understanding player/weapon synchronization, latency calculation, and deterministic testing.

---

## 1. Latency & Backlog Metrics (Lead vs. Ping)

HCDE separates network transport delay (RTT) from client-side execution buffering (backlog). Historically, legacy code conflated these metrics into a single "Delay" number on the scoreboard, creating confusion during localhost play where network latency was sub-millisecond but the engine still felt buffered.

```
+-------------------------------------------------------------+
| Scoreboard Columns                                          |
|                                                             |
| Player Name      Frags      Lead (Tics)     Ping (ms)       |
| ----------------------------------------------------------- |
| DoomPlayer        42         6               0 ms           |
+-------------------------------------------------------------+
```

### 1.1 Command Lead (Tics)
**Command Lead** represents the client's local input and rendering buffer running *ahead* of the server-authoritative world simulation tick.
*   **Formula:** `LeadTics = ClientTic - gametic`
*   **Localhost Backlog (The 6-Tic Backlog):** In a localhost environment, the network ping is 0ms, yet a Command Lead of ~6 tics is consistently observed. This buffer is structurally required to keep prediction stable and is derived from a combination of:
    1.  **Consistency-Check Delay:** GZDoom's input pipeline buffers inputs to verify that tick packets have not been dropped or reordered before advancing.
    2.  **`extratics` Buffering:** Configured via `net_extratic` (defaulting to 1), this adds a safety margin to the client-side command queue to cushion against network jitter.
    3.  **Snapshot-to-Command Race:** The asynchronous thread boundaries between packet receipt and the main engine loop (`TryRunTics` vs. `NetUpdate`) introduce a pipeline alignment margin.
    4.  **`availableTics` Calculation:** Budget limits on how many world steps can be run per render frame force a minimum client-side command backlog to prevent the local prediction window from sliding into an unpredicted, stuttering state.

### 1.2 Ping (ms)
**Ping** represents the actual Round-Trip Time (RTT) of the underlying UDP transport packet network path.
*   **Sampling:** Sampled via high-resolution millisecond timestamps (`I_msTime()`) sent within keepalive heartbeat exchanges (`SendHeartbeat` / `GetPackets` network loop).
*   **Storage:** Stored in `FClientNetState::AverageLatency` for each active player slot.
*   **Diagnostics:** Logged once per second per peer to the `net.ping` `DebugTrace` channel:
    ```
    DebugTrace::Debugf("net.ping", "client=%d lead_tics=%d lead_ms=%d rtt_ms=%d TicDup=%d extratics=%d delta=%d", ...)
    ```

---

## 2. Live Packet Flow & Topology

The HCDE live gameplay protocol is categorized into separated network lanes to protect critical player commands from being starved by massive world replication traffic (e.g., in monster-heavy Invasion waves).

```
  Client                                                            Server
+--------+                                                        +--------+
|        | -------- [HGP_CLIENT_INPUTS] (Command Lane) ---------> |        |
|        |                                                        |        |
|        | <------- [HGP_SERVER_SNAPSHOT] (Snapshot Lane) ------- |        |
|        |                                                        |        |
|        | <------- [HLANE_PRESENTATION_ECHO] (CVar Gated) ------ |        |
+--------+                                                        +--------+
```

### 2.1 Command Lane (`HGP_CLIENT_INPUTS`)
Carries frequent, reliable, lightweight client inputs packed via `HCDEBuildNativeClientInputPayload`:
*   **Payload Contents:** User buttons, pitch, yaw, roll, forward/side/up movement, and current transaction sequence number acknowledgements.
*   **Priority:** Highest. Protected against world actor delta starvation.

### 2.2 Snapshot Lane (`HGP_SERVER_SNAPSHOT`)
Carries server-authoritative world updates packed via `HCDEBuildNativeServerSnapshotPayload` and delta arrays compiled in `HCDEAppendServerWorldDeltas`:
*   **Payload Contents:** Authority positions, velocities, angles, and basic health metrics of all replicated entities.

### 2.3 Presentation Echo Lane (`HLANE_PRESENTATION_ECHO`)
A diagnostic piggyback channel gated behind the console variable `net_echo_debug` (default 0). When active, it appends authoritative client states to snapshot payloads:
*   **Payload Size:** ~24 bytes per player at 1 Hz. Zero network or CPU cost when disabled.
*   **Echo Fields per Player:**
    *   `ReadyWeapon` class name (`FName` index, 4 bytes)
    *   `PendingWeapon` class name (`FName` index, 4 bytes)
    *   psprite `PSP_WEAPON` state name (`FName` index, 4 bytes) and remaining `Tics` (int16, 2 bytes)
    *   `WeaponState` status flags (uint16, 2 bytes)
    *   `playerstate` transition enum (uint8, 1 byte)
    *   `viewheight` (int16, 2 bytes, fixed-point `/256`)

---

## 3. Client-Side Desynchronization Warnings

When `net_echo_debug` is enabled, the client reads the incoming `HLANE_PRESENTATION_ECHO` packet inside `HCDEValidateServerWorldDeltas` and compares the server's authoritative state to its local prediction state. Any mismatch immediately triggers a warning to the `net.desync` `DebugTrace` channel:

```
[net.desync] [PLAYER STATE DESYNC] player=0 server playerstate=PST_LIVE local playerstate=PST_DEAD
[net.desync] [WEAPON DESYNC] player=0 server ReadyWeapon=Chainsaw client ReadyWeapon=None
[net.desync] [PSPRITE DESYNC] player=0 server state=Ready client state=Lower server tics=5 client tics=-1
```

### Common Desync Targets:
1.  **Player State Mismatches:** Occurs if a client predicts survival or death independently of the server.
2.  **Weapon Class Mismatches:** Triggered when the client locally equips a different weapon than the server recognizes.
3.  **Psprite Animations:** Triggered if a weapon animation gets stuck or lower/raise animations go out of sync (causing the "weapon stuck down" bug).
4.  **Viewheight Desyncs:** Occurs if the client and server disagree on crouch, jump, or portal-crossing vertical states.

---

## 4. DebugTrace Channel Taxonomy

Debugging networking problems in GZDoom has historically been difficult due to log noise. The `DebugTrace` system isolates logging into dedicated, high-performance, filterable channels:

| Channel Name | Severity | Purpose / Description |
| :--- | :--- | :--- |
| `net.ping` | `Debug` | Logs per-peer command backlog (`lead_tics`), network latency (`rtt_ms`), `TicDup`, and sequence deltas. |
| `net.desync` | `Warning` | Outputs state discrepancies found by the Presentation Echo or Mirror Parity checks. |
| `playsim.psprite` | `Debug` | Records logical weapon changes, layer modifications (`P_SetPsprite`), state loops, and setup sequences. |
| `playsim.playerstate` | `Mark` | Tracks player life cycles (`PST_LIVE` $\leftrightarrow$ `PST_DEAD` $\leftrightarrow$ `PST_REBORN` etc.) with custom contextual reason labels. |
| `playsim.actor` | `Info` | Tracks replication registration, mirror spawns, and flag configurations (`MF_SOLID`/`MF_SHOOTABLE`). |

---

## 5. State Repair Decision Tree

When the client processes snapshot inputs and world deltas, it must choose how to reconcile prediction errors. Snapping too aggressively causes visual stutter; failing to snap causes desynchronization (e.g., walking through walls or getting weapons permanently stuck).

```
                      Is player dead/respawned?
                             /         \
                           YES          NO
                           /              \
        Perform Hard Respawn Repair     Is there hard drift (> 128 units)?
        - Snap positions & velocities            /         \
        - Wipe old psprite stacks              YES          NO
        - Set PST_LIVE authoritative          /              \
                                     Perform Pose Snap     Queue Soft Repair
                                     - Instant snap        - Interpolate pose
                                     - Clear interpolators - Merge health smoothly
```

### 5.1 Hard Respawn Repair
*   **Trigger:** Server reports a live state (`PST_LIVE`) but the local client is still dead/corpse-like, or vice versa.
*   **Action:**
    1.  Clear corpse flags (`MF_CORPSE`) and restore baseline flags (`MF_SOLID | MF_SHOOTABLE`).
    2.  Hard-snap position, velocity, yaw, and pitch to server coordinates.
    3.  Wipe predicted psprite states and bring the authoritative weapon back up (`P_SetupPsprites` or `PickNewWeapon`).
    4.  Reset logical weapon flags (`player.WeaponState = 0`, `refire = 0`, `attackdown = false`) to match a clean spawn.

### 5.2 Hard Pose Snap
*   **Trigger:** Client drift relative to the server exceeds the threshold (`HCDEServerReconcileHardDistance` $\approx$ 128 units), often caused by teleporters, line portals, or ACS scripted teleports.
*   **Action:**
    1.  Instant hard snap to server coordinates.
    2.  Set `RF_NOINTERPOLATEVIEW` to prevent camera interpolation across portals or long distances.
    3.  Call `P_ClearPredictionData()` to wipe predicted movement frames.

### 5.3 Soft Drift Queue
*   **Trigger:** Client drift is minor and health/ground state corrections are needed.
*   **Action:**
    1.  Queue the correction using `HCDEQueuePredictedLocalHealthRepair`.
    2.  Apply health and pose modifications sequentially to maintain simulation smoothness.
    3.  Keep visual interpolators intact to hide corrections.

---

## 6. Automated Integration Testing (`net_self_test`)

To prevent regressions, the engine includes a fully automated integration test harness. By running the `net_self_test` console command, the engine orchestrates a complete client-server game session on a local loopback.

```
+------------------------------------------------------------+
| Parents / Main Thread                                      |
|                                                            |
|  - Spawns server process (hcdeserv.exe -port 5029)         |
|  - Spawns client process (hcde.exe -join 127.0.0.1:5029)   |
|  - Feeds deterministic commands for 12 seconds            |
|  - Terminates child processes & parses trace logs          |
|  - Outputs PASS/FAIL result based on desync counts         |
+------------------------------------------------------------+
```

### Deterministic Input Sequence
When `net_self_test_run_client` is enabled, the client's input engine runs a cyclic loop (250 tics) containing exact commands:
1.  **Tics 0–34 (Move):** Hold forward movement.
2.  **Tics 35–69 (Attack):** Fire primary weapon.
3.  **Tics 70–104 (Switch):** Switch/Zoom weapons.
4.  **Tics 105–139 (Move Back):** Hold backward movement.
5.  **Tics 140–174 (Respawn):** Trigger "Use" buttons (simulates respawn handoffs).
6.  **Tics 175–209 (Attack Again):** Re-engage primary firing loops.
7.  **Tics 210–249 (Idle / Finalize):** Finalize sequence.

This cycle evaluates prediction correctness, animation transitions, weapon switching, and respawn handoffs. If the Presentation Echo notices any state discrepancies during this loop, the test fails, isolating the exact desynchronized frame in the logs.

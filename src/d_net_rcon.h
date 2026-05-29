#pragma once

// HCDE RCON (Remote Console) -- scaffold.
//
// Roadmap: integration plan #24. The intent is a narrow admin-only command
// channel that an operator (or external launcher) can use to drive a
// dedicated server: kick clients, toggle map, dump diagnostics, etc.
//
// Scope of THIS scaffold (Phase 0 / surface only):
//
//   - Define `sv_rcon_enable` and `sv_rcon_password` CVARs so an operator
//     can configure an authority instance ahead of the actual transport
//     landing.
//   - Provide a `rcon_status` CCMD that prints whether RCON would be
//     accepting connections, whether a password is set, and reminds the
//     operator that the transport is not yet wired.
//   - Hold a single `HCDERconShouldAcceptCommands()` predicate so future
//     packet/transport code has a clear gating point.
//
// What is intentionally NOT here yet:
//
//   - No packet lane. RCON must NOT borrow the gameplay packet stream;
//     command/auth flow is a separate concern from snapshot/world
//     authority. A future change adds a dedicated packet ID (or a
//     side-channel TCP/UNIX socket) and a small command dispatcher.
//   - No password verification. Storing a verifier (HMAC of a server
//     nonce, or libsodium-style password hash) is part of the transport
//     change. This scaffold only knows whether a password is set, never
//     whether one matches.
//   - No allow/deny list of commands. The first transport patch must
//     ship with an explicit allowlist (no `exec`, no `dir`, no demo
//     manipulation, etc.) before exposing a real socket.
//
// See `docs/HCDE_RCON.md` for the full design.

#include <cstdint>

// True iff the operator has enabled RCON AND a password is configured.
// Returns false on listen hosts (RCON only makes sense for an authority
// running without a local console). The future transport gates on this.
bool HCDERconShouldAcceptCommands();

// True if a password is set (any non-empty value). Used by `rcon_status`
// without leaking the password itself.
bool HCDERconHasPasswordConfigured();

// Phase 1 transport-only state machine. These functions do not open sockets
// yet; they keep listener state/status coherent until the real TCP loop lands.
void HCDERconStartListener();
void HCDERconStopListener();
void HCDERconPollListener();

// Per-frame ingress drain stub. Future TCP loop will:
//   1. Accept any pending connections (cap at a small N -- maybe 4).
//   2. For each connection, recv() up to one length-prefixed frame.
//   3. Hand frames to the auth/dispatcher (not yet present).
//   4. Reject and disconnect on overlong frames or auth failure.
// Today this is a no-op so the eventual scheduler that ticks RCON has a
// stable call site. Returns the number of frames processed (0 today).
int HCDERconDrainIngress();

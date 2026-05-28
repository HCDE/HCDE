/*
** hcde_rcon_protocol.h
**
** Small authenticated UDP protocol used by the HCDE RCON utility.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#pragma once

#include <cstddef>
#include <cstdint>

// Wire format (all multi-byte fields are big-endian):
//
// Request:
//   uint32  RequestMarker  ('HCRQ', 0x48435251)
//   uint8   Version        (== Version constant below)
//   uint32  Nonce          (client-chosen; must be unique per request, the
//                           server keeps a small replay ring keyed on
//                           (sender, nonce))
//   uint32  Auth           (FNV-1a hash over "HCDE-RCON-v1" || Nonce ||
//                           password || command; see ComputeAuth)
//   bytes   Command        (printable ASCII, no NUL/CR/LF in the body,
//                           terminated by a single trailing 0x00)
//
// Response:
//   uint32  ResponseMarker ('HCRP', 0x48435250)
//   uint8   Version
//   uint8   Status         (enum Status below)
//   uint32  Nonce          (echoes the request nonce; client must match this
//                           against the original to bind reply to request)
//   bytes   Message        (UTF-8 text, NUL-terminated, fits in
//                           MaxMessageBytes including the terminator)
//
// The protocol is intentionally narrow: it carries one allowlisted command
// per packet, has no streaming, and assumes the admin runs the client tool
// over a private link or local loopback unless sv_rcon_allow_remote is set.

namespace hcde::rcon_protocol
{
constexpr uint32_t RequestMarker = 0x48435251u;  // HCRQ
constexpr uint32_t ResponseMarker = 0x48435250u; // HCRP
constexpr uint8_t Version = 1u;
constexpr uint16_t DefaultGamePort = 5029u;
constexpr size_t MaxCommandBytes = 512u;
constexpr size_t MaxMessageBytes = 512u;

enum Status : uint8_t
{
	StatusQueued = 0u,
	StatusRejected = 1u,
	StatusAuthFailed = 2u,
	StatusDisabled = 3u,
	StatusMalformed = 4u,
};

inline uint32_t Fnv1aUpdate(uint32_t hash, const void* data, size_t length)
{
	const uint8_t* bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < length; ++i)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

inline uint32_t ComputeAuth(const char* password, uint32_t nonce, const char* command)
{
	uint32_t hash = 2166136261u;
	const char domain[] = "HCDE-RCON-v1";
	const uint8_t nonceBytes[4] =
	{
		uint8_t(nonce >> 24),
		uint8_t(nonce >> 16),
		uint8_t(nonce >> 8),
		uint8_t(nonce),
	};
	hash = Fnv1aUpdate(hash, domain, sizeof(domain) - 1u);
	hash = Fnv1aUpdate(hash, nonceBytes, sizeof(nonceBytes));
	if (password != nullptr)
	{
		const char* p = password;
		while (*p != '\0')
			++p;
		hash = Fnv1aUpdate(hash, password, size_t(p - password));
	}
	if (command != nullptr)
	{
		const char* p = command;
		while (*p != '\0')
			++p;
		hash = Fnv1aUpdate(hash, command, size_t(p - command));
	}
	return hash;
}
}

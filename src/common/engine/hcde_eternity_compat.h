/*
** hcde_eternity_compat.h
**
** HCDE-managed compatibility detection for Eternity Engine mod resources.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#pragma once

enum EHCDEEternityCompatFlags : unsigned int
{
	HCDE_ETERNITYCOMPAT_EMAPINFO = 1u << 0,
	HCDE_ETERNITYCOMPAT_EDF = 1u << 1,
	HCDE_ETERNITYCOMPAT_EXTRADATA = 1u << 2,
};

void HCDE_EternityCompat_DetectLoadedResources();
bool HCDE_EternityCompat_IsActive(unsigned int flags);
unsigned int HCDE_EternityCompat_GetFlags();

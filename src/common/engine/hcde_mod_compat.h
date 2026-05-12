/*
** hcde_mod_compat.h
**
** HCDE-managed compatibility resources for known gameplay mods.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#pragma once

#include <vector>

#include "fs_filesystem.h"

class FConfigFile;

enum EHCDEModCompatFlags : unsigned int
{
	HCDE_MODCOMPAT_ALIENS_PLAYER0_INPUT = 1u << 0,
};

void HCDE_ModCompat_AppendFiles(std::vector<FileSys::ResourceName>& pwads, FConfigFile* config);
bool HCDE_ModCompat_IsActive(unsigned int flags);

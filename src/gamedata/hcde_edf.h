/*
** hcde_edf.h
**
** HCDE-owned parser for a supported subset of Eternity EDF.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#pragma once

#include <vector>

struct ReverbContainer;

// Extern declaration for the EDF-parsed reverb containers.
// These are created during EDF parsing and used by EMAPINFO to link
// defaultenvironment properties to actual reverb definitions.
extern std::vector<ReverbContainer*> HCDEReverbContainers;

void HCDE_EdfCompat_ParseLoadedResources();
void HCDE_EdfCompat_ApplyActorMappings();

// Clear all EDF-parsed reverb containers. Called during cleanup/reset.
void HCDE_ClearEDFReverbContainers();

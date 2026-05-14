/*
** hcde_emapinfo.h
**
** HCDE-owned parser for a supported subset of Eternity EMAPINFO.
**
**---------------------------------------------------------------------------
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
*/

#pragma once

struct level_info_t;

int HCDE_ParseEMapInfo(int lumpnum, level_info_t* defaultinfo);
void HCDE_ClearEMapInfoCompat();
bool HCDE_EMapInfo_IsEternityMap(const char* mapname);
int HCDE_EMapInfo_FindExtraDataLump(const char* mapname, const char* edname);

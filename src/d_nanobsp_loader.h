#pragma once

#include "tarray.h"
#include "utility/nodebuilder/nodebuild.h"

class FLevelLocals;

bool HCDENanoBSPShouldUseForBuildFromScratch();
bool HCDENanoBSPBuildFromScratch(
	FNodeBuilder::FLevel& leveldata,
	TArray<FNodeBuilder::FPolyStart>& polyspots,
	TArray<FNodeBuilder::FPolyStart>& anchors,
	bool buildGLNodes,
	FLevelLocals& level);


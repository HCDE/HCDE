// HCDE NanoBSP loader scaffold.
//
// Roadmap board item: #4 ("NanoBSP from Woof"). This file exposes the gating
// CVAR, a status CCMD, and a per-map dispatch counter. The vendored Woof
// `nano_bsp.c/.h` are present as reference source but not compiled directly:
// Woof's implementation assumes global arrays that HCDE does not expose. The
// runtime path below ports the NanoBSP partitioning logic onto HCDE map arrays
// and keeps the existing fallback/diagnostic guardrails.
//
// Hard rules carried over from the audit:
//
//   - Net/replay determinism. Two clients on the same WAD must produce
//     identical subsector ordering, segment-to-line mapping, and
//     line-of-sight results. If a future NanoBSP build cannot guarantee
//     that, the loader must keep value 0 (existing builder) for that map.
//   - Save/demo compatibility. Subsector indices are referenced by
//     gameplay paths; any reordering breaks saves. Hence the value 1 path
//     is "build-from-scratch only" -- maps with valid GL nodes are
//     untouched.
//   - Playsim isolation. The builder runs at map load; it must not reach
//     into actor flags, p_local helpers, or snapshot/replication.
//
// The per-call diagnostic added 2026-05-29 records the last 8 dispatches so
// soak runs can audit which maps actually request the NanoBSP path and why
// the adapter falls back. No behaviour change vs. the existing builder.

#include "d_nanobsp_loader.h"
#include "d_main.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "g_levellocals.h"
#include "printf.h"
#include "zstring.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Loader selector. Server-controlled in netgames so all clients agree on
// the build path; CVAR_SERVERINFO ensures the value participates in the
// existing server-info handshake the way other compat flags already do.
//
//   0 = Use the existing ZDBSP-style builder. (Default.)
//   1 = Use NanoBSP for build-from-scratch maps only (no valid GL nodes).
//   2 = Use NanoBSP unconditionally (advanced soak path).
CUSTOM_CVAR(Int, hcde_nanobsp_loader, 0, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}

namespace
{
constexpr int kHistorySlots = 8;

struct FNanoBSPDispatchRecord
{
	bool   Used = false;
	char   MapName[16] = "";
	int    VertexCount = 0;
	int    LineCount = 0;
	int    SideCount = 0;
	int    PolyspotCount = 0;
	int    AnchorCount = 0;
	bool   BuildGLNodes = false;
	bool   Emitted = false;
	uint32_t Blockers = 0;
	char   FallbackReason[96] = "";
};

int NanoBSPDispatchRequests = 0;
int NanoBSPAdapterFallbacks = 0;
int NanoBSPPreflightPasses = 0;
int NanoBSPPreflightRejects = 0;
int NanoBSPPolyobjectCases = 0;
int NanoBSPHistoryWriteCursor = 0;
FNanoBSPDispatchRecord NanoBSPHistory[kHistorySlots];

enum ENanoBSPAdapterBlocker : uint32_t
{
	HNBLOCK_NONE = 0,
	HNBLOCK_EMPTY_GEOMETRY = 1u << 0,
	HNBLOCK_MISSING_ARRAY = 1u << 1,
	HNBLOCK_POLYOBJECTS = 1u << 2,
	HNBLOCK_EMIT_FAILED = 1u << 3,
};

constexpr int kNanoBSPFastThreshold = 128;
constexpr int kNanoBSPSplitCost = 11;
constexpr fixed_t kNanoBSPDistanceEpsilon = FRACUNIT / 64;
constexpr int kNanoBSPNoIndex = -1;

struct FNanoBSPVertex
{
	fixed_t X = 0;
	fixed_t Y = 0;
};

struct FNanoBSPSeg
{
	int V1 = kNanoBSPNoIndex;
	int V2 = kNanoBSPNoIndex;
	int Line = kNanoBSPNoIndex;
	int Side = 0;
	sector_t* FrontSector = nullptr;
	sector_t* BackSector = nullptr;
	int Next = kNanoBSPNoIndex;
};

struct FNanoBSPNode
{
	int SegHead = kNanoBSPNoIndex; // Non-empty means leaf/subsector.
	int Index = kNanoBSPNoIndex;
	fixed_t X = 0;
	fixed_t Y = 0;
	fixed_t DX = 0;
	fixed_t DY = 0;
	FNanoBSPNode* Right = nullptr;
	FNanoBSPNode* Left = nullptr;
};

struct FNanoBSPBuildContext
{
	TArray<FNanoBSPVertex> Vertices;
	TArray<FNanoBSPSeg> Segs;
	TArray<node_t> Nodes;
	TArray<subsector_t> Subsectors;
	TArray<seg_t> OutputSegs;
	FNodeBuilder::FLevel* LevelData = nullptr;
	FLevelLocals* Level = nullptr;
	int NodeCount = 0;
	int SubsectorCount = 0;
};

struct FNanoBSPPartitionEval
{
	int Left = 0;
	int Right = 0;
	int Split = 0;
};

uint32_t PreflightAdapterInput(
	const FNodeBuilder::FLevel& leveldata,
	const TArray<FNodeBuilder::FPolyStart>& polyspots,
	const TArray<FNodeBuilder::FPolyStart>& anchors)
{
	uint32_t blockers = HNBLOCK_NONE;
	if (leveldata.NumVertices <= 0 || leveldata.NumLines <= 0 || leveldata.NumSides <= 0)
	{
		blockers |= HNBLOCK_EMPTY_GEOMETRY;
	}
	if (leveldata.Vertices == nullptr || leveldata.Lines == nullptr || leveldata.Sides == nullptr)
	{
		blockers |= HNBLOCK_MISSING_ARRAY;
	}
	if (polyspots.Size() > 0 || anchors.Size() > 0)
	{
		blockers |= HNBLOCK_POLYOBJECTS;
	}
	return blockers;
}

void DeleteNanoBSPTree(FNanoBSPNode* node)
{
	if (node == nullptr)
		return;
	DeleteNanoBSPTree(node->Left);
	DeleteNanoBSPTree(node->Right);
	delete node;
}

int NanoBSPPushSeg(FNanoBSPBuildContext& ctx, const FNanoBSPSeg& seg)
{
	return int(ctx.Segs.Push(seg));
}

int NanoBSPPushVertex(FNanoBSPBuildContext& ctx, fixed_t x, fixed_t y)
{
	FNanoBSPVertex vertex;
	vertex.X = x;
	vertex.Y = y;
	return int(ctx.Vertices.Push(vertex));
}

int NanoBSPCreateSegs(FNanoBSPBuildContext& ctx)
{
	int listHead = kNanoBSPNoIndex;
	FNodeBuilder::FLevel& leveldata = *ctx.LevelData;

	for (int lineIndex = 0; lineIndex < leveldata.NumLines; ++lineIndex)
	{
		line_t& line = leveldata.Lines[lineIndex];
		for (int side = 0; side < 2; ++side)
		{
			if (line.sidedef[side] == nullptr)
				continue;

			const ptrdiff_t frontVertex = line.v1 - leveldata.Vertices;
			const ptrdiff_t backVertex = line.v2 - leveldata.Vertices;
			if (frontVertex < 0 || frontVertex >= leveldata.NumVertices ||
				backVertex < 0 || backVertex >= leveldata.NumVertices)
			{
				return kNanoBSPNoIndex;
			}

			FNanoBSPSeg seg;
			seg.V1 = side == 0 ? int(frontVertex) : int(backVertex);
			seg.V2 = side == 0 ? int(backVertex) : int(frontVertex);
			seg.Line = lineIndex;
			seg.Side = side;
			seg.FrontSector = side == 0 ? line.frontsector : line.backsector;
			seg.BackSector = side == 0 ? line.backsector : line.frontsector;
			seg.Next = listHead;
			listHead = NanoBSPPushSeg(ctx, seg);
		}
	}
	return listHead;
}

void NanoBSPBoundingBox(const FNanoBSPBuildContext& ctx, int head, fixed_t bbox[4])
{
	bbox[BOXLEFT] = bbox[BOXBOTTOM] = INT_MAX;
	bbox[BOXRIGHT] = bbox[BOXTOP] = INT_MIN;
	for (int segIndex = head; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
	{
		const FNanoBSPSeg& seg = ctx.Segs[segIndex];
		const FNanoBSPVertex& v1 = ctx.Vertices[seg.V1];
		const FNanoBSPVertex& v2 = ctx.Vertices[seg.V2];
		bbox[BOXLEFT] = std::min({ bbox[BOXLEFT], v1.X, v2.X });
		bbox[BOXRIGHT] = std::max({ bbox[BOXRIGHT], v1.X, v2.X });
		bbox[BOXBOTTOM] = std::min({ bbox[BOXBOTTOM], v1.Y, v2.Y });
		bbox[BOXTOP] = std::max({ bbox[BOXTOP], v1.Y, v2.Y });
	}
}

void NanoBSPMergeBounds(fixed_t out[4], const fixed_t box1[4], const fixed_t box2[4])
{
	out[BOXLEFT] = std::min(box1[BOXLEFT], box2[BOXLEFT]);
	out[BOXBOTTOM] = std::min(box1[BOXBOTTOM], box2[BOXBOTTOM]);
	out[BOXRIGHT] = std::max(box1[BOXRIGHT], box2[BOXRIGHT]);
	out[BOXTOP] = std::max(box1[BOXTOP], box2[BOXTOP]);
}

int NanoBSPPointOnSide(const FNanoBSPBuildContext& ctx, const FNanoBSPSeg& part, fixed_t x, fixed_t y)
{
	const FNanoBSPVertex& v1 = ctx.Vertices[part.V1];
	const FNanoBSPVertex& v2 = ctx.Vertices[part.V2];
	const int64_t dx = int64_t(v2.X) - int64_t(v1.X);
	const int64_t dy = int64_t(v2.Y) - int64_t(v1.Y);
	const int64_t relx = int64_t(x) - int64_t(v1.X);
	const int64_t rely = int64_t(y) - int64_t(v1.Y);
	const int64_t cross = relx * dy - rely * dx;
	const int64_t epsilon = int64_t(kNanoBSPDistanceEpsilon) * std::max<int64_t>(std::llabs(dx), std::llabs(dy));
	if (cross < -epsilon)
		return 1;
	if (cross > epsilon)
		return -1;
	return 0;
}

bool NanoBSPSameDirection(const FNanoBSPBuildContext& ctx, const FNanoBSPSeg& part, const FNanoBSPSeg& seg)
{
	const FNanoBSPVertex& pv1 = ctx.Vertices[part.V1];
	const FNanoBSPVertex& pv2 = ctx.Vertices[part.V2];
	const FNanoBSPVertex& sv1 = ctx.Vertices[seg.V1];
	const FNanoBSPVertex& sv2 = ctx.Vertices[seg.V2];
	const int64_t pdx = int64_t(pv2.X) - int64_t(pv1.X);
	const int64_t pdy = int64_t(pv2.Y) - int64_t(pv1.Y);
	const int64_t sdx = int64_t(sv2.X) - int64_t(sv1.X);
	const int64_t sdy = int64_t(sv2.Y) - int64_t(sv1.Y);
	return sdx * pdx + sdy * pdy > 0;
}

int NanoBSPSegOnSide(const FNanoBSPBuildContext& ctx, int partIndex, int segIndex)
{
	if (partIndex == segIndex)
		return 1;

	const FNanoBSPSeg& part = ctx.Segs[partIndex];
	const FNanoBSPSeg& seg = ctx.Segs[segIndex];
	const FNanoBSPVertex& v1 = ctx.Vertices[seg.V1];
	const FNanoBSPVertex& v2 = ctx.Vertices[seg.V2];
	const int side1 = NanoBSPPointOnSide(ctx, part, v1.X, v1.Y);
	const int side2 = NanoBSPPointOnSide(ctx, part, v2.X, v2.Y);
	if (side1 == 0 && side2 == 0)
		return NanoBSPSameDirection(ctx, part, seg) ? 1 : -1;
	if ((side1 * side2) < 0)
		return 0;
	return (side1 >= 0 && side2 >= 0) ? 1 : -1;
}

bool NanoBSPEvalPartition(const FNanoBSPBuildContext& ctx, int partIndex, int soup, FNanoBSPPartitionEval& eval)
{
	eval = {};
	const FNanoBSPSeg& part = ctx.Segs[partIndex];
	const FNanoBSPVertex& v1 = ctx.Vertices[part.V1];
	const FNanoBSPVertex& v2 = ctx.Vertices[part.V2];
	if (std::abs(v2.X - v1.X) < 4 * kNanoBSPDistanceEpsilon &&
		std::abs(v2.Y - v1.Y) < 4 * kNanoBSPDistanceEpsilon)
	{
		return false;
	}

	for (int segIndex = soup; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
	{
		const int side = NanoBSPSegOnSide(ctx, partIndex, segIndex);
		if (side < 0) ++eval.Left;
		else if (side > 0) ++eval.Right;
		else ++eval.Split;
	}
	return eval.Split > 0 || (eval.Left > 0 && eval.Right > 0);
}

int NanoBSPPickNodeFast(const FNanoBSPBuildContext& ctx, int soup)
{
	int count = 0;
	for (int segIndex = soup; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
		++count;
	if (count < kNanoBSPFastThreshold)
		return kNanoBSPNoIndex;

	fixed_t bbox[4];
	NanoBSPBoundingBox(ctx, soup, bbox);
	const fixed_t midX = bbox[BOXLEFT] / 2 + bbox[BOXRIGHT] / 2;
	const fixed_t midY = bbox[BOXBOTTOM] / 2 + bbox[BOXTOP] / 2;
	int verticalPart = kNanoBSPNoIndex;
	int horizontalPart = kNanoBSPNoIndex;
	fixed_t verticalDistance = INT_MAX;
	fixed_t horizontalDistance = INT_MAX;

	for (int segIndex = soup; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
	{
		const FNanoBSPSeg& seg = ctx.Segs[segIndex];
		const FNanoBSPVertex& v1 = ctx.Vertices[seg.V1];
		const FNanoBSPVertex& v2 = ctx.Vertices[seg.V2];
		if (v1.X == v2.X)
		{
			const fixed_t distance = std::abs(v1.X - midX);
			if (distance < verticalDistance)
			{
				verticalPart = segIndex;
				verticalDistance = distance;
			}
		}
		else if (v1.Y == v2.Y)
		{
			const fixed_t distance = std::abs(v1.Y - midY);
			if (distance < horizontalDistance)
			{
				horizontalPart = segIndex;
				horizontalDistance = distance;
			}
		}
	}

	FNanoBSPPartitionEval verticalEval, horizontalEval;
	const bool verticalOk = verticalPart != kNanoBSPNoIndex && NanoBSPEvalPartition(ctx, verticalPart, soup, verticalEval);
	const bool horizontalOk = horizontalPart != kNanoBSPNoIndex && NanoBSPEvalPartition(ctx, horizontalPart, soup, horizontalEval);
	if (verticalOk && horizontalOk)
	{
		const int verticalCost = std::abs(verticalEval.Left - verticalEval.Right) * 2 + verticalEval.Split * kNanoBSPSplitCost;
		const int horizontalCost = std::abs(horizontalEval.Left - horizontalEval.Right) * 2 + horizontalEval.Split * kNanoBSPSplitCost;
		return horizontalCost < verticalCost ? horizontalPart : verticalPart;
	}
	if (verticalOk) return verticalPart;
	if (horizontalOk) return horizontalPart;
	return kNanoBSPNoIndex;
}

int NanoBSPPickNodeSlow(const FNanoBSPBuildContext& ctx, int soup)
{
	int best = kNanoBSPNoIndex;
	int bestCost = INT_MAX;
	for (int segIndex = soup; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
	{
		FNanoBSPPartitionEval eval;
		if (!NanoBSPEvalPartition(ctx, segIndex, soup, eval))
			continue;
		const int cost = std::abs(eval.Left - eval.Right) * 2 + eval.Split * kNanoBSPSplitCost;
		if (cost < bestCost)
		{
			best = segIndex;
			bestCost = cost;
		}
	}
	return best;
}

int NanoBSPComputeIntersection(FNanoBSPBuildContext& ctx, const FNanoBSPSeg& part, const FNanoBSPSeg& seg)
{
	const FNanoBSPVertex& p1 = ctx.Vertices[part.V1];
	const FNanoBSPVertex& p2 = ctx.Vertices[part.V2];
	const FNanoBSPVertex& s1 = ctx.Vertices[seg.V1];
	const FNanoBSPVertex& s2 = ctx.Vertices[seg.V2];
	const long double px = p1.X;
	const long double py = p1.Y;
	const long double rx = (long double)p2.X - p1.X;
	const long double ry = (long double)p2.Y - p1.Y;
	const long double qx = s1.X;
	const long double qy = s1.Y;
	const long double sx = (long double)s2.X - s1.X;
	const long double sy = (long double)s2.Y - s1.Y;
	const long double denom = sx * ry - sy * rx;
	if (denom == 0.0L)
	{
		return NanoBSPPushVertex(ctx, s1.X, s1.Y);
	}
	const long double along = ((px - qx) * ry - (py - qy) * rx) / denom;
	const fixed_t x = fixed_t(std::llround(qx + sx * along));
	const fixed_t y = fixed_t(std::llround(qy + sy * along));
	return NanoBSPPushVertex(ctx, x, y);
}

void NanoBSPPrepend(FNanoBSPBuildContext& ctx, int segIndex, int& listHead)
{
	ctx.Segs[segIndex].Next = listHead;
	listHead = segIndex;
}

void NanoBSPSplitSegs(FNanoBSPBuildContext& ctx, int partIndex, int soup, int& lefts, int& rights)
{
	lefts = kNanoBSPNoIndex;
	rights = kNanoBSPNoIndex;
	while (soup != kNanoBSPNoIndex)
	{
		const int segIndex = soup;
		soup = ctx.Segs[segIndex].Next;
		ctx.Segs[segIndex].Next = kNanoBSPNoIndex;

		const int where = NanoBSPSegOnSide(ctx, partIndex, segIndex);
		if (where < 0)
		{
			NanoBSPPrepend(ctx, segIndex, lefts);
			continue;
		}
		if (where > 0)
		{
			NanoBSPPrepend(ctx, segIndex, rights);
			continue;
		}

		FNanoBSPSeg split = ctx.Segs[segIndex];
		const int splitVertex = NanoBSPComputeIntersection(ctx, ctx.Segs[partIndex], ctx.Segs[segIndex]);
		split.V1 = splitVertex;
		ctx.Segs[segIndex].V2 = splitVertex;
		const int splitIndex = NanoBSPPushSeg(ctx, split);

		if (NanoBSPPointOnSide(ctx, ctx.Segs[partIndex],
			ctx.Vertices[ctx.Segs[segIndex].V1].X,
			ctx.Vertices[ctx.Segs[segIndex].V1].Y) < 0)
		{
			NanoBSPPrepend(ctx, segIndex, lefts);
			NanoBSPPrepend(ctx, splitIndex, rights);
		}
		else
		{
			NanoBSPPrepend(ctx, segIndex, rights);
			NanoBSPPrepend(ctx, splitIndex, lefts);
		}
	}
}

FNanoBSPNode* NanoBSPCreateLeaf(int soup)
{
	FNanoBSPNode* node = new FNanoBSPNode;
	node->SegHead = soup;
	return node;
}

FNanoBSPNode* NanoBSPSubdivideSegs(FNanoBSPBuildContext& ctx, int soup)
{
	int part = NanoBSPPickNodeFast(ctx, soup);
	if (part == kNanoBSPNoIndex)
		part = NanoBSPPickNodeSlow(ctx, soup);
	if (part == kNanoBSPNoIndex)
		return NanoBSPCreateLeaf(soup);

	FNanoBSPNode* node = new FNanoBSPNode;
	const FNanoBSPVertex& v1 = ctx.Vertices[ctx.Segs[part].V1];
	const FNanoBSPVertex& v2 = ctx.Vertices[ctx.Segs[part].V2];
	node->X = v1.X;
	node->Y = v1.Y;
	node->DX = v2.X - v1.X;
	node->DY = v2.Y - v1.Y;
	const fixed_t minSize = 64 * FRACUNIT;
	while (std::abs(node->DX) < minSize && std::abs(node->DY) < minSize)
	{
		node->DX *= 2;
		node->DY *= 2;
	}

	int lefts, rights;
	NanoBSPSplitSegs(ctx, part, soup, lefts, rights);
	if (lefts == kNanoBSPNoIndex || rights == kNanoBSPNoIndex)
	{
		delete node;
		return NanoBSPCreateLeaf(lefts != kNanoBSPNoIndex ? lefts : rights);
	}
	node->Right = NanoBSPSubdivideSegs(ctx, rights);
	node->Left = NanoBSPSubdivideSegs(ctx, lefts);
	return node;
}

void NanoBSPCountStuff(FNanoBSPBuildContext& ctx, FNanoBSPNode* node)
{
	if (node->SegHead != kNanoBSPNoIndex)
	{
		node->Index = ctx.SubsectorCount++;
		return;
	}
	NanoBSPCountStuff(ctx, node->Left);
	NanoBSPCountStuff(ctx, node->Right);
	node->Index = ctx.NodeCount++;
}

uint32_t NanoBSPWriteNode(FNanoBSPBuildContext& ctx, FNanoBSPNode* nanoNode, fixed_t bbox[4])
{
	uint32_t index = uint32_t(nanoNode->Index);
	if (nanoNode->SegHead != kNanoBSPNoIndex)
	{
		index |= 0x80000000u;
		NanoBSPBoundingBox(ctx, nanoNode->SegHead, bbox);
		subsector_t& out = ctx.Subsectors[nanoNode->Index];
		std::memset(&out, 0, sizeof(out));
		out.firstline = reinterpret_cast<seg_t*>(uintptr_t(ctx.OutputSegs.Size()));
		for (int segIndex = nanoNode->SegHead; segIndex != kNanoBSPNoIndex; segIndex = ctx.Segs[segIndex].Next)
		{
			const FNanoBSPSeg& seg = ctx.Segs[segIndex];
			seg_t outSeg = {};
			outSeg.v1 = &ctx.Level->vertexes[seg.V1];
			outSeg.v2 = &ctx.Level->vertexes[seg.V2];
			outSeg.linedef = &ctx.LevelData->Lines[seg.Line];
			outSeg.sidedef = seg.Side >= 0 ? ctx.LevelData->Lines[seg.Line].sidedef[seg.Side] : nullptr;
			outSeg.frontsector = seg.FrontSector;
			outSeg.backsector = seg.BackSector;
			outSeg.PartnerSeg = nullptr;
			outSeg.Subsector = &out;
			outSeg.segnum = int(ctx.OutputSegs.Size());
			ctx.OutputSegs.Push(outSeg);
			++out.numlines;
		}
		return index;
	}

	node_t& out = ctx.Nodes[nanoNode->Index];
	std::memset(&out, 0, sizeof(out));
	out.x = nanoNode->X;
	out.y = nanoNode->Y;
	out.dx = nanoNode->DX;
	out.dy = nanoNode->DY;
	for (int childIndex = 0; childIndex < 2; ++childIndex)
	{
		FNanoBSPNode* child = childIndex == 0 ? nanoNode->Right : nanoNode->Left;
		out.intchildren[childIndex] = NanoBSPWriteNode(ctx, child, out.nb_bbox[childIndex]);
	}
	NanoBSPMergeBounds(bbox, out.nb_bbox[0], out.nb_bbox[1]);
	return index;
}

bool NanoBSPExtractToLevel(FNanoBSPBuildContext& ctx, FNanoBSPNode* root)
{
	ctx.NodeCount = 0;
	ctx.SubsectorCount = 0;
	NanoBSPCountStuff(ctx, root);
	if (ctx.SubsectorCount <= 0)
		return false;

	TArray<int> lineV1;
	TArray<int> lineV2;
	lineV1.Alloc(ctx.LevelData->NumLines);
	lineV2.Alloc(ctx.LevelData->NumLines);
	for (int i = 0; i < ctx.LevelData->NumLines; ++i)
	{
		lineV1[i] = int(ctx.LevelData->Lines[i].v1 - ctx.LevelData->Vertices);
		lineV2[i] = int(ctx.LevelData->Lines[i].v2 - ctx.LevelData->Vertices);
	}

	ctx.Level->vertexes.Alloc(ctx.Vertices.Size());
	for (unsigned int i = 0; i < ctx.Vertices.Size(); ++i)
	{
		ctx.Level->vertexes[i].set(ctx.Vertices[i].X, ctx.Vertices[i].Y);
		ctx.Level->vertexes[i].vertexnum = i;
	}

	ctx.Nodes.Alloc(ctx.NodeCount);
	ctx.Subsectors.Alloc(ctx.SubsectorCount);
	ctx.OutputSegs.Clear();
	fixed_t dummy[4];
	NanoBSPWriteNode(ctx, root, dummy);

	ctx.Level->nodes.Alloc(ctx.Nodes.Size());
	for (unsigned int i = 0; i < ctx.Nodes.Size(); ++i)
	{
		ctx.Level->nodes[i] = ctx.Nodes[i];
		ctx.Level->nodes[i].nodenum = i;
	}
	ctx.Level->subsectors.Alloc(ctx.Subsectors.Size());
	for (unsigned int i = 0; i < ctx.Subsectors.Size(); ++i)
	{
		ctx.Level->subsectors[i] = ctx.Subsectors[i];
		ctx.Level->subsectors[i].subsectornum = i;
	}
	ctx.Level->segs.Alloc(ctx.OutputSegs.Size());
	for (unsigned int i = 0; i < ctx.OutputSegs.Size(); ++i)
	{
		ctx.Level->segs[i] = ctx.OutputSegs[i];
		ctx.Level->segs[i].segnum = i;
	}
	for (unsigned int i = 0; i < ctx.Level->subsectors.Size(); ++i)
	{
		subsector_t& sub = ctx.Level->subsectors[i];
		sub.firstline = &ctx.Level->segs[uintptr_t(sub.firstline)];
		for (unsigned int j = 0; j < sub.numlines; ++j)
		{
			sub.firstline[j].Subsector = &sub;
		}
	}
	for (unsigned int i = 0; i < ctx.Level->nodes.Size(); ++i)
	{
		node_t& node = ctx.Level->nodes[i];
		for (int child = 1; child >= 0; --child)
		{
			if (node.intchildren[child] & 0x80000000u)
			{
				node.children[child] = reinterpret_cast<uint8_t*>(&ctx.Level->subsectors[node.intchildren[child] & 0x7fffffffu]) + 1;
			}
			else
			{
				node.children[child] = &ctx.Level->nodes[node.intchildren[child]];
			}
		}
		for (int side = 0; side < 2; ++side)
		{
			for (int box = 0; box < 4; ++box)
			{
				node.bbox[side][box] = FIXED2FLOAT(node.nb_bbox[side][box]);
			}
		}
	}
	for (int i = 0; i < ctx.LevelData->NumLines; ++i)
	{
		ctx.LevelData->Lines[i].v1 = &ctx.Level->vertexes[lineV1[i]];
		ctx.LevelData->Lines[i].v2 = &ctx.Level->vertexes[lineV2[i]];
	}
	return true;
}

void DescribeAdapterBlockers(uint32_t blockers, char* out, size_t outSize)
{
	if (blockers == HNBLOCK_NONE)
	{
		std::snprintf(out, outSize, "adapter-ready");
		return;
	}

	const char* separator = "";
	std::snprintf(out, outSize, "adapter-blocked:");
	size_t used = std::strlen(out);
	auto append = [&](const char* text)
	{
		if (used >= outSize)
			return;
		const int written = std::snprintf(out + used, outSize - used, "%s%s", separator, text);
		if (written > 0)
		{
			used += size_t(written);
			if (used > outSize) used = outSize;
			separator = ",";
		}
	};

	if ((blockers & HNBLOCK_EMPTY_GEOMETRY) != 0) append("empty-geometry");
	if ((blockers & HNBLOCK_MISSING_ARRAY) != 0) append("missing-array");
	if ((blockers & HNBLOCK_POLYOBJECTS) != 0) append("polyobjects");
	if ((blockers & HNBLOCK_EMIT_FAILED) != 0) append("emit-failed");
}

void RecordDispatch(
	const FLevelLocals& level,
	int vertexCount,
	int lineCount,
	int sideCount,
	int polyspotCount,
	int anchorCount,
	bool buildGLNodes,
	bool emitted,
	uint32_t blockers,
	const char* fallbackReason)
{
	const int slot = NanoBSPHistoryWriteCursor % kHistorySlots;
	FNanoBSPDispatchRecord& rec = NanoBSPHistory[slot];
	rec.Used = true;
	const char* mapname = level.MapName.GetChars();
	if (mapname == nullptr) mapname = "<unknown>";
	std::strncpy(rec.MapName, mapname, sizeof(rec.MapName) - 1);
	rec.MapName[sizeof(rec.MapName) - 1] = '\0';
	rec.VertexCount = vertexCount;
	rec.LineCount = lineCount;
	rec.SideCount = sideCount;
	rec.PolyspotCount = polyspotCount;
	rec.AnchorCount = anchorCount;
	rec.BuildGLNodes = buildGLNodes;
	rec.Emitted = emitted;
	rec.Blockers = blockers;
	if (fallbackReason != nullptr)
	{
		std::strncpy(rec.FallbackReason, fallbackReason, sizeof(rec.FallbackReason) - 1);
		rec.FallbackReason[sizeof(rec.FallbackReason) - 1] = '\0';
	}
	else
	{
		rec.FallbackReason[0] = '\0';
	}
	++NanoBSPHistoryWriteCursor;
}
}

// True iff the map loader should consult the NanoBSP dispatch path for a
// forced rebuild. The map loader only calls this from the build-from-scratch
// branch, so mode 1 never affects maps with valid existing nodes.
bool HCDENanoBSPShouldUseForBuildFromScratch()
{
	const int mode = *hcde_nanobsp_loader;
	return (mode == 1 || mode == 2) && !V_IsHardwareRenderer();
}

bool HCDENanoBSPBuildFromScratch(
	FNodeBuilder::FLevel& leveldata,
	TArray<FNodeBuilder::FPolyStart>& polyspots,
	TArray<FNodeBuilder::FPolyStart>& anchors,
	bool buildGLNodes,
	FLevelLocals& level)
{
	++NanoBSPDispatchRequests;

	const uint32_t blockers = PreflightAdapterInput(leveldata, polyspots, anchors);
	if (blockers == HNBLOCK_NONE)
		++NanoBSPPreflightPasses;
	else
		++NanoBSPPreflightRejects;
	if ((blockers & HNBLOCK_POLYOBJECTS) != 0)
		++NanoBSPPolyobjectCases;

	if (blockers != HNBLOCK_NONE)
	{
		++NanoBSPAdapterFallbacks;
		char reason[96];
		DescribeAdapterBlockers(blockers, reason, sizeof(reason));
		RecordDispatch(level, leveldata.NumVertices, leveldata.NumLines, leveldata.NumSides,
			int(polyspots.Size()), int(anchors.Size()), buildGLNodes, /*emitted=*/false, blockers, reason);
		return false;
	}

	FNanoBSPBuildContext ctx;
	ctx.LevelData = &leveldata;
	ctx.Level = &level;
	ctx.Vertices.Alloc(leveldata.NumVertices);
	for (int i = 0; i < leveldata.NumVertices; ++i)
	{
		ctx.Vertices[i].X = leveldata.Vertices[i].fixX();
		ctx.Vertices[i].Y = leveldata.Vertices[i].fixY();
	}

	const int soup = NanoBSPCreateSegs(ctx);
	if (soup == kNanoBSPNoIndex)
	{
		++NanoBSPAdapterFallbacks;
		RecordDispatch(level, leveldata.NumVertices, leveldata.NumLines, leveldata.NumSides,
			int(polyspots.Size()), int(anchors.Size()), buildGLNodes, /*emitted=*/false,
			HNBLOCK_MISSING_ARRAY, "adapter-blocked:invalid-line-vertices");
		return false;
	}

	FNanoBSPNode* root = NanoBSPSubdivideSegs(ctx, soup);
	const bool emitted = root != nullptr && NanoBSPExtractToLevel(ctx, root);
	DeleteNanoBSPTree(root);
	if (!emitted)
	{
		++NanoBSPAdapterFallbacks;
		RecordDispatch(level, leveldata.NumVertices, leveldata.NumLines, leveldata.NumSides,
			int(polyspots.Size()), int(anchors.Size()), buildGLNodes, /*emitted=*/false,
			HNBLOCK_EMIT_FAILED, "adapter-blocked:emit-failed");
		return false;
	}

	char reason[96];
	std::snprintf(reason, sizeof(reason), "adapter-emitted:hcde-nanobsp");
	RecordDispatch(level, leveldata.NumVertices, leveldata.NumLines, leveldata.NumSides,
		int(polyspots.Size()), int(anchors.Size()), buildGLNodes, /*emitted=*/true, HNBLOCK_NONE, reason);
	DPrintf(DMSG_NOTIFY, "HCDE NanoBSP emitted %u verts, %u segs, %u subsectors, %u nodes\n",
		level.vertexes.Size(), level.segs.Size(), level.subsectors.Size(), level.nodes.Size());
	return true;
}

CCMD(r_nanobsp_status)
{
	const int mode = *hcde_nanobsp_loader;
	const char* modeName = "existing-zdbsp";
	if (mode == 1) modeName = "nanobsp-build-from-scratch";
	else if (mode == 2) modeName = "nanobsp-always";

	Printf(PRINT_HIGH, "\n=== HCDE NanoBSP loader ===\n");
	Printf(PRINT_HIGH, "  hcde_nanobsp_loader  = %d (%s)\n", mode, modeName);
	Printf(PRINT_HIGH, "  effective-build-path = %s\n",
		HCDENanoBSPShouldUseForBuildFromScratch() ? "nanobsp" : "existing-zdbsp");
	Printf(PRINT_HIGH, "  dispatch-requests    = %d\n", NanoBSPDispatchRequests);
	Printf(PRINT_HIGH, "  adapter-fallbacks    = %d\n", NanoBSPAdapterFallbacks);
	Printf(PRINT_HIGH, "  preflight-ok         = %d\n", NanoBSPPreflightPasses);
	Printf(PRINT_HIGH, "  preflight-rejects    = %d\n", NanoBSPPreflightRejects);
	Printf(PRINT_HIGH, "  polyobject-cases     = %d\n", NanoBSPPolyobjectCases);
	Printf(PRINT_HIGH, "  vendoring-status     = Woof source present as reference; HCDE adapter emits native arrays\n");
	Printf(PRINT_HIGH, "  determinism-harness  = comparison/report scaffold present\n");
	Printf(PRINT_HIGH, "  remaining-work:\n");
	Printf(PRINT_HIGH, "    1. prove deterministic ordering vs. ZDBSP on a curated map list\n");
	Printf(PRINT_HIGH, "    2. compare subsector/seg counts and LOS samples against existing builder\n");
	Printf(PRINT_HIGH, "    3. soak across Doom 1/2, Plutonia, TNT, modern megawads\n");
	Printf(PRINT_HIGH, "    4. decide whether polyobject maps can leave the fallback path\n");

	if (NanoBSPDispatchRequests == 0)
	{
		Printf(PRINT_HIGH, "  recent-dispatches    = none yet (set hcde_nanobsp_loader=1 and trigger a forced rebuild)\n");
	}
	else
	{
		Printf(PRINT_HIGH, "  recent-dispatches (newest last):\n");
		const int total = NanoBSPHistoryWriteCursor;
		const int start = total > kHistorySlots ? total - kHistorySlots : 0;
		for (int i = start; i < total; ++i)
		{
			const FNanoBSPDispatchRecord& rec = NanoBSPHistory[i % kHistorySlots];
			if (!rec.Used) continue;
			Printf(PRINT_HIGH, "    map=%-10s verts=%5d lines=%5d sides=%5d poly=%3d anchors=%3d gl=%d emitted=%d blockers=0x%02x reason=%s\n",
				rec.MapName, rec.VertexCount, rec.LineCount, rec.SideCount,
				rec.PolyspotCount, rec.AnchorCount,
				rec.BuildGLNodes ? 1 : 0, rec.Emitted ? 1 : 0,
				unsigned(rec.Blockers), rec.FallbackReason);
		}
	}
	Printf(PRINT_HIGH, "  see docs/HCDE_NANOBSP_AUDIT.md for boundaries.\n");
	Printf(PRINT_HIGH, "==========================\n");
}

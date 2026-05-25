/*
** a_action.cpp
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 1993-1996 id Software
** Copyright 1994-1996 Raven Software
** Copyright 1999-2016 Marisa Heit
** Copyright 2002-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include "actor.h"
#include "p_conversation.h"
#include "p_lnspec.h"
#include "d_player.h"
#include "p_local.h"
#include "p_terrain.h"
#include "p_enemy.h"
#include "serializer.h"
#include "vm.h"
#include "actorinlines.h"

EXTERN_CVAR(Int, sv_corpsequeuesize)
EXTERN_CVAR(Int, sv_corpsefilter)

// Centralized corpse-queue enforcement used by both explicit A_QueueCorpse
// states and generic A_NoBlocking deaths.
static void HCDE_QueueCorpse(AActor* corpse)
{
	if (corpse == nullptr || corpse->Level == nullptr)
	{
		return;
	}

	// Bit 0 controls monster corpse filtering. If it is disabled, leave the
	// body alone and let the map/mod decide its own lifetime.
	if ((sv_corpsefilter & 1) == 0)
	{
		return;
	}

	auto& corpsequeue = corpse->Level->CorpseQueue;

	// De-duplicate in case multiple state paths try to queue the same corpse.
	auto index = corpsequeue.FindEx([=](auto& element) { return element == corpse; });
	if (index < corpsequeue.Size())
	{
		corpsequeue.Delete(index);
	}

	// Zero or negative means "do not retain corpses" for this queue.
	if (sv_corpsequeuesize <= 0)
	{
		corpse->Destroy();
		return;
	}

	while (corpsequeue.Size() >= (unsigned)sv_corpsequeuesize)
	{
		AActor* oldCorpse = corpsequeue[0];
		if (oldCorpse && oldCorpse != corpse)
		{
			oldCorpse->Destroy();
		}
		corpsequeue.Delete(0);
	}

	corpsequeue.Push(MakeObjPtr<AActor*>(corpse));
	GC::WriteBarrier(corpse);
}

//----------------------------------------------------------------------------
//
// PROC A_NoBlocking
//
//----------------------------------------------------------------------------

void A_Unblock(AActor *self, bool drop)
{
	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->Alpha = 1.;
		self->visdir = 0;
	}

	self->flags &= ~MF_SOLID;

	// If the actor has a conversation that sets an item to drop, drop that.
	if (self->Conversation != NULL && self->Conversation->DropType != NULL)
	{
		P_DropItem (self, self->Conversation->DropType, -1, 256);
		self->Conversation = NULL;
		return;
	}

	self->Conversation = NULL;

	// If the actor has attached metadata for items to drop, drop those.
	if (drop && !self->IsKindOf(NAME_PlayerPawn))	// [GRB]
	{
		auto di = self->GetDropItems();

		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					PClassActor *ti = PClass::FindActor(di->Name);
					if (ti != NULL)
					{
						P_DropItem (self, ti, di->Amount, di->Probability);
					}
				}
				di = di->Next;
			}
		}
	}

	// Apply corpse queue policy for all non-player corpses that use
	// A_NoBlocking, not just actors with explicit A_QueueCorpse calls.
	if ((self->flags & MF_CORPSE) && self->player == nullptr && !self->IsKindOf(NAME_PlayerPawn))
	{
		HCDE_QueueCorpse(self);
	}
}

//----------------------------------------------------------------------------
//
// CorpseQueue routines (historically Hexen-specific, now enforced for
// generic A_NoBlocking corpses as well).
//
//----------------------------------------------------------------------------

// throw another corpse on the queue
DEFINE_ACTION_FUNCTION(AActor, A_QueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	if (self->flags & MF_CORPSE)
	{
		HCDE_QueueCorpse(self);
	}
	return 0;
}

// Remove an actor from the queue (for resurrection)
DEFINE_ACTION_FUNCTION(AActor, A_DeQueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	auto &corpsequeue = self->Level->CorpseQueue;
	auto index = corpsequeue.FindEx([=](auto &element) { return element == self; });
	if (index < corpsequeue.Size())
	{
		corpsequeue.Delete(index);
	}
	return 0;
}

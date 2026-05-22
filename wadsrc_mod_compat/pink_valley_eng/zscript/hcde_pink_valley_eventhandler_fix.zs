/*
** hcde_pink_valley_eventhandler_fix.zs
**
** Pink Valley defines event handlers that call CountInv through players[0].mo
** without checking whether player 0 has a valid pawn yet. In dedicated-join
** startup windows this can be null and abort the VM. Replace those handlers
** with null-safe HCDE-owned equivalents.
**
** SPDX-License-Identifier: GPL-3.0-or-later
*/

class HCDEPinkValleyCarnageCompat : EventHandler replaces Carnage
{
	private clearscope PlayerPawn GetPrimaryPlayerMo()
	{
		// Dedicated-join sessions can make player 0 invalid while the active human
		// client is in another slot. Pick the first live pawn instead of assuming
		// slot zero exists.
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
			{
				continue;
			}

			let pawn = players[i].mo;
			if (pawn != null)
			{
				return pawn;
			}
		}

		return null;
	}

	override void WorldThingDied(WorldEvent e)
	{
		let primary = GetPrimaryPlayerMo();
		if (primary == null || e.thing == null)
		{
			return;
		}

		if (primary.CountInv("ACARNAGE") == 1)
		{
			primary.A_GiveInventory("HealthBonus", 10);
		}
	}

	override void WorldThingDamaged(WorldEvent e)
	{
		let primary = GetPrimaryPlayerMo();
		if (primary == null || e.thing == null)
		{
			return;
		}

		if (primary.CountInv("ACARNAGE") == 1)
		{
			primary.A_GiveInventory("HealthBonus", 1);
		}
	}
}

class HCDEPinkValleyHerramientasCompat : EventHandler replaces HERRAMIENTAS
{
	private vector3 MononoSafeSpawn;
	private double MononoSafeMaxDist;
	private double MononoOobWorldLimit;

	private int TrapTicks;
	private int StartupTicks;
	private int RecoverCooldownTicks;
	private bool StartupValidated;

	private bool IsMononoMap()
	{
		if (level == null)
		{
			return false;
		}

		if (
			level.MapName.CompareNoCase("MONONO") == 0 ||
			level.MapName.CompareNoCase("I_WANT_MY_MONONO") == 0 ||
			level.LevelName.CompareNoCase("WHERES MONONO?") == 0
		)
		{
			return true;
		}

		// Some handoff paths expose map identity through LevelInfo fields first.
		if (
			level.info.MapName.CompareNoCase("MONONO") == 0 ||
			level.info.MapName.CompareNoCase("I_WANT_MY_MONONO") == 0 ||
			level.info.LevelName.CompareNoCase("WHERES MONONO?") == 0 ||
			level.info.MapLabel.CompareNoCase("MONONO") == 0
		)
		{
			return true;
		}

		return false;
	}

	private void RestorePlayableState(PlayerPawn primary)
	{
		if (primary.Speed < 0.40)
		{
			primary.Speed = 0.40;
		}

		primary.reactiontime = 0;

		if (primary.player != null)
		{
			primary.player.cheats &= ~(CF_FROZEN | CF_TOTALLYFROZEN);
		}
	}

	private bool NeedsStartupRecovery(PlayerPawn primary)
	{
		bool inHardOob =
			(primary.Pos.X > MononoOobWorldLimit || primary.Pos.X < -MononoOobWorldLimit) ||
			(primary.Pos.Y > MononoOobWorldLimit || primary.Pos.Y < -MononoOobWorldLimit);

		if (inHardOob)
		{
			return true;
		}

		bool outsideSpawnLane =
			(primary.Pos.X < (MononoSafeSpawn.X - MononoSafeMaxDist)) ||
			(primary.Pos.X > (MononoSafeSpawn.X + MononoSafeMaxDist)) ||
			(primary.Pos.Y < (MononoSafeSpawn.Y - MononoSafeMaxDist)) ||
			(primary.Pos.Y > (MononoSafeSpawn.Y + MononoSafeMaxDist));

		if (outsideSpawnLane)
		{
			return true;
		}

		if (primary.Speed < 0.05)
		{
			return true;
		}

		return primary.player != null && (primary.player.cheats & (CF_FROZEN | CF_TOTALLYFROZEN)) != 0;
	}

	private void RecoverToPlayableSpawn(PlayerPawn primary)
	{
		vector3 spawnPos = MononoSafeSpawn;
		double spawnAngle = 180.0;

		if (primary.player != null)
		{
			let pnum = level.PlayerNum(primary.player);
			if (pnum >= 0)
			{
				let [pickedPos, pickedAngle] = level.PickPlayerStart(pnum, PPS_NOBLOCKINGCHECK);
				if (pickedPos.X != 0.0 || pickedPos.Y != 0.0 || pickedPos.Z != 0.0)
				{
					spawnPos = pickedPos;
					spawnAngle = pickedAngle;
				}
			}
		}

		let moved = primary.Teleport(spawnPos, spawnAngle, TF_FORCED | TF_NOFOG);
		if (!moved)
		{
			primary.SetOrigin((spawnPos.X, spawnPos.Y, spawnPos.Z), false);
		}
	}

	private clearscope PlayerPawn GetPrimaryPlayerMo()
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
			{
				continue;
			}

			let pawn = players[i].mo;
			if (pawn != null)
			{
				return pawn;
			}
		}

		return null;
	}

	override void WorldLoaded(WorldEvent e)
	{
		MononoSafeSpawn = (-2760.0, 2174.0, 0.0);
		MononoSafeMaxDist = 2048.0;
		MononoOobWorldLimit = 12000.0;
		TrapTicks = 0;
		StartupTicks = 0;
		RecoverCooldownTicks = 0;
		StartupValidated = false;
	}

	override void WorldTick()
	{
		if (!IsMononoMap())
		{
			return;
		}

		let primary = GetPrimaryPlayerMo();
		if (primary == null || primary.health <= 0)
		{
			return;
		}

		// First-time MONONO spawn validation: dedicated-join flows can occasionally
		// inherit an invalid start position. Verify once and force the canonical
		// spawn lane if the start location is too far away.
		if (!StartupValidated)
		{
			StartupTicks++;
			RestorePlayableState(primary);

			if (StartupTicks >= 4 && NeedsStartupRecovery(primary))
			{
				RecoverToPlayableSpawn(primary);
				RestorePlayableState(primary);
				RecoverCooldownTicks = 0;
				TrapTicks = 0;
			}

			// Keep the guard active for a short grace window to absorb delayed
			// script-side speed/freeze changes on network joins.
			if (StartupTicks >= 70 || !NeedsStartupRecovery(primary))
			{
				StartupValidated = true;
			}
		}

		// A_NEW_DAY can leave speed clamped to 0 at level exit. Keep a short MONONO
		// startup window that reasserts playable speed.
		if (StartupTicks < 700)
		{
			RestorePlayableState(primary);
		}

		// Runtime compatibility guard: if the player is shoved into far out-of-
		// bounds coordinates during scripted transitions, recover safely.
		bool inTrapCorridor =
			(primary.Pos.X > 1600.0 && primary.Pos.X < 2300.0 && primary.Pos.Y < -30000.0) ||
			(primary.Pos.Y < -15000.0) ||
			(primary.Pos.X > MononoOobWorldLimit || primary.Pos.X < -MononoOobWorldLimit) ||
			(primary.Pos.Y > MononoOobWorldLimit);

		if (!inTrapCorridor)
		{
			TrapTicks = 0;
			if (RecoverCooldownTicks > 0)
			{
				RecoverCooldownTicks--;
			}
			return;
		}

		if (RecoverCooldownTicks > 0)
		{
			RecoverCooldownTicks--;
			return;
		}

		RestorePlayableState(primary);

		TrapTicks++;
		if (TrapTicks < 35)
		{
			return;
		}

		RecoverToPlayableSpawn(primary);
		RestorePlayableState(primary);

		RecoverCooldownTicks = 35;
		TrapTicks = 0;
	}

	override void WorldThingDamaged(WorldEvent e)
	{
		let primary = GetPrimaryPlayerMo();
		if (primary == null || e.thing == null)
		{
			return;
		}

		let mar = primary.CountInv("HERRAM");
		let saw = primary.CountInv("HERRAS");

		if (!e.thing.bNoBlood && mar == 1 && e.thing.Distance3D(primary) <= 200)
		{
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 0, 3, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, -3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 0, -3, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 0, 0, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 0, -1, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNE", 0, 0, 0, 0, 1, 10, 0, SXF_NOCHECKPOSITION);
		}

		if (mar == 1 && e.thing.CountInv("DINOFETI") >= 1)
		{
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 0, 3, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, -3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 0, -3, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 0, 0, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 0, -1, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCE", 0, 0, 0, 0, 1, 10, 0, SXF_NOCHECKPOSITION);
		}

		if (saw == 1 && !e.thing.bNoBlood)
		{
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 0, 3, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, -3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 0, -3, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 0, 0, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 0, -1, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("CARNESAW", 0, 0, 0, 0, 1, 10, 0, SXF_NOCHECKPOSITION);
		}

		if (saw == 1 && e.thing.CountInv("DINOFETI") >= 1)
		{
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 0, 3, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, -3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 3, 0, 13, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 0, -3, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 0, 0, 12, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 0, -1, 10, 0, SXF_NOCHECKPOSITION);
			e.thing.A_SpawnItemEx("DULCESAW", 0, 0, 0, 0, 1, 10, 0, SXF_NOCHECKPOSITION);
		}
	}
}

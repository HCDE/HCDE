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
		if (!playeringame[0])
		{
			return null;
		}

		return players[0].mo;
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

	private clearscope PlayerPawn GetPrimaryPlayerMo()
	{
		if (!playeringame[0])
		{
			return null;
		}

		return players[0].mo;
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
		if (level == null || level.MapName != "MONONO")
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
			if (StartupTicks >= 4)
			{
				let startupMoved = primary.Teleport(MononoSafeSpawn, 180.0, TF_FORCED | TF_NOFOG);
				if (!startupMoved)
				{
					primary.SetOrigin((MononoSafeSpawn.X, MononoSafeSpawn.Y, primary.Pos.Z), false);
				}

				if (primary.Speed < 0.40)
				{
					primary.Speed = 0.40;
				}

				StartupValidated = true;
				RecoverCooldownTicks = 0;
				TrapTicks = 0;
			}
		}

		// A_NEW_DAY can leave speed clamped to 0 at level exit. Keep a short MONONO
		// startup window that reasserts playable speed.
		if (StartupTicks < 700 && primary.Speed < 0.25)
		{
			primary.Speed = 0.40;
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

		if (primary.Speed < 0.10)
		{
			primary.Speed = 0.30;
		}

		TrapTicks++;
		if (TrapTicks < 35)
		{
			return;
		}

		let recovered = primary.Teleport(MononoSafeSpawn, 180.0, TF_FORCED | TF_NOFOG);
		if (!recovered)
		{
			primary.SetOrigin((MononoSafeSpawn.X, MononoSafeSpawn.Y, primary.Pos.Z), false);
		}

		if (primary.Speed < 0.40)
		{
			primary.Speed = 0.40;
		}

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

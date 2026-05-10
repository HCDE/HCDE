/*
** sharedmisc.zs
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 1993-1996 id Software
** Copyright 1999-2016 Marisa Heit
** Copyright 2006-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

// Default class for unregistered doomednums -------------------------------

class Unknown : Actor
{
	Default
	{
		Radius 32;
		Height 56;
		+NOGRAVITY
		+NOBLOCKMAP
		+DONTSPLASH
	}
	States
	{
	Spawn:
		UNKN A -1;
		Stop;
	}
}

// Route node for monster patrols -------------------------------------------

class PatrolPoint : Actor
{
	Default
	{
		Radius 8;
		Height 8;
		Mass 10;
		+NOGRAVITY
		+NOBLOCKMAP
		+DONTSPLASH
		+NOTONAUTOMAP
		RenderStyle "None";
	}
}

// A special to execute when a monster reaches a matching patrol point ------

class PatrolSpecial : Actor
{
	Default
	{
		Radius 8;
		Height 8;
		Mass 10;
		+NOGRAVITY
		+NOBLOCKMAP
		+DONTSPLASH
		+NOTONAUTOMAP
		RenderStyle "None";
	}
}

// Map spot ----------------------------------------------------------------

class MapSpot : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+NOGRAVITY
		+DONTSPLASH
		+NOTONAUTOMAP
		RenderStyle "None";
		CameraHeight 0;
	}
}

// same with different editor number for Legacy maps -----------------------

class FS_Mapspot : Mapspot
{
}

// Map spot with gravity ---------------------------------------------------

class MapSpotGravity : MapSpot
{
	Default
	{
		-NOBLOCKMAP
		-NOSECTOR
		-NOGRAVITY
		+NOTONAUTOMAP
	}
}

// Point Pushers -----------------------------------------------------------

class PointPusher : Actor
{
	Default
	{
		+NOBLOCKMAP
		+INVISIBLE
		+NOCLIP
		+NOTONAUTOMAP
	}
}

class PointPuller : Actor
{
	Default
	{
		+NOBLOCKMAP
		+INVISIBLE
		+NOCLIP
		+NOTONAUTOMAP
	}
}

// Skulltag/Zandronum invasion spot shims ----------------------------------

class BaseMonsterInvasionSpot : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+NOGRAVITY
		+DONTSPLASH
		+NOTONAUTOMAP
		RenderStyle "None";
	}
	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	}
}

class CustomMonsterInvasionSpot : BaseMonsterInvasionSpot {}

class BasePickupInvasionSpot : BaseMonsterInvasionSpot {}

class CustomPickupInvasionSpot : BasePickupInvasionSpot {}

class BaseWeaponInvasionSpot : BaseMonsterInvasionSpot {}

class CustomWeaponInvasionSpot : BaseWeaponInvasionSpot {}

class WeakDoomMonsterSpot : CustomMonsterInvasionSpot {}
class PowerfulDoomMonsterSpot : CustomMonsterInvasionSpot {}
class VeryPowerfulDoomMonsterSpot : CustomMonsterInvasionSpot {}
class AnyDoomMonsterSpot : CustomMonsterInvasionSpot {}
class ImpSpot : CustomMonsterInvasionSpot {}
class DemonSpot : CustomMonsterInvasionSpot {}
class SpectreSpot : CustomMonsterInvasionSpot {}
class ZombieManSpot : CustomMonsterInvasionSpot {}
class ShotgunGuySpot : CustomMonsterInvasionSpot {}
class ChaingunGuySpot : CustomMonsterInvasionSpot {}
class CacodemonSpot : CustomMonsterInvasionSpot {}
class RevenantSpot : CustomMonsterInvasionSpot {}
class FatsoSpot : CustomMonsterInvasionSpot {}
class ArachnotronSpot : CustomMonsterInvasionSpot {}
class HellKnightSpot : CustomMonsterInvasionSpot {}
class BaronOfHellSpot : CustomMonsterInvasionSpot {}
class LostSoulSpot : CustomMonsterInvasionSpot {}
class PainElementalSpot : CustomMonsterInvasionSpot {}
class CyberdemonSpot : CustomMonsterInvasionSpot {}
class SpiderMastermindSpot : CustomMonsterInvasionSpot {}
class ArchvileSpot : CustomMonsterInvasionSpot {}
class WolfensteinSSSpot : CustomMonsterInvasionSpot {}
class StimpackSpot : CustomPickupInvasionSpot {}
class MedikitSpot : CustomPickupInvasionSpot {}
class HealthBonusSpot : CustomPickupInvasionSpot {}
class ArmorBonusSpot : CustomPickupInvasionSpot {}
class MaxHealthBonusSpot : CustomPickupInvasionSpot {}
class MaxArmorBonusSpot : CustomPickupInvasionSpot {}
class GreenArmorSpot : CustomPickupInvasionSpot {}
class BlueArmorSpot : CustomPickupInvasionSpot {}
class DoomsphereSpot : CustomPickupInvasionSpot {}
class GuardsphereSpot : CustomPickupInvasionSpot {}
class InvisibilitySphereSpot : CustomPickupInvasionSpot {}
class BlurSphereSpot : CustomPickupInvasionSpot {}
class InvulnerabilitySphereSpot : CustomPickupInvasionSpot {}
class MegasphereSpot : CustomPickupInvasionSpot {}
class RandomPowerupSpot : CustomPickupInvasionSpot {}
class SoulsphereSpot : CustomPickupInvasionSpot {}
class TimeFreezeSphereSpot : CustomPickupInvasionSpot {}
class TurbosphereSpot : CustomPickupInvasionSpot {}
class StrengthRuneSpot : CustomPickupInvasionSpot {}
class RageRuneSpot : CustomPickupInvasionSpot {}
class DrainRuneSpot : CustomPickupInvasionSpot {}
class SpreadRuneSpot : CustomPickupInvasionSpot {}
class ResistanceRuneSpot : CustomPickupInvasionSpot {}
class RegenerationRuneSpot : CustomPickupInvasionSpot {}
class ProsperityRuneSpot : CustomPickupInvasionSpot {}
class ReflectionRuneSpot : CustomPickupInvasionSpot {}
class HasteRuneSpot : CustomPickupInvasionSpot {}
class HighJumpRuneSpot : CustomPickupInvasionSpot {}
class ClipSpot : CustomPickupInvasionSpot {}
class ShellSpot : CustomPickupInvasionSpot {}
class RocketAmmoSpot : CustomPickupInvasionSpot {}
class CellSpot : CustomPickupInvasionSpot {}
class ClipBoxSpot : CustomPickupInvasionSpot {}
class ShellBoxSpot : CustomPickupInvasionSpot {}
class RocketBoxSpot : CustomPickupInvasionSpot {}
class CellPackSpot : CustomPickupInvasionSpot {}
class RandomClipAmmoSpot : CustomPickupInvasionSpot {}
class RandomBoxAmmoSpot : CustomPickupInvasionSpot {}
class BackpackSpot : CustomPickupInvasionSpot {}
class BerserkSpot : CustomPickupInvasionSpot {}
class ChainsawSpot : CustomWeaponInvasionSpot {}
class ShotgunSpot : CustomWeaponInvasionSpot {}
class SuperShotgunSpot : CustomWeaponInvasionSpot {}
class ChaingunSpot : CustomWeaponInvasionSpot {}
class RocketLauncherSpot : CustomWeaponInvasionSpot {}
class PlasmaRifleSpot : CustomWeaponInvasionSpot {}
class BFG9000Spot : CustomWeaponInvasionSpot {}

// Bloody gibs -------------------------------------------------------------

class RealGibs : Actor
{
	Default
	{
		+DROPOFF
		+CORPSE
		+NOTELEPORT
		+DONTGIB
	}
	States
	{
	Spawn:
		goto GenericCrush;
	}
}

// Gibs that can be placed on a map. ---------------------------------------
//
// These need to be a separate class from the above, in case someone uses
// a deh patch to change the gibs, since ZDoom actually creates a gib class
// for actors that get crushed instead of changing their state as Doom did.

class Gibs : RealGibs
{
	Default
	{
		ClearFlags;
	}
}

// Needed for loading Build maps -------------------------------------------

class CustomSprite : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
	}
	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	}

	override void BeginPlay ()
	{
		Super.BeginPlay ();

		String name = String.Format("BTIL%04d", args[0] & 0xffff);
		picnum = TexMan.CheckForTexture (name, TexMan.TYPE_Build);
		if (!picnum.Exists())
		{
			Destroy();
			return;
		}

		Scale.X = args[2] / 64.;
		Scale.Y = args[3] / 64.;

		int cstat = args[4];
		if (cstat & 2)
		{
			A_SetRenderStyle((cstat & 512) ? 0.6666 : 0.3333, STYLE_Translucent);
		}
		if (cstat & 4) bXFlip = true;
		if (cstat & 8) bYFlip = true;
		if (cstat & 16) bWallSprite = true;
		if (cstat & 32) bFlatSprite = true;
	}
}

// SwitchableDecoration: Activate and Deactivate change state --------------

class SwitchableDecoration : Actor
{
	override void Activate (Actor activator)
	{
		SetStateLabel("Active");
	}

	override void Deactivate (Actor activator)
	{
		SetStateLabel("Inactive");
	}

}

class SwitchingDecoration : SwitchableDecoration
{
	override void Deactivate (Actor activator)
	{
	}
}

// Sector flag setter ------------------------------------------------------

class SectorFlagSetter : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "None";
	}

	override void BeginPlay ()
	{
		Super.BeginPlay ();
		CurSector.Flags |= args[0];
		Destroy();
	}
}

// Marker for sounds : Actor -------------------------------------------------------

class SpeakerIcon : Unknown
{
	States
	{
	Spawn:
		SPKR A -1 BRIGHT;
		Stop;
	}
	Default
	{
		Scale 0.125;
	}
}

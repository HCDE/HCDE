// HCDE Predator Economy ZScript extension surface.
//
// This is intentionally inert: it gives mods a stable class to derive from
// once the C++ side replicates predator role/currency, but it does not alter
// movement, weapons, inventory, damage, or scoring by itself.

class HCDEPredatorPawn : PlayerPawn abstract
{
	private bool hcdeIsPredator;
	private int hcdePredatorCurrency;

	virtual void HCDE_SetPredatorRole(bool isPredator)
	{
		hcdeIsPredator = isPredator;
	}

	virtual bool HCDE_IsPredator() const
	{
		return hcdeIsPredator;
	}

	virtual void HCDE_SetPredatorCurrency(int amount)
	{
		if (amount < 0)
		{
			amount = 0;
		}
		hcdePredatorCurrency = amount;
	}

	virtual int HCDE_GetPredatorCurrency() const
	{
		return hcdePredatorCurrency;
	}
}

// HCDE Predator Economy ZScript extension surface.
//
// This is intentionally inert: it gives mods a stable class to derive from
// once the C++ side replicates predator role/currency, but it does not alter
// movement, weapons, inventory, damage, or scoring by itself.

class HCDEPredatorPawn : PlayerPawn abstract
{
	private bool hcdeIsPredator;
	private int hcdePredatorCurrency;
	private int hcdePredatorRoundState;
	private int hcdePredatorLastBuyItem;
	private int hcdePredatorLastBuyResult;
	private int hcdePredatorLastBuyCost;
	private int hcdePredatorLastBuyBalance;
	private int hcdePredatorLastBuyRequestId;
	private int hcdePredatorLastBuySequence;

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

	virtual void HCDE_SetPredatorRoundState(int state)
	{
		hcdePredatorRoundState = state;
	}

	virtual int HCDE_GetPredatorRoundState() const
	{
		return hcdePredatorRoundState;
	}

	// Mirrors EHCDEPredatorState::Buy without importing C++ enum names into
	// ZScript. UI code can use this to decide when to show a buy prompt.
	virtual bool HCDE_CanPredatorBuy() const
	{
		return hcdePredatorRoundState == 2 && !hcdeIsPredator;
	}

	// Cosmetic/UI-only buy-result mirror. The server remains the source of
	// truth: C++ validates funds, phase, and item legality before this data is
	// exposed through snapshots/events.
	virtual void HCDE_SetPredatorBuyResult(int item, int result, int cost, int balance, int requestId, int authoritySequence)
	{
		hcdePredatorLastBuyItem = item;
		hcdePredatorLastBuyResult = result;
		hcdePredatorLastBuyCost = cost;
		hcdePredatorLastBuyBalance = balance;
		hcdePredatorLastBuyRequestId = requestId;
		hcdePredatorLastBuySequence = authoritySequence;
	}

	virtual int HCDE_GetPredatorLastBuyItem() const
	{
		return hcdePredatorLastBuyItem;
	}

	virtual int HCDE_GetPredatorLastBuyResult() const
	{
		return hcdePredatorLastBuyResult;
	}

	virtual int HCDE_GetPredatorLastBuyCost() const
	{
		return hcdePredatorLastBuyCost;
	}

	virtual int HCDE_GetPredatorLastBuyBalance() const
	{
		return hcdePredatorLastBuyBalance;
	}

	virtual int HCDE_GetPredatorLastBuyRequestId() const
	{
		return hcdePredatorLastBuyRequestId;
	}

	virtual int HCDE_GetPredatorLastBuySequence() const
	{
		return hcdePredatorLastBuySequence;
	}
}

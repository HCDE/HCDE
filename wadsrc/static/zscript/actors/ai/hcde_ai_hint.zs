// HCDE AI Director hint surface.
//
// Hints are inert data carriers. The C++ director may later attach one through
// normal actor/inventory APIs when the default-off hint CVAR is enabled. Merely
// defining this class does not change monster AI or replication.

class HCDEAIRegroupHint : Inventory
{
	int GroupId;
	int StoredTic;

	virtual void HCDE_SetRegroupHint(int groupId, int storedTic)
	{
		GroupId = groupId;
		StoredTic = storedTic;
	}
}

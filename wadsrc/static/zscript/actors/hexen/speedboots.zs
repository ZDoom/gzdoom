

class ArtiSpeedBoots : PowerupGiver
{
	Default
	{
		+FLOATBOB
		+COUNTITEM
		Inventory.PickupFlash "PickupFlash";
		Inventory.Icon "ARTISPED";
		Inventory.PickupMessage "$TXT_ARTISPEED";
		Tag "$TAG_ARTISPEED";
		Powerup.Type "PowerSpeed";
	}
	States
	{
	Spawn:
		SPED ABCDEFGH 3 Bright;
		Loop;
	}
}

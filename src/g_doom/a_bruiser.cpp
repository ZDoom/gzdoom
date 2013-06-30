static FRandom pr_bruisattack ("BruisAttack");


DEFINE_ACTION_FUNCTION(AActor, A_BruisAttack)
{
	if (!self->target)
		return;
				
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_bruisattack()%8+1)*10;
		S_Sound (self, CHAN_WEAPON, "baron/melee", 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("BaronBall"));
}

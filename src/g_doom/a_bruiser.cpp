static FRandom pr_bruisattack ("BruisAttack");


DEFINE_ACTION_FUNCTION(AActor, A_BruisAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;
				
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_bruisattack()%8+1)*10;
		S_Sound (self, CHAN_WEAPON, "baron/melee", 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return 0;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindActor("BaronBall"));
	return 0;
}

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// g_combat.c

#include "g_local.h"

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/


qboolean CanDamage (edict_t *targ, edict_t *inflictor)
{
	vec3_t	dest;
	trace_t	trace;

// bmodels need special checking because their origin is 0,0,0
	if (targ->movetype == MOVETYPE_PUSH)
	{
		VectorAdd (targ->absmin, targ->absmax, dest);
		VectorScale (dest, 0.5, dest);
		trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
		if (trace.fraction == 1.0)
			return true;
		if (trace.ent == targ)
			return true;
		return false;
	}
	
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;


	return false;
}


/*
============
Killed
============
*/
void Killed (edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	//gi.bprintf(PRINT_HIGH, "KILLED: targ = %s, inflictor = %s, attacker = %s, damage = %i, point = %s\n", targ->classname, inflictor->classname, attacker->classname, damage, vtos(point));
	if (targ->health < -999)
		targ->health = -999;

	targ->enemy = attacker;
	
	if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD))
	{
//		targ->svflags |= SVF_DEADMONSTER;	// now treat as a different content type
		if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY))
		{
			level.killed_monsters++;
			if (coop->value && attacker->client)
				attacker->client->resp.score++;
			// medics won't heal monsters that they kill themselves
			if (strcmp(attacker->classname, "monster_medic") == 0)
				targ->owner = attacker;
		}
	}
	//gi.bprintf(PRINT_HIGH, "%s killed with %s by %s\n", targ->classname, inflictor->classname, attacker->classname);
	

	if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE)
	{	// doors, triggers, etc
		
		targ->die (targ, inflictor, attacker, damage, point);
		return;
	}
	if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD))
	{
		targ->touch = NULL;
		monster_death_use (targ);
	}
	
	if(targ->die)
	targ->die (targ, inflictor, attacker, damage, point);
}


/*
================
SpawnDamage
================
*/
void SpawnDamage (int type, vec3_t origin, vec3_t normal, int damage)
{
	vec3_t origin_org;
	vec3_t normal2;
	//gi.bprintf(PRINT_HIGH, "NORMAL = %s", vtos(normal));
	if (!normal)
		VectorSet(normal2, crandom(), crandom(), crandom());
	else
		VectorCopy(normal, normal2);
	VectorCopy(origin, origin_org);
	//gi.bprintf(PRINT_HIGH, "SPAWNDAMAGE: type = %i\n", type);
	if (type == TE_BLOOD)
	{

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_LASER_SPARKS);
		gi.WriteByte(50 + damage);
		gi.WritePosition(origin);
		gi.WriteDir(normal2);
		gi.WriteByte(233);
		gi.multicast(origin, MULTICAST_PVS);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_SPLASH);
		gi.WriteByte(50+damage);
		//VectorScale2(normal, -1, normal);
		//if (!normal)
		VectorMA(origin, 24, normal2, origin);
		gi.WritePosition(origin);
		
	
		gi.WriteDir(normal2);
		gi.WriteByte(SPLASH_BLOOD);
		gi.multicast(origin, MULTICAST_PVS);

		//VectorScale2(normal, -1, normal); //change back

	}
	else
	{
		if (damage > 255)
			damage = 255;
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(type);
		//	gi.WriteByte (damage);
		gi.WritePosition(origin);
		gi.WriteDir(normal2);
		gi.multicast(origin, MULTICAST_PVS);
	}
	VectorCopy(origin_org, origin);
}


/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack
point		point at which the damage is being inflicted
normal		normal vector from that point
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_ENERGY			damage is from an energy based weapon
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/
static int CheckPowerArmor (edict_t *ent, vec3_t point, vec3_t normal, int damage, int dflags, int mod)
{
	gclient_t	*client;
	int			save;
	int			power_armor_type;
	int			index;
	int			damagePerCell;
	int			pa_te_type;
	int			power;
	int			power_used;

	if (!damage)
		return 0;

	client = ent->client;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	if (client)
	{
		power_armor_type = PowerArmorType (ent);
		if (power_armor_type != POWER_ARMOR_NONE)
		{
			index = ITEM_INDEX(FindItem("Cells"));
			power = client->pers.inventory[index];
		}
	}
	else if (ent->svflags & SVF_MONSTER)
	{
		power_armor_type = ent->monsterinfo.power_armor_type;
		power = ent->monsterinfo.power_armor_power;
	}
	else
		return 0;

	if (power_armor_type == POWER_ARMOR_NONE)
		return 0;
	if (!power)
		return 0;

	if (power_armor_type == POWER_ARMOR_SCREEN)
	{
		vec3_t		vec;
		float		dot;
		vec3_t		forward;

		// only works if damage point is in front
		AngleVectors (ent->s.angles, forward, NULL, NULL);
		VectorSubtract (point, ent->s.origin, vec);
		VectorNormalize (vec);
		dot = DotProduct (vec, forward);
		if (dot <= 0.3)
			return 0;

		damagePerCell = 1;
		pa_te_type = TE_SCREEN_SPARKS;
		damage = damage / 3;
		if (!damage)
			damage = 1;
	}
	else
	{
		damagePerCell = 2;
		pa_te_type = TE_SHIELD_SPARKS;
		damage = (2 * damage) / 3;
	}

	save = power * damagePerCell;
	if (!save)
		return 0;
	if (save > damage)
		save = damage;

	if (ent->powerarmor_time > level.time)
	{
		if (mod == MOD_SHOTGUN || mod == MOD_SSHOTGUN || mod == MOD_RAILGUN_FRAG)
		{
			if (rand() % 12 < 12 / 6)
				SpawnDamage(pa_te_type, point, normal, (int)save);

		}
		else if (mod == MOD_CHAINGUN)
		{
			if (rand() % 5 == 6)
				SpawnDamage(pa_te_type, point, normal, (int)save);
		}
		else
			SpawnDamage(pa_te_type, point, normal, save);

	}

	ent->powerarmor_time = level.time + 0.2;

	power_used = save / damagePerCell;
	if (!power_used)
		power_used = 1;
	//gi.bprintf(PRINT_HIGH, "POWER ARMOR CHECKS, damagePerCell = %i, power_used = %i, save = %i, power = %i, damage = %i\n", damagePerCell, power_used, save, power, damage);
	if (client)
		client->pers.inventory[index] -= power_used;
	else
		ent->monsterinfo.power_armor_power -= power_used;
	return save;
}

static int CheckArmor (edict_t *ent, vec3_t point, vec3_t normal, int damage, int te_sparks, int dflags)
{
	gclient_t	*client;
	int			save;
	int			index;
	gitem_t		*armor;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	index = ArmorIndex (ent);
	if (!index)
		return 0;

	armor = GetItemByIndex (index);

	if (dflags & DAMAGE_ENERGY)
		save = ceilf(((gitem_armor_t *)armor->info)->energy_protection*damage);
	else
		save = ceilf(((gitem_armor_t *)armor->info)->normal_protection*damage);
	if (save >= client->pers.inventory[index])
		save = client->pers.inventory[index];

	if (!save)
		return 0;

	client->pers.inventory[index] -= save;
	SpawnDamage (te_sparks, point, normal, save);

	return save;
}

void M_ReactToDamage (edict_t *targ, edict_t *attacker)
{
	if (!attacker)
		return;
	if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
		return;

	if (attacker == targ)
		return;

	M_retreat(targ);

	if (attacker == targ->enemy)
		return;
	// if we are a good guy monster and our attacker is a player
	// or another good guy, do not get mad at them
	if (targ->monsterinfo.aiflags & AI_GOOD_GUY)
	{
		if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
			return;
	}

	// we now know that we are not both good guys

	// if attacker is a client, get mad at them because he's good and we're not
	if (attacker->client)
	{
		targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

		// this can only happen in coop (both new and old enemies are clients)
		// only switch if can't see the current enemy
		if (targ->enemy && targ->enemy->client)
		{
			if (visible(targ, targ->enemy))
			{
				targ->oldenemy = attacker;
				return;
			}
			targ->oldenemy = targ->enemy;
		}
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ);
		return;
	}

	// it's the same base (walk/swim/fly) type and a different classname and it's not a tank
	// (they spray too much), get mad at them
	if (((targ->flags & (FL_FLY|FL_SWIM)) == (attacker->flags & (FL_FLY|FL_SWIM))) &&
		!(attacker->svflags & SVF_MONSTER) && //disabled infighting for now
		/* (strcmp (targ->classname, attacker->classname) != 0) &&*/
		 (strcmp(attacker->classname, "monster_tank") != 0) &&
		 (strcmp(attacker->classname, "monster_supertank") != 0) &&
		 (strcmp(attacker->classname, "monster_makron") != 0) &&
		 (strcmp(attacker->classname, "monster_jorg") != 0) )
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ);
	}
	// if they *meant* to shoot us, then shoot back
	else if (attacker->enemy == targ)
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ);
	}
	// otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
	else if (attacker->enemy && attacker->enemy != targ)
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker->enemy;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ);
	}
}

qboolean CheckTeamDamage(edict_t *targ, edict_t *attacker)
{
	//FIXME make the next line real and uncomment this block
	// if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
	return false;
}
//==============================================================
// AQC Scale damage amount by location of hit on player's body..
//==============================================================
#define IsIn(x, a, b)     ( ( x ) >= ( a ) && ( x ) <= ( b ) )
#define LEG_DAMAGE     (height/2.0)-fabsf(targ->mins[2])-3
#define STOMACH_DAMAGE (height/1.6)-fabsf(targ->mins[2])
#define CHEST_DAMAGE   (height/1.3)-fabsf(targ->mins[2])
#define NECK_DAMAGE    (height/1.2)-fabsf(targ->mins[2])
#define HEAD_DAMAGE    (height/0.7)-fabsf(targ->mins[2])
#define ARM1_DAMAGE    (height/1.4)-fabsf(targ->mins[2])+8  // Should go about between the chest and stomach, and going over by 8
#define ARM2_DAMAGE    (height/1.4)-fabsf(targ->mins[2])-8// Should go about between the chest and stomach, and going over by -8

float location_scaling(edict_t *targ, vec3_t point, float damage, int  mod, int headshot) 
{
	float z_rel, height;
	edict_t *ent;
	//int mod2 = mod;
	meansOfDeath = mod;
	if (!(targ->flags&FL_GODMODE))
		if (!VectorEmpty(point))
			if (IsIn(mod, MOD_BLASTER, MOD_RAILGUN)) {
				height = fabsf(targ->mins[2]) + targ->maxs[2];
				z_rel = point[2] - targ->s.origin[2];
				if (z_rel < LEG_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *LEG*\n");
					return 0.66f;  // Scale down by 2/3

				}
				else if (z_rel < STOMACH_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *STOMACH*\n");
					return 1.0;  // Scale down by 1/3

				}
				else if (z_rel < CHEST_DAMAGE)
				{
					//	gi.bprintf(PRINT_HIGH, "HIT IN *CHEST*\n");
					return 1.20f;  // Scale up by 1/5

				}
				if (z_rel < NECK_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *NECK*\n");
					//mod |= MOD_NECKSHOT;
					mod += 200;
					meansOfDeath = mod;
					//	gi.bprintf(PRINT_HIGH, "HIT IN *NECK*, mod = %i, meansOfDeath = %i\n", mod, meansOfDeath);
					headshot = 1;
					return 2.00;  // Scale up by 3X (Come on, a neck shot atleast has to kill;) )

				}
				if (z_rel < HEAD_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *HEAD*\n");

					//mod |= MOD_HEADSHOT;
					headshot = 1;
					//damage *= 4;
					mod += 200;
					meansOfDeath = mod;

					//gi.bprintf(PRINT_HIGH, "HIT IN *HEAD*, mod = %i, meansOfDeath = %i\n", mod, meansOfDeath);
					return 3.00;  // Scale up by 8X (Pretty much dead...)

				}
				if (z_rel < ARM1_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *ARM1*\n");
					return 0.66f;  // Scale down by 1/3

				}
				if (z_rel < ARM2_DAMAGE)
				{
					//gi.bprintf(PRINT_HIGH, "HIT IN *ARM2*\n");
					return 0.66f;  // Scale down by 1/3

				}

				else
					return 1.0;
			} // Normal Damage if hit anywhere else

	return 1.0; // keep damage the same..

}
qboolean can_gib(edict_t *targ, int mod)
{
	if (!real_gibbing->value)
		return true;
	else
	{
		if (mod == MOD_BARREL || mod == MOD_BFG_BLAST || mod == MOD_BFG_EFFECT || mod == MOD_BFG_LASER || mod == MOD_BOMB ||
			mod == MOD_CRUSH || mod == MOD_EXPLOSIVE || mod == MOD_GRAVITY_GRENADE || mod == MOD_GRENADE || mod == MOD_G_SPLASH ||
			mod == MOD_HANDGRENADE || mod == MOD_HELD_GRAVITY_GRENADE || mod == MOD_HELD_GRENADE || mod == MOD_HG_SPLASH ||
			mod == MOD_LAVA || mod == MOD_RAILGUN || mod == MOD_RAILGUN_FRAG || mod == MOD_ROCKET || mod == MOD_R_SPLASH ||
			mod == MOD_SLIME || mod == MOD_SPLASH || mod == MOD_TARGET_LASER || mod == MOD_TELEFRAG || mod == MOD_TRIGGER_HURT)
			return true;
	}
	return false;
}
void T_Damage (edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod)
{
	gclient_t	*client;
	int			take, temp_take;
	int			save;
	int			asave;
	int			psave;
	int			te_sparks;
	int			had_hp = targ->health;
	//gi.bprintf(PRINT_HIGH, "T_DAMAGE CALL");
	if (!targ->takedamage)
		return;
	if (targ->pickup_master)
		attacker = targ->pickup_master;

	if (targ->takedamage == DAMAGE_PUSH)
		goto knockback;

	//return;
	//VectorSet(normal, random(), random(), random());
	//gi.bprintf(PRINT_HIGH, "T_DAMAGE: attacker = %s, victim = %s\n", attacker->classname, targ->classname);
	if ((targ->svflags & SVF_GRENADE || targ->svflags & SVF_ROCKET) && !(dflags & DAMAGE_RADIUS))
		damage *= 5;

	add_sp_score(attacker, damage, SCORE_DAMAGE_DEALT);
	add_sp_score(targ, damage, SCORE_DAMAGE_RECEIVED);
	
	// friendly fire avoidance
	// if enabled you can't hurt teammates (but you can hurt yourself)
	// knockback still occurs
	if ((targ != attacker) && ((deathmatch->value && ((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS))) || coop->value))
	{
		if (OnSameTeam (targ, attacker))
		{
			if ((int)(dmflags->value) & DF_NO_FRIENDLY_FIRE)
				damage = 0;
			else
				mod |= MOD_FRIENDLY_FIRE;
		}
	}
	meansOfDeath = mod;
	
	// easy mode takes half damage
	if (skill->value == 0 && deathmatch->value == 0 && targ->client)
	{
		damage *= 0.5;
		if (!damage)
			damage = 1;
	}
	if (skill->value > 3 && attacker->svflags & SVF_MONSTER)
	{
		damage *= skill->value * 0.1;
	}
	client = targ->client;
	
	if (dflags & DAMAGE_BULLET)
	{
		if ((mod == MOD_SHOTGUN || mod == MOD_SSHOTGUN || mod == MOD_RAILGUN_FRAG) && rand() % 100 > 25)
			te_sparks = TE_SHOTGUN;
		else
			te_sparks = TE_BULLET_SPARKS;	
	}
	else
		te_sparks = TE_SPARKS;

	VectorNormalize(dir);
	
// bonus damage for suprising a monster
	
	if (!(dflags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER) && (attacker->movetype == MOVETYPE_WALK) && (!targ->enemy) && (targ->health > 0))
		damage *= 1.25;
	
knockback:
	if (targ->flags & FL_NO_KNOCKBACK)
		knockback = 0;

// figure momentum add
	if (!(dflags & DAMAGE_NO_KNOCKBACK))
	{
		if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP))
		{
			vec3_t	kvel;
			float	mass;

			if (targ->mass < 50)
				mass = 50;
			else
				mass = targ->mass;

		//	if (targ->client  && attacker == targ)
		//		VectorScale (dir, 1600.0 * (float)knockback / mass, kvel);	// the rocket jump hack...
		//	else
			if (dflags & DAMAGE_RADIUS)
			{
				if (targ->svflags & SVF_BOLT) /// ??????
				{
					VectorScale(dir, 500.0 * (float)knockback / mass, kvel);
					damage *= 0.01;

				}
				else
				{
					VectorScale(dir, 500.0 * (float)knockback / mass, kvel);
				}
			}
			else if (mod == MOD_ROCKET || mod == MOD_GRENADE || mod == MOD_HANDGRENADE)
			{
				VectorScale(dir, 150.0 * (float)knockback / mass, kvel);
				targ->velocity[2] += clamp(VectorLength(kvel) * 0.5, 200, -1000);
			}
			else
				VectorScale(dir, 500.0 * (float)knockback / mass, kvel);

			VectorAdd (targ->velocity, kvel, targ->velocity);
		}
	}
	if (targ->takedamage == DAMAGE_PUSH)
		return;
	//gi.bprintf(PRINT_HIGH, "damage take = %i", take);
	take = damage;
	save = 0;
	//gi.bprintf(PRINT_HIGH, "%s hurting %s by %s\n", attacker->classname, targ->classname, inflictor->classname);

	// check for godmode
	if ( (targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION) )
	{
		take = 0;
		save = damage;
		SpawnDamage (te_sparks, point, normal, save);
	}
	
	// check for invincibility
	if (item_mod->value && client)
	{
		if (targ->client->pers.invu_health > 0)
		{

			targ->client->pers.invu_health -= take;
			if (targ->client->pers.invu_health < 0)
			{
				save = damage;
				take = targ->client->pers.invu_health * (-1);
				targ->client->pers.invu_health = 0;
				targ->client->ps.stats[STAT_TIMER_ICON] = 0;


			}
			else
			{
				take = 0;
				save = damage;

			}




			if (targ->pain_debounce_time < level.time)
			{
				gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_IDLE, 0);
				targ->pain_debounce_time = level.time + 2;
			}
		}
	}
	else if ((client && client->invincible_framenum > level.framenum) && !(dflags & DAMAGE_NO_PROTECTION))
	{
		if (targ->pain_debounce_time < level.time)
		{
			gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
			targ->pain_debounce_time = level.time + 2;
		}
		take = 0;
		save = damage;
	}
	if (targ->svflags & SVF_MONSTER)// && targ->mins[0] > -60)// && targ->health > 0)
	{
		vec3_t add_punch;
		VectorClear(add_punch);
		int pdmg = take - ((take + fabs(targ->mins[0]) * 2.0f) * 0.5);
		pdmg = clamp(pdmg, 45, 0);
		//if (mod == MOD_SHOTGUN || MOD_SSHOTGUN)
		//	pdmg *= 0.25;
		
		add_punch[0] += pdmg * (0.5 + (random() * 0.5)); //UPDOWN
		add_punch[1] += pdmg * (0.5 + (random() * 0.5)); //SIDE
		add_punch[2] += pdmg * (0.5 + (random() * 0.5));

		


		if (rand() & 1)
			add_punch[0] *= -1;
		if (rand() & 1)
			add_punch[1] *= -1;
		if (rand() & 1)
			add_punch[2] *= -1;
		//gi.bprintf(PRINT_HIGH, "DEBUG: ADDED PUNCH TO ANGLES: %s, take = %i\n", vtos(add_punch), take);
		targ->s.angles[0] = targ->s.angles[0] + add_punch[0];
		targ->s.angles[1] = targ->s.angles[1] + add_punch[1];
		targ->s.angles[2] = targ->s.angles[2] + add_punch[2];
		//return;
	}
	
	psave = CheckPowerArmor (targ, point, normal, take, dflags, mod);
	take -= psave;
	add_sp_score(targ, psave, SCORE_DAMAGE_SAVED);
	asave = CheckArmor (targ, point, normal, take, te_sparks, dflags);
	take -= asave;
	
	//treat cheat/powerup savings the same as armor
	asave += save;
	add_sp_score(targ, damage - take, SCORE_DAMAGE_SAVED);

	add_sp_score(targ, asave, SCORE_DAMAGE_SAVED);

	// team damage avoidance
	if (!(dflags & DAMAGE_NO_PROTECTION) && CheckTeamDamage (targ, attacker))
		return;
	int headshot = 0;

	if (targ->health > 0 && (!(dflags & DAMAGE_RADIUS)))
	
		take *= location_scaling(targ, point, damage, mod, headshot);
	
// do the damage
	if (targ->svflags & SVF_GRAVITYGRENADE && inflictor->svflags & SVF_GRAVITYGRENADE)
		return;

	if (take)
	{
		if ((targ->svflags & SVF_MONSTER) || (client))
		{
			//gi.bprintf(PRINT_HIGH, "T_DAMAGE: ENTITY IS BLEEDING SOMEHOW, client = %i, SVF_MONSTER = %i\n", (client), (targ->svflags & SVF_MONSTER) );

			SpawnDamage(TE_BLOOD, point, normal, take);

		}
		else
		{
			//gi.bprintf(PRINT_HIGH, "T_DAMAGE: ENTITY IS SPARKING SOMEHOW, client = %i, SVF_MONSTER = %i\n", (client), (targ->svflags & SVF_MONSTER));
			SpawnDamage(te_sparks, point, normal, take);

		}
		if(!(targ->svflags & SVF_NOGIBS) && targ != inflictor)
			ThrowGib_damage(targ, inflictor, attacker, take, point);
		
		//if(attacker->svflags & SVF_MONSTER && (attacker->monsterinfo.monster_type == MONSTER_GUNNER))
		//	gi.bprintf(PRINT_HIGH, "DEBUG: GUNNER ATTACKED SOMEONE!\n");

		if (((attacker->svflags & SVF_MONSTER) && (attacker->monsterinfo.monster_type == MONSTER_GUNNER) && (targ->svflags & SVF_GRENADE) && (inflictor->svflags & SVF_GRENADE)) &&
			((targ->owner && targ->owner == attacker) ||
				(targ->owner_solid && targ->owner_solid == attacker)))
			//this is for gunner grenades to make them stop exploding in a trail back to the gunner hurting/killing him.
			//I guess temp measure.
		{
			//gi.bprintf(PRINT_HIGH, "DEBUG: GUNNER GRENADES DON'T HURT THEMSELVES!\n"); // *if they are from the same monster
			goto skip_damage;
		}
		if (real_gibbing->value && targ->health - take < 0 && !can_gib(targ, mod))
		{
			if (had_hp > 0)
			{
				targ->health -= had_hp; // = 0
				temp_take = (int)((take - had_hp) * DEAD_DMG_REDUCTION);
				if (temp_take < 1)
					temp_take = 1;
				targ->health = targ->health - temp_take;
			}
			else
			{
				temp_take = (int)(take * DEAD_DMG_REDUCTION);
				if (temp_take < 1)
					temp_take = 1;
				targ->health = targ->health - temp_take;
			}
			
		}
		else
			targ->health = targ->health - take;
			
		if (targ->health <= 0)
		{
			if ((targ->svflags & SVF_MONSTER) || (client))
				targ->flags |= FL_NO_KNOCKBACK;
			//gi.bprintf(PRINT_HIGH, "%s killed with %s by %s\n", targ->classname, inflictor->classname, attacker->classname);
			add_sp_score(attacker, targ->max_health, SCORE_KILLS);

			if(headshot)
				targ->flags |= FL_HEADSHOT;
			
			Killed (targ, inflictor, attacker, take, point);
			return;
		}
	}
	
skip_damage:

	if (targ->svflags & SVF_MONSTER)
	{
		
		M_ReactToDamage (targ, attacker);
		targ->monsterinfo.temp_inaccuracy += (take + save)* 2;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take))
		{
			targ->pain (targ, attacker, knockback, take);
			// nightmare mode monsters don't go into pain frames often
			if (skill->value == 3)
				targ->pain_debounce_time = level.time + 5;
		}
	}
	else if (client)
	{
		if (!(targ->flags & FL_GODMODE) && (take))
			targ->pain (targ, attacker, knockback, take);
	}
	else if (take)
	{
		if (targ->pain)
			targ->pain (targ, attacker, knockback, take);
	}
	
	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client)
	{
		client->damage_parmor += psave;
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		VectorCopy (point, client->damage_from);
	}
}


/*
============
T_RadiusDamage
============
*/
void T_RadiusDamage (edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod)
{
	float	points;
	edict_t	*ent = NULL;
	vec3_t	v;
	vec3_t	dir;
	radius *= 0.55f;
	
	//gi.bprintf(PRINT_HIGH, "T_RadiusDamage\n");


	while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL)
	{
		if (ent == ignore)
			continue;

		if (!ent->takedamage)
			continue;

		if (inflictor == ent && ent->svflags & SVF_PROJECTILE)
			continue;
		//gi.bprintf(PRINT_HIGH, "inflictor = %s proj = %i, ent = %s proj = %i\n", inflictor->classname, inflictor->svflags & SVF_PROJECTILE, ent->classname, ent->svflags & SVF_PROJECTILE);
	//	if (inflictor->svflags & SVF_PROJECTILE)
		//	continue;

		VectorAdd (ent->mins, ent->maxs, v);
		VectorMA (ent->s.origin, 0.5, v, v);
		VectorSubtract (inflictor->s.origin, v, v);
		points = damage - 0.5 * VectorLength (v);

		//if (ent == attacker)
		//	points = points * 0.5;

		if (points > 0)
		{
			if (CanDamage (ent, inflictor))
			{
				VectorSubtract (ent->s.origin, inflictor->s.origin, dir);
				T_Damage (ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
			}
		}
	}
	
	 spawn_shockwave(inflictor, attacker, damage * 0.5, radius * 4, mod); 

}
void shockwave_think(edict_t *self)
{
	self->nextthink = level.time + FRAMETIME;
	trace_t tr;
	edict_t *ent;
	vec3_t dir, vel_add, frame_vel;
	float points;
	float kick;
	float radius = (self->dmg_radius / 3) * (self->count + 1);
	float inner_radius = (self->dmg_radius / 4) * self->count;
	ent = NULL;
	vec3_t normal, point;

	while ((ent = findradius(ent, self->s.origin, radius)) != NULL)
	{

	//	gi.bprintf(PRINT_HIGH, "SHOCKWAVE_THINK: SEARCHING FOR SOMEONE TO DAMAGE, RADIUS = %f\n", radius);
		if (!ent->inuse)
			continue;
		if(self->activator == ent)
			continue;
		if (!ent->takedamage )
		{
			if (check_knockback(ent))
			{
				
				//if(!ent->s.origin)
				//gi.bprintf(PRINT_HIGH, "shockwave_think: DEBRIS1: dir = %f %f %f\n", dir[0], dir[1], dir[2]);

				VectorSubtract(ent->s.origin, self->s.origin, dir);

				points = clamp(self->dmg / (1 + (VectorLength(dir) * 0.02)), 300, 5);
				if (self->s.effects & EF_GIB && self->mass == 100)
				{
					ent->avelocity[0] += points * crandom() * 0.1;
					ent->avelocity[1] += points * crandom() * 0.1;
					ent->avelocity[2] += points * crandom() * 0.1;

				}
				else
				{
					ent->avelocity[0] += points * crandom() * 25;
					ent->avelocity[1] += points * crandom() * 25;
					ent->avelocity[2] += points * crandom() * 25;
				}
				dir[0] *= 1 + (crandom() * 0.5);
				dir[0] *= 1 + (crandom() * 0.5);
				dir[0] *= 1 + (crandom() * 0.5);
				VectorNormalize(dir);
				//gi.bprintf(PRINT_HIGH, "shockwave_think: points = %f\n", points);

				VectorMA(ent->velocity, points, dir, ent->velocity);
				//gi.bprintf(PRINT_HIGH, "SHOCKWAVE THINK: ent org = %s, self org = %s\n", vtos(ent->s.origin), vtos(self->s.origin)); //, points, dir[0], dir[1], dir[2], vtos(ent->velocity));

				//gi.bprintf(PRINT_HIGH, "shockwave_think: DEBRIS2:vel1 = %s, vel2 = %s, points = %f, self->dmg = %i, dir\n", vtos(DEBRIS_EDICT->velocity), vtos(DEBRIS_VELOCITY));
				VectorCopy(ent->velocity, frame_vel);
				VectorScale(frame_vel, FRAMETIME, frame_vel);
				
				tr = gi.trace(ent->s.origin, NULL, NULL, frame_vel, NULL, MASK_SOLID); //to see if destination is blocked/wall
				if (tr.fraction != 1)
				{

					if (tr.fraction < 0.4)
					{
						ent->velocity[2] += 25 + (random() * 75) + (25 * (1 - tr.fraction));
						
						//VectorScale(ent->velocity, -1, ent->velocity);
						ent->velocity[2] *= -1;
					}
					else
						ent->velocity[2] += 50 + (25 * (1 - tr.fraction));


					VectorScale(ent->velocity, 1 + (1 - tr.fraction), ent->velocity); //1-tr.fraction

					VectorCopy(ent->velocity, frame_vel);
					VectorScale(frame_vel, FRAMETIME, frame_vel);
					int axis = 0;
					int num = 0;
					trace:
					if (!axis)
						axis = (rand() % 2) + 1;
					else if (axis == 1)
						axis = 2;
					else if (axis == 2)
						axis = 1;

					tr = gi.trace(ent->s.origin, NULL, NULL, frame_vel, NULL, MASK_SOLID); //to see if destination is blocked/wall
					if (tr.fraction < 0.4)
					{
						ent->velocity[axis] *= -1;
					}
					tr = gi.trace(ent->s.origin, NULL, NULL, frame_vel, NULL, MASK_SOLID); //to see if destination is blocked/wall
					VectorCopy(ent->velocity, frame_vel);
					VectorScale(frame_vel, FRAMETIME, frame_vel);
					if (tr.fraction < 0.4 && !num)
					{
						num++;
						goto trace;
					}
						
				}
				else
				{
					if(!ent->groundentity)
					ent->velocity[2] += 50 + (points * 0.25);

					ent->velocity[2] += 50 + (points * 0.25);

				}
				if (ent->velocity[2] > 250)
					ent->velocity[2] = 250;
			}
			continue;
		}
		VectorClear(dir);
		//gi.bprintf(PRINT_HIGH, "SHOCKWAVE_THINK: FOUND SOMEONE TO DAMAGE: %s, waterlevel = %i\n", ent->classname, self->waterlevel);
		VectorSubtract(ent->s.origin, self->s.origin, dir);

		if (VectorLength(dir) < inner_radius - (inner_radius * 0.25))
		{
			//gi.bprintf(PRINT_HIGH, "SHOCKWAVE_THINK: VL_DIR = %f, inner radius = %f\n", VectorLength(dir), inner_radius);

			continue;
		}
		if (!CanDamageSphere(self->s.origin, ent, radius) && !CanDamageSphere(self->s.origin, ent, inner_radius))
			continue;

		points = self->dmg / (VectorLength(dir) * 0.025);
		kick = points / 4 ;
		if (gi.pointcontents(self->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA) && !ent->waterlevel && VectorLength(dir) > 64)
			continue;
		if (gi.pointcontents(self->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA) && !ent->waterlevel && VectorLength(dir) < 64)
			points *= 0.25;

		if (!(gi.pointcontents(self->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) && ent->waterlevel == 3 && VectorLength(dir) > 64)
			continue;

		if (!(gi.pointcontents(self->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) && ent->waterlevel == 3 && VectorLength(dir) < 64)
			points *= 0.25;

		if (gi.pointcontents(self->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA) && ent->waterlevel == 3)
			points *= 2;

		
		VectorNormalize(dir);
		//gi.bprintf(PRINT_HIGH, "SHOCKWAVE_THINK: SEARCHING FOR ENTITIES IN RADIUS, found = %s, points = %f, radius was = %f\n", ent->classname, points, radius);
		
		VectorCopy(dir, normal);
		VectorCopy(dir, point);
		//VectorScale(point, -1, point);
		VectorMA(ent->s.origin, ent->mins[0], point, point);
		T_Damage(ent, self, self->owner, dir, point, normal, (int)points, (int)kick, DAMAGE_RADIUS, self->mod);

	}

	self->count++;

	if (self->count == 3)
		G_FreeEdict(self);

}
void spawn_shockwave(edict_t *inflictor, edict_t *attacker, int damage, int radius, int mod)
{
	
	edict_t *swave;
	swave = G_Spawn();
	VectorCopy(inflictor->s.origin, swave->s.origin);
	swave->think = shockwave_think;
	swave->nextthink = level.time + 0.1;
	swave->dmg = damage;
	swave->dmg_radius = radius;
	swave->owner = attacker;
	swave->activator = inflictor;
	swave->mod = mod;
	swave->classname = "shockwave";
	shockwave_think(swave);
	
}
qboolean check_knockback(edict_t *self)
{
	if (strcmp(self->classname, "gibx") == 0)
		return true;
	if (strcmp(self->classname, "gibd") == 0)
		return true;
	else if (strcmp(self->classname, "grenade") == 0)
		return true;
	else if (strcmp(self->classname, "hgrenade") == 0)
		return true;

	else if (strcmp(self->classname, "bolt") == 0)
		return true;

	else if (strcmp(self->classname, "sgrenade") == 0)
		return true;
	else if (strcmp(self->classname, "rocket") == 0)
		return true;
	else if (strcmp(self->classname, "subexpl") == 0)
		return true;
	else if (strcmp(self->classname, "smodel") == 0)
		return true;
	else if (strcmp(self->classname, "bfg blast") == 0)
		return true;
	else if (strcmp(self->classname, "debris") == 0)
		return true;
	else if (strcmp(self->classname, "damagegib") == 0)
		return true;
	//else if (self->client || self->svflags & SVF_MONSTER)
	//	return true;
	return false;
}

qboolean CanDamageSphere(vec3_t start, edict_t *targ, float radius)
{
	vec3_t point, point_org;
	trace_t trace;
	int point_num = 0;
	VectorCopy(targ->s.origin, point_org);
	VectorCopy(point_org, point);
	//VectorScale(point_org, 1.1, point_org);
trace:
	if(point_num > 6)
		trace = gi.trace(start, NULL, NULL, targ->s.origin, NULL, MASK_SHOT);
	else
		trace = gi.trace(start, NULL, NULL, point, NULL, MASK_SHOT);

	//debug_trail(start, point);
	/*if (trace.fraction == 1)
	{
		if(point_num < 6)
		point_num = 6;

		//gi.bprintf(PRINT_HIGH, "SHOCKWAVE CANDAMAGESPHERE: CAN  radius = %f, point_num = %i\n", radius, point_num);
		if (point_num >= 6)
			return true;
	}
	else
		gi.bprintf(PRINT_HIGH, "SHOCKWAVE CANDAMAGESPHERE: hit %s\n", trace.ent->classname);
		*/
	if (targ == trace.ent)
		return true;
	point_num++;

	if (point_num == 1)
	{
		VectorCopy(point_org, point);
		point[0] = -(radius / 2);
		goto trace;
	}
	else if (point_num == 2)
	{
		VectorCopy(point_org, point);
		point[0] = (radius / 2);
		goto trace;
	}
	else if (point_num == 3)
	{
		VectorCopy(point_org, point);
		point[1] = -(radius / 2);
		goto trace;
	}
	else if (point_num == 4)
	{
		VectorCopy(point_org, point);
		point[1] = (radius / 2);
		goto trace;
	}
	else if (point_num == 5)
	{
		VectorCopy(point_org, point);
		point[2] = -(radius / 2);
		goto trace;
	}
	else if (point_num == 6)
	{
		VectorCopy(point_org, point);
		point[2] = (radius / 2);
		goto trace;
	}

	//gi.bprintf(PRINT_HIGH, "SHOCKWAVE CANDAMAGESPHERE: CAN'T  radius = %f, point_num = %i\n", radius, point_num);
	return false;
}

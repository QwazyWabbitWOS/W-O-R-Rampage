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
#include "g_local.h"
#include "mtwist.h"

void M_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void M_calcvel(edict_t *self);
void M_isplayerstrafing(edict_t *self);
void M_calcstrafepos(edict_t *self, vec3_t enemypos);
void M_save_enemy_pos(edict_t *self);
void M_clear_enemy_pos(edict_t *self);
//
// monster weapons
//

//FIXME mosnters should call these with a totally accurate direction
// and we can mess it up based on skill.  Spread should be for normal
// and we can tighten or loosen based on skill.  We could muck with
// the damages too, but I'm not sure that's such a good idea.
void monster_fire_bullet (edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype)
{
	fire_bullet (self, start, dir, damage, kick, hspread, vspread, MOD_UNKNOWN);
	spawn_m_muzzleflash(self, start, dir, flashtype);

	/*gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);*/
}

void monster_fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype)
{
	fire_shotgun (self, start, aimdir, damage, kick, hspread, vspread, count, MOD_SHOTGUN);
	spawn_m_muzzleflash(self, start, aimdir, flashtype);
	/*gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);*/
}

void monster_fire_blaster (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect)
{

	fire_blaster (self, start, dir, damage, speed, effect, 0);
	spawn_m_muzzleflash(self, start, dir, flashtype+ 1000);


	return;
	/*else
	{
		gi.WriteByte(svc_muzzleflash2);
		gi.WriteShort(self - g_edicts);
		gi.WriteByte(flashtype);
		gi.multicast(start, MULTICAST_PVS);
	}*/
}	

void monster_fire_shotgun_grenade(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype)
{
	fire_shotgun_grenade(self, start, dir, damage, speed);

	gi.WriteByte(svc_muzzleflash2);
	gi.WriteShort(self - g_edicts);
	gi.WriteByte(flashtype);
	gi.multicast(start, MULTICAST_PVS);
}
void monster_fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype)
{
	fire_grenade (self, start, aimdir, damage, speed, 2.5, damage+40);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype)
{
	fire_rocket (self, start, dir, damage, speed, damage+20, damage);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}	

void monster_fire_railgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype)
{
	fire_rail (self, start, aimdir, damage, kick);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}


void monster_fire_bfg (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype)
{
	fire_bfg (self, start, aimdir, damage, speed, damage_radius);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}



//
// Monster utility functions
//

static void M_FliesOff (edict_t *self)
{
	self->s.effects &= ~EF_FLIES;
	self->s.sound = 0;
}

static void M_FliesOn (edict_t *self)
{
	if (self->waterlevel)
		return;
	self->s.effects |= EF_FLIES;
	self->s.sound = gi.soundindex ("infantry/inflies1.wav");
	self->think = M_FliesOff;
	self->nextthink = level.time + 60;
}

void M_FlyCheck (edict_t *self)
{
	if (self->waterlevel)
		return;

	if (random() > 0.5)
		return;

	self->think = M_FliesOn;
	self->nextthink = level.time + 5 + 10 * random();
}

void AttackFinished (edict_t *self, float time)
{
	self->monsterinfo.attack_finished = level.time + time;
}


void M_CheckGround (edict_t *ent)
{
	vec3_t		point;
	trace_t		trace;

	if (ent->flags & (FL_SWIM|FL_FLY))
		return;

	if (ent->velocity[2] > 100)
	{
		ent->groundentity = NULL;
		return;
	}

// if the hull point one-quarter unit down is solid the entity is on ground
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] - 0.25;

	trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, point, ent, MASK_MONSTERSOLID);

	// check steepness
	if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
	{
		ent->groundentity = NULL;
		return;
	}

//	ent->groundentity = trace.ent;
//	ent->groundentity_linkcount = trace.ent->linkcount;
//	if (!trace.startsolid && !trace.allsolid)
//		VectorCopy (trace.endpos, ent->s.origin);
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, ent->s.origin);
		ent->groundentity = trace.ent;
		ent->groundentity_linkcount = trace.ent->linkcount;
		ent->velocity[2] = 0;
	}
}


void M_CatagorizePosition (edict_t *ent)
{
	vec3_t		point;
	int			cont;

//
// get waterlevel
//
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] + ent->mins[2] + 1;	
	cont = gi.pointcontents (point);

	if (!(cont & MASK_WATER))
	{
		ent->waterlevel = 0;
		ent->watertype = 0;
		return;
	}

	ent->watertype = cont;
	ent->waterlevel = 1;
	point[2] += 26;
	cont = gi.pointcontents (point);
	if (!(cont & MASK_WATER))
		return;

	ent->waterlevel = 2;
	point[2] += 22;
	cont = gi.pointcontents (point);
	if (cont & MASK_WATER)
		ent->waterlevel = 3;
}


void M_WorldEffects (edict_t *ent)
{
	int		dmg;

	if (ent->health > 0)
	{
		if (!(ent->flags & FL_SWIM))
		{
			if (ent->waterlevel < 3)
			{
				ent->air_finished = level.time + 12;
			}
			else if (ent->air_finished < level.time)
			{	// drown!
				if (ent->pain_debounce_time < level.time)
				{
					dmg = 2 + 2 * floorf(level.time - ent->air_finished);
					if (dmg > 15)
						dmg = 15;
					T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
					ent->pain_debounce_time = level.time + 1;
				}
			}
		}
		else
		{
			if (ent->waterlevel > 0)
			{
				ent->air_finished = level.time + 9;
			}
			else if (ent->air_finished < level.time)
			{	// suffocate!
				if (ent->pain_debounce_time < level.time)
				{
					dmg = 2 + 2 * floorf(level.time - ent->air_finished);
					if (dmg > 15)
						dmg = 15;
					T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
					ent->pain_debounce_time = level.time + 1;
				}
			}
		}
	}
	
	if (ent->waterlevel == 0)
	{
		if (ent->flags & FL_INWATER)
		{	
			gi.sound(ent, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
			ent->flags &= ~FL_INWATER;
		}
		return;
	}

	if ((ent->watertype & CONTENTS_LAVA) && !(ent->flags & FL_IMMUNE_LAVA))
	{
		if (ent->damage_debounce_time < level.time)
		{
			ent->damage_debounce_time = level.time + 0.2;
			T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 10*ent->waterlevel, 0, 0, MOD_LAVA);
		}
	}
	if ((ent->watertype & CONTENTS_SLIME) && !(ent->flags & FL_IMMUNE_SLIME))
	{
		if (ent->damage_debounce_time < level.time)
		{
			ent->damage_debounce_time = level.time + 1;
			T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 4*ent->waterlevel, 0, 0, MOD_SLIME);
		}
	}
	
	if ( !(ent->flags & FL_INWATER) )
	{	
		if (!(ent->svflags & SVF_DEADMONSTER))
		{
			if (ent->watertype & CONTENTS_LAVA)
				if (random() <= 0.5)
					gi.sound(ent, CHAN_BODY, gi.soundindex("player/lava1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(ent, CHAN_BODY, gi.soundindex("player/lava2.wav"), 1, ATTN_NORM, 0);
			else if (ent->watertype & CONTENTS_SLIME)
				gi.sound(ent, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
			else if (ent->watertype & CONTENTS_WATER)
				gi.sound(ent, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		}

		ent->flags |= FL_INWATER;
		ent->damage_debounce_time = 0;
	}
}


void M_droptofloor (edict_t *ent)
{
	vec3_t		end;
	trace_t		trace;

	ent->s.origin[2] += 1;
	VectorCopy (ent->s.origin, end);
	end[2] -= 256;
	
	trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);

	if (trace.fraction == 1 || trace.allsolid)
		return;

	VectorCopy (trace.endpos, ent->s.origin);

	gi.linkentity (ent);
	M_CheckGround (ent);
	M_CatagorizePosition (ent);
}


void M_SetEffects (edict_t *ent)
{
	ent->s.effects &= ~(EF_COLOR_SHELL|EF_POWERSCREEN);
	ent->s.renderfx &= ~(RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE);

	if (ent->monsterinfo.aiflags & AI_RESURRECTING)
	{
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= RF_SHELL_RED;
	}

	if (ent->health <= 0)
		return;

	if (ent->powerarmor_time > level.time)
	{
		if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SCREEN)
		{
			ent->s.effects |= EF_POWERSCREEN;
		}
		else if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SHIELD)
		{
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}
}


void M_MoveFrame (edict_t *self)
{
	mmove_t	*move;
	int		index;
	//if (self->enemy)
	//	gi.bprintf(PRINT_HIGH, "DEBUG: M_MoveFrame, time = %f\n", level.time);
	move = self->monsterinfo.currentmove;
	self->nextthink = level.time + FRAMETIME;

	if ((self->monsterinfo.nextframe) && (self->monsterinfo.nextframe >= move->firstframe) && (self->monsterinfo.nextframe <= move->lastframe))
	{
		self->s.frame = self->monsterinfo.nextframe;
		self->monsterinfo.nextframe = 0;
	}
	else
	{
		if (self->s.frame == move->lastframe)
		{
			if (move->endfunc)
			{
				move->endfunc (self);

				// regrab move, endfunc is very likely to change it
				move = self->monsterinfo.currentmove;

				// check for death
				if (self->svflags & SVF_DEAD)
					return;
			}
		}

		if (self->s.frame < move->firstframe || self->s.frame > move->lastframe)
		{
			self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
			self->s.frame = move->firstframe;
		}
		else
		{
			if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
			{
				if (M_isrunningbackwards(self) && self->groundentity && self->health > 0 || self->monsterinfo.aiflags & AI_REVERSE_ANIM)
				{
					self->s.frame--;
					if (self->s.frame < move->firstframe)
						self->s.frame = move->lastframe;
				}
				else
				{
					self->s.frame++;
					if (self->s.frame > move->lastframe)
						self->s.frame = move->firstframe;
				}

			}
		}
	}

	index = self->s.frame - move->firstframe;
	if (move->frame[index].aifunc)
		if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
			move->frame[index].aifunc (self, move->frame[index].dist * self->monsterinfo.scale);
		else
			move->frame[index].aifunc (self, 0);

	if (move->frame[index].thinkfunc)
		move->frame[index].thinkfunc (self);
}


void monster_think (edict_t *self)
{
	/*if (self->s.frame == 474 && !self->dmg)
	{
		self->svflags |= SVF_GRENADE;
		self->dmg = rand() % 1000;
	}*/
	/*if (self->enemy)
	{
		if (self->enemy->health <= 0)
			self->enemy = NULL;
	}*/
	/*if (self->mins[2] == -21.6 && self->monsterinfo.aiflags & AI_DUCKED && !(self->s.frame <= 30 && self->s.frame <= 38 || self->s.frame <= 45 && self->s.frame <= 49))
	{
		gi.bprintf(PRINT_HIGH, "MONSTER_THINK: soldier duck bugged! Frame = %i", self->s.frame);
	}*/
	//if (self->enemy && self->enemy->client)//(int)(self->enemy->client->ucmd->forwardmove))
	//	gi.bprintf(PRINT_HIGH, "DEBUG: forwardmove = %i, lime = %f, enemy=%s\n", (int)(self->enemy->client->forwardmove),level.time, self->enemy->classname);
		//if(self->enemy && self->enemy->client && self->health > 0 && self->goalentity)
		//gi.bprintf(PRINT_HIGH, "DEBUG: forwardmove = %i, old forwardmove %i, sidemove = %i, old sidemove = %i\n", (int)(self->enemy->client->ucmd->forwardmove), (int)(self->enemy->client->olducmd->forwardmove), (int)(self->enemy->client->ucmd->sidemove), (int)(self->enemy->client->olducmd->sidemove));
	//	gi.bprintf(PRINT_HIGH, "DEBUG: goalentity = %s at %s\n", self->goalentity->classname, vtos(self->goalentity->s.origin));
	M_calcvel(self);
	if (self->monsterinfo.temp_inaccuracy > 0)
		self->monsterinfo.temp_inaccuracy -= clamp((int)(1 * (self->max_health * 0.01)), 999, 1);
	if (self->monsterinfo.temp_inaccuracy < 0)
		self->monsterinfo.temp_inaccuracy = 0;
	if (!VectorCompare(self->s.origin, self->old_origin2) || !self->groundentity)
	{
		self->monsterinfo.aimdur = 2;
	}
	else
	{
		self->monsterinfo.aimdur -= 0.1f;
	}
	self->monsterinfo.aimdur = clamp(self->monsterinfo.aimdur, 2, 1);
	if (strcmp(self->classname, "monster_jorg") && level.time > 1 && self->monsterinfo.monster_type != MONSTER_INSANE)
	{
		
		if (mt_ldrand() < 0.05)//if (!self->movetarget && mt_ldrand() < 0.1 && strcmp(self->classname, "monster_jorg")) //everyone except makron will change directions when idle
		{
			float yawb = self->s.angles[YAW];
			float fraction;
			vec3_t dest;
			float dist = 512;
			int tries = 0;

			rand_dir:
			self->ideal_yaw = crandom() * 180;
			self->s.angles[YAW] = self->ideal_yaw;
			
			fraction = scan_dir(self, SCAN_FORWARD, dist, dest);
			//spawn_blood_point(dest);
			//gi.bprintf(PRINT_HIGH, "MONSTER THINK: fraction = %f\n", fraction);
			if (fraction < 0.9 && tries < 100)// && fraction - random() > 0.1)
			{
				if(tries <= 8)
					dist--;
				else 
					dist -= 16;
				tries++;
				goto rand_dir;
			}
				
			self->s.angles[YAW] = yawb;
		}

		if (!self->enemy && (self->s.angles[YAW]) != self->ideal_yaw)
			M_ChangeYaw(self);
	}
	if (self->monsterinfo.aiflags & AI_CHARGEDSHOT && self->monsterinfo.charge < BLASTER_MAX_CHARGE)
	{
		self->monsterinfo.charge++;
		//gi.bprintf(PRINT_HIGH, "MONSTER THINK: charge = %i\n", self->monsterinfo.charge);
	}
	if (self->flags & FL_FLY && self->monsterinfo.monster_type != MONSTER_INSANE)
	{
		if (self->health > 0)
		{
			self->velocity[0] += crandom() * 10;
			self->velocity[1] += crandom() * 10;
			self->velocity[2] += crandom() * 10;
		}
		else
		{
			self->velocity[0] += crandom() * 100;
			self->velocity[1] += crandom() * 100;
			self->velocity[2] += crandom() * 100;
		}
	}
	//BELOW IS A CHECK TO SEE IF THE PLAYER IS AIMING AT US, IF YES THEN DODGE
	if (self->health > 0 && self->monsterinfo.dodge && self->enemy && !(self->enemy->svflags & SVF_MONSTER))// && !strncmp(self->classname, "monster_soldier", 11))
	{
		if (visible(self, self->enemy) && infront(self, self->enemy) && visible(self->enemy, self) && infront_aiming(self->enemy, self))
		{
			if(self->monsterinfo.monster_type == MONSTER_SOLDIER)
				self->monsterinfo.dodge(self, self->enemy, 0.0);
			self->monsterinfo.aiflags |= AI_AIMED_AT;

			if(rand() & 1)
				self->monsterinfo.aiflags |= AI_JUMPATTACK;
			//gi.bprintf(PRINT_HIGH, "MONSTER THINK: ENEMY IS AIMING AT US!!!\n");
		}
		else
			self->monsterinfo.aiflags &= ~AI_AIMED_AT;
		
	}
	if (self->monsterinfo.emp_effect_left && !(self->svflags & SVF_DEAD))
	{
		//gi.bprintf(PRINT_HIGH, "self->monsterinfo.emp_effect_left = %i", self->monsterinfo.emp_effect_left);
		self->nextthink = level.time + FRAMETIME;
		
		self->monsterinfo.emp_effect_left = clamp(self->monsterinfo.emp_effect_left - 1, self->monsterinfo.emp_effect_left, 0);
		if (self->movetype == MOVETYPE_FLY)
			self->movetype = MOVETYPE_FLY_B;
		if (!self->monsterinfo.emp_effect_left == 1 && self->movetype == MOVETYPE_FLY_B)
			self->movetype = MOVETYPE_FLY;
		return;
	}
	else if (self->health > 0) // this is to fix fucked up angles by grav grenades
	{
		//if (self->enemy && self->health > 0)
		//	gi.bprintf(PRINT_HIGH, "DEBUG: monster_think, level.time = %f\n", level.time);
		fix_angles(self);
		if (level.framenum % (int)(1 / FRAMETIME) / 3 == 0)
		{
			self->monsterinfo.predict_skew = 1 + (crandom() * (self->monsterinfo.predict_skew_mult));
			if (self->enemy)
			{
				if(visible(self, self->enemy) && infront(self, self->enemy))
					M_save_enemy_pos(self);
				else if(self->monsterinfo.last_seen < level.time - MONSTER_STRAFE_MAX_TIME_DETECTION)
				{
					M_clear_enemy_pos(self);
				}
			}
		}
		if (self->monsterinfo.predict_skew_mult < 0.25)
			self->monsterinfo.predict_skew_mult += 0.01 * frame_mult();
		if (self->enemy)
		{
			float dot;
			vec3_t vel, oldvel;
			if (self->enemy->client)
			{
				VectorCopy(self->enemy->velocity, vel);
				VectorCopy(self->enemy->client->oldvelocity, oldvel);
			}
			else
			{
				VectorCopy(self->enemy->monsterinfo.velocity, vel);
				VectorCopy(self->enemy->monsterinfo.oldvelocity, oldvel);
			}
			VectorNormalize(vel);
			VectorNormalize(oldvel);
			dot = (DotProduct(vel, oldvel) + 1) / 2;
			if (self->monsterinfo.predict_skew_mult >= 0 && dot != 0.5)
			{
				self->monsterinfo.predict_skew_mult -= dot * 0.025 * frame_mult();
				
			}
			self->monsterinfo.predict_skew_mult = clamp(self->monsterinfo.predict_skew_mult, 0.25, 0);
			M_isplayerstrafing(self);
			//gi.bprintf(PRINT_HIGH, "DEBUG: vel = %s, oldvel = %s, dot = %f, skewm = %f\n", vtos(self->enemy->velocity), vtos(self->enemy->client->oldvelocity), dot, self->monsterinfo.predict_skew_mult);
		}

	}
		

	//if(self->s.frame >= 294 && self->s.frame <= 348)
	//	gi.bprintf(PRINT_HIGH, "frame num = %i\n", self->s.frame);

	/*if (self->svflags & SVF_GRENADE)
	{
		//self->think = monster_think;
		//self->nextthink = level.time + FRAMETIME;
		gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER frame is = %i, origin = %s, oldorg2 = %s, id = %i\n", self->s.frame, vtos(self->s.origin), vtos(self->old_origin2), self->dmg);
		
	}*/
	M_MoveFrame (self);

	if (self->linkcount != self->monsterinfo.linkcount)
	{
		
		self->monsterinfo.linkcount = self->linkcount;
		M_CheckGround (self);
	}

	M_CatagorizePosition (self);

	M_WorldEffects (self);
	M_SetEffects (self);
	

	if (self->health <= 0)
	{
		//gi.bprintf(PRINT_HIGH, "dead! angles = %s\n", vtos(self->move_angles));

		if (self->s.angles[0] != 0 || self->s.angles[2] != 0)
		{
			//gi.bprintf(PRINT_HIGH, "fixing angles! angles = %s\n", vtos(self->move_angles));

			fix_angles(self);
			self->nextthink = level.time + FRAMETIME;
			self->noise_index2 = 1;
			return;
		}
		else if (self->count)
		{
			self->nextthink = level.time + FRAMETIME;
			self->noise_index2 = 0;

		}
	}
	
}


/*
================
monster_use

Using a monster makes it angry at the current activator
================
*/
void monster_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->enemy)
		return;
	if (self->health <= 0)
		return;
	if (activator->flags & FL_NOTARGET)
		return;
	if (!(activator->client) && !(activator->monsterinfo.aiflags & AI_GOOD_GUY))
		return;
	
// delay reaction so if the monster is teleported, its sound is still heard
	self->enemy = activator;
	FoundTarget (self);
}


void monster_start_go (edict_t *self);


void monster_triggered_spawn (edict_t *self)
{
	self->s.origin[2] += 1;
	KillBox (self);

	self->solid = SOLID_BBOX;
	self->movetype = MOVETYPE_STEP;
	self->svflags &= ~SVF_NOCLIENT;
	self->air_finished = level.time + 12;
	gi.linkentity (self);

	monster_start_go (self);

	if (self->enemy && !(self->spawnflags & 1) && !(self->enemy->flags & FL_NOTARGET))
	{
		FoundTarget (self);
	}
	else
	{
		self->enemy = NULL;
	}
}

void monster_triggered_spawn_use (edict_t *self, edict_t *other, edict_t *activator)
{
	// we have a one frame delay here so we don't telefrag the guy who activated us
	self->think = monster_triggered_spawn;
	self->nextthink = level.time + FRAMETIME;
	if (activator->client)
		self->enemy = activator;
	self->use = monster_use;
}

void monster_triggered_start (edict_t *self)
{
	self->solid = SOLID_NOT;
	self->movetype = MOVETYPE_NONE;
	self->svflags |= SVF_NOCLIENT;
	self->nextthink = 0;
	self->use = monster_triggered_spawn_use;
}


/*
================
monster_death_use

When a monster dies, it fires all of its targets with the current
enemy as activator.
================
*/
void monster_death_use (edict_t *self)
{
	self->flags &= ~(FL_FLY|FL_SWIM);
	self->monsterinfo.aiflags &= AI_GOOD_GUY;

	if (self->item)
	{
		Drop_Item (self, self->item);
		self->item = NULL;
	}

	if (self->deathtarget)
		self->target = self->deathtarget;

	if (!self->target)
		return;

	G_UseTargets (self, self->enemy);
}


//============================================================================

qboolean monster_start (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return false;
	}

	if ((self->spawnflags & 4) && !(self->monsterinfo.aiflags & AI_GOOD_GUY))
	{
		self->spawnflags &= ~4;
		self->spawnflags |= 1;
//		gi.dprintf("fixed spawnflags on %s at %s\n", self->classname, vtos(self->s.origin));
	}

	if (!(self->monsterinfo.aiflags & AI_GOOD_GUY))
		level.total_monsters++;

	self->nextthink = level.time + FRAMETIME;
	self->svflags |= SVF_MONSTER;
	self->s.renderfx |= RF_FRAMELERP;
	self->takedamage = DAMAGE_AIM;
	self->air_finished = level.time + 12;
	self->use = monster_use;
	self->max_health = self->health;
	self->clipmask = MASK_MONSTERSOLID;
	self->monsterinfo.aggression =  mt_ldrand();
	self->monsterinfo.charge = 0;
	if(mt_ldrand() > 0.5)
		self->monsterinfo.aiflags |= AI_BRUTAL;
	//self->touch = M_touch;
	self->s.skinnum = 0;
	self->deadflag = DEAD_NO;
	self->svflags &= ~SVF_DEADMONSTER;

	if (!self->monsterinfo.checkattack)
		self->monsterinfo.checkattack = M_CheckAttack;
	VectorCopy (self->s.origin, self->s.old_origin);

	if (st.item)
	{
		self->item = FindItemByClassname (st.item);
		if (!self->item)
			gi.dprintf("%s at %s has bad item: %s\n", self->classname, vtos(self->s.origin), st.item);
	}

	// randomize what frame they start on
	if (self->monsterinfo.currentmove)
		self->s.frame = self->monsterinfo.currentmove->firstframe + (rand() % (self->monsterinfo.currentmove->lastframe - self->monsterinfo.currentmove->firstframe + 1));

	return true;
}

void monster_start_go (edict_t *self)
{
	vec3_t	v;

	if (self->health <= 0)
		return;

		
	// check for target to combat_point and change to combattarget
	if (self->target)
	{
		qboolean	notcombat;
		qboolean	fixup;
		edict_t		*target;

		target = NULL;
		notcombat = false;
		fixup = false;
		while ((target = G_Find (target, FOFS(targetname), self->target)) != NULL)
		{
			if (strcmp(target->classname, "point_combat") == 0)
			{
				self->combattarget = self->target;
				fixup = true;
			}
			else
			{
				notcombat = true;
			}
		}
		if (notcombat && self->combattarget)
			gi.dprintf("%s at %s has target with mixed types\n", self->classname, vtos(self->s.origin));
		if (fixup)
			self->target = NULL;
	}

	// validate combattarget
	if (self->combattarget)
	{
		edict_t		*target;

		target = NULL;
		while ((target = G_Find (target, FOFS(targetname), self->combattarget)) != NULL)
		{
			if (strcmp(target->classname, "point_combat") != 0)
			{
				gi.dprintf("%s at (%i %i %i) has a bad combattarget %s : %s at (%i %i %i)\n",
					self->classname, (int)self->s.origin[0], (int)self->s.origin[1], (int)self->s.origin[2],
					self->combattarget, target->classname, (int)target->s.origin[0], (int)target->s.origin[1],
					(int)target->s.origin[2]);
			}
		}
	}

	if (self->target)
	{
		self->goalentity = self->movetarget = G_PickTarget(self->target);
		if (!self->movetarget)
		{
			gi.dprintf ("%s can't find target %s at %s\n", self->classname, self->target, vtos(self->s.origin));
			self->target = NULL;
			self->monsterinfo.pausetime = 100000000;
			self->monsterinfo.stand (self);
		}
		else if (strcmp (self->movetarget->classname, "path_corner") == 0)
		{
			VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
			self->ideal_yaw = self->s.angles[YAW] = vectoyaw(v);
			self->monsterinfo.walk (self);
			self->target = NULL;
		}
		else
		{
			self->goalentity = self->movetarget = NULL;
			self->monsterinfo.pausetime = 100000000;
			self->monsterinfo.stand (self);
		}
	}
	else
	{
		self->monsterinfo.pausetime = 100000000;
		self->monsterinfo.stand (self);
	}

	self->max_health = self->health; //ADDED

	self->think = monster_think;
	self->nextthink = level.time + FRAMETIME;
}


void walkmonster_start_go (edict_t *self)
{
	if (!(self->spawnflags & 2) && level.time < 1)
	{
		M_droptofloor (self);

		if (self->groundentity)
			if (!M_walkmove (self, 0, 0))
				gi.dprintf ("%s in solid at %s\n", self->classname, vtos(self->s.origin));
	}
	if (!self->yaw_speed)
	{
		if (mt_ldrand() > 0.5)
		{
			self->monsterinfo.aiflags |= AI_IMPATIENT;
		}
		self->yaw_speed = (BASE_YAW_SPEED * (0.75 + (mt_ldrand() * 0.25))) / (1 + VectorLength(self->mins) + VectorLength(self->maxs) + (self->mass * 0.1));
		self->viewheight = 25;
	}
	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}

void walkmonster_start (edict_t *self)
{
	self->think = walkmonster_start_go;
	monster_start (self);
}


void flymonster_start_go (edict_t *self)
{
	if (!M_walkmove (self, 0, 0))
		gi.dprintf ("%s in solid at %s\n", self->classname, vtos(self->s.origin));

	if (!self->yaw_speed)
		self->yaw_speed = 10;
	self->viewheight = 25;

	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}


void flymonster_start (edict_t *self)
{
	self->flags |= FL_FLY;
	self->think = flymonster_start_go;
	monster_start (self);
}


void swimmonster_start_go (edict_t *self)
{
	if (!self->yaw_speed)
		self->yaw_speed = 10;
	self->viewheight = 10;

	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}

void swimmonster_start (edict_t *self)
{
	self->flags |= FL_SWIM;
	self->think = swimmonster_start_go;
	monster_start (self);
}

qboolean M_isrunningbackwards(edict_t *self)
{
	//if(self->enemy)
	//gi.bprintf(PRINT_HIGH, "DEBUG: angle difference to 180 = %f\n", get_angledifference(self, 180));
	if (!M_isrunning(self))
	{
		//if(self->enemy)
			//gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER IS NOT RUNNING!\n");
		return false;
	}
	else if (get_angledifference(self, ANGLE_BACKWARDS) > 0.05)
	{
		//if (self->enemy)
		//	gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER IS RUNNING BACKWARDS!\n");
		return true;
	}
	//if (self->enemy)
		//gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER IS NOT RUNNING BACKWARDS!\n");

return false;
}
qboolean M_isrunning(edict_t *self)
{
	

	if (self->monsterinfo.monster_type == MONSTER_BERSERKER)
	{
	if (inbetw(self->s.frame, 36, 41) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 36, 41))
		return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_JORG)
	{
		if (inbetw(self->s.frame, 163, 187) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 163, 187))
			return true;
	}
	else if(self->monsterinfo.monster_type == MONSTER_RIDER)
	{
		if (inbetw(self->s.frame, 474, 490) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 474, 490))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_BRAIN)
	{
		if (inbetw(self->s.frame, 0, 52) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 0, 52))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_CHICK)
	{
		if (inbetw(self->s.frame, 181, 207) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 181, 207))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_GLADIATOR)
	{
		if (inbetw(self->s.frame, 23, 28) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 23, 28))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_GUNNER)
	{
		if (inbetw(self->s.frame, 94, 107) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 94, 107))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_INFANTRY)
	{
		if (inbetw(self->s.frame, 92, 99) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 92, 99))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_MEDIC)
	{
		if (inbetw(self->s.frame, 102, 107) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 102, 107))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_MUTANT)
	{
		if (inbetw(self->s.frame, 56, 61) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 56, 61))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_PARASITE)
	{
		if (inbetw(self->s.frame, 68, 82) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 68, 82))
			return true;
	}
	else if (self->monsterinfo.monster_type == MONSTER_SOLDIER)
	{
		//if(self->enemy)
		//	gi.bprintf(PRINT_HIGH, "DEBUG: DETECTED MONSTER IS SOLDIER! frame= %i, nframe= %i\n", self->s.frame, self->monsterinfo.nextframe);
		if ((inbetw(self->s.frame, 475, 498) || inbetw(self->s.frame, 97, 108)) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 475, 498) || inbetw(self->monsterinfo.nextframe, 97, 108))
			return true; 
	}
	else if (self->monsterinfo.monster_type == MONSTER_TANK)
	{
		if (inbetw(self->s.frame, 30, 54) && !self->monsterinfo.nextframe || inbetw(self->monsterinfo.nextframe, 30, 54))
			return true;
	}

	
return false;
}
void M_retreat_think(edict_t *self)
{
	//gi.bprintf(PRINT_HIGH, "DEBUG: DISTANCE BETWEEN POINT RETREAT AND THE MONSTER IS %f! REMOVE DISTANCE IS %f!\n", get_dist(self, self->owner), (self->owner->mins[0] + self->owner->mins[1] * 2));
	//if (self->owner && self != self->owner->goalentity )
	//	self->owner->goalentity = self;
	
	if (!self->owner || self->owner->health <= 0 || self->delay < level.time || get_dist(self, self->owner) < (self->owner->maxs[0] + self->owner->maxs[1] * 2))
	{
		if (self->owner)
		{	
			self->owner->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
			if (self->owner->enemy)
				self->owner->goalentity = self->owner->enemy;
			if (rand() % 2 && self->owner->monsterinfo.attack)
				self->owner->monsterinfo.aiflags |= AI_TEMP_STAND_GROUND;
		}
		self->owner->monsterinfo.retreat_point = NULL;
		G_FreeEdict(self);
		return;
	}
	else
		self->nextthink = level.time + FRAMETIME;
	if (random() < 0.04 || visible(self->owner, self->owner->enemy) && infront(self->owner, self->owner->enemy) && visible(self->owner->enemy, self->owner) && infront_aiming(self->owner->enemy, self->owner))
	{
		if (!(self->owner->monsterinfo.aiflags & AI_STAND_GROUND)) // make the monster stop for a second and shoot or something while retreating
		{
			self->owner->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
			self->owner->monsterinfo.aiflags |= AI_STAND_GROUND;
			//self->owner->monsterinfo.attack(self);
			self->owner->goalentity = self->owner->enemy;
			self->nextthink = level.time + (FRAMETIME * 10);
			//gi.bprintf(PRINT_HIGH, "DEBUG: RETREAT, let's stop!\n");
		}
		else
		{
			self->owner->monsterinfo.aiflags &= ~AI_STAND_GROUND;
			self->owner->monsterinfo.aiflags |= AI_COMBAT_POINT;
			self->owner->combattarget = NULL;
			self->owner->goalentity = self;
			self->owner->monsterinfo.run(self->owner);
			//gi.bprintf(PRINT_HIGH, "DEBUG: RETREAT, let's move again!\n");
		}
	}
	//if (!(level.framenum & 5))
	//	spawn_explosion(self->s.origin, TE_ROCKET_EXPLOSION_WATER);
}
void M_retreat_to(edict_t *self, vec3_t origin)
{
	if (self->monsterinfo.retreat_point)
	{
		G_FreeEdict(self->monsterinfo.retreat_point);
	}
	trace_t tr;
	vec3_t start, end, step;
	edict_t *rpoint;
	int stepnum = 0;
	VectorCopy(self->s.origin, start);
	VectorSubtract(origin, self->s.origin, step);
	VectorNormalize(step);

scan:
	if (stepnum > RETREAT_MAX_STEP_NUM) //we don't want infinite loops
		goto skip;
	VectorMA(self->s.origin, RETREAT_SCAN_STEP * stepnum, step, start);
	VectorMA(self->s.origin, RETREAT_SCAN_STEP * (stepnum + 1), step, end);
	tr = gi.trace(start, NULL, NULL, end, self, CONTENTS_SOLID);
	if (tr.fraction < 1)
	{
		VectorCopy(tr.endpos, end);
		end[2] -= 256;
		tr = gi.trace(tr.endpos, NULL, NULL, end, self, CONTENTS_SOLID);
		if (tr.fraction == 1)
		{
			//spawn_explosion(self->s.origin, TE_ROCKET_EXPLOSION_WATER);
			return;
		}
		else
			VectorCopy(start, origin);
	}
	else 
	{
		stepnum++;
		goto scan;
	}
	
	skip:

	
	rpoint = G_Spawn();
	
	rpoint->classname = "retreat_point";
	rpoint->movetype = MOVETYPE_NONE;
	rpoint->think = M_retreat_think;
	rpoint->nextthink = level.time + FRAMETIME;
	rpoint->delay = level.time + MONSTER_RETREAT_TIMEOUT;
	VectorCopy(origin, rpoint->s.origin);
	self->goalentity = rpoint;
	rpoint->owner = self;
	self->monsterinfo.retreat_point = rpoint;

}
void M_retreat(edict_t *self)
{
	
	//TODO check if we are gonna fall from high ground when retreating
	//TODO check if you are gonna retreat down or up stairs
	//TODO why we can't trace with monsters mins maxs?

	//IF WE USE THIS FUNCTION TOO MANY TIMES IT MAKES AN INFINITE LOOP!!!!

	//gi.bprintf(PRINT_HIGH, "DEBUG: BEGINNING OF MONSTER RETREAT\n");
	if (self->monsterinfo.last_retreat + MIN_RETREAT_TIME > level.time);
		return;
	self->monsterinfo.last_retreat = level.time;
	if (!self->enemy)
	{
		//gi.bprintf(PRINT_HIGH, "DEBUG: RETREAT - GOT HERE WITH NO ENEMY!!\n");
		return;
	}
	if (self->max_health > 500 || !self->monsterinfo.attack)
		return;

	if (!self->enemy && self->health > 0 && self->max_health > 0 && self->health < self->max_health)
	{
		//gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER WOULD RETREAT, BUT NO ENEMY AND HEALTH CHECKS\n");
		return;
	}
	float percent_hp = (float)(self->max_health - self->health) / (float)self->max_health;
	float chance = percent_hp + (random() * 0.25);
	//gi.bprintf(PRINT_HIGH, "DEBUG: CHANCE TO RETREAT = %f, maxhp = %i, hp = %i, calc = %f", chance, self->max_health, self->health, percent_hp);
	if ((chance > 0.6 + self->monsterinfo.aggression || percent_hp < 0.1 && self->monsterinfo.aggression < 0.9) && self->monsterinfo.last_retreat < level.time)
	{
		//gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER WOULD RETREAT, BUT CHANCE CHECKS\n");
		return;
	}

	vec3_t point1, point2, point3, dir;
	trace_t tr;
	VectorCopy(self->s.origin, dir);
	dir[2] = self->enemy->s.origin[2];
	VectorSubtract(dir, self->enemy->s.origin, dir);
	VectorNormalize(dir);
	VectorMA(self->s.origin, TRACE_LENGTH, dir, point1);
	//debug_trail(self->s.origin, point1);
	//gi.bprintf(PRINT_HIGH, "DEBUG: start = %s, end = %s\n", vtos(self->s.origin), vtos(point1));

	tr = gi.trace(self->s.origin, NULL, NULL, point1, self, CONTENTS_SOLID);

	//gi.bprintf(PRINT_HIGH, "DEBUG: We hit %s, completion = %f%\n", tr.ent->classname, tr.fraction*100);
	VectorCopy(point1, point2);
	point2[0] += crandom() * 4096;
	point2[1] += crandom() * 4096;
	VectorCopy(point1, point3);
	point3[0] += crandom() * 4096;
	point3[1] += crandom() * 4096;
	VectorCopy(tr.endpos, point1);

	VectorSubtract(self->s.origin, point2, dir);
	VectorNormalize(dir);
	VectorMA(point2, (self->mins[0] + self->mins[1]) * 0.5, dir, point2); //back off a bit

	tr = gi.trace(self->s.origin, NULL, NULL, point2, self, CONTENTS_SOLID);

	//gi.bprintf(PRINT_HIGH, "DEBUG: We hit %s, completion = %f%\n", tr.ent->classname, tr.fraction * 100);
	VectorCopy(tr.endpos, point2);

	VectorSubtract(self->s.origin, point3, dir);
	VectorNormalize(dir);
	VectorMA(point2, (self->mins[0] + self->mins[1]) * 0.5, dir, point3); //back off a bit

	tr = gi.trace(self->s.origin, NULL, NULL, point3, self, CONTENTS_SOLID);

	//gi.bprintf(PRINT_HIGH, "DEBUG: We hit %s, completion = %f%\n", tr.ent->classname, tr.fraction * 100);
	VectorCopy(tr.endpos, point3);
	
	if (get_dist_point(self->s.origin, point1) > get_dist_point(self->s.origin, point2) && get_dist_point(self->s.origin, point1) > get_dist_point(self->s.origin, point3))
	{
		M_retreat_to(self, point1);
	}
	else if (get_dist_point(self->s.origin, point2) > get_dist_point(self->s.origin, point1) && get_dist_point(self->s.origin, point2) > get_dist_point(self->s.origin, point3))
	{
		M_retreat_to(self, point2);
	}
	else if (get_dist_point(self->s.origin, point3) > get_dist_point(self->s.origin, point1) && get_dist_point(self->s.origin, point3) > get_dist_point(self->s.origin, point2))
	{
		M_retreat_to(self, point3);
	}
	else
	{
	//	gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER %s WOULD RETREAT, BUT WE HAVE INVALID POINTS! %s %s %s\n", vtos(self->s.origin), vtos(point1), vtos(point2), vtos(point3));
		

		return;
	}


	gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER IS RETREATING!\n");
	self->monsterinfo.aiflags |= AI_COMBAT_POINT;
	self->monsterinfo.last_retreat = level.time + MONSTER_RETREAT_DEBOUNCE;
}
void M_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (self->svflags & SVF_MONSTER && other->svflags & SVF_MONSTER)
	{
		float dist = get_dist2d(self, other);
		float distv = get_dist_v(self, other);
		//if(distv > 0)
		//	gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER TOUCHED MONSTER AND NOT THE SAME ORIGIN!\n");
		//if (dist > (fabsf(self->mins[0]) + fabsf(self->mins[0]) * 1.42) + (fabsf(other->mins[0]) + fabsf(other->mins[0]) * 1.42))
		//{
			
			if (distv > (fabsf(self->mins[2]) + fabsf(self->maxs[2]) * 0.45) + (fabsf(other->mins[2]) + fabsf(other->maxs[2]) * 0.45))
			{
				//gi.bprintf(PRINT_HIGH, "DEBUG: MONSTER HIGHER THEN OTHER!\n");
				vec3_t dir;

				if (self->s.origin[2] > other->s.origin[2])
				{
					VectorSubtract(self->s.origin, other->s.origin, dir);
					dir[2] = 0;
					VectorNormalize(dir);
					VectorMA(self->velocity, 100, dir, self->velocity);
					self->velocity[2] += 1000;
				}
				else
				{
					VectorSubtract(other->s.origin, self->s.origin, dir);
					dir[2] = 0;
					VectorNormalize(dir);
					VectorMA(other->velocity, 100, dir, other->velocity);
					other->velocity[2] += 1000;
				}
			}
	}
}
void M_calcvel(edict_t *self)
{
	vec3_t vel;
	VectorSubtract(self->s.origin, self->old_origin2, vel);
	VectorScale(vel, 1/FRAMETIME, vel);
	VectorCopy(self->monsterinfo.velocity, self->monsterinfo.oldvelocity);
	VectorCopy(vel, self->monsterinfo.velocity);
	//if (self->enemy)
		//gi.bprintf(PRINT_HIGH, "DEBUG: UPDATING VELOCITY!!! origin %s oldorigin%s\n",vtos(self->s.origin), vtos(self->old_origin2));
}
void M_save_enemy_pos(edict_t *self)
{
	VectorCopy(self->monsterinfo.enemy_pos4, self->monsterinfo.enemy_pos5);
	VectorCopy(self->monsterinfo.enemy_pos3, self->monsterinfo.enemy_pos4);
	VectorCopy(self->monsterinfo.enemy_pos2, self->monsterinfo.enemy_pos3);
	VectorCopy(self->monsterinfo.enemy_pos1, self->monsterinfo.enemy_pos2);
	VectorCopy(self->enemy->s.origin, self->monsterinfo.enemy_pos1);
	self->monsterinfo.last_seen = level.time;
}

void M_calcstrafepos(edict_t *self, vec3_t enemypos)
{
	vec3_t pos1, pos2, pos3, pos4, pos5;
	VectorCopy(self->monsterinfo.enemy_pos5, pos5);
	VectorCopy(self->monsterinfo.enemy_pos4, pos4);
	VectorCopy(self->monsterinfo.enemy_pos3, pos3);
	VectorCopy(self->monsterinfo.enemy_pos2, pos2);
	VectorCopy(self->monsterinfo.enemy_pos1, pos1);
	VectorScale(pos1, 0.2f, pos1);
	VectorScale(pos2, 0.2f, pos2);
	VectorScale(pos3, 0.2f, pos3);
	VectorScale(pos4, 0.2f, pos4);
	VectorScale(pos5, 0.2f, pos5);
	VectorAdd(enemypos, pos1, enemypos);
	VectorAdd(enemypos, pos2, enemypos);
	VectorAdd(enemypos, pos3, enemypos);
	VectorAdd(enemypos, pos4, enemypos);
	VectorAdd(enemypos, pos5, enemypos);
}
void M_save_enemy_vel(edict_t *self)
{
	float diff;
	self->enemy->client->oldforwardmove = self->enemy->client->forwardmove;
	self->enemy->client->oldsidemove = self->enemy->client->sidemove;

	diff = get_angledifference(self->enemy, ANGLE_FORWARD);

	if(diff > 0.25)
		self->enemy->client->forwardmove = 1;
	else if (diff < -0.25)
		self->enemy->client->forwardmove = -1;
	else
		self->enemy->client->forwardmove = 0;
	//gi.bprintf(PRINT_HIGH, "DEBUG: PLAYER CHANGED STRAFE DIRECTION, forward move = %i, forward move = %i, side move = %i, side move = %i, last strafe on = %f, diff =%f\n", self->enemy->client->forwardmove, self->enemy->client->oldforwardmove, self->enemy->client->sidemove, self->enemy->client->oldsidemove, self->monsterinfo.last_enemy_strafe, diff);

	
	diff = get_angledifference(self->enemy, ANGLE_RIGHT);
	if (diff > 0.25)
		self->enemy->client->sidemove = 1;
	else if (diff < -0.25)
		self->enemy->client->sidemove = -1;
	else
		self->enemy->client->sidemove = 0;
}
void M_isplayerstrafing(edict_t *self)
{
	if (!self->enemy->client)
		return;

	
	M_save_enemy_vel(self);
	//if (!self->enemy->client->forwardmove)

	if (!self->enemy->client->forwardmove && (self->enemy->client->sidemove > 0 && self->enemy->client->oldsidemove <= 0 || self->enemy->client->sidemove < 0 && self->enemy->client->oldsidemove >= 0))
	{
		if (self->monsterinfo.last_enemy_strafe > level.time - MONSTER_STRAFE_MAX_TIME_DETECTION)
		{
			self->monsterinfo.aiflags |= AI_PLAYER_STRAFING;
			//gi.bprintf(PRINT_HIGH, "DEBUG: DETECTED THAT ENEMY IS STRAFING!!!\n");
		}
		else
		{
			self->monsterinfo.aiflags &= ~AI_PLAYER_STRAFING;
			//gi.bprintf(PRINT_HIGH, "DEBUG: DETECTED THAT ENEMY IS NOT STRAFING ANYMORE!!!\n");

		}
		self->monsterinfo.last_enemy_strafe = level.time;
	}
	else if (self->monsterinfo.last_enemy_strafe < level.time - MONSTER_STRAFE_MAX_TIME_DETECTION && self->monsterinfo.aiflags & AI_PLAYER_STRAFING)
	{
		self->monsterinfo.aiflags &= ~AI_PLAYER_STRAFING;
		//gi.bprintf(PRINT_HIGH, "DEBUG: DETECTED THAT ENEMY IS NOT STRAFING ANYMORE!!!\n");
	}
}
void M_clear_enemy_pos(edict_t *self)
{
	VectorClear(self->monsterinfo.enemy_pos5);
	VectorClear(self->monsterinfo.enemy_pos4);
	VectorClear(self->monsterinfo.enemy_pos3);
	VectorClear(self->monsterinfo.enemy_pos2);
	VectorClear(self->monsterinfo.enemy_pos1);
}
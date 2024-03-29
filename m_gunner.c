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
/*
==============================================================================

GUNNER

==============================================================================
*/

#include "g_local.h"
#include "m_gunner.h"


static int	sound_pain;
static int	sound_pain2;
static int	sound_death;
static int	sound_idle;
static int	sound_open;
static int	sound_search;
static int	sound_sight;


void gunner_idlesound (edict_t *self)
{
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

void gunner_sight (edict_t *self, edict_t *other)
{
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

void gunner_search (edict_t *self)
{
	gi.sound(self, CHAN_VOICE, sound_search, 1, ATTN_NORM, 0);
}


qboolean visible (edict_t *self, edict_t *other);
void GunnerGrenade (edict_t *self);
void GunnerFire (edict_t *self);
void gunner_fire_chain(edict_t *self);
void gunner_refire_chain(edict_t *self);


void gunner_stand (edict_t *self);

mframe_t gunner_frames_fidget [] =
{
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, gunner_idlesound},
	{ai_stand, 0, NULL},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL}
};
mmove_t	gunner_move_fidget = {FRAME_stand31, FRAME_stand70, gunner_frames_fidget, gunner_stand};

void gunner_fidget (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		return;
	if (random() <= 0.05)
		self->monsterinfo.currentmove = &gunner_move_fidget;
}

mframe_t gunner_frames_stand [] =
{
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, gunner_fidget},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, gunner_fidget},

	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, NULL},
	{ai_stand, 0, gunner_fidget}
};
mmove_t	gunner_move_stand = {FRAME_stand01, FRAME_stand30, gunner_frames_stand, NULL};

void gunner_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &gunner_move_stand;
}


mframe_t gunner_frames_walk [] =
{
	{ai_walk, 0, NULL},
	{ai_walk, 3, NULL},
	{ai_walk, 4, NULL},
	{ai_walk, 5, NULL},
	{ai_walk, 7, NULL},
	{ai_walk, 2, NULL},
	{ai_walk, 6, NULL},
	{ai_walk, 4, NULL},
	{ai_walk, 2, NULL},
	{ai_walk, 7, NULL},
	{ai_walk, 5, NULL},
	{ai_walk, 7, NULL},
	{ai_walk, 4, NULL}
};
mmove_t gunner_move_walk = {FRAME_walk07, FRAME_walk19, gunner_frames_walk, NULL};

void gunner_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_walk;
}

mframe_t gunner_frames_run [] =
{
	{ai_run, 26, NULL},
	{ai_run, 9,  NULL},
	{ai_run, 9,  NULL},
	{ai_run, 9,  NULL},
	{ai_run, 15, NULL},
	{ai_run, 10, NULL},
	{ai_run, 13, NULL},
	{ai_run, 6,  NULL}
};

mmove_t gunner_move_run = {FRAME_run01, FRAME_run08, gunner_frames_run, NULL};

void gunner_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &gunner_move_stand;
	else
		self->monsterinfo.currentmove = &gunner_move_run;
}

mframe_t gunner_frames_runandshoot [] =
{
	{ai_run, 32, NULL},
	{ai_run, 15, NULL},
	{ai_run, 10, NULL},
	{ai_run, 18, NULL},
	{ai_run, 8,  NULL},
	{ai_run, 20, NULL}
};

mmove_t gunner_move_runandshoot = {FRAME_runs01, FRAME_runs06, gunner_frames_runandshoot, NULL};

void gunner_runandshoot (edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_runandshoot;
}

mframe_t gunner_frames_pain3 [] =
{
	{ai_move, -3, faster_pain},
	{ai_move, 1,	 faster_pain},
	{ai_move, 1,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 1,	 faster_pain}
};
mmove_t gunner_move_pain3 = {FRAME_pain301, FRAME_pain305, gunner_frames_pain3, gunner_run};

mframe_t gunner_frames_pain2 [] =
{
	{ai_move, -2, faster_pain},
	{ai_move, 11, faster_pain},
	{ai_move, 6,	faster_pain},
	{ai_move, 2,	 faster_pain},
	{ai_move, -1, faster_pain},
	{ai_move, -7, faster_pain},
	{ai_move, -2, faster_pain},
	{ai_move, -7, faster_pain}
};
mmove_t gunner_move_pain2 = {FRAME_pain201, FRAME_pain208, gunner_frames_pain2, gunner_run};

mframe_t gunner_frames_pain1 [] =
{
	{ai_move, 2,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, -5, faster_pain},
	{ai_move, 3,	 faster_pain},
	{ai_move, -1, faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 1,	 faster_pain},
	{ai_move, 1,	 faster_pain},
	{ai_move, 2,	 faster_pain},
	{ai_move, 1,	 faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, -2, faster_pain},
	{ai_move, -2, faster_pain},
	{ai_move, 0,	 faster_pain},
	{ai_move, 0,	 faster_pain}
};
mmove_t gunner_move_pain1 = {FRAME_pain101, FRAME_pain118, gunner_frames_pain1, gunner_run};

void gunner_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (rand()&1)
		gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 10)
		self->monsterinfo.currentmove = &gunner_move_pain3;
	else if (damage <= 25)
		self->monsterinfo.currentmove = &gunner_move_pain2;
	else
		self->monsterinfo.currentmove = &gunner_move_pain1;
}

void gunner_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -15);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEAD;
	self->nextthink = 0;
	gi.linkentity (self);
}

mframe_t gunner_frames_death [] =
{
	{ai_move, 0,	 NULL},
	{ai_move, 0,	 NULL},
	{ai_move, 0,	 NULL},
	{ai_move, -7, NULL},
	{ai_move, -3, NULL},
	{ai_move, -5, NULL},
	{ai_move, 8,	 NULL},
	{ai_move, 6,	 NULL},
	{ai_move, 0,	 NULL},
	{ai_move, 0,	 NULL},
	{ai_move, 0,	 NULL}
};
mmove_t gunner_move_death = {FRAME_death01, FRAME_death11, gunner_frames_death, gunner_dead};

void gunner_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{

	// check for gib
	if (self->health <= self->gib_health)
	{
		gib_target(self, damage, GIB_MED, point);
		self->deadflag = DEAD_DEAD;
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

	// regular death
	if (!(self->flags & FL_HEADSHOT))
	{
		gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
	}
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &gunner_move_death;
}


void gunner_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	if (skill->value >= 2)
	{
		if (random() > 0.5)
			GunnerGrenade (self);
	}

	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void gunner_duck_hold (edict_t *self)
{
	if (random() > 0.5 && self->groundentity)
	{
		self->monsterinfo.aiflags |= AI_JUMPDODGE;
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	}
	else
	{
		if (level.time >= self->monsterinfo.pausetime)
			self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
		else
			self->monsterinfo.aiflags |= AI_HOLD_FRAME;
	}
}

void gunner_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t gunner_frames_duck [] =
{
	{ai_move, 1,  gunner_duck_down},
	{ai_move, 1,  NULL},
	{ai_move, 1,  gunner_duck_hold},
	{ai_move, 0,  NULL},
	{ai_move, -1, monster_jump},
	{ai_move, -1, NULL},
	{ai_move, 0,  gunner_duck_up},
	{ai_move, -1, NULL}
};
mmove_t	gunner_move_duck = {FRAME_duck01, FRAME_duck08, gunner_frames_duck, gunner_run};

void gunner_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.95)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.currentmove = &gunner_move_duck;
}


void gunner_opengun (edict_t *self)
{
	gi.sound(self, CHAN_VOICE, sound_open, 1, ATTN_IDLE, 0);
}

void GunnerFire (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	target;
	vec3_t	aim;
	int		flash_number;
	if (!M_CheckClearShot(self))
	{
		self->monsterinfo.currentmove = &gunner_move_run;
		self->s.frame = 0;
		self->monsterinfo.nextframe = 0;
		self->monsterinfo.attack_finished = 0;
		return;
	}

	int damage = 3;
	if (skill->value > 3)
		damage *= skill->value;

	flash_number = MZ2_GUNNER_MACHINEGUN_1 + (self->s.frame - FRAME_attak216);

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, monster_flash_offset[flash_number], forward, right, start);

	// project enemy back a bit and target there
	VectorCopy (self->enemy->s.origin, target);
	predict_shot(self, start, 0, target, TYPE_HITSCAN);
	//target[2] += self->enemy->viewheight;

	VectorSubtract (target, start, aim);
	VectorNormalize (aim);
	monster_fire_bullet (self, start, aim, damage, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}

void GunnerGrenade (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	aim, end;
	int		flash_number;
	float difference_z, dist2d;

	vec3_t angles, angles_predicted;
	predict_shot(self, self->s.origin, 800, end, TYPE_GRENADE);
	if (!M_CheckClearShot(self) || ffire_radius_check(self))
	{
		abort:
		self->monsterinfo.currentmove = &gunner_move_run;
		self->s.frame = 0;
		self->monsterinfo.nextframe = 0;
		self->monsterinfo.attack_finished = 0;
		//if(ffire_radius_check(self))
		//	gi.bprintf(PRINT_HIGH, "DEBUG: ABORTING SHOOTING GRENADE, FRIENDLIES NEARBY TARGET!\n");
		//else if (!M_CheckClearShot(self))
			//gi.bprintf(PRINT_HIGH, "DEBUG: ABORTING SHOOTING GRENADE, FRIENDLIES IN THE LINE OF FIRE!\n");
		return;
	}

	int damage = 50;
	if (skill->value > 3)
		damage *= skill->value * 0.5;

	if (self->s.frame == FRAME_attak105)
		flash_number = MZ2_GUNNER_GRENADE_1;
	else if (self->s.frame == FRAME_attak108)
		flash_number = MZ2_GUNNER_GRENADE_2;
	else if (self->s.frame == FRAME_attak111)
		flash_number = MZ2_GUNNER_GRENADE_3;
	else // (self->s.frame == FRAME_attak114)
		flash_number = MZ2_GUNNER_GRENADE_4;
	
	difference_z = diff(self->s.origin[2], end[2]);
	dist2d = get_dist2d_point(self->s.origin, end);
	
	if (end[2] > self->s.origin[2] && dist2d + difference_z > 1024)
		goto abort;
	//gi.bprintf(PRINT_HIGH, "range = %f, difference = %f", range, difference);

	VectorCopy(self->s.angles, angles);
	// below is my not so good take on math ^^ but works ^^
	if (difference_z < 64)
	{
		if (self->s.origin[2] < end[2])
			difference_z *= -0.15f;
		else
			difference_z *= 0.1f;
		angles[0] = angles[0] + difference_z + 10 - (dist2d  / 32);
	}
	else if (self->s.origin[2] < end[2])
	{
		
			
		angles[0] = angles[0] - (difference_z / 4.5) + (dist2d / 64);
		//gi.bprintf(PRINT_HIGH, "DEBUG: SHOOTING UP!");
	}
	else
	{
		
		angles[0] = angles[0] + (difference_z / 2.1)  - (dist2d / 12);
		//gi.bprintf(PRINT_HIGH, "DEBUG: SHOOTING DOWN!");
	}
	
	//gi.bprintf(PRINT_HIGH, "angles = %f %f %f, end = %f %f %f, difference Z = %f, monster origin = %s, dist2d = %f\n", angles[0], angles[1], angles[2], end[0], end[1], end[2], difference_z, vtos(self->s.origin), dist2d);
	//spawn_explosion(end, TE_ROCKET_EXPLOSION_WATER);
	///angles[0] = angles[0] + ((angles[0] * crandom()) * 0.25);
	//angles[1] = angles[1] - 2 + ((angles[0] * crandom()) * 0.25);
	if (angles[0] > 85)
	{
		angles[0] = 85;
		
	}
	if (angles[0] <= -85)
	{
		angles[0] = -85;
		//gi.bprintf(PRINT_HIGH, "DEBUG: TRYING TO ABORT GRENADE, TOO BIG ANGLE!\n");
		goto abort;
	}
	
	//angles[YAW] -= 3;
	AngleVectors(angles, forward, right, NULL);

	G_ProjectSource(self->s.origin, monster_flash_offset[flash_number], forward, right, start);


	VectorSubtract(end, start, end);

	VectorNormalize(end);
	vectoangles(end, angles_predicted);
	angles[YAW] = angles_predicted[YAW];
	AngleVectors(angles, forward, right, NULL);
	G_ProjectSource(self->s.origin, monster_flash_offset[flash_number], forward, right, start);
	VectorCopy(forward, aim);
	
	

	
	//FIXME : do a spread -225 -75 75 225 degrees around forward
	//predict_shot(self, start, 600, forward);

	//forward[0] += crandom() * 8;
	//forward[1] += crandom() * 8; //some spread, lazy ass way, for now
	//forward[2] += crandom() * 8;

	//gi.bprintf(PRINT_HIGH, "DEBUG: GUNNER FIRING GRENADE, origin = %s, start = %s, aim pos = %s\n", vtos(self->s.origin), vtos(start), vtos(forward));

	//VectorSubtract(forward, start, aim);
	//VectorNormalize(aim);
	//VectorCopy (forward, aim);

	monster_fire_grenade (self, start, aim, damage, 800, flash_number);
}

mframe_t gunner_frames_attack_chain [] =
{
	/*
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	*/
	{ai_charge, 0, gunner_opengun},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, NULL}
};
mmove_t gunner_move_attack_chain = {FRAME_attak209, FRAME_attak215, gunner_frames_attack_chain, gunner_fire_chain};

mframe_t gunner_frames_fire_chain [] =
{
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire},
	{ai_charge,   0, GunnerFire}
};
mmove_t gunner_move_fire_chain = {FRAME_attak216, FRAME_attak223, gunner_frames_fire_chain, gunner_refire_chain};

mframe_t gunner_frames_endfire_chain [] =
{
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame}
};
mmove_t gunner_move_endfire_chain = {FRAME_attak224, FRAME_attak230, gunner_frames_endfire_chain, gunner_run};

mframe_t gunner_frames_attack_grenade [] =
{
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, GunnerGrenade},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, GunnerGrenade},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, GunnerGrenade},
	{ai_charge, 0, NULL},
	{ai_charge, 0, NULL},
	{ai_charge, 0, GunnerGrenade},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame},
	{ai_charge, 0, monster_skip_frame}
};
mmove_t gunner_move_attack_grenade = {FRAME_attak101, FRAME_attak121, gunner_frames_attack_grenade, gunner_run};

void gunner_attack(edict_t *self)
{


	if (range (self, self->enemy) == RANGE_MELEE)
	{
		self->monsterinfo.currentmove = &gunner_move_attack_chain;
	}
	else
	{
		if (random() <= 0.5 && !ffire_radius_check(self))
			self->monsterinfo.currentmove = &gunner_move_attack_grenade;
		else
			self->monsterinfo.currentmove = &gunner_move_attack_chain;
	}
}

void gunner_fire_chain(edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_fire_chain;
}

void gunner_refire_chain(edict_t *self)
{
	if (self->enemy->health > 0)
		if ( visible (self, self->enemy) )
			if (random() <= 0.9)
			{
				if (!M_CheckClearShot(self))
				{
					self->monsterinfo.currentmove = &gunner_move_run;
					self->s.frame = 0;
					self->monsterinfo.nextframe = 0;
					self->monsterinfo.attack_finished = 0;
					return;
				}
				self->monsterinfo.currentmove = &gunner_move_fire_chain;
				return;
			}
	self->monsterinfo.currentmove = &gunner_move_endfire_chain;
}

/*QUAKED monster_gunner (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_gunner (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	sound_death = gi.soundindex ("gunner/death1.wav");	
	sound_pain = gi.soundindex ("gunner/gunpain2.wav");	
	sound_pain2 = gi.soundindex ("gunner/gunpain1.wav");	
	sound_idle = gi.soundindex ("gunner/gunidle1.wav");	
	sound_open = gi.soundindex ("gunner/gunatck1.wav");	
	sound_search = gi.soundindex ("gunner/gunsrch1.wav");	
	sound_sight = gi.soundindex ("gunner/sight1.wav");	

	gi.soundindex ("gunner/gunatck2.wav");
	gi.soundindex ("gunner/gunatck3.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/gunner/tris.md2");
	VectorSet (self->mins, -20, -20, -24); //was 16
	VectorSet (self->maxs, 20, 20, 32);

	self->health = 175;
	self->gib_health = -70;
	self->mass = 200;

	self->pain = gunner_pain;
	self->die = gunner_die;

	self->monsterinfo.stand = gunner_stand;
	self->monsterinfo.walk = gunner_walk;
	self->monsterinfo.run = gunner_run;
	self->monsterinfo.dodge = gunner_dodge;
	self->monsterinfo.attack = gunner_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = gunner_sight;
	self->monsterinfo.search = gunner_search;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &gunner_move_stand;	
	self->monsterinfo.scale = MODEL_SCALE;
	self->monsterinfo.monster_type = MONSTER_GUNNER;
	walkmonster_start (self);
}

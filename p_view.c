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
#include "m_player.h"



static	edict_t		*current_player;
static	gclient_t	*current_client;

static	vec3_t	forward, right, up;
float	xyspeed;

float	bobmove;
int		bobcycle;		// odd cycles are right foot going forward
float	bobfracsin;		// sin(bobfrac*M_PI)

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll(vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;

	side = DotProduct(velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabsf(side);

	value = sv_rollangle->value;

	if (side < sv_rollspeed->value)
		side = side * value / sv_rollspeed->value;
	else
		side = value;

	return side * sign;

}


/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback(edict_t *player)
{
	gclient_t	*client;
	float	side;
	float	realcount, count, kick;
	vec3_t	v = { 0 };
	int		r, l;
	static	vec3_t	power_color = { 0.0, 1.0, 0.0 };
	static	vec3_t	acolor = { 1.0, 1.0, 1.0 };
	static	vec3_t	bcolor = { 1.0, 0.0, 0.0 };

	client = player->client;

	// flash the backgrounds behind the status numbers
	client->ps.stats[STAT_FLASHES] = 0;
	if (client->damage_blood)
		client->ps.stats[STAT_FLASHES] |= 1;
	if (client->damage_armor && !(player->flags & FL_GODMODE) && (client->invincible_framenum <= level.framenum))
		client->ps.stats[STAT_FLASHES] |= 2;

	// total points of damage shot at the player this frame
	count = (client->damage_blood + client->damage_armor + client->damage_parmor);
	if (count == 0)
		return;		// didn't take any damage

	// start a pain animation if still in the player model
	if (client->anim_priority == ANIM_BASIC && VectorLength(player->velocity) > 100 && random() > 0.8 && client->damage_blood < 11)
	{
		// this is blocking pain animation when running??? :D shit, can't remember, but it looks like it
	}
	else if (client->anim_priority < ANIM_PAIN && player->s.modelindex == 255)
	{
		static int		i;

		client->anim_priority = ANIM_PAIN;
		if (client->ps.pmove.pm_flags & PMF_DUCKED)
		{
			player->s.frame = FRAME_crpain1 - 1;
			client->anim_end = FRAME_crpain4;
		}
		else
		{
			i = (i + 1) % 3;
			switch (i)
			{
			case 0:
				player->s.frame = FRAME_pain101 - 1;
				client->anim_end = FRAME_pain104;
				break;
			case 1:
				player->s.frame = FRAME_pain201 - 1;
				client->anim_end = FRAME_pain204;
				break;
			case 2:
				player->s.frame = FRAME_pain301 - 1;
				client->anim_end = FRAME_pain304;
				break;
			}
		}
	}

	realcount = count;
	if (count < 10)
		count = 10;	// always make a visible effect

	// play an apropriate pain sound
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && (client->invincible_framenum <= level.framenum))
	{
		r = 1 + (rand() & 1);
		player->pain_debounce_time = level.time + 0.7;
		if (player->health < 25)
			l = 25;
		else if (player->health < 50)
			l = 50;
		else if (player->health < 75)
			l = 75;
		else
			l = 100;
		gi.sound(player, CHAN_VOICE, gi.soundindex(va("*pain%i_%i.wav", l, r)), 1, ATTN_NORM, 0);
	}

	// the total alpha of the blend is always proportional to count
	if (client->damage_alpha < 0)
		client->damage_alpha = 0;
	client->damage_alpha += count * 0.01;
	if (client->damage_alpha < 0.2)
		client->damage_alpha = 0.2f;
	if (client->damage_alpha > 0.6)
		client->damage_alpha = 0.6f;		// don't go too saturated

	// the color of the blend will vary based on how much was absorbed
	// by different armors
	VectorClear(v);
	if (client->damage_parmor)
		VectorMA(v, (float)client->damage_parmor / realcount, power_color, v);
	if (client->damage_armor)
		VectorMA(v, (float)client->damage_armor / realcount, acolor, v);
	if (client->damage_blood)
		VectorMA(v, (float)client->damage_blood / realcount, bcolor, v);
	VectorCopy(v, client->damage_blend);


	//
	// calculate view angle kicks
	//
	kick = abs(client->damage_knockback);
	if (kick && player->health > 0)	// kick of 0 means no view adjust at all
	{
		kick = kick * 100 / player->health;

		if (kick < count*0.5)
			kick = count * 0.5;
		if (kick > 25)
			kick = 25;

		VectorSubtract(client->damage_from, player->s.origin, v);
		VectorNormalize(v);

		/*side = DotProduct (v, right);
		client->v_dmg_roll = kick * side * 0.3 * ((skill->value / 2) + 1) * (random() + 1);
		client->v_dmg_yaw = kick * side * 0.3 * ((skill->value / 2) + 1) * (random() + 1);

		side = -DotProduct (v, forward);
		client->v_dmg_pitch = kick * side * 0.3 * ((skill->value / 2) + 1) * (random() + 1);
		*/
		side = DotProduct(v, right);
		vec3_t kick_v = { 0 };
		kick_v[ROLL] += kick * side * 0.3f * ((skill->value / 2) + 1) * ((random() / 2) + 1);
		kick_v[YAW] += kick * side * 0.3f * ((skill->value / 2) + 1) * ((random() / 2) + 1);

		side = -DotProduct(v, forward);
		kick_v[PITCH] += kick * side * 0.3f * ((skill->value / 2) + 1) * ((random() / 2) + 1);

		if (VectorLength(kick_v) > 5)
			VectorScale(kick_v, 0.75, kick_v);
		if (VectorLength(kick_v) > 10)
			VectorScale(kick_v, 0.75, kick_v);
		if (VectorLength(kick_v) > 15)
			VectorScale(kick_v, 0.75, kick_v);
		if (VectorLength(kick_v) > 20)
			VectorScale(kick_v, 0.75, kick_v);

		VectorAdd(kick_v, client->v_dmg_smooth, client->v_dmg_smooth);
		client->v_dmg_time = level.time + DAMAGE_TIME + (kick / 50);
	}

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_parmor = 0;
	client->damage_knockback = 0;
}




/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset(edict_t *ent)
{
	float		*angles;
	float		bob;
	float		ratio;
	float		delta;
	vec3_t		v = { 0 };


	//===================================

		// base angles
	angles = ent->client->ps.kick_angles;

	// if dead, fix the angle and don't add any kick
	if (ent->deadflag)
	{
		VectorClear(angles);

		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = ent->client->killer_yaw;
	}
	else
	{
		// add angles based on weapon kick

		VectorCopy(ent->client->kick_angles, angles);

		// add angles based on damage kick

		ratio = (ent->client->v_dmg_time - level.time) / DAMAGE_TIME;
		if (ratio < 0)
		{
			ratio = 0;
			ent->client->v_dmg_pitch = 0;
			ent->client->v_dmg_roll = 0;
			ent->client->v_dmg_yaw = 0;
		}
		//gi.bprintf(PRINT_HIGH, "angles = %s, v_dmg_pry %f %f %f, v_dmg_smooth = %s\n", vtos(angles), ent->client->v_dmg_pitch, ent->client->v_dmg_yaw, ent->client->v_dmg_roll, vtos(ent->client->v_dmg_smooth));
		ent->client->v_dmg_pitch += ent->client->v_dmg_smooth[0] * 0.75;
		ent->client->v_dmg_yaw += ent->client->v_dmg_smooth[1] * 0.75;
		ent->client->v_dmg_roll += ent->client->v_dmg_smooth[2] * 0.75;

		VectorScale(ent->client->v_dmg_smooth, 0.75, ent->client->v_dmg_smooth);

		if (fabsf(ent->client->v_dmg_pitch) > 20)
			ent->client->v_dmg_pitch *= 0.75;
		if (fabsf(ent->client->v_dmg_yaw) > 20)
			ent->client->v_dmg_yaw *= 0.75;
		if (fabsf(ent->client->v_dmg_roll) > 20)
			ent->client->v_dmg_roll *= 0.75;

		angles[PITCH] += ent->client->v_dmg_pitch;
		angles[ROLL] += ent->client->v_dmg_roll;
		angles[YAW] += ent->client->v_dmg_yaw;

		// add pitch based on fall kick
		ent->client->v_dmg_pitch *= 0.5;
		ent->client->v_dmg_roll *= 0.5;
		ent->client->v_dmg_yaw *= 0.5;
		ratio = (ent->client->fall_time - level.time) / FALL_TIME;
		if (ratio < 0)
			ratio = 0;
		angles[PITCH] += ratio * ent->client->fall_value;

		// add angles based on velocity

		delta = DotProduct(ent->velocity, forward);
		angles[PITCH] += delta * run_pitch->value;

		delta = DotProduct(ent->velocity, right);
		angles[ROLL] += delta * run_roll->value;

		// add angles based on bob

		delta = bobfracsin * bob_pitch->value * xyspeed;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		angles[PITCH] += delta;
		delta = bobfracsin * bob_roll->value * xyspeed;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		if (bobcycle & 1)
			delta = -delta;
		angles[ROLL] += delta;
	}

	//===================================

		// base origin

	VectorClear(v);

	// add view height

	v[2] += ent->viewheight;

	// add fall height

	ratio = (ent->client->fall_time - level.time) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	v[2] -= ratio * ent->client->fall_value * 0.4f;

	// add bob height

	bob = bobfracsin * xyspeed * bob_up->value;
	if (bob > 6)
		bob = 6;
	//gi.DebugGraph (bob *2, 255);
	v[2] += bob;

	// add kick offset

	VectorAdd(v, ent->client->kick_origin, v);

	// absolutely bound offsets
	// so the view can never be outside the player box

	if (v[0] < -14)
		v[0] = -14;
	else if (v[0] > 14)
		v[0] = 14;
	if (v[1] < -14)
		v[1] = -14;
	else if (v[1] > 14)
		v[1] = 14;
	if (v[2] < -22)
		v[2] = -22;
	else if (v[2] > 30)
		v[2] = 30;

	VectorCopy(v, ent->client->ps.viewoffset);
}

/*
==============
SV_CalcGunOffset
==============
*/
void SV_CalcGunOffset(edict_t *ent)
{
	int		i;
	float	delta;

	// gun angles from bobbing
	ent->client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005;
	ent->client->ps.gunangles[YAW] = xyspeed * bobfracsin * 0.01;
	if (bobcycle & 1)
	{
		ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
		ent->client->ps.gunangles[YAW] = -ent->client->ps.gunangles[YAW];
	}

	ent->client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005;

	// gun angles from delta movement
	for (i = 0; i < 3; i++)
	{
		delta = ent->client->oldviewangles[i] - ent->client->ps.viewangles[i];
		if (delta > 180)
			delta -= 360;
		if (delta < -180)
			delta += 360;
		if (delta > 45)
			delta = 45;
		if (delta < -45)
			delta = -45;
		if (i == YAW)
			ent->client->ps.gunangles[ROLL] += 0.1*delta;
		ent->client->ps.gunangles[i] += 0.2 * delta;
	}

	// gun height
	VectorClear(ent->client->ps.gunoffset);
	//	ent->ps->gunorigin[2] += bob;
	ent->client->ps.gunangles[PITCH] += ent->client->v_dmg_pitch * (-1);
	ent->client->ps.gunangles[ROLL] += ent->client->v_dmg_roll / 2;
	ent->client->ps.gunangles[YAW] += ent->client->v_dmg_yaw * (-1);
	// gun_x / gun_y / gun_z are development tools
	for (i = 0; i < 3; i++)
	{
		if (ent->client->v_dmg_pitch > 0)
			ent->client->ps.gunoffset[i] += forward[i] * (gun_y->value - (ent->client->v_dmg_pitch / 10));
		else
			ent->client->ps.gunoffset[i] += forward[i] * (gun_y->value);
		ent->client->ps.gunoffset[i] += right[i] * gun_x->value;

		if (ent->client->v_dmg_pitch > 0)
			ent->client->ps.gunoffset[i] += up[i] * (-gun_z->value - (ent->client->v_dmg_pitch / 10));
		else
			ent->client->ps.gunoffset[i] += up[i] * (-gun_z->value);
	}
}


/*
=============
SV_AddBlend
=============
*/
void SV_AddBlend(float r, float g, float b, float a, float *v_blend)
{
	float	a2, a3;

	if (a <= 0)
		return;
//	a2 = 0.1		+		(0.9)*0.25
	// a2 = 0.325
	a2 = v_blend[3] + (1 - v_blend[3])*a;	// new total alpha
	a3 = v_blend[3] / a2;		// fraction of color from old
	//a3 = 0.1625
	v_blend[0] = v_blend[0] * a3 + r * (1 - a3); // 1 * 0.1625 + 1 * ( 0.8375)
	v_blend[1] = v_blend[1] * a3 + g * (1 - a3);
	v_blend[2] = v_blend[2] * a3 + b * (1 - a3);
	v_blend[3] = a2;
}


/*
=============
SV_CalcBlend
=============
*/
void SV_CheckSun(edict_t *ent)
{
	vec3_t angles = { 0 };
	vec3_t offset = { 0 };
	vec3_t start, forward, right;
	vec3_t origin = { 0 };
	trace_t tr;

	if (level.sun == NULL)
		return;

	VectorCopy(ent->client->v_angle, angles);
	AngleVectors(angles, forward, right, NULL);
	VectorSet(offset, 0, 0, ent->viewheight);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
	tr = gi.trace(start, NULL, NULL, level.sun->s.origin, ent, MASK_SHOT);

	if (tr.surface && (tr.surface->flags & SURF_SKY) && ent->s.angles[1] <= -108 + 69 && ent->s.angles[1] >= -108 - 69 && ent->s.angles[0] <= -7 + 45 && ent->s.angles[0] >= -7 - 45)
	{
		SV_AddBlend(1.0, 1.0, 0.5, clamp((0.345 * sunblend->value) - (diff(ent->s.angles[1], -108) * 0.005 * sunblend->value) - (diff(ent->s.angles[0], -7) * 0.00766 * sunblend->value), 1, 0), ent->client->ps.blend);

	}

	VectorMA(start, 4096.0, forward, origin);
	//gi.bprintf(PRINT_HIGH, "DEBUG: origin = %s, sun origin = %s diff = %f\n", vtos(origin), vtos(level.sun->s.origin), diff(ent->s.angles[1], -108));

	vec3_t	vec = { 0 };
	float	dot;
	int inview = 0;
	AngleVectors(ent->s.angles, forward, NULL, NULL);
	VectorSubtract(level.sun->s.origin, ent->s.origin, vec);
	VectorNormalize(vec);

	dot = DotProduct(vec, forward);
	if (dot >= cos(10 * (M_PI / 360)))
		inview = 1;

	//if (dot > 0.3)
		
}
void SV_CheckSky(edict_t *ent)
{
	int check_num = 0;
	vec3_t offset[16] = { 0 };
	vec3_t offset_sky = { 0 };
	int initialize = 0;

	initialize_offset:
	if (!initialize)
	{
		VectorClear(offset[check_num]);
		check_num++;
		if (check_num >= 16)
		{
			initialize = 1;
			
			
		}
		goto initialize_offset;
	}
	else
		check_num = 0;

	offset[0][0] = 64;
	offset[0][1] = 64;


	offset[1][0] = -32;
	offset[2][0] = -32;
	offset[3][0] = -32;
	offset[4][0] = -32;


	offset[5][1] = -32;
	offset[6][1] = -32;
	offset[7][1] = -32;
	offset[8][1] = -32;

	offset[9][0] = 32;
	offset[10][0] = 32;
	offset[11][0] = 32;
	offset[12][0] = 32;
	offset[13][1] = 32;
	offset[14][1] = 32;


	offset[15][1] = 32;


	VectorClear(offset_sky);
	offset_sky[2] = 4096;
	vec3_t origin = { 0 };
	vec3_t origin_sky = { 0 };
	VectorCopy(ent->s.origin, origin);

	SV_CheckSun(ent);
	check:
	VectorAdd(origin, offset[check_num], origin);
	VectorClear(origin_sky);
	VectorAdd(origin, offset_sky, origin_sky);
	//gi.bprintf(PRINT_HIGH, "DEBUG: origin = %s origin_sky = %s check_num = %i skyblend = %f\n", vtos(origin), vtos(origin_sky), check_num, ent->skyblend);
	//gi.bprintf(PRINT_HIGH, "DEBUG: angles = %s\n", vtos(ent->s.angles));

	
	trace_t tr = gi.trace(origin, NULL, NULL, origin_sky, ent, MASK_SHOT);
	//debug_trail(origin, origin_sky);
	if (tr.surface && (tr.surface->flags & SURF_SKY))
		goto skyisnear;
	else if(check_num < 15)
	{
		check_num++;
		goto check;
	}

	if(ent->skyblend > skyblend_minimum->value)
		ent->skyblend -= skyblend_speed->value;
	SV_AddBlend(1.0, 0.2f, 0.0, ent->skyblend, ent->client->ps.blend);
	return;
skyisnear:
	if (ent->skyblend < skyblend_maximum->value)
		ent->skyblend += skyblend_speed->value;
	SV_AddBlend(1.0, 0.2f, 0.0, ent->skyblend, ent->client->ps.blend);

}
void SV_CalcBlend(edict_t *ent)
{
	int		contents;
	vec3_t	vieworg = { 0 };
	int		remaining;
	//ent->client->pers.inventory[ITEM_INDEX(FindItem("item_quad"))] = 0;

	ent->client->ps.blend[0] = ent->client->ps.blend[1] =
		ent->client->ps.blend[2] = ent->client->ps.blend[3] = 0;

	// add for contents
	VectorAdd(ent->s.origin, ent->client->ps.viewoffset, vieworg);
	contents = gi.pointcontents(vieworg);
	if (contents & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER))
		ent->client->ps.rdflags |= RDF_UNDERWATER;
	else
		ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	if (contents & (CONTENTS_SOLID | CONTENTS_LAVA))
		SV_AddBlend(1.0, 0.3f, 0.0, 0.6f, ent->client->ps.blend);
	else if (contents & CONTENTS_SLIME)
		SV_AddBlend(0.0, 0.1f, 0.05f, 0.6f, ent->client->ps.blend);
	else if (contents & CONTENTS_WATER)
		SV_AddBlend(0.5, 0.3f, 0.2f, 0.4f, ent->client->ps.blend);
	SV_CheckSky(ent);


	if (item_mod->value)
	{

		if (ent->client->pers.items_activated & FL_QUAD)
		{
			SV_AddBlend(0, 0, 1, 0.08f, ent->client->ps.blend);

			if (((level.framenum) % 10) == 0)
			{
				if (ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))])
				{
					add_sp_score(current_player, 2, SCORE_ITEM_USAGE);
					ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))]--;

				}
				else
				{
					ent->client->pers.items_activated &= ~FL_QUAD;
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_IDLE, 0);
				}

			}
			if (ent->client->pers.quad_health <= 0)
			{
				ent->client->pers.items_activated &= ~FL_QUAD;
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_IDLE, 0);
				ent->client->pers.inventory[ITEM_INDEX(FindItem("Quad Damage"))]--;
			}
		}

		if (ent->client->pers.items_activated & FL_INV)
		{
			SV_AddBlend(1, 1, 0, 0.08f, ent->client->ps.blend);
			if (((level.framenum) % 10) == 0)
			{
				if (ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))])
				{
					add_sp_score(current_player, 2, SCORE_ITEM_USAGE);
					ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))]--;
				}
				else
				{
					ent->client->pers.items_activated &= ~FL_INV;
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_IDLE, 0);
				}
			}
			if (ent->client->pers.invu_health <= 0)
			{
				ent->client->pers.items_activated &= ~FL_INV;
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_IDLE, 0);
				ent->client->pers.inventory[ITEM_INDEX(FindItem("Invulnerability"))]--;
			}

		}

		if (ent->client->pers.items_activated & FL_ENV)
		{
			SV_AddBlend(0, 1, 0, 0.08f, ent->client->ps.blend);

		}
		if (ent->client->pers.items_activated & FL_REB)
		{
			SV_AddBlend(0.4f, 1, 0.4f, 0.04f, ent->client->ps.blend);

		}

	}
	else
	{

		// add for powerups
		if (ent->client->quad_framenum > level.framenum)
		{
			remaining = ent->client->quad_framenum - level.framenum;
			if (remaining == 30)	// beginning to fade
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_IDLE, 0);
			if (remaining > 30 || (remaining & 4))
				SV_AddBlend(0, 0, 1, 0.08f, ent->client->ps.blend);
		}
		else if (ent->client->invincible_framenum > level.framenum)
		{
			remaining = ent->client->invincible_framenum - level.framenum;
			if (remaining == 30)	// beginning to fade
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_IDLE, 0);
			if (remaining > 30 || (remaining & 4))
				SV_AddBlend(1, 1, 0, 0.08f, ent->client->ps.blend);
		}
		else if (ent->client->enviro_framenum > level.framenum)
		{
			remaining = ent->client->enviro_framenum - level.framenum;
			if (remaining == 30)	// beginning to fade
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_IDLE, 0);
			if (remaining > 30 || (remaining & 4))
				SV_AddBlend(0, 1, 0, 0.08f, ent->client->ps.blend);
		}
		else if (ent->client->breather_framenum > level.framenum)
		{
			remaining = ent->client->breather_framenum - level.framenum;
			if (remaining == 30)	// beginning to fade
				gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_IDLE, 0);
			if (remaining > 30 || (remaining & 4))
				SV_AddBlend(0.4f, 1, 0.4f, 0.04f, ent->client->ps.blend);
		}

	}
	// add for damage
	if (ent->client->damage_alpha > 0)
		SV_AddBlend(ent->client->damage_blend[0], ent->client->damage_blend[1]
			, ent->client->damage_blend[2], ent->client->damage_alpha, ent->client->ps.blend);

	if (ent->client->bonus_alpha > 0)
		SV_AddBlend(0.85f, 0.7f, 0.3f, ent->client->bonus_alpha, ent->client->ps.blend);

	// drop the damage value
	ent->client->damage_alpha -= 0.06f;
	if (ent->client->damage_alpha < 0)
		ent->client->damage_alpha = 0;

	// drop the bonus value
	ent->client->bonus_alpha -= 0.1f;
	if (ent->client->bonus_alpha < 0)
		ent->client->bonus_alpha = 0;
	if (current_client->pers.items_activated & FL_GENERATOR)
	{
		if ((level.framenum % 10) == 0)
		{
			if(current_player->health > 0)
				PlayerNoise(current_player, ent->s.origin, PNOISE_SELF);
			gi.sound(current_player, CHAN_ITEM, gi.soundindex("items/gen.wav"), 0.25, ATTN_STATIC, 0);

		}
		if ((level.framenum % 100) == 0 && current_client->pers.inventory[ITEM_INDEX(FindItem("cells"))] < current_client->pers.max_cells)
		{
			current_client->pers.inventory[ITEM_INDEX(FindItem("cells"))]++;
			add_sp_score(current_player, 15, SCORE_ITEM_USAGE);

		}
	}
	if (item_mod)
	{
		if (current_client->pers.items_activated & FL_ACTIVATING_R || current_client->pers.items_activated & FL_DEACTIVATING_R)
		{
			if (current_player->delay + 0.1 > level.time && current_player->delay - 1.1 <= level.time)
			{
				ent->client->ps.blend[0] = 0;
				ent->client->ps.blend[1] = 0;
				ent->client->ps.blend[2] = 0;
				ent->client->ps.blend[3] = 1;


			}
			else if (current_player->delay < level.time)
			{
				ent->client->ps.gunframe = 0;
				ent->client->weaponstate = WEAPON_ACTIVATING;
				if (current_client->pers.items_activated & FL_REB)
				{
					current_client->pers.items_activated &= ~FL_REB;
					current_client->pers.items_activated &= ~FL_DEACTIVATING_R;
				}
				else
				{
					current_client->pers.items_activated |= FL_REB;
					current_client->pers.items_activated &= ~FL_ACTIVATING_R;
				}
			}
		}
	
		if (current_client->pers.items_activated & FL_ACTIVATING_E || current_client->pers.items_activated & FL_DEACTIVATING_E)
		{
			if (current_player->delay > level.time && current_player->delay - 1.0 <= level.time)
			{
				ent->client->ps.blend[0] = 0;
				ent->client->ps.blend[1] = 0;
				ent->client->ps.blend[2] = 0;
				ent->client->ps.blend[3] = 1;

			}
			if (current_player->delay > level.time && current_player->delay - 1.0 <= level.time && current_player->delay - 1.1 > level.time)
			{
				if(current_client->pers.items_activated & FL_REB)
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/reb_off.wav"), 0.5, ATTN_STATIC, 0);
				else
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/reb_on.wav"), 0.5, ATTN_STATIC, 0);

			}
			else if (current_player->delay < level.time)
			{
				ent->client->ps.gunframe = 0;
				ent->client->weaponstate = WEAPON_ACTIVATING;
				if (current_client->pers.items_activated & FL_ENV)
				{
					current_client->pers.items_activated &= ~FL_ENV;
					current_client->pers.items_activated &= ~FL_DEACTIVATING_E;
				}
				else
				{
					current_client->pers.items_activated |= FL_ENV;
					current_client->pers.items_activated &= ~FL_ACTIVATING_E;
				}
			}
		}
	}

}


/*
=================
P_FallingDamage
=================
*/
void P_FallingDamage(edict_t *ent)
{
	float	delta;
	int		damage;
	vec3_t	dir = { 0 };

	if (ent->s.modelindex != 255)
		return;		// not in the player model

	if (ent->movetype == MOVETYPE_NOCLIP)
		return;

	if ((ent->client->oldvelocity[2] < 0) && (ent->velocity[2] > ent->client->oldvelocity[2]) && (!ent->groundentity) && !ent->client->ctf_grapplestate)
	{
		delta = ent->client->oldvelocity[2];
	}
	else
	{
		if (!ent->groundentity)
			return;
		delta = ent->velocity[2] - ent->client->oldvelocity[2];
	}
	delta = delta * delta * 0.0001;

	// never take falling damage if completely underwater
	if (ent->waterlevel == 3)
		return;
	if (ent->waterlevel == 2)
		delta *= 0.25;
	if (ent->waterlevel == 1)
		delta *= 0.5;

	if (delta < 1)
		return;

	if (delta < 3)
	{
		ent->s.event = EV_FOOTSTEP;
		return;
	}

	ent->client->fall_value = delta * 0.5;
	if (ent->client->fall_value > 40)
		ent->client->fall_value = 40;
	ent->client->fall_time = level.time + FALL_TIME;
	if (delta > 30)//				25														//7
		delta = clamp(delta + (25 - ((ent->client->pers.jump_charge / jump_max_charge->value) * 25)), 999, 30);
	if (delta > 30)
	{
		if (ent->health > 0)
		{
			if (delta >= 55)
				ent->s.event = EV_FALLFAR;
			else
				ent->s.event = EV_FALL;
		}
		ent->pain_debounce_time = level.time;	// no normal pain sound
		damage = (delta - 30) / 2;
		if (damage < 1)
			damage = 1;
		VectorSet(dir, 0, 0, 1);

		if (!deathmatch->value || !((int)dmflags->value & DF_NO_FALLING))
			T_Damage(ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, 0, MOD_FALLING);
	}
	else
	{
		ent->s.event = EV_FALLSHORT;

	}

	float damping = clamp(delta - 5, 999, 0);
	damping = clamp(damping / 25, 1, 0);
	ent->client->pers.jump_charge = clamp(ent->client->pers.jump_charge - (damping * jump_max_charge->value), jump_max_charge->value, 0);
	damping = clamp(damping, 1, JUMP_DAMP_MIN_VOL);
	damping *= 0.5;
	if (damping > JUMP_DAMP_MIN_VOL)
		gi.sound(ent, CHAN_AUTO, gi.soundindex("player/jump_a.wav"), damping, ATTN_IDLE, 0);
}



/*
=============
P_WorldEffects
=============
*/
void P_WorldEffects(void)
{
	qboolean	breather;
	qboolean	envirosuit;
	int			waterlevel, old_waterlevel;

	if (current_player->movetype == MOVETYPE_NOCLIP)
	{
		current_player->air_finished = level.time + 12;	// don't need air
		return;
	}

	waterlevel = current_player->waterlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = waterlevel;

	breather = current_client->breather_framenum > level.framenum;
	envirosuit = current_client->enviro_framenum > level.framenum;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		if (current_player->watertype & CONTENTS_LAVA)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/lava_in.wav"), 1, ATTN_IDLE, 0);
		else if (current_player->watertype & CONTENTS_SLIME)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_IDLE, 0);
		else if (current_player->watertype & CONTENTS_WATER)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_IDLE, 0);
		current_player->flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player->damage_debounce_time = level.time - 1;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && !waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_IDLE, 0);
		current_player->flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != 3 && waterlevel == 3)
	{
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_un.wav"), 1, ATTN_IDLE, 0);
	}

	//
	// check for head just coming out of water
	//
	if (old_waterlevel == 3 && waterlevel != 3)
	{
		if (current_player->air_finished < level.time)
		{	// gasp for air
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp1.wav"), 1, ATTN_IDLE, 0);
			PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		}
		else  if (current_player->air_finished < level.time + 11)
		{	// just break surface
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp2.wav"), 1, ATTN_IDLE, 0);
		}
	}

	//
	// check for drowning
	//
	if (waterlevel == 3)
	{
		if (item_mod->value)
		{
			if (current_client->pers.items_activated & FL_ENV || current_client->pers.items_activated & FL_REB)
			{
				if (current_client->pers.inventory[ITEM_INDEX(FindItem("cells"))])
				{
					if(!(current_client->pers.items_activated & FL_UNDERWATER))
					current_player->air_finished = level.time + 10;

					if (((level.framenum) % 25) == 0)
					{
						current_client->pers.inventory[ITEM_INDEX(FindItem("cells"))]--;

						if(current_client->pers.items_activated & FL_REB)
							current_client->pers.rebr_health -= 5;
						else
							current_client->pers.envi_health -= 5;
						if (!current_client->breather_sound)
							gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_STATIC, 0);
						else
							gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_STATIC, 0);

						current_client->breather_sound ^= 1;

						PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);

						//FIXME: release a bubble? // SURE!!!
						vec3_t start = { 0 };
						VectorCopy(current_player->s.origin, start);
						start[2] += current_player->viewheight;
						spawn_bubble(current_player, start);
						add_sp_score(current_player, 10, SCORE_ITEM_USAGE);
					}
				}
			}
			else if (current_client->pers.items_activated & FL_ACTIVATING_R)	
			{
				if (current_player->delay > level.time)
				{
					current_client->pers.items_activated |= FL_REB;
					current_client->pers.items_activated &= ~FL_ACTIVATING_R;
				}
			}
			else if (current_client->pers.items_activated & FL_ACTIVATING_E)
			{
				if (current_player->delay > level.time)
				{
					current_client->pers.items_activated |= FL_REB;
					current_client->pers.items_activated &= ~FL_ACTIVATING_E;
				}
			}
		}
		else
		{
			// breather or envirosuit give air
			if (breather || envirosuit)
			{
				current_player->air_finished = level.time + 10;

				if (((int)(current_client->breather_framenum - level.framenum) % 25) == 0)
				{
					if (!current_client->breather_sound)
						gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_STATIC, 0);
					else
						gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_STATIC, 0);
					current_client->breather_sound ^= 1;
					PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
					add_sp_score(current_player, 10, SCORE_ITEM_USAGE);

					//FIXME: release a bubble? // SURE!!!
					vec3_t start = { 0 };
					VectorCopy(current_player->s.origin, start);
					start[2] += current_player->viewheight;
					spawn_bubble(current_player, start);
				}
			}
		}

		// if out of air, start drowning
		if (current_player->air_finished < level.time)
		{	// drown!
			if (current_player->client->next_drown_time < level.time
				&& current_player->health > 0)
			{
				current_player->client->next_drown_time = level.time + 1;

				// take more damage the longer underwater
				current_player->dmg += 2;
				if (current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player->health <= current_player->dmg)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/drown1.wav"), 1, ATTN_NORM, 0);
				else if (rand() & 1)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp2.wav"), 1, ATTN_NORM, 0);

				current_player->pain_debounce_time = level.time;

				T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	}
	else
	{
		current_player->air_finished = level.time + 12;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if (item_mod->value)
	{
		if (waterlevel && (current_player->watertype&(CONTENTS_LAVA | CONTENTS_SLIME)))
		{
			if (current_player->watertype & CONTENTS_LAVA)
			{
				if (current_player->health > 0
					&& current_player->pain_debounce_time <= level.time)
				{
					if (rand() & 1)
						gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
					else
						gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);
					current_player->pain_debounce_time = level.time + 1 + (crandom() * 0.2);
				}

				if (current_client->pers.items_activated & FL_ENV && current_client->pers.envi_health > 0)	// take 1/3 damage with envirosuit
				{
					add_sp_score(current_player, 10, SCORE_ITEM_USAGE);
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_LAVA);
				}
				else
				{
					if (current_client->pers.envi_health > 0)
						current_client->pers.envi_health = current_client->pers.envi_health - (3 * waterlevel);
					else
						current_client->pers.envi_health = 0;
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3 * waterlevel, 0, 0, MOD_LAVA);
				}
			}

			if (current_player->watertype & CONTENTS_SLIME)
			{
				if (!(current_client->pers.items_activated & FL_ENV) || current_client->pers.envi_health <= 0)
				{	// no damage from slime with envirosuit
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_SLIME);
				}
				else
				{
					add_sp_score(current_player, 10, SCORE_ITEM_USAGE);
					if (current_client->pers.envi_health > 0)
						current_client->pers.envi_health = current_client->pers.envi_health - (1 * waterlevel);
					else
						current_client->pers.envi_health = 0;
				}
			}
		}
	}
	else
	{

		if (waterlevel && (current_player->watertype&(CONTENTS_LAVA | CONTENTS_SLIME)))
		{
			if (current_player->watertype & CONTENTS_LAVA)
			{
				if (current_player->health > 0
					&& current_player->pain_debounce_time <= level.time
					&& current_client->invincible_framenum < level.framenum)
				{
					if (rand() & 1)
						gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
					else
						gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);
					current_player->pain_debounce_time = level.time + 1;
				}

				if (current_client->pers.items_activated & FL_ENV)	// take 1/3 damage with envirosuit{
				{
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_LAVA);
				}
				else
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3 * waterlevel, 0, 0, MOD_LAVA);
			}

			if (current_player->watertype & CONTENTS_SLIME)
			{
				if (!(current_client->pers.items_activated & FL_ENV))
				{	// no damage from slime with envirosuit
					T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_SLIME);
				}
				else
				{
					add_sp_score(current_player, 10, SCORE_ITEM_USAGE);
				}
			}
		}

	}
}


/*
===============
G_SetClientEffects
===============
*/
void G_SetClientEffects(edict_t *ent)
{
	int		pa_type;
	int		remaining;

	ent->s.effects = 0;
	ent->s.renderfx = 0;

	if (ent->health <= 0 || level.intermissiontime)
		return;

	if (ent->powerarmor_time > level.time)
	{
		pa_type = PowerArmorType(ent);
		if (pa_type == POWER_ARMOR_SCREEN)
		{
			ent->s.effects |= EF_POWERSCREEN;
		}
		else if (pa_type == POWER_ARMOR_SHIELD)
		{
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}
	if (item_mod->value)
	{
		if (ent->client->pers.quad_health > 0 && !(ent->s.effects & EF_QUAD) && ent->client->pers.items_activated & FL_QUAD)
			ent->s.effects |= EF_QUAD;
		if (ent->client->pers.invu_health > 0 && !(ent->s.effects & EF_PENT) && ent->client->pers.items_activated & FL_ENV)
			ent->s.effects |= EF_PENT;
	}
	else
	{
		if (ent->client->quad_framenum > level.framenum)
		{
			remaining = ent->client->quad_framenum - level.framenum;
			if (remaining > 30 || (remaining & 4))
				ent->s.effects |= EF_QUAD;
		}
		if (ent->client->invincible_framenum > level.framenum)
		{
			remaining = ent->client->invincible_framenum - level.framenum;
			if (remaining > 30 || (remaining & 4))
				ent->s.effects |= EF_PENT;
		}
	}




	// show cheaters!!!
	if (ent->flags & FL_GODMODE)
	{
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	}
}


/*
===============
G_SetClientEvent
===============
*/
void G_SetClientEvent(edict_t *ent)
{
	if (ent->s.event)
		return;

	if (ent->groundentity && xyspeed > 225)
	{
		if ((int)(current_client->bobtime + bobmove) != bobcycle)
			ent->s.event = EV_FOOTSTEP;
	}
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound(edict_t *ent)
{
	char	*weap;

	if (ent->client->pers.game_helpchanged != game.helpchanged)
	{
		ent->client->pers.game_helpchanged = game.helpchanged;
		ent->client->pers.helpchanged = 1;
	}

	// help beep (no more than three times)
	if (ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 3 && !(level.framenum & 63))
	{
		ent->client->pers.helpchanged++;
		gi.sound(ent, CHAN_VOICE, gi.soundindex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
	}


	if (ent->client->pers.weapon)
		weap = ent->client->pers.weapon->classname;
	else
		weap = "";

	if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA | CONTENTS_SLIME)))
		ent->s.sound = snd_fry;
	else if (strcmp(weap, "weapon_railgun") == 0)
		ent->s.sound = gi.soundindex("weapons/rg_hum.wav");
	else if (strcmp(weap, "weapon_bfg") == 0)
		ent->s.sound = gi.soundindex("weapons/bfg_hum.wav");
	else if (ent->client->weapon_sound)
		ent->s.sound = ent->client->weapon_sound;
	else
		ent->s.sound = 0;
}

/*
===============
G_SetClientFrame
===============
*/
void G_SetClientFrame(edict_t *ent)
{
	gclient_t	*client;
	qboolean	duck, run;

	//if (item_mod->value)
	//	gi.bprintf(PRINT_HIGH, "ITEM MOD: ENABLED!\n");
	if (ent->s.modelindex != 255)
		return;		// not in the player model

	client = ent->client;

	if (client->ps.pmove.pm_flags & PMF_DUCKED)
		duck = true;
	else
		duck = false;
	if (xyspeed)
		run = true;
	else
		run = false;

	// check for stand/duck and stop/go transitions
	if (duck != client->anim_duck && client->anim_priority < ANIM_DEATH)
		goto newanim;
	if (run != client->anim_run && client->anim_priority == ANIM_BASIC)
		goto newanim;
	if (!ent->groundentity && client->anim_priority <= ANIM_WAVE)
		goto newanim;

	if (client->anim_priority == ANIM_REVERSE)
	{
		if (ent->s.frame > client->anim_end)
		{
			ent->s.frame--;
			return;
		}
	}
	else if (ent->s.frame < client->anim_end)
	{	// continue an animation
		ent->s.frame++;
		return;
	}

	if (client->anim_priority == ANIM_DEATH)
		return;		// stay there
	if (client->anim_priority == ANIM_JUMP)
	{
		if (!ent->groundentity)
			return;		// stay there
		ent->client->anim_priority = ANIM_WAVE;
		ent->s.frame = FRAME_jump3;
		ent->client->anim_end = FRAME_jump6;
		return;
	}

newanim:
	// return to either a running or standing frame
	client->anim_priority = ANIM_BASIC;
	client->anim_duck = duck;
	client->anim_run = run;

	if (!ent->groundentity)
	{
		client->anim_priority = ANIM_JUMP;
		if (ent->s.frame != FRAME_jump2)
			ent->s.frame = FRAME_jump1;
		client->anim_end = FRAME_jump2;
	}
	else if (run)
	{	// running
		if (duck)
		{
			ent->s.frame = FRAME_crwalk1;
			client->anim_end = FRAME_crwalk6;
		}
		else
		{
			ent->s.frame = FRAME_run1;
			client->anim_end = FRAME_run6;
		}
	}
	else
	{	// standing
		if (duck)
		{
			ent->s.frame = FRAME_crstnd01;
			client->anim_end = FRAME_crstnd19;
		}
		else
		{
			ent->s.frame = FRAME_stand01;
			client->anim_end = FRAME_stand40;
		}
	}
}

void HudClientIcon(edict_t *ent)
{
	char hud_guy_str[8];
	int status = 0;
	int health = 0;


	int points = ent->client->damage_blood + ent->client->damage_parmor + ent->client->damage_armor;


	if (ent->health > 0)
	{
		ent->client->hudguy_d *= 0.95f;
		points += ent->client->hudguy_d;
	}
	//gi.bprintf(PRINT_HIGH, "hudguy u = %f, d = %f\n", ent->client->hudguy_u, ent->client->hudguy_d);

	if (!status && ent->client->hudguy_u > level.time && ent->health > 0)
	{
		ent->client->hudguy_d = points;
		return;

	}
	if (points)
	{
		if (points < 33)
			status = 1;
		if (points >= 33) 
		{
			status = 2;// +rand() % 2;
			if(ent->client->v_dmg_yaw > 0)
				status += 1;
		}
			

	}

	if (ent->health >= 100)
		health = 0;
	else if (ent->health >= 75)
		health = 1;
	else if (ent->health >= 50)
		health = 2;
	else if (ent->health >= 25)
		health = 3;
	else if (ent->health >= 0 || status != 0)
		health = 4;
	else if (status == 0)
		health = 5;

	if (status)
		ent->client->hudguy_u = level.time + HUD_GUY_STAY_TIME + ((points - ent->client->hudguy_d) / 100);
	Com_sprintf(hud_guy_str, sizeof(hud_guy_str), "f_%i_%i", status, health);

	ent->client->hudguy = hud_guy_str;
	ent->client->ps.stats[STAT_GUY] = gi.imageindex(ent->client->hudguy);
	//gi.bprintf(PRINT_HIGH, "hudguy = %s\n", va("f_%i_%i", status, health));
	ent->client->hudguy_d = points;
}

/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame(edict_t *ent)
{
	float	bobtime;
	int		i;

	current_player = ent;
	current_client = ent->client;
	//gi.bprintf(PRINT_HIGH, "SCORE: %i\n", ent->client->resp.score);
	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	// 
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	for (i = 0; i < 3; i++)
	{
		current_client->ps.pmove.origin[i] = ent->s.origin[i] * 8.0;
		current_client->ps.pmove.velocity[i] = ent->velocity[i] * 8.0;
	}

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermissiontime)
	{
		// FIXME: add view drifting here?
		current_client->ps.blend[3] = 0;
		current_client->ps.fov = 90;
		G_SetStats(ent);
		return;
	}

	AngleVectors(ent->client->v_angle, forward, right, up);

	// burn from lava, etc
	P_WorldEffects();

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->client->v_angle[PITCH] > 180)
		ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH]) / 3;
	else
		ent->s.angles[PITCH] = ent->client->v_angle[PITCH] / 3;
	ent->s.angles[YAW] = ent->client->v_angle[YAW];
	ent->s.angles[ROLL] = 0;
	ent->s.angles[ROLL] = SV_CalcRoll(ent->s.angles, ent->velocity) * 4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrtf(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->bobtime = 0;	// start at beginning of cycle again
	}
	else if (ent->groundentity)
	{	// so bobbing only cycles when on ground
		if (xyspeed > 210)
			bobmove = 0.25;
		else if (xyspeed > 100)
			bobmove = 0.125;
		else
			bobmove = 0.0625;
	}

	bobtime = (current_client->bobtime += bobmove);

	if (current_client->ps.pmove.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// detect hitting the floor
	P_FallingDamage(ent);

	HudClientIcon(ent);

	// apply all the damage taken this frame
	P_DamageFeedback(ent);

	// determine the view offsets
	SV_CalcViewOffset(ent);

	// determine the gun offsets
	SV_CalcGunOffset(ent);

	// determine the full screen color blend
	// must be after viewoffset, so eye contents can be
	// accurately determined
	// FIXME: with client prediction, the contents
	// should be determined by the client
	SV_CalcBlend(ent);
	//gi.bprintf(PRINT_HIGH, "blend = %f %f %f %f\n", ent->client->ps.blend[0], ent->client->ps.blend[1], ent->client->ps.blend[2], ent->client->ps.blend[3]);
	// chase cam stuff
	if (ent->client->resp.spectator)
		G_SetSpectatorStats(ent);
	else
		G_SetStats(ent);
	G_CheckChaseStats(ent);

	G_SetClientEvent(ent);

	G_SetClientEffects(ent);

	G_SetClientSound(ent);

	G_SetClientFrame(ent);

	VectorCopy(ent->s.origin, ent->s.old_origin);
	VectorCopy(ent->velocity, ent->client->oldvelocity);
	VectorCopy(ent->client->ps.viewangles, ent->client->oldviewangles);

	// clear weapon kicks
	VectorClear(ent->client->kick_origin);
	VectorClear(ent->client->kick_angles);

	// if the scoreboard is up, update it
	if (ent->client->showscores && !(level.framenum & 31))
	{
		DeathmatchScoreboardMessage(ent, ent->enemy);
		gi.unicast(ent, false);
	}
	/*
	// if the scoreboard is up, update it
	if (ent->client->showscores )
	{
		if(!deathmatch->value || coop->value)
			IntermissionScoreboardMessage(ent);
		else if(!(level.framenum & 31))
			DeathmatchScoreboardMessage(ent, ent->enemy);
		gi.unicast(ent, false);
	}*/
}


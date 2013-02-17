/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2009 Darklegion Development

This file is part of Daemon.

Daemon is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"
#include "g_spawn.h"

//the same as InitTrigger
void InitBrushSensor( gentity_t *self )
{
	if ( !VectorCompare( self->s.angles, vec3_origin ) )
	{
		G_SetMovedir( self->s.angles, self->movedir );
	}

	trap_SetBrushModel( self, self->model );
	self->r.contents = CONTENTS_TRIGGER; // replaces the -1 from trap_SetBrushModel
	self->r.svFlags = SVF_NOCLIENT;
}

// the wait time has passed, so set back up for another activation
void trigger_checkWaitForReactivation_think( gentity_t *ent )
{
	ent->nextthink = 0;
}

void trigger_checkWaitForReactivation( gentity_t *self )
{
	if ( self->wait > 0 )
	{
		self->think = trigger_checkWaitForReactivation_think;
		entity_SetNextthink( self );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		self->touch = 0;
		self->nextthink = level.time + FRAMETIME;
		self->think = G_FreeEntity;
	}
}

// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void trigger_multiple_trigger( gentity_t *ent, gentity_t *activator )
{
	ent->activator = activator;

	if ( ent->nextthink )
	{
		return; // can't retrigger until the wait is over
	}

	if ( activator && activator->client )
	{
		if ( ( ent->spawnflags & 1 ) &&
		     activator->client->ps.stats[ STAT_TEAM ] != TEAM_HUMANS )
		{
			return;
		}

		if ( ( ent->spawnflags & 2 ) &&
		     activator->client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS )
		{
			return;
		}
	}

	G_UseTargets( ent, ent->activator );
	trigger_checkWaitForReactivation( ent );
}

void trigger_multiple_use( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	trigger_multiple_trigger( ent, other );

	if ( g_debugEntities.integer >= -1 ) //dont't warn about anything with -1 or lower
	{
		G_Printf( "^3ERROR: ^7It appears as if ^5%s^7 is targeted by ^5%s^7 to enforce firing, which is undefined behavior — stop doing that! This WILL break in future releases and toggle the sensor instead.\n", ent->classname, activator->classname );
	}
}

void trigger_multiple_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if ( !other->client && other->s.eType != ET_BUILDABLE )
	{
		return;
	}

	trigger_multiple_trigger( self, other );
}

/*QUAKED trigger_multiple (.5 .5 .5) ?
"wait" : Seconds between triggerings, 0.5 default, -1 = one time only.
"random"  wait variance, default is 0
Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)
*/
void SP_trigger_multiple( gentity_t *ent )
{
	if (!ent->wait)
		ent->wait = 0.5f;

	if ( ent->waitVariance >= ent->wait && ent->wait >= 0 )
	{
		ent->waitVariance = ent->wait - FRAMETIME;
		G_Printf( "trigger_multiple has random >= wait\n" );
	}

	ent->touch = trigger_multiple_touch;
	ent->use = trigger_multiple_use;

	InitBrushSensor( ent );
	trap_LinkEntity( ent );
}

/*
==============================================================================

trigger_start

==============================================================================
*/

void sensor_start_think( gentity_t *ent )
{
	G_UseTargets( ent, ent );
	G_FreeEntity( ent );
}

/**
 * Warning: The following comment contains information, that might be parsed and used by radiant based mapeditors.
 */
/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
Automatic trigger. It will fire the entities it targets as soon as it spawns in the game.

target: this points to the entity to activate.
*/
void SP_sensor_start( gentity_t *ent )
{
	// we must have some delay to make sure our use targets are present
	ent->nextthink = level.time + 300;
	ent->think = sensor_start_think;
}

/*
==============================================================================

timer

==============================================================================
*/

void sensor_timer_think( gentity_t *self )
{
	G_UseTargets( self, self->activator );
	// set time before next firing
	entity_SetNextthink( self );
}

void sensor_timer_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	self->activator = activator;

	// if on, turn it off
	if ( self->nextthink )
	{
		self->nextthink = 0;
		return;
	}

	// turn it on
	sensor_timer_think( self );
}

/**
 * Warning: The following comment contains information, that might be parsed and used by radiant based mapeditors.
 */
/*QUAKED trigger_timer (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON
Time delay trigger that will continuously fire its targets after a preset time delay. The time delay can also be randomized. When triggered, the timer will toggle on/off.
Formerly known as func_timer.

wait: delay in seconds between each triggering of its targets (default 1).
random: random time variance in seconds added or subtracted from "wait" delay (default 0 - see Notes).
target, target2, target3, target4: this points to the entities to trigger.
targetname, targetname2, targetname3, targetname3: any triggering entity that targets one of these names will toggle the timer on/off when activated.

-------- SPAWNFLAGS --------
START_ON: timer will start on in the game and continuously fire its targets.

-------- NOTES --------
When the random key is set, its value is used to calculate a minimum and a maximum delay.
The final time delay will be a random value anywhere between the minimum and maximum values: (min delay = wait - random) (max delay = wait + random).
*/
void SP_sensor_timer( gentity_t *self )
{
	if (!self->wait)
		self->wait = 1.0f;

	if (!self->waitVariance)
		self->waitVariance = 1.0f;

	self->use = sensor_timer_use;
	self->think = sensor_timer_think;

	if ( self->waitVariance >= self->wait )
	{
		self->waitVariance = self->wait - FRAMETIME;
		G_Printf( "trigger_timer at %s has random >= wait\n", vtos( self->s.origin ) );
	}

	if ( self->spawnflags & 1 )
	{
		self->nextthink = level.time + FRAMETIME;
		self->activator = self;
	}

	self->r.svFlags = SVF_NOCLIENT;
}

/*
===============
G_notify_sensor_stage

Called when stages change
===============
*/
void G_notify_sensor_stage( team_t team, stage_t stage )
{
	int       i;
	gentity_t *ent;

	for ( i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++ )
	{
		if ( !ent->inuse )
		{
			continue;
		}

		if ( !Q_stricmp( ent->classname, "sensor_stage" ) )
		{
			if ( team == ent->conditions.team && stage == ent->conditions.stage )
			{
				G_UseTargets( ent, ent );
			}
		}
	}
}

/*
===============
sensor_stage_use
===============
*/
void sensor_stage_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	G_UseTargets( self, self );

	if ( g_debugEntities.integer >= -1 ) //dont't warn about anything with -1 or lower
	{
		G_Printf( "^3ERROR: ^7It appears as if ^5%s^7 is targeted by ^5%s^7 to enforce firing, which is undefined behavior — stop doing that! This WILL break in future releases and toggle the sensor instead.\n", self->classname, activator->classname );
	}
}

void SP_sensor_stage( gentity_t *self )
{
	G_SpawnInt( "team", "0", ( int * ) &self->conditions.team );

	self->use = sensor_stage_use;

	self->r.svFlags = SVF_NOCLIENT;
}

/*
===============
sensor_win_use
===============
*/
void sensor_win_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	G_UseTargets( self, self );

	if ( g_debugEntities.integer >= -1 ) //dont't warn about anything with -1 or lower
	{
		G_Printf( "^3ERROR: ^7It appears as if ^5%s^7 is targeted by ^5%s^7 to enforce firing, which is undefined behavior — stop doing that! This WILL break in future releases and toggle the sensor instead.\n", self->classname, activator->classname );
	}
}



void G_notify_sensor_win( team_t winningTeam )
{
	int       i;
	gentity_t *ent;

	for ( i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++ )
	{
		if ( !ent->inuse )
		{
			continue;
		}

		if ( !Q_stricmp( ent->classname, "sensor_win" ) )
		{
			if ( winningTeam == ent->conditions.team )
			{
				G_UseTargets( ent, ent );
			}
		}
	}
}


void SP_sensor_win( gentity_t *self )
{
	G_SpawnInt( "team", "0", ( int * ) &self->conditions.team );

	self->use = sensor_win_use;

	self->r.svFlags = SVF_NOCLIENT;
}

/*
===============
sensor_buildable_match
===============
*/
qboolean sensor_buildable_match( gentity_t *self, gentity_t *activator )
{
	int i = 0;

	if ( !activator )
	{
		return qfalse;
	}

	//if there is no buildable list every buildable triggers
	if ( self->conditions.buildables[ i ] == BA_NONE )
	{
		return qtrue;
	}
	else
	{
		//otherwise check against the list
		for ( i = 0; self->conditions.buildables[ i ] != BA_NONE; i++ )
		{
			if ( activator->s.modelindex == self->conditions.buildables[ i ] )
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
===============
sensor_class_match
===============
*/
qboolean sensor_class_match( gentity_t *self, gentity_t *activator )
{
	int i = 0;

	if ( !activator )
	{
		return qfalse;
	}

	//if there is no class list every class triggers (stupid case)
	if ( self->conditions.classes[ i ] == PCL_NONE )
	{
		return qtrue;
	}
	else
	{
		//otherwise check against the list
		for ( i = 0; self->conditions.classes[ i ] != PCL_NONE; i++ )
		{
			if ( activator->client->ps.stats[ STAT_CLASS ] == self->conditions.classes[ i ] )
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
===============
sensor_equipment_match
===============
*/
qboolean sensor_equipment_match( gentity_t *self, gentity_t *activator )
{
	int i = 0;

	if ( !activator )
	{
		return qfalse;
	}

	if ( self->conditions.weapons[ i ] == WP_NONE && self->conditions.upgrades[ i ] == UP_NONE )
	{
		//if there is no equipment list all equipment triggers for the old behavior of target_equipment, but not the new or different one
		return qtrue;
	}
	else
	{
		//otherwise check against the lists
		for ( i = 0; self->conditions.weapons[ i ] != WP_NONE; i++ )
		{
			if ( BG_InventoryContainsWeapon( self->conditions.weapons[ i ], activator->client->ps.stats ) )
			{
				return qtrue;
			}
		}

		for ( i = 0; self->conditions.upgrades[ i ] != UP_NONE; i++ )
		{
			if ( BG_InventoryContainsUpgrade( self->conditions.upgrades[ i ], activator->client->ps.stats ) )
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
===============
trigger_buildable_trigger
===============
*/
void sensor_buildable_trigger( gentity_t *self, gentity_t *activator )
{
	self->activator = activator;

	if ( self->s.eFlags & EF_NODRAW )
	{
		return;
	}

	if ( self->nextthink )
	{
		return; // can't retrigger until the wait is over
	}

	if ( self->s.eFlags & EF_DEAD )
	{
		if ( !sensor_buildable_match( self, activator ) )
		{
			G_UseTargets( self, activator );
			trigger_checkWaitForReactivation( self );
		}
	}
	else
	{
		if ( sensor_buildable_match( self, activator ) )
		{
			G_UseTargets( self, activator );
			trigger_checkWaitForReactivation( self );
		}
	}
}

/*
===============
trigger_equipment_trigger
===============
*/
void sensor_client_trigger( gentity_t *self, gentity_t *activator )
{
	//sanity check
	if ( !activator || !activator->client )
	{
		return;
	}
	if ( self->s.eFlags & EF_NODRAW )
	{
		return;
	}
	self->activator = activator;

	if ( self->nextthink )
	{
		return; // can't retrigger until the wait is over
	}

	if ( ( self->conditions.upgrades[0] || self->conditions.weapons[0] ) && activator->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
	{
		if ( self->s.eFlags & EF_DEAD )
		{
			if ( !sensor_equipment_match( self, activator ) )
			{
				G_UseTargets( self, activator );
				trigger_checkWaitForReactivation( self );
			}
		}
		else
		{
			if ( sensor_equipment_match( self, activator ) )
			{
				G_UseTargets( self, activator );
				trigger_checkWaitForReactivation( self );
			}
		}
	}
	else if ( self->conditions.classes[0] && activator->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
	{
		if ( self->s.eFlags & EF_DEAD )
		{
			if ( !sensor_class_match( self, activator ) )
			{
				G_UseTargets( self, activator );
				trigger_checkWaitForReactivation( self );
			}
		}
		else
		{
			if ( sensor_class_match( self, activator ) )
			{
				G_UseTargets( self, activator );
				trigger_checkWaitForReactivation( self );
			}
		}
	}
}

void sensor_player_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if ( self->conditions.buildables[0] && other->s.eType == ET_BUILDABLE )
	{
		sensor_buildable_trigger( self, other );
	}
	else if ( other->client )
	{
		sensor_client_trigger( self, other );
	}
}

void sensor_touch_use( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	ent->s.eFlags ^= EF_NODRAW;
}

//for compatibility
void SP_sensor_touch_compat( gentity_t *entity )
{
	if (!entity->wait)
		entity->wait = 0.5f;

	if ( entity->waitVariance >= entity->wait && entity->wait >= 0 )
	{
		entity->waitVariance = entity->wait - FRAMETIME;
		G_Printf( "^3WARNING: ^7%s has waitVariance >= wait\n", entity->classname );
	}

	SP_sensor_touch( entity );
}

/*
===============
SP_sensor_touch
===============
*/
void SP_sensor_touch( gentity_t *self )
{
	entity_ParseConditions( self );

	self->touch = sensor_player_touch;
	self->use = sensor_touch_use;

	// SPAWN_DISABLED
	if ( self->spawnflags & 1 )
	{
		self->s.eFlags |= EF_NODRAW;
	}

	// NEGATE
	if ( self->spawnflags & 2 )
	{
		self->s.eFlags |= EF_DEAD;
	}

	InitBrushSensor( self );
	trap_LinkEntity( self );
}

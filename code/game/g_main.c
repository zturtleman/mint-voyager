/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//

#include "g_local.h"

level_locals_t	level;

typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
	int			gameFlags;
	float		rangeMin;
	float		rangeMax;
	qboolean	rangeIntegral;

	int			modificationCount;  // for tracking changes
} cvarTable_t;

// game cvar flags
#define GCF_TRACK_CHANGE	1	// track this variable, and announce if changed
#define GCF_TEAM_SHADER		2	// if changed, update shader state
#define GCF_DO_RESTART		4	// if changed, require fresh map load in map_restart

#define RANGE_ALL 0, 0, qfalse
#define RANGE_BOOL 0, 1, qtrue
#define RANGE_INT(min,max) min, max, qtrue
#define RANGE_FLOAT(min,max) min, max, qfalse

gentity_t		g_entities[MAX_GENTITIES];
gplayer_t		g_players[MAX_CLIENTS];
gconnection_t	g_connections[MAX_CLIENTS];

vmCvar_t	g_gametype;
vmCvar_t	g_dmflags;
vmCvar_t	g_fraglimit;
vmCvar_t	g_timelimit;
vmCvar_t	g_capturelimit;
vmCvar_t	g_friendlyFire;
vmCvar_t	g_password;
vmCvar_t	g_needpass;
vmCvar_t	g_maxplayers;
vmCvar_t	g_maxGamePlayers;
vmCvar_t	g_dedicated;
vmCvar_t	g_speed;
vmCvar_t	g_gravity;
vmCvar_t	g_cheats;
vmCvar_t	g_knockback;
vmCvar_t	g_quadfactor;
vmCvar_t	g_forcerespawn;
vmCvar_t	g_inactivity;
vmCvar_t	g_debugMove;
vmCvar_t	g_debugDamage;
vmCvar_t	g_weaponRespawn;
vmCvar_t	g_weaponTeamRespawn;
vmCvar_t	g_motd;
vmCvar_t	g_synchronousClients;
vmCvar_t	g_warmup;
vmCvar_t	g_doWarmup;
vmCvar_t	g_restarted;
vmCvar_t	g_logfile;
vmCvar_t	g_logfileSync;
vmCvar_t	g_podiumDist;
vmCvar_t	g_podiumDrop;
vmCvar_t	g_allowVote;
vmCvar_t	g_teamAutoJoin;
vmCvar_t	g_teamForceBalance;
vmCvar_t	g_banIPs;
vmCvar_t	g_filterBan;
vmCvar_t	g_smoothClients;
vmCvar_t	pmove_overbounce;
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
vmCvar_t	g_rankings;
vmCvar_t	g_listEntity;
vmCvar_t	g_singlePlayer;
#ifdef MISSIONPACK
vmCvar_t	g_obeliskHealth;
vmCvar_t	g_obeliskRegenPeriod;
vmCvar_t	g_obeliskRegenAmount;
vmCvar_t	g_obeliskRespawnDelay;
vmCvar_t	g_cubeTimeout;
#endif
vmCvar_t	g_redteam;
vmCvar_t	g_blueteam;
#ifdef MISSIONPACK
vmCvar_t	g_proxMineTimeout;
#endif
vmCvar_t	g_playerCapsule;
vmCvar_t	g_instagib;

static cvarTable_t		gameCvarTable[] = {
	// don't override the cheat state set by the system
	{ &g_cheats, "sv_cheats", "", 0, 0, RANGE_ALL },

	// noset vars
	{ NULL, "gameversion", PRODUCT_NAME " " PRODUCT_VERSION " " PLATFORM_STRING " " PRODUCT_DATE, CVAR_SERVERINFO | CVAR_ROM, 0, RANGE_ALL },
	{ NULL, "gameprotocol", GAME_PROTOCOL, CVAR_SERVERINFO | CVAR_ROM, 0, RANGE_ALL },
	{ &g_restarted, "g_restarted", "0", CVAR_ROM, 0, RANGE_ALL },

	// latched vars
	{ &g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, GCF_DO_RESTART, RANGE_INT(0, GT_MAX_GAME_TYPE-1)  },
	{ &g_instagib, "g_instagib", "0", CVAR_LATCH, GCF_DO_RESTART, RANGE_BOOL },

	{ &g_maxplayers, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, RANGE_ALL },
	{ &g_maxGamePlayers, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, RANGE_INT(0, MAX_CLIENTS-1) },

	// change anytime vars
	{ &g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, GCF_TRACK_CHANGE, RANGE_INT(0, INT_MAX) },
	{ &g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, GCF_TRACK_CHANGE, RANGE_INT(0, INT_MAX/60000) },
	{ &g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, GCF_TRACK_CHANGE, RANGE_INT(0, INT_MAX) },

	{ &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, RANGE_BOOL },

	{ &g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, GCF_TRACK_CHANGE, RANGE_BOOL },

	{ &g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE, 0, RANGE_BOOL },
	{ &g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE, 0, RANGE_BOOL },

	{ &g_warmup, "g_warmup", "20", CVAR_ARCHIVE, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_doWarmup, "g_doWarmup", "0", CVAR_ARCHIVE, GCF_TRACK_CHANGE, RANGE_BOOL },
	{ &g_logfile, "g_log", "games.log", CVAR_ARCHIVE, 0, RANGE_ALL },
	{ &g_logfileSync, "g_logsync", "0", CVAR_ARCHIVE, 0, RANGE_ALL },

	{ &g_password, "g_password", "", CVAR_USERINFO, 0, RANGE_ALL },

	{ &g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, RANGE_ALL },
	{ &g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, RANGE_ALL },

	{ &g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, RANGE_BOOL },

	{ &g_dedicated, "dedicated", "0", 0, 0, RANGE_ALL },

	{ &g_speed, "g_speed", "250", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_gravity, "g_gravity", "800", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_knockback, "g_knockback", "1000", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_quadfactor, "g_quadfactor", "3", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_weaponRespawn, "g_weaponrespawn", "5", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_weaponTeamRespawn, "g_weaponTeamRespawn", "30", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_forcerespawn, "g_forcerespawn", "20", 0, GCF_TRACK_CHANGE, RANGE_ALL },
	{ &g_inactivity, "g_inactivity", "0", 0, GCF_TRACK_CHANGE, RANGE_BOOL },
	{ &g_debugMove, "g_debugMove", "0", 0, 0, RANGE_BOOL },
	{ &g_debugDamage, "g_debugDamage", "0", 0, 0, RANGE_BOOL },
	{ &g_motd, "g_motd", "", 0, 0, RANGE_ALL },

	{ &g_podiumDist, "g_podiumDist", "80", 0, 0, RANGE_ALL },
	{ &g_podiumDrop, "g_podiumDrop", "70", 0, 0, RANGE_ALL },

	{ &g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, RANGE_BOOL },
	{ &g_listEntity, "g_listEntity", "0", 0, 0, RANGE_ALL },

	{ &g_singlePlayer, "ui_singlePlayerActive", "0", CVAR_SYSTEMINFO | CVAR_ROM, 0, RANGE_ALL },

#ifdef MISSIONPACK
	{ &g_obeliskHealth, "g_obeliskHealth", "2500", 0, 0, RANGE_ALL },
	{ &g_obeliskRegenPeriod, "g_obeliskRegenPeriod", "1", 0, 0, RANGE_ALL },
	{ &g_obeliskRegenAmount, "g_obeliskRegenAmount", "15", 0, 0, RANGE_ALL },
	{ &g_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", CVAR_SYSTEMINFO, 0, RANGE_ALL },

	{ &g_cubeTimeout, "g_cubeTimeout", "30", 0, 0, RANGE_ALL },
#endif

	{ &g_redteam, "g_redteam", DEFAULT_REDTEAM_NAME, CVAR_ARCHIVE | CVAR_SYSTEMINFO, GCF_TRACK_CHANGE | GCF_TEAM_SHADER, RANGE_ALL },
	{ &g_blueteam, "g_blueteam", DEFAULT_BLUETEAM_NAME, CVAR_ARCHIVE | CVAR_SYSTEMINFO, GCF_TRACK_CHANGE | GCF_TEAM_SHADER, RANGE_ALL },

#ifdef MISSIONPACK
	{ &g_proxMineTimeout, "g_proxMineTimeout", "20000", 0, 0, RANGE_ALL },
#endif
	{ &g_playerCapsule, "g_playerCapsule", "0", 0, 0, RANGE_BOOL },
	{ &g_smoothClients, "g_smoothClients", "1", 0, 0, RANGE_BOOL },
	{ &pmove_overbounce, "pmove_overbounce", "0", CVAR_SYSTEMINFO, 0, RANGE_BOOL },
	{ &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, RANGE_BOOL },
	{ &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, RANGE_ALL },

	{ &g_rankings, "g_rankings", "0", 0, 0, RANGE_ALL }

};

static int gameCvarTableSize = ARRAY_LEN( gameCvarTable );


void G_InitGame( int levelTime, int randomSeed, int restart );
void G_RunFrame( int levelTime );
void G_ShutdownGame( int restart );
qboolean G_SnapshotCallback( int entityNum, int playerNum );
void G_VidRestart( void );
int G_MapRestart( int levelTime, int restartTime );
void CheckExitRules( void );


/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {
	switch ( command ) {
	case GAME_GETAPINAME:
		return (intptr_t)GAME_API_NAME;
	case GAME_GETAPIVERSION:
		return ( GAME_API_MAJOR_VERSION << 16) | ( GAME_API_MINOR_VERSION & 0xFFFF );
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;
	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;
	case GAME_PLAYER_CONNECT:
		return (intptr_t)PlayerConnect( arg0, arg1, arg2, arg3, arg4 );
	case GAME_PLAYER_THINK:
		PlayerThink( arg0 );
		return 0;
	case GAME_PLAYER_USERINFO_CHANGED:
		PlayerUserinfoChanged( arg0 );
		return 0;
	case GAME_PLAYER_DISCONNECT:
		return PlayerDisconnect( arg0, arg1 );
	case GAME_PLAYER_BEGIN:
		PlayerBegin( arg0 );
		return 0;
	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;
	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;
	case GAME_CONSOLE_COMMAND:
		return G_ConsoleCommand();
	case GAME_SNAPSHOT_CALLBACK:
		return G_SnapshotCallback( arg0, arg1 );
	case GAME_VID_RESTART:
		G_VidRestart();
		return 0;
	case GAME_MAP_RESTART:
		return G_MapRestart( arg0, arg1 );
	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );
	case GAME_CONSOLE_COMPLETEARGUMENT:
		return G_ConsoleCompleteArgument(arg0);
	default:
		G_Error( "game vmMain: unknown command %i", command );
		break;
	}

	return -1;
}


void QDECL G_DPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	if (!trap_Cvar_VariableIntegerValue("developer")) {
		return;
	}

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	trap_Print( text );
}

void QDECL G_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	trap_Print( text );
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	trap_Error( text );
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=MAX_CLIENTS, e=g_entities+i ; i < level.num_entities ; i++,e++ ) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

	G_DPrintf ("%i teams with %i entities\n", c, c2);
}

void G_RemapTeamShaders( void ) {
#ifdef MISSIONPACK
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f); 
	AddRemap("textures/ctf2/redteam02", string, f); 
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f); 
	AddRemap("textures/ctf2/blueteam02", string, f); 
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );

		if ( cv->rangeMin != 0 || cv->rangeMax != 0 ) {
			trap_Cvar_CheckRange( cv->cvarName, cv->rangeMin, cv->rangeMax, cv->rangeIntegral );
		}

		if ( cv->vmCvar ) {
			cv->modificationCount = cv->vmCvar->modificationCount;
		} else {
			cv->modificationCount = 0;
		}

		if ( cv->gameFlags & GCF_TEAM_SHADER ) {
			remapped = qtrue;
		}
	}

	if ( remapped ) {
		G_RemapTeamShaders();
	}

	// Don't allow single player gametype to be used in multiplayer.
	if ( g_gametype.integer == GT_SINGLE_PLAYER && !g_singlePlayer.integer) {
		trap_Cvar_SetValue( "g_gametype", GT_FFA );
		trap_Cvar_Update( &g_gametype );
	}

	// Don't allow instagib in single player mode.
	if ( g_singlePlayer.integer && g_instagib.integer) {
		trap_Cvar_SetValue( "g_instagib", 0 );
		trap_Cvar_Update( &g_instagib );
	}

	if ( g_instagib.integer ) {
		trap_Cvar_Set( "sv_gametypeName", va( "Instagib %s", bg_displayGametypeNames[g_gametype.integer] ) );
		trap_Cvar_Set( "sv_gametypeNetName", va( "Insta%s", bg_netGametypeNames[g_gametype.integer] ) );
	} else {
		trap_Cvar_Set( "sv_gametypeName", bg_displayGametypeNames[g_gametype.integer] );
		trap_Cvar_Set( "sv_gametypeNetName", bg_netGametypeNames[g_gametype.integer] );
	}

	level.warmupModificationCount = g_warmup.modificationCount;
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		if ( cv->vmCvar ) {
			trap_Cvar_Update( cv->vmCvar );

			if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
				cv->modificationCount = cv->vmCvar->modificationCount;

				if ( cv->gameFlags & GCF_TRACK_CHANGE ) {
					trap_SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", 
						cv->cvarName, cv->vmCvar->string ) );
				}

				if ( cv->gameFlags & GCF_TEAM_SHADER ) {
					remapped = qtrue;
				}
			}
		}
	}

	if ( remapped ) {
		G_RemapTeamShaders();
	}
}

/*
============
G_InitGame

============
*/
void G_InitGame( int levelTime, int randomSeed, int restart ) {
	int					i, j;

	G_DPrintf ("------- Game Initialization -------\n");
	G_DPrintf ("gameversion: %s\n", PRODUCT_NAME " " PRODUCT_VERSION " " PLATFORM_STRING " " PRODUCT_DATE);
	G_DPrintf ("gameprotocol: %s\n", GAME_PROTOCOL);

	srand( randomSeed );

	Swap_Init();

	G_BotInitBotLib();

	G_RegisterCvars();

	G_ProcessIPBans();

	// tell server entity and player state size and network field info
	trap_SetNetFields( sizeof (entityState_t), sizeof (entityState_t) - sizeof (int), bg_entityStateFields, bg_numEntityStateFields,
					   sizeof (playerState_t), 0, bg_playerStateFields, bg_numPlayerStateFields );

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	if ( g_gametype.integer != GT_SINGLE_PLAYER && g_logfile.string[0] ) {
		if ( g_logfileSync.integer ) {
			trap_FS_FOpenFile( g_logfile.string, &level.logFile, FS_APPEND_SYNC );
		} else {
			trap_FS_FOpenFile( g_logfile.string, &level.logFile, FS_APPEND );
		}
		if ( !level.logFile ) {
			G_Printf( "WARNING: Couldn't open logfile: %s\n", g_logfile.string );
		} else {
			char	serverinfo[MAX_INFO_STRING];

			trap_GetServerinfo( serverinfo, sizeof( serverinfo ) );

			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}
	} else {
		G_Printf( "Not logging to disk.\n" );
	}

	G_InitWorldSession();

	G_RegisterCommands( );

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all client connections for this game
	level.maxconnections = g_maxplayers.integer;
	memset( g_connections, 0, MAX_CLIENTS * sizeof(g_connections[0]) );
	level.connections = g_connections;

	// clear local player nums
	for ( i=0 ; i<level.maxconnections ; i++ ) {
		for ( j=0; j<MAX_SPLITVIEW ; j++ ) {
			level.connections[i].localPlayerNums[j] = -1;
		}
	}

	// initialize all players for this game
	level.maxplayers = g_maxplayers.integer;
	memset( g_players, 0, MAX_CLIENTS * sizeof(g_players[0]) );
	level.players = g_players;

	// setup player entity fields
	for ( i=0 ; i<level.maxplayers ; i++ ) {
		g_entities[i].player = level.players + i;
	}

	// always leave room for the max number of players,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but players
	level.num_entities = MAX_CLIENTS;

	for ( i=0 ; i<MAX_CLIENTS ; i++ ) {
		g_entities[i].classname = "playerslot";
	}

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.players[0].ps, sizeof( level.players[0] ) );

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString();

	// general initialization
	G_FindTeams();

	// make sure we have flags for CTF, etc
	if( g_gametype.integer >= GT_TEAM ) {
		G_CheckTeamItems();
	}

	SaveRegisteredItems();

	G_DPrintf ("-----------------------------------\n");

	if( g_gametype.integer == GT_SINGLE_PLAYER || trap_Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
	}

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	G_RemapTeamShaders();

	// clear ready players from intermission
	trap_SetConfigstring( CS_PLAYERS_READY, "" );
	trap_SetConfigstring( CS_INTERMISSION, "" );
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	G_DPrintf ("==== ShutdownGame ====\n");

	if ( level.logFile ) {
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
		level.logFile = 0;
	}

	// write all the player session data so we can get it back
	G_WriteSessionData();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}
}


/*
=================
G_SnapshotCallback
=================
*/
qboolean G_SnapshotCallback( int entityNum, int playerNum ) {
	gentity_t *ent;

	ent = &g_entities[ entityNum ];

	if ( ent->snapshotCallback ) {
		return ent->snapshotCallback( ent, &g_entities[ playerNum ] );
	}

	return qtrue;
}

/*
=================
G_VidRestart

Model handles are no longer valid, re-register all models.
=================
*/
void G_VidRestart( void ) {

}

/*
=================
G_FullMapRestart

Check if full map restart is needed for some cvars to take affect.
=================
*/
qboolean G_NeedFullMapRestart( void ) {
	int			i;
	cvarTable_t	*cv;
	char		value[ MAX_CVAR_VALUE_STRING ];
	char		latched[ MAX_CVAR_VALUE_STRING ];

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		if ( cv->gameFlags & GCF_DO_RESTART ) {
			trap_Cvar_VariableStringBuffer( cv->cvarName, value, sizeof (value) );
			trap_Cvar_LatchedVariableStringBuffer( cv->cvarName, latched, sizeof (latched) );

			if ( strcmp( value, latched ) ) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
=================
G_MapRestart

This is called by the server when map_restart command is issued and when restart time is hit.

return restart time, -1 to do a full map reload, or INT_MAX to prevent restart.
=================
*/
int G_MapRestart( int levelTime, int restartTime ) {
	int			delay;
	char		buf[12];

	if ( trap_Argc() > 1 ) {
		trap_Argv( 1, buf, sizeof (buf) );
		delay = atoi( buf );
	} else if ( g_doWarmup.integer ) {
		// warmup delays using g_warmup after map restart
		delay = 0;
	} else {
		delay = 5;
	}

	// restart time hit
	if ( ( restartTime && levelTime >= restartTime ) || ( !restartTime && delay <= 0 ) ) {
		if ( G_NeedFullMapRestart() ) {
			return -1;
		}

		return 0;
	}

	// don't let user change restart time
	if ( restartTime ) {
		return restartTime;
	}

	restartTime = levelTime + delay * 1000;

	trap_SetConfigstring( CS_WARMUP, va( "%i", restartTime ) );
	return restartTime;
}

//===================================================================

void QDECL Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	Q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	trap_Error( text );
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);

	trap_Print( text );
}

void QDECL Com_DPrintf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);

	G_DPrintf ("%s", text);
}

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gplayer_t	*player;
	gplayer_t	*nextInLine;

	if ( level.numPlayingPlayers >= 2 ) {
		return;
	}

	// never change during intermission
	if ( level.intermissiontime ) {
		return;
	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxplayers ; i++ ) {
		player = &level.players[i];
		if ( player->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( player->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard players
		if ( player->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			player->sess.spectatorPlayer < 0  ) {
			continue;
		}

		if(!nextInLine || player->sess.spectatorNum > nextInLine->sess.spectatorNum)
			nextInLine = player;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.players ], "f" );
}

/*
=======================
AddTournamentQueue

Add player to end of tournament queue
=======================
*/

void AddTournamentQueue(gplayer_t *player)
{
	int index;
	gplayer_t *curplayer;
	
	for(index = 0; index < level.maxplayers; index++)
	{
		curplayer = &level.players[index];
		
		if(curplayer->pers.connected != CON_DISCONNECTED)
		{
			if(curplayer == player)
				curplayer->sess.spectatorNum = 0;
			else if(curplayer->sess.sessionTeam == TEAM_SPECTATOR)
				curplayer->sess.spectatorNum++;
		}
	}
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			playerNum;

	if ( level.numPlayingPlayers != 2 ) {
		return;
	}

	playerNum = level.sortedPlayers[1];

	if ( level.players[ playerNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ playerNum ], "s" );
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			playerNum;

	if ( level.numPlayingPlayers != 2 ) {
		return;
	}

	playerNum = level.sortedPlayers[0];

	if ( level.players[ playerNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ playerNum ], "s" );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			playerNum;

	playerNum = level.sortedPlayers[0];
	if ( level.players[ playerNum ].pers.connected == CON_CONNECTED ) {
		level.players[ playerNum ].sess.wins++;
		PlayerUserinfoChanged( playerNum );
	}

	playerNum = level.sortedPlayers[1];
	if ( level.players[ playerNum ].pers.connected == CON_CONNECTED ) {
		level.players[ playerNum ].sess.losses++;
		PlayerUserinfoChanged( playerNum );
	}

}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gplayer_t	*ca, *cb;

	ca = &level.players[*(int *)a];
	cb = &level.players[*(int *)b];

	// sort special players last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorPlayer < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorPlayer < 0  ) {
		return -1;
	}

	// then connecting players
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorNum > cb->sess.spectatorNum ) {
			return -1;
		}
		if ( ca->sess.spectatorNum < cb->sess.spectatorNum ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every player connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
	gplayer_t	*cl;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedPlayers = 0;
	level.numNonSpectatorPlayers = 0;
	level.numPlayingPlayers = 0;
	level.numVotingPlayers = 0;		// don't count bots

	for (i = 0; i < ARRAY_LEN(level.numTeamVotingPlayers); i++)
		level.numTeamVotingPlayers[i] = 0;

	for ( i = 0 ; i < level.maxplayers ; i++ ) {
		if ( level.players[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedPlayers[level.numConnectedPlayers] = i;
			level.numConnectedPlayers++;

			if ( level.players[i].sess.sessionTeam != TEAM_SPECTATOR ) {
				level.numNonSpectatorPlayers++;
			
				// decide if this should be auto-followed
				if ( level.players[i].pers.connected == CON_CONNECTED ) {
					level.numPlayingPlayers++;
					if ( !(g_entities[i].r.svFlags & SVF_BOT) ) {
						level.numVotingPlayers++;
						if ( level.players[i].sess.sessionTeam == TEAM_RED )
							level.numTeamVotingPlayers[0]++;
						else if ( level.players[i].sess.sessionTeam == TEAM_BLUE )
							level.numTeamVotingPlayers[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	qsort( level.sortedPlayers, level.numConnectedPlayers, 
		sizeof(level.sortedPlayers[0]), SortRanks );

	// set the rank value for all players that are connected and not spectators
	if ( g_gametype.integer >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedPlayers; i++ ) {
			cl = &level.players[ level.sortedPlayers[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {	
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingPlayers; i++ ) {
			cl = &level.players[ level.sortedPlayers[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next player is checked
				level.players[ level.sortedPlayers[i] ].ps.persistant[PERS_RANK] = rank;
			} else {
				// we are tied with the previous player
				level.players[ level.sortedPlayers[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.players[ level.sortedPlayers[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( g_gametype.integer == GT_SINGLE_PLAYER && level.numPlayingPlayers == 1 ) {
				level.players[ level.sortedPlayers[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( g_gametype.integer >= GT_TEAM ) {
		trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedPlayers == 0 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedPlayers == 1 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.players[ level.sortedPlayers[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.players[ level.sortedPlayers[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.players[ level.sortedPlayers[1] ].ps.persistant[PERS_SCORE] ) );
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission, send the new info to everyone
	if ( level.intermissiontime ) {
		SendScoreboardMessageToAllClients();
	}
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxplayers ; i++ ) {
		if ( level.players[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MovePlayerToIntermission

When the intermission starts, this will be called for all players.
If a new player connects, this will be called after the spawn function.
========================
*/
void MovePlayerToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->player->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}

	FindIntermissionPoint();
	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->player->ps.origin );
	VectorCopy (level.intermission_angle, ent->player->ps.viewangles);
	ent->player->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->player->ps.powerups, 0, sizeof(ent->player->ps.powerups) );

	ent->player->ps.eFlags = 0;
	ent->player->ps.contents = 0;
	ent->s.eFlags = 0;
	ent->s.contents = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.event = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint( void ) {
	gentity_t	*ent, *target;
	vec3_t		dir;

	// find the intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle, qfalse );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}

}

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void ) {
	int			i;
	gentity_t	*player;

	if ( level.intermissiontime ) {
		return;		// already active
	}

	// if in tournement mode, change the wins / losses
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		AdjustTournamentScores();
	}

	level.intermissiontime = level.time;
	// move all players to the intermission point
	for (i=0 ; i< level.maxplayers ; i++) {
		player = g_entities + i;
		if (!player->inuse)
			continue;
		// respawn if dead
		if (player->health <= 0) {
			PlayerRespawn(player);
		}
		MovePlayerToIntermission( player );
		trap_UnlinkEntity(player);
	}
#ifdef MISSIONPACK
	if (g_singlePlayer.integer) {
		trap_Cvar_SetValue("ui_singlePlayerActive", 0);
		UpdateTournamentInfo();
	}
#else
	// if single player game
	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		UpdateTournamentInfo();
		SpawnModelsOnVictoryPads();
	}
#endif
	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();

}


/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar 

=============
*/
void ExitLevel (void) {
	int		i;
	gplayer_t *cl;
	char nextmap[MAX_STRING_CHARS];
	char d1[MAX_STRING_CHARS];

	//bot interbreeding
	BotInterbreedEndMatch();

	// if we are running a tournement map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( g_gametype.integer == GT_TOURNAMENT  ) {
		if ( !level.restarted ) {
			RemoveTournamentLoser();
			trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			level.changemap = NULL;
			level.intermissiontime = 0;
		}
		return;	
	}

	trap_Cvar_VariableStringBuffer( "nextmap", nextmap, sizeof(nextmap) );
	trap_Cvar_VariableStringBuffer( "d1", d1, sizeof(d1) );

	if( !Q_stricmp( nextmap, "map_restart 0" ) && Q_stricmp( d1, "" ) ) {
		trap_Cvar_Set( "nextmap", "vstr d2" );
		trap_Cmd_ExecuteText( EXEC_APPEND, "vstr d1\n" );
	} else {
		trap_Cmd_ExecuteText( EXEC_APPEND, "vstr nextmap\n" );
	}

	level.changemap = NULL;
	level.intermissiontime = 0;

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< g_maxplayers.integer ; i++ ) {
		cl = level.players + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before changing to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< g_maxplayers.integer ; i++) {
		if ( level.players[i].pers.connected == CON_CONNECTED ) {
			level.players[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024];
	int			min, tens, sec;

	sec = ( level.time - level.startTime ) / 1000;

	min = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	Com_sprintf( string, sizeof(string), "%3i:%i%i ", min, tens, sec );

	va_start( argptr, fmt );
	Q_vsnprintf(string + 7, sizeof(string) - 7, fmt, argptr);
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + 7 );
	}

	if ( !level.logFile ) {
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gplayer_t		*cl;
#ifdef MISSIONPACK
	qboolean won = qtrue;
	team_t team = TEAM_RED;
#endif
	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedPlayers;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.players[level.sortedPlayers[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  player: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedPlayers[i],	cl->pers.netname );
#ifdef MISSIONPACK
		if (g_singlePlayer.integer && !(g_entities[cl - level.players].r.svFlags & SVF_BOT)) {
			team = cl->sess.sessionTeam;
		}
		if (g_singlePlayer.integer && g_gametype.integer < GT_TEAM) {
			if (g_entities[cl - level.players].r.svFlags & SVF_BOT && cl->ps.persistant[PERS_RANK] == 0) {
				won = qfalse;
			}
		}
#endif

	}

#ifdef MISSIONPACK
	if (g_singlePlayer.integer) {
		if (g_gametype.integer >= GT_TEAM) {
			if (team == TEAM_BLUE) {
				won = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED];
			} else {
				won = level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE];
			}
		}
		trap_Cmd_ExecuteText( EXEC_APPEND, (won) ? "spWin\n" : "spLose\n" );
	}
#endif


}


/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady, playerCount;
	int			i;
	gplayer_t	*cl;
	clientList_t	readyList;

	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		return;
	}

	// see which players are ready
	ready = 0;
	notReady = 0;
	playerCount = 0;
	Com_ClientListClear( &readyList );
	for (i=0 ; i< g_maxplayers.integer ; i++) {
		cl = level.players + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}

		playerCount++;
		if ( cl->readyToExit ) {
			ready++;
			Com_ClientListAdd( &readyList, i );
		} else {
			notReady++;
		}
	}

	// update configstring so it can be displayed on the scoreboard
	trap_SetConfigstring( CS_PLAYERS_READY, Com_ClientListString( &readyList ) );

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

	// only test ready status when there are real players present
	if ( playerCount > 0 ) {
		// if nobody wants to go, clear timer
		if ( !ready ) {
			level.readyToExit = qfalse;
			return;
		}

		// if everyone wants to go, go now
		if ( !notReady ) {
			ExitLevel();
			return;
		}
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if ( level.numPlayingPlayers < 2 ) {
		return qfalse;
	}
	
	if ( g_gametype.integer >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.players[level.sortedPlayers[0]].ps.persistant[PERS_SCORE];
	b = level.players[level.sortedPlayers[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
void CheckExitRules( void ) {
 	int			i;
	gplayer_t	*cl;
	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if ( level.intermissionQueued ) {
#ifdef MISSIONPACK
		int time = (g_singlePlayer.integer) ? SP_INTERMISSION_DELAY_TIME : INTERMISSION_DELAY_TIME;
		if ( level.time - level.intermissionQueued >= time ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#else
		if ( level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#endif
		return;
	}

	// check for sudden death
	if ( ScoreIsTied() ) {
		// always wait for sudden death
		return;
	}

	if ( g_timelimit.integer && !level.warmupTime ) {
		if ( level.time - level.startTime >= g_timelimit.integer*60000 ) {
			trap_SendServerCommand( -1, "print \"Timelimit hit.\n\"");
			LogExit( "Timelimit hit." );
			return;
		}
	}

	if ( g_gametype.integer < GT_CTF && g_fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, "print \"Red hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, "print \"Blue hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		for ( i=0 ; i< g_maxplayers.integer ; i++ ) {
			cl = level.players + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer ) {
				LogExit( "Fraglimit hit." );
				trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
					cl->pers.netname ) );
				return;
			}
		}
	}

	if ( g_gametype.integer >= GT_CTF && g_capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Red hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Blue hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}
	}
}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/


/*
=============
CheckTournament

Once a frame, check for changes in tournement player state
=============
*/
void CheckTournament( void ) {
	// check because we run 3 game frames before calling PlayerConnect and/or PlayerBegin
	// for players on a map_restart
	if ( level.numPlayingPlayers == 0 ) {
		return;
	}

	if ( g_gametype.integer == GT_TOURNAMENT ) {

		// pull in a spectator if needed
		if ( level.numPlayingPlayers < 2 ) {
			AddTournamentPlayer();
		}

		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingPlayers != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingPlayers == 2 ) {
				// fudge by -1 to account for extra delays
				if ( g_warmup.integer > 1 ) {
					level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
				} else {
					level.warmupTime = 0;
				}

				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_SetValue( "g_restarted", 1 );
			trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	} else if ( g_gametype.integer != GT_SINGLE_PLAYER && level.warmupTime != 0 ) {
		int		counts[TEAM_NUM_TEAMS];
		qboolean	notEnough = qfalse;

		if ( g_gametype.integer >= GT_TEAM ) {
			counts[TEAM_BLUE] = TeamCount( -1, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( -1, TEAM_RED );

			if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1) {
				notEnough = qtrue;
			}
		} else if ( level.numPlayingPlayers < 2 ) {
			notEnough = qtrue;
		}

		if ( notEnough ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			// fudge by -1 to account for extra delays
			if ( g_warmup.integer > 1 ) {
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
			} else {
				level.warmupTime = 0;
			}

			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_SetValue( "g_restarted", 1 );
			trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	}
}


/*
==================
CheckVote
==================
*/
void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;
		trap_Cmd_ExecuteText( EXEC_APPEND, va("%s\n", level.voteString ) );
	}
	if ( !level.voteTime ) {
		return;
	}
	if ( level.time - level.voteTime >= VOTE_TIME ) {
		trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );
	} else {
		// ATVI Q3 1.32 Patch #9, WNF
		if ( level.voteYes > level.numVotingPlayers/2 ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Vote passed.\n\"" );
			level.voteExecuteTime = level.time + 3000;
		} else if ( level.voteNo >= level.numVotingPlayers/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );

}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	G_TeamCommand( team, message );
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int playerNum) {
	gplayer_t *player;
	int i;

	player = &level.players[playerNum];

	if ( player->pers.connected == CON_DISCONNECTED ) {
		PrintTeam(team, va("print \"%s is not connected\n\"", player->pers.netname) );
		return;
	}
	if ( player->sess.sessionTeam != team ) {
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", player->pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxplayers ; i++ ) {
		if (level.players[i].sess.sessionTeam != team)
			continue;
		if (level.players[i].sess.teamLeader) {
			level.players[i].sess.teamLeader = qfalse;
			PlayerUserinfoChanged( i );
		}
	}
	player->sess.teamLeader = qtrue;
	PlayerUserinfoChanged( playerNum );
	PrintTeam(team, va("print \"%s is the new team leader\n\"", player->pers.netname) );
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxplayers ; i++ ) {
		if (level.players[i].sess.sessionTeam != team)
			continue;
		if (level.players[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxplayers) {
		for ( i = 0 ; i < level.maxplayers ; i++ ) {
			if (level.players[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.players[i].sess.teamLeader = qtrue;
				break;
			}
		}

		if (i >= level.maxplayers) {
			for ( i = 0 ; i < level.maxplayers ; i++ ) {
				if (level.players[i].sess.sessionTeam != team)
					continue;
				level.players[i].sess.teamLeader = qtrue;
				break;
			}
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}
	if ( level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME ) {
		trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
	} else {
		if ( level.teamVoteYes[cs_offset] > level.numTeamVotingPlayers[cs_offset]/2 ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Team vote passed.\n\"" );
			//
			if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
				//set the team leader
				SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
			}
			else {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
			}
		} else if ( level.teamVoteNo[cs_offset] >= level.numTeamVotingPlayers[cs_offset]/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );

}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;

	if ( g_password.modificationCount != lastMod ) {
		lastMod = g_password.modificationCount;
		if ( *g_password.string && Q_stricmp( g_password.string, "none" ) ) {
			trap_Cvar_SetValue( "g_needpass", 1 );
		} else {
			trap_Cvar_SetValue( "g_needpass", 0 );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent) {
	int	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		return;
	}
	if (thinktime > level.time) {
		return;
	}
	
	ent->nextthink = 0;
	if (!ent->think) {
		G_Error ( "NULL ent->think");
	}
	ent->think (ent);
}

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

	// get any cvar changes
	G_UpdateCvars();

	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->player ) {
					ent->player->ps.externalEvent = 0;
					// predicted events should never be set to zero
					//ent->player->ps.events[0] = 0;
					//ent->player->ps.events[1] = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				G_FreeEntity( ent );
				continue;
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
			G_RunMissile( ent );
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
			G_RunItem( ent );
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		if ( i < MAX_CLIENTS ) {
			G_RunPlayer( ent );
			continue;
		}

		G_RunThink( ent );
	}

	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxplayers ; i++, ent++ ) {
		if ( ent->inuse ) {
			PlayerEndFrame( ent );
		}
	}

	// see if it is time to do a tournement restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

	if (g_listEntity.integer) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_SetValue("g_listEntity", 0);
	}

	// record the time at the end of this frame - it should be about
	// the time the next frame begins - when the server starts
	// accepting commands from connected clients
	level.frameStartTime = trap_Milliseconds();
}

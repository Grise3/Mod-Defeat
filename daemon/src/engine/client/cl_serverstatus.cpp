/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

// cl_serverstatus.cpp: client for serverstatus requests
// (the more detailed kind of server status request containing a full list of players,
// unlike serverinfo which only has counts)

#include "common/Common.h"
#include "client.h"
#include "engine/framework/Network.h"

#define MAX_SERVERSTATUSREQUESTS 16

struct serverStatus_t
{
	char     string[ BIG_INFO_STRING ];
	netadr_t address;
	int      time, startTime;
	bool pending;
	bool print;
	bool retrieved;
};

serverStatus_t cl_serverStatusList[ MAX_SERVERSTATUSREQUESTS ];
int            serverStatusCount;

static Cvar::Range<Cvar::Cvar<int>> cl_serverStatusResendTime(
	"cl_serverStatusResendTime", "timeout (ms) for serverstatus requests",
	Cvar::NONE, 750, 100, 9999);

/*
===================
CL_GetServerStatus
===================
*/
static serverStatus_t *CL_GetServerStatus( const netadr_t& from )
{
//	serverStatus_t *serverStatus;
	int i, oldest, oldestTime;

//	serverStatus = nullptr;
	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( NET_CompareAdr( from, cl_serverStatusList[ i ].address ) )
		{
			return &cl_serverStatusList[ i ];
		}
	}

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( cl_serverStatusList[ i ].retrieved )
		{
			return &cl_serverStatusList[ i ];
		}
	}

	oldest = -1;
	oldestTime = 0;

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( oldest == -1 || cl_serverStatusList[ i ].startTime < oldestTime )
		{
			oldest = i;
			oldestTime = cl_serverStatusList[ i ].startTime;
		}
	}

	if ( oldest != -1 )
	{
		return &cl_serverStatusList[ oldest ];
	}

	serverStatusCount++;
	return &cl_serverStatusList[ serverStatusCount & ( MAX_SERVERSTATUSREQUESTS - 1 ) ];
}

/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen )
{
	int            i;
	netadr_t       to;
	serverStatus_t *serverStatus;

	// if no server address then reset all server status requests
	if ( !serverAddress )
	{
		for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
		{
			cl_serverStatusList[ i ].address.port = 0;
			cl_serverStatusList[ i ].retrieved = true;
		}

		return false;
	}

	// get the address
	if ( !NET_StringToAdr( serverAddress, &to, netadrtype_t::NA_UNSPEC ) )
	{
		return false;
	}

	serverStatus = CL_GetServerStatus( to );

	// if no server status string then reset the server status request for this address
	if ( !serverStatusString )
	{
		serverStatus->retrieved = true;
		return false;
	}

	// if this server status request has the same address
	if ( NET_CompareAdr( to, serverStatus->address ) )
	{
		// if we received a response for this server status request
		if ( !serverStatus->pending )
		{
			Q_strncpyz( serverStatusString, serverStatus->string, maxLen );
			serverStatus->retrieved = true;
			serverStatus->startTime = 0;
			return true;
		}
		// resend the request regularly
		else if ( serverStatus->startTime < Sys::Milliseconds() - cl_serverStatusResendTime.Get() )
		{
			serverStatus->print = false;
			serverStatus->pending = true;
			serverStatus->retrieved = false;
			serverStatus->time = 0;
			serverStatus->startTime = Sys::Milliseconds();
			Net::OutOfBandPrint( netsrc_t::NS_CLIENT, to, "getstatus" );
			return false;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved )
	{
		serverStatus->address = to;
		serverStatus->print = false;
		serverStatus->pending = true;
		serverStatus->retrieved = false;
		serverStatus->startTime = Sys::Milliseconds();
		serverStatus->time = 0;
		Net::OutOfBandPrint( netsrc_t::NS_CLIENT, to, "getstatus" );
		return false;
	}

	return false;
}

/*
===================
CL_ServerStatusResponse
===================
*/
void CL_ServerStatusResponse( const netadr_t& from, msg_t *msg )
{
	const char           *s;
	int            i, score, ping;
	int            len;
	serverStatus_t *serverStatus;

	serverStatus = nullptr;

	for ( i = 0; i < MAX_SERVERSTATUSREQUESTS; i++ )
	{
		if ( NET_CompareAdr( from, cl_serverStatusList[ i ].address ) )
		{
			serverStatus = &cl_serverStatusList[ i ];
			break;
		}
	}

	// if we didn't request this server status
	if ( !serverStatus )
	{
		return;
	}

	s = MSG_ReadStringLine( msg );

	len = 0;
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "%s", s );

	if ( serverStatus->print )
	{
		Log::CommandInteractionMessage("Server settings:" );
		// print cvars
		for (const auto& kv: InfoStringToMap(s)) {
			Log::CommandInteractionMessage(Str::Format("%-24s%s", kv.first, kv.second));
		}
	}

	len = strlen( serverStatus->string );
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\" );

	if ( serverStatus->print )
	{
		Log::CommandInteractionMessage( "\nPlayers:" );
		Log::CommandInteractionMessage( "num: score: ping: name:" );
	}

	for ( i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++ )
	{
		len = strlen( serverStatus->string );
		Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\%s", s );

		if ( serverStatus->print )
		{
			score = ping = 0;
			sscanf( s, "%d %d", &score, &ping );
			s = strchr( s, ' ' );

			if ( s )
			{
				s = strchr( s + 1, ' ' );
			}

			if ( s )
			{
				s++;
			}
			else
			{
				s = "unknown";
			}

			Log::CommandInteractionMessage(Str::Format("%-2d   %-3d    %-3d   %s", i, score, ping, s));
		}
	}

	len = strlen( serverStatus->string );
	Com_sprintf( &serverStatus->string[ len ], sizeof( serverStatus->string ) - len, "\\" );

	serverStatus->time = Sys::Milliseconds();
	serverStatus->address = from;
	serverStatus->pending = false;

	if ( serverStatus->print )
	{
		serverStatus->retrieved = true;
	}
}

/*
==================
CL_ServerStatus_f
==================
*/
void CL_ServerStatus_f()
{
	netadr_t       to, *toptr = nullptr;
	const char     *server;
	serverStatus_t *serverStatus;
	int            argc;
	netadrtype_t   family = netadrtype_t::NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 )
	{
		if ( cls.state != connstate_t::CA_ACTIVE || clc.demoplaying )
		{
			Log::Notice( "Not connected to a server." );
			Cmd_PrintUsage("[-4|-6] <server>", nullptr);
			return;
		}

		toptr = &clc.serverAddress;
	}

	if ( !toptr )
	{
		memset( &to, 0, sizeof( netadr_t ) );

		if ( argc == 2 )
		{
			server = Cmd_Argv( 1 );
		}
		else
		{
			if ( !strcmp( Cmd_Argv( 1 ), "-4" ) )
			{
				family = netadrtype_t::NA_IP;
			}
			else if ( !strcmp( Cmd_Argv( 1 ), "-6" ) )
			{
				family = netadrtype_t::NA_IP6;
			}
			else
			{
				Log::Warn( "only -4 or -6 as address type understood." );
			}

			server = Cmd_Argv( 2 );
		}

		toptr = &to;

		if ( !NET_StringToAdr( server, toptr, family ) )
		{
			return;
		}
	}

	Net::OutOfBandPrint( netsrc_t::NS_CLIENT, *toptr, "getstatus" );

	serverStatus = CL_GetServerStatus( *toptr );
	serverStatus->address = *toptr;
	serverStatus->print = true;
	serverStatus->pending = true;
}

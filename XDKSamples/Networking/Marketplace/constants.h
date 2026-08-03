//-----------------------------------------------------------------------------
// File: Constants.h
//
// Desc: constant definitions
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

// Colors for the glyphs above players heads
#define LOCAL_PLAYER_COLOR          0xffa0a010
#define TALKING_LOCAL_COLOR         0xffffffa0
#define REMOTE_PLAYER_COLOR         0xff1010a0
#define TALKING_REMOTE_COLOR        0xffa0a0ff
#define RECEIVING_COLOR             0xffffffff
#define ON_PRIVATE_COLOR            0xffa01010
#define TALKING_ON_PRIVATE_COLOR    0xffffa0a0

// Player defines
#define ID_UNPROCESSED           0x0000  // target players haven't been selected yet
#define ID_NORMAL_VOICE          0xefff  // voice going to be separated out to nearest players
#define ID_PRIVATE_CHANNEL       0xeffe  // voice going to private channel
#define ID_PODIUM                0xeffd  // voice going to podium
#define ID_FIRST_PLAYER          0xf000  // first player / player on host machine


// timeout before dropping a player
#define PLAYER_TIMEOUT           4.0f    // haven't heard from them in 4 seconds, assume they dropped

// UI defines
#define ERROR_MSG_LEN   30
#define ACCOUNTS_PER_SCREEN 6

// Soundbit defines
#define MAX_PACKETS_PER_SOUNDBIT 400            // 10 seconds of speech max
#define SOUNDBIT_ROWS   8                       // # of different voices
#define SOUNDBIT_COLS   10                      // Samples per voice
#define SOUNDBIT_VOICE_BUFFER           73333333    // cpu cycles to buffer on soundbit playback (0.1s)

// Voice defines
#define MAX_SIMULTANEOUS_REMOTE_TALKERS  8       // maximum number of decoded streams
#define COMPRESSED_VOICE_SIZE            10
#define MAX_BUNDLED_PACKETS              20

// length of time a single voice packet represents - used to fake things in the soundbit manager
#define VOICE_PACKET_TIME               0.02f   // 20 ms per voice packet
#define VOICE_PACKET_TIME_IN_CPU_CYCLES 14666667

// Dsound distances for audio 
#define MARKET_DISTANCE_FACTOR          5.0f    // distance factor for audio
#define MARKET_MIN_VOICE_DISTANCE       8.0f    // falloff rate for voice
#define MARKET_PODIUM_VOICE_DISTANCE    200.0f  // falloff rate for voice at the podium

// Network defines
#define UNRELIABLE_PORT 1000        // port for udp
#define RELIABLE_PORT   1001        // tcp port (not used at the moment)
#define FULL_PACKET_OVERHEAD    44  // 44 bytes in packet overhead for peer-to-peer

// Screen location for marketplace
#define MARKET_X  -60.0f
#define MARKET_Y  0.0f
#define MARKET_SCALE 1.0f
#define MAX_COORD_X 100.0f
#define MAX_COORD_Y 100.0f
#define COLLISION_ITERATIONS  10

// graphics indices in rdf file - hardcoded
#define GFX_MARKETPLACE 0
#define GFX_PODIUM      1
#define GFX_FOUNTAIN    2
#define GFX_PLANTS      3
#define GFX_TREE        4
#define GFX_GIRL_START_IDLE  5   // index in RDF file where the animations start
#define GFX_GIRL_START_WALK  6
#define GFX_GIRL_FACING_STRIDE  9  // 9 frames per direction - 1 per idle, 8 per walk

#define GFX_IDLE_SPEED  0.15f       // time per frame of an idle
#define GFX_WALK_SPEED  0.1f       // time per frame of a full speed walk

#define GFX_PLAYER_RADIUS   1.3f
#define GFX_PODIUM_RADIUS   3.0f
#define GFX_TREE_RADIUS     4.0f
#define GFX_PLANTS_RADIUS   4.0f
#define GFX_FOUNTAIN_RADIUS 9.3f

// Gameplay defines 
#define MAX_SPEED 15.0f    // movement speed ( scaled based on analog stick )

// Game metrics
#define MAX_PLAYERS 32
#define MAX_CHANNELS 16     // maximum number of people sending at once (configurable in server properties) 

// voice mode defines
#define VM_SERVER_FORWARD 0
#define VM_PEER_TO_PEER   1

// server flags 
#define SF_SERVER_PROPS_CHANGED 0x1

// extra flags
#define EF_TALKPARAMS 0x1

// location of the broadcast spot for the marketplace
#define PODIUM_LOCATION     D3DXVECTOR3( 100.0f, 4.0f, 0.0f )
#define PODIUM_RADIUS       100.0f

// window of stats to capture ( for stats-display window )
// 5 seconds worth of stats at 50ms network updates
#define STATS_WINDOW_SIZE   ( 20 * 5 )  


#define GLYPH_BOT_ICON          L"\400"
#define GLYPH_PLAYER_ICON       L"\401"
#define GLYPH_BROADCAST_ICON    L"\402"
#define GLYPH_TALK_ICON         L"\403"
#define GLYPH_PRIVATE_TALK_ICON L"\404"
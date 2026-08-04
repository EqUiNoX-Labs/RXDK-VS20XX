//-----------------------------------------------------------------------------
// File: Marketplace.cpp
//
// Desc: The Marketplace demo demonstrates proximity-based conversations for large 
//       numbers of users over speaker and headset.   It shows a method for dynamically
//       allocating bandwidth using simple peer prediction for a scalable mix of
//       peer-to-peer and client-server voice traffic.
//
//       Note that this is a demo more than a sample; the source code probably isn't
//       directly reusable, but it is more an implementation of the concepts that 
//       will be in the forthcoming whitepaper.  
//
//       Marketplace is the main application class; this file defines the state
//       machine and message pump for the application.
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

// stuck all of the include.h files here for convenience
#include "commoninclude.h"



//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,   XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_LEFTSTICK,     XBHELP_PLACEMENT_1, L"Move player" },    
    { XBHELP_WHITE_BUTTON,  XBHELP_PLACEMENT_1, L"Show netgraph" },
    { XBHELP_BLACK_BUTTON,  XBHELP_PLACEMENT_1, L"Show names" },    
};

XBHELP_CALLOUT g_HostHelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,   XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_LEFTSTICK,     XBHELP_PLACEMENT_1, L"Move player" },
    { XBHELP_Y_BUTTON,      XBHELP_PLACEMENT_1, L"Host menu" },
    { XBHELP_WHITE_BUTTON,  XBHELP_PLACEMENT_1, L"Show netgraph" },
    { XBHELP_BLACK_BUTTON,  XBHELP_PLACEMENT_1, L"Show names" },    
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )
#define NUM_HOST_HELP_CALLOUTS ( sizeof(g_HostHelpCallouts) / sizeof(g_HostHelpCallouts[0]) )

Marketplace g_Marketplace;

//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    if( FAILED( g_Marketplace.Create() ) )
        return;
    g_Marketplace.Run();
}




//-----------------------------------------------------------------------------
// Name: Marketplace::Marketplace()
// Desc: Constructor for Marketplace class
//-----------------------------------------------------------------------------
Marketplace::Marketplace() 
            :CXBApplication()           
{
    m_State = STATE_SELECT_ACCOUNT;
    m_iCurrentUISelection = 0;
    m_iCurrentUIDisplay = 0;
    m_hOnlineTask = NULL;
    m_fLastNetworkUpdate = 0.0f;
    m_fLastVoiceUpdate = 0.0f;
    m_bHost = false;

    // initial host properties
    m_HostProps.byVoiceMode = VM_PEER_TO_PEER;
    m_HostProps.byNetworkUpdateRate = 5;
    m_HostProps.byVoiceUpdateRate = 5;
    m_HostProps.bySpeechTimeout = 10;
    m_HostProps.byNumClientChannelsIn = 4;
    m_HostProps.byNumClientChannelsOut = 4;

    m_fTime = 0.0f;
    m_bDrawHelp = FALSE;    
    m_bShowStats = false;
    m_bBots = false;
    m_bShowNames = false;
    
    // reusable sockaddr structure
    m_SockAddr.sin_family = AF_INET;
    m_SockAddr.sin_addr.s_addr = INADDR_ANY;     
    m_SockAddr.sin_port = htons( UNRELIABLE_PORT );
    
    StatsReset();
}


//-----------------------------------------------------------------------------
// Name: Marketplace::Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT Marketplace::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;


    if( FAILED( m_IconFont.Create( "IconFont.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

     // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;

    // Get information on all accounts for this Xbox
    if( FAILED( XBOnline_GetUserList( m_UserAccountList ) ) )
        return E_FAIL;

    if( FAILED( m_SpriteDraw.Create( "MarketplaceMedia.xpr" ) ) )
        return E_FAIL;
    
    if( m_UserAccountList.empty() )
    {
        m_State = STATE_ERROR;
        wcscpy( m_ErrorMsg, L"No online accounts available" );
        m_ErrorNextState = STATE_ERROR;
    }
    else
        m_State = STATE_SELECT_ACCOUNT;  

    
    BOOL bSuccess = m_VDPSock.Open( CXBSocket::Type_VDP );
    if( !bSuccess )
    {
        m_State = STATE_ERROR;
        wcscpy( m_ErrorMsg, L"Socket error" );
        m_ErrorNextState = STATE_ERROR;
    }
    
    // For now, create an unreliable socket.  It's a best practice that game flow data go over
    // a reliable socket, but that is not what we are demonstrating with this sample.

    SOCKADDR_IN directAddr;
    directAddr.sin_family = AF_INET;
    directAddr.sin_addr.s_addr = INADDR_ANY;
    directAddr.sin_port = htons( UNRELIABLE_PORT );
        
    INT iResult = m_VDPSock.Bind( &directAddr );
    if( iResult == SOCKET_ERROR )
    {
        m_State = STATE_ERROR;
        wcscpy( m_ErrorMsg, L"Socket error" );
        m_ErrorNextState = STATE_ERROR;
    }
    
    DWORD dwNonBlocking = 1;
    iResult = m_VDPSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    if( iResult == SOCKET_ERROR )
    {
        m_State = STATE_ERROR;
        wcscpy( m_ErrorMsg, L"Socket error" );
        m_ErrorNextState = STATE_ERROR;
    }
    
    // initialize the audio manager with the dsp image, the wavebank, and the sound effect files for Xact.

    g_AudioMgr.Initialize("D:\\media\\sounds\\xactsounds_memory.xwb", "D:\\media\\sounds\\xactsounds.xsb", "D:\\media\\dsstdfx.bin");
    

    // create the ambient crowd noise- the volume of this will scale based on how many people are talking at once

    m_pCrowdAmbient = g_AudioMgr.CreateAmbientSound();    
    m_pCrowdAmbient->Initialize( "MarketplaceAmbience" );

    // create the doorbell as an ambient sound- it gets played everytime someone joins

    m_pDoorbell = g_AudioMgr.CreateAmbientSound();
    m_pDoorbell->Initialize( "chimes" );

    // create the objects (trees, fountain, podium, etc)
    CreateObjects();

    g_AudioMgr.CommitSettings();

    // pull in the voice db for the bots
    g_SoundbitDB.ReadFromFile("D:\\media\\marketplace.sdb");

    return S_OK;
}




//------------------------------------------------------------------------------
// Name: Marketplace::CreateObjects()
// Desc: Create all of the trees, the fountain and the podium
//------------------------------------------------------------------------------
VOID Marketplace::CreateObjects()
{        
    DisplayedObject *pObj;    

    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 50.0f, 50.0f, 0.0f ), GFX_FOUNTAIN, GFX_FOUNTAIN_RADIUS );
    
    PositionedWaveBankSound *pSound;
    pSound = g_AudioMgr.CreatePositionedWaveBankSound();
    pSound->Initialize("Fountain", pObj->GetPosition() );  // fountain loops, so no problem
    pObj->SetWavSound( pSound );

    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 92.0f, 12.0f, 0.0f), GFX_PODIUM, GFX_PODIUM_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 55.0f, 60.0f, 0.0f), GFX_PLANTS, GFX_PLANTS_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 20.0f, 15.0f, 0.0f), GFX_PLANTS, GFX_PLANTS_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 30.0f, 90.0f, 0.0f), GFX_PLANTS, GFX_PLANTS_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 90.0f, 70.0f, 0.0f), GFX_PLANTS, GFX_PLANTS_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 70.0f, 90.0f, 0.0f), GFX_PLANTS, GFX_PLANTS_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 65.0f, 40.0f, 0.0f), GFX_TREE, GFX_TREE_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 10.0f, 80.0f, 0.0f), GFX_TREE, GFX_TREE_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 87.0f, 84.0f, 0.0f), GFX_TREE, GFX_TREE_RADIUS );
    pObj = new DisplayedObject;
    pObj->Initialize( D3DXVECTOR3( 70.0f, 10.0f, 0.0f), GFX_TREE, GFX_TREE_RADIUS );  
}




//------------------------------------------------------------------------------
// Name: Marketplace::CreateBots()
// Desc: Creates the bots, putting them in 2 clusters of 4 each, and assigns
//       a different voice from the soundbit database to each of them
//------------------------------------------------------------------------------
VOID Marketplace::CreateBots( XNADDR *pxnAddr)
{ 
    PlayerBot *pBot;
    if (m_bHost)
    {
        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 0 );
        pBot->SetPosition( D3DXVECTOR3( 25.0f, 55.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( -1.0f, 0.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 1 );
        pBot->SetPosition( D3DXVECTOR3( 15.0f, 55.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 1.0f, 0.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 2 );
        pBot->SetPosition( D3DXVECTOR3( 20.0f, 60.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 0.0f, -1.0f, 0.0f ) );        
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 3 );
        pBot->SetPosition( D3DXVECTOR3( 20.0f, 50.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 4 );
        pBot->SetPosition( D3DXVECTOR3( 85.0f, 35.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( -1.0f, 0.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 5 ); 
        pBot->SetPosition( D3DXVECTOR3( 75.0f, 35.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 1.0f, 0.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 6 );
        pBot->SetPosition( D3DXVECTOR3( 80.0f, 40.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 0.0f, -1.0f, 0.0f ) );        
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );

        pBot = (PlayerBot *)g_PlayerMgr.CreateNewPlayer( LOCALFLAG_BOT );
        pBot->SetVoice( 7 );
        pBot->SetPosition( D3DXVECTOR3( 80.0f, 30.0f, 0.0f ) );
        pBot->SetFacing( D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) );
        pBot->SetOnlineData( pxnAddr, NULL, pBot->GetXUID() );
    } 
}




//------------------------------------------------------------------------------
// Name: Marketplace::GetInAddrFromXnAddr()
// Desc: 
//------------------------------------------------------------------------------
VOID Marketplace::GetInAddrFromXnAddr( IN_ADDR *ina, XNADDR *pxna )
{
    XNetXnAddrToInAddr( pxna, &m_SessionID, ina );
}




//------------------------------------------------------------------------------
// Name: Marketplace::GetXnAddrFromInAddr()
// Desc: 
//------------------------------------------------------------------------------
VOID Marketplace::GetXnAddrFromInAddr( XNADDR *pxna, IN_ADDR *ina )
{
    XNetInAddrToXnAddr( *ina, pxna, &m_SessionID );
}



//-----------------------------------------------------------------------------
// Name: Marketplace::FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT Marketplace::FrameMove()
{
    HRESULT hr;
    DWORD dwNumSessions;
    XNADDR xnaddr;
    INT iResult;
    WORD wMessage, wMe;
    Player *pPlayer;
    int chg;
    Packet packet;   
        
    m_fTime += m_fElapsedTime;    

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // our big state graph
    // you don't actually have to pay attention to any of this- the Marketplace itself is updated in
    // UpdateMarketplace().  This is all just framework and UI stuff

    switch ( m_State )
    {
    case STATE_ERROR:
        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
            m_State = m_ErrorNextState;
        break;
    
    case STATE_SELECT_ACCOUNT:
        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_iCurrentUISelection--;
        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_iCurrentUISelection++;

        m_iCurrentUISelection += m_UserAccountList.size();
        m_iCurrentUISelection %= m_UserAccountList.size();

        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
        {
            m_State = STATE_LOGGING_IN;
            m_iChosenAccount  = m_iCurrentUISelection;

            const DWORD Services[] = { XONLINE_MATCHMAKING_SERVICE };
            const DWORD dwNumServices = sizeof( Services ) / sizeof( Services[0] );    
            
             XONLINE_USER pUserList[ XGetPortCount() ] = { 0 };
             CopyMemory( &pUserList[ 0 ], &m_UserAccountList[ m_iCurrentUISelection ],
                            sizeof( XONLINE_USER ) );                        

            g_AudioMgr.StopXHV();
            HRESULT hr = XOnlineLogon( pUserList, Services, dwNumServices, NULL, &m_hOnlineTask );
            g_AudioMgr.StartXHV();
            if( FAILED(hr) )
            {
                XOnlineTaskClose( m_hOnlineTask );
                m_State = STATE_ERROR;
                m_ErrorNextState = STATE_SELECT_ACCOUNT;
                wcscpy( m_ErrorMsg, L"Error logging on" );
            }            
        }

        break;

    case STATE_LOGGING_IN:
        
        hr = XOnlineTaskContinue( m_hOnlineTask );
    
        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        {
                XOnlineTaskClose( m_hOnlineTask );
                m_State = STATE_SELECT_ACCOUNT;
                m_iCurrentUISelection = 0;
                m_iCurrentUIDisplay = 0;                                                       
        }


        if( hr != XONLINETASK_S_RUNNING )
        {
            if( hr != XONLINE_S_LOGON_CONNECTION_ESTABLISHED  )
            {
                XOnlineTaskClose( m_hOnlineTask );
                m_State = STATE_ERROR;
                m_iCurrentUISelection = 0;
                m_iCurrentUIDisplay = 0;                       
                m_ErrorNextState = STATE_SELECT_ACCOUNT;
                wcscpy( m_ErrorMsg, L"Can't connect to server" );
                break;
            }
            m_iCurrentUISelection = 0;
            m_State = STATE_SELECT_MODE;               
        }        
        break;

    case STATE_SELECT_MODE:
        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_iCurrentUISelection = m_iCurrentUISelection - 1;
        }

        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_iCurrentUISelection = m_iCurrentUISelection + 1;
        }

        m_iCurrentUISelection = ( m_iCurrentUISelection + 4 ) % 4;

        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
        {
            if ( m_iCurrentUISelection == 0 )            
            {
                m_State = STATE_HOST_GAME;
                m_bBots = true;
            }
            else if ( m_iCurrentUISelection == 1 )
            {
                m_State = STATE_HOST_GAME;
                m_bBots = false;
            }
            else if ( m_iCurrentUISelection == 2 )
            {
                hr = m_Query.Query();
                if (FAILED(hr))
                {
                    m_State = STATE_ERROR;
                    wcscpy( m_ErrorMsg, L"Couldn't create match query" );
                    m_ErrorNextState = STATE_ERROR;
                }
    
                m_State = STATE_FIND_HOSTS;
            }
            else
            {                
                m_iUICol = m_iUIRow = 0;
                g_AudioMgr.SetSimulationMode( TRUE );
                m_State = STATE_SOUNDBIT_EDITOR;
            }
        }
        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        {
            XOnlineTaskClose( m_hOnlineTask );
            m_iCurrentUISelection = 0;
            m_iCurrentUIDisplay = 0;
            m_State = STATE_SELECT_ACCOUNT;            
        }
        break;
    case STATE_FIND_HOSTS:

        // use the auto-generated match.cpp/.h to try and find a marketplace session to join
        // (quickmatch only)

        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        {
            m_Query.Cancel();
            m_State = STATE_SELECT_MODE;            
            break;
        }

        hr = XOnlineTaskContinue( m_hOnlineTask );
        if( FAILED( hr ) ) 
        {
            m_State = STATE_ERROR;
            m_ErrorNextState = STATE_SELECT_ACCOUNT;
            wcscpy( m_ErrorMsg, L"Online task quit" );
            break;
        }
        
        hr = m_Query.Process();
        if ( hr == XONLINETASK_S_RUNNING ) break;

        if ( FAILED( hr ))
        {
            m_State = STATE_ERROR;
            m_iCurrentUISelection = 0;
            m_ErrorNextState = STATE_SELECT_MODE;                    
            m_Query.Cancel();
            wcscpy( m_ErrorMsg, L"Error finding hosts" );
            break;
        }
       
        dwNumSessions = m_Query.Results.Size();
        if( dwNumSessions == 0 )
        {
            m_State = STATE_ERROR;
            m_iCurrentUISelection = 0;
            m_ErrorNextState = STATE_SELECT_MODE;            
            m_Query.Cancel();            
            wcscpy( m_ErrorMsg, L"No sessions found" );
            break;
        }
        
        m_SessionID      = m_Query.Results[0].SessionID;
        m_KeyExchangeKey = m_Query.Results[0].KeyExchangeKey;
        m_HostXnAddr     = m_Query.Results[0].HostAddress;
    
        if ( XNetRegisterKey( &m_SessionID, &m_KeyExchangeKey ) != NO_ERROR )
        {
            m_State = STATE_ERROR;
            m_ErrorNextState = STATE_ERROR;
            wcscpy( m_ErrorMsg, L"Couldn't register Key from Session- Fatal error" );
        }
        
        XNetXnAddrToInAddr( &m_HostXnAddr, &m_SessionID, &m_HostInAddr );
        
        // Normally we would set presence here, but that's not what this sample
        // is demonstrating.


        packet.ResetPacket();
        packet << (WORD)MSG_JOIN_GAME;
        packet << m_UserAccountList[ m_iChosenAccount ].xuid; 
        
        WCHAR wszDisplay[100];
        ZeroMemory( wszDisplay, 100 * sizeof(WCHAR) );
        swprintf( wszDisplay, L"%S", m_UserAccountList[ m_iChosenAccount ].szGamertag );        
        packet.WriteRawBytes( wszDisplay, XONLINE_GAMERTAG_SIZE * sizeof( WCHAR ) );
                
        m_SockAddr.sin_addr = m_HostInAddr;                
        packet.SendToSocket( &m_VDPSock, &m_SockAddr );
      
        //----------------------------
        
        m_State = STATE_JOINING_GAME;
        break;    

    case STATE_HOST_GAME:
        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        {        
            m_iCurrentUISelection = 0;
            m_State = STATE_SELECT_MODE;            
            break;
        }       
                
        m_Session.SetName(L"Market");
        m_Session.PublicOpen = 24;
        m_Session.PrivateOpen = 12;
        m_Session.PublicFilled = 12;
        m_Session.PrivateFilled = 6;
        m_Session.SessionID.ab[0] = 0xff;
        m_Session.KeyExchangeKey.ab[0] = 0xff;

        m_Session.Create();

        do
        {
            hr = XOnlineTaskContinue( m_hOnlineTask );            
            if (FAILED(hr))
            {
                m_State = STATE_ERROR;
                m_iCurrentUISelection = 0;
                m_ErrorNextState = STATE_SELECT_MODE;            
                wcscpy( m_ErrorMsg, L"Error creating session" );
                break;
            }
            hr = m_Session.Process();
        } while( hr == XONLINETASK_S_RUNNING );
    
        if( hr != XONLINETASK_S_SUCCESS )
        {
            m_State = STATE_ERROR;
            m_iCurrentUISelection = 0;
            m_ErrorNextState = STATE_SELECT_MODE;            
            wcscpy( m_ErrorMsg, L"Error creating session" );
            break;
        }
    
        m_SessionID = m_Session.SessionID;
        m_KeyExchangeKey = m_Session.KeyExchangeKey;
      
        XNetGetTitleXnAddr( &xnaddr );       

        pPlayer = g_PlayerMgr.CreateNewPlayer();       
        g_PlayerMgr.SetLocalPlayerLUID( pPlayer->LUID() );
        
        pPlayer->SetOnlineData( &xnaddr, NULL, m_UserAccountList[ m_iChosenAccount ].xuid );
        
        swprintf( wszDisplay, L"%S", m_UserAccountList[ m_iChosenAccount ].szGamertag );
        pPlayer->SetName( wszDisplay ); 
        
        // All players start in the same location, near the top of the screen in the middle

        pPlayer->SetPosition( D3DXVECTOR3( 50.0f, 20.0f, 0.0f ) );
    
        m_bHost = true;
        
        // create the bots, and set the voice to be routed to the host
        // (bots ignore voice, except by going quiet when someone is talking)

        if ( m_bBots )
            CreateBots( &xnaddr ); // they need an address

        g_AudioMgr.EnableAll();
        m_State = STATE_PLAYING_GAME;
        break;

    case STATE_JOINING_GAME:                      
        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        {        
            m_Query.Cancel();
            m_iCurrentUISelection = 0;
            m_State = STATE_SELECT_MODE;            
            break;
        }
    
        // get packets, looking for the MSG_JOIN_OK one
        do
        {
            packet.ResetPacket();
                      
            iResult = packet.ReceiveFromSocket( &m_VDPSock, &m_RecvSockAddr ); 

            // If message waiting, process it
            if( iResult != SOCKET_ERROR && iResult > 0 )
            {
                packet >> wMessage;
                switch (wMessage)
                {
                case MSG_JOIN_OK:               
                    m_State = STATE_PLAYING_GAME;
                    packet >> wMe;                   
                    
                    g_PlayerMgr.ReadPlayerListFull( packet );
                    g_PlayerMgr.SetLocalPlayerLUID( wMe );
                   
                    packet.ReadRawBytes( &m_HostProps, sizeof(HostProperties) );
                    
                    g_AudioMgr.EnableAll();

                    break;
                }
            }        
        } while( iResult != SOCKET_ERROR && iResult > 0 );                     

        break;
      
    case STATE_PLAYING_GAME:
        m_iCurrentUISelection = 0;
        UpdateMarketplace();       
        break;

    case STATE_HOST_MENU:      

        // we still have to update things while in the host menu
        g_VoiceMgr.Update();
        UpdateNetwork();    
        UpdateSounds();       

        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_iCurrentUISelection--;
        if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_iCurrentUISelection++;

        m_iCurrentUISelection += NUM_HOST_PROPS;
        m_iCurrentUISelection %= NUM_HOST_PROPS;

        chg = 0;
        if ( m_bHost )
        {
            if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT ) chg = -1;
            else if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) chg = 1;
        }

        if ( chg ) 
        {
            switch( m_iCurrentUISelection )
            {
            case 0:
                m_HostProps.byVoiceMode = 1 - m_HostProps.byVoiceMode;
        
                if ( m_HostProps.byVoiceMode == VM_PEER_TO_PEER )
                {
                    m_HostProps.byNumClientChannelsIn = 4;
                    m_HostProps.byNumClientChannelsOut = 4;
                }
                else if ( m_HostProps.byVoiceMode == VM_SERVER_FORWARD )
                {
                    m_HostProps.byNumClientChannelsIn = 0;
                    m_HostProps.byNumClientChannelsOut = 0;
                }

                break;
            case 1:
                if ( ( m_HostProps.byNetworkUpdateRate < 255 ) && ( chg == 1 ) )
                    m_HostProps.byNetworkUpdateRate ++;
                else if ( ( m_HostProps.byNetworkUpdateRate > 1 ) && ( chg == -1 ) )
                    m_HostProps.byNetworkUpdateRate --;

                break;
            case 2:
                if ( ( m_HostProps.byVoiceUpdateRate < 255 ) && ( chg == 1 ) )
                    m_HostProps.byVoiceUpdateRate ++;
                else if ( ( m_HostProps.byVoiceUpdateRate > 1 ) && ( chg == -1 ) )
                    m_HostProps.byVoiceUpdateRate --;
                break;
            case 3:
                if ( ( m_HostProps.bySpeechTimeout < 255 ) && ( chg == 1 ) )
                    m_HostProps.bySpeechTimeout ++;
                else if ( ( m_HostProps.bySpeechTimeout > 1 ) && ( chg == -1 ) )
                    m_HostProps.bySpeechTimeout --;
                break;  
            case 4:
                if ( ( m_HostProps.byNumClientChannelsIn < MAX_CHANNELS ) && ( chg == 1 ) )
                    m_HostProps.byNumClientChannelsIn ++;
                if ( ( m_HostProps.byNumClientChannelsIn > 0 ) && ( chg == -1 ) )
                    m_HostProps.byNumClientChannelsIn --;
                break;
            case 5:
                if ( ( m_HostProps.byNumClientChannelsOut < MAX_CHANNELS ) && ( chg == 1 ) )
                    m_HostProps.byNumClientChannelsOut ++;
                if ( ( m_HostProps.byNumClientChannelsOut > 0 ) && ( chg == -1 ) )
                    m_HostProps.byNumClientChannelsOut --;
                break;
            }
        }

        if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] || m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ])
        {
            if (m_bHost)        
                m_dwServerFlags |= SF_SERVER_PROPS_CHANGED;
            m_State = STATE_PLAYING_GAME;       
        }   
        break;

    case STATE_SOUNDBIT_EDITOR:
        UpdateSoundbitEditor();
        break;
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Marketplace::Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT Marketplace::Render()
{
    unsigned int i;
    WCHAR wszDisplay[100];

    // Draw a gradient filled background and clear the zbuffer
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // State tree for rendering- the marketplace itself when you are in a session 
    // is rendered in drawmarketplace
    
    switch ( m_State )
    {
    case STATE_ERROR:
        m_Font.DrawText( 160, 200, 0xffffffff, m_ErrorMsg );
        if ( m_ErrorNextState != STATE_ERROR )
            m_Font.DrawText( 180, 240, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to continue" );
        else
            m_Font.DrawText( 180, 240, 0xffffffff, L"Reset and fix problem to continue" );
        break;

    case STATE_SELECT_ACCOUNT:
        unsigned int iEnd;
        
        if ( ( m_iCurrentUISelection >= ( m_iCurrentUIDisplay + ACCOUNTS_PER_SCREEN ) ) ||
            ( m_iCurrentUISelection < m_iCurrentUIDisplay) )
        {
        
            if ( m_iCurrentUISelection < m_UserAccountList.size() - ACCOUNTS_PER_SCREEN )
                m_iCurrentUIDisplay = m_iCurrentUISelection;
            else if ( m_UserAccountList.size() > ACCOUNTS_PER_SCREEN )
                m_iCurrentUIDisplay = m_UserAccountList.size() - ACCOUNTS_PER_SCREEN;
            else
                m_iCurrentUIDisplay = 0;
        }

        iEnd = m_iCurrentUIDisplay + ACCOUNTS_PER_SCREEN;
        if ( iEnd > m_UserAccountList.size() ) iEnd = m_UserAccountList.size();

        m_Font.Begin();
        m_Font.DrawText( 150.0f, 200.0f - 24 * 2, 0xffffffff, L"Select an account:" );
        for( i = m_iCurrentUIDisplay; i < iEnd; i++ )
        {
            swprintf( wszDisplay, L"%S", m_UserAccountList[ i ].szGamertag );
            m_Font.DrawText( 180.0f, 200.0f + 24 * ( i - m_iCurrentUIDisplay ), 0xffffffff, wszDisplay );
        }
        m_Font.DrawText( 150.0f, 200.0f + 24 * (m_iCurrentUISelection - m_iCurrentUIDisplay), 0xffffff00, GLYPH_RIGHT_TICK);
        
        m_Font.End();
        break;
    case STATE_LOGGING_IN:
        m_Font.DrawText( 160.0f, 200.0f, 0xffffffff, L"Logging in.." );
        m_Font.DrawText( 150.0f, 248.0f, 0xffffffff, L"Press " GLYPH_B_BUTTON L" to cancel" );
        break;
    case STATE_SELECT_MODE:
        m_Font.DrawText( 150.0f, 152.0f, 0xffffffff, L"Select a mode:" );
        m_Font.DrawText( 180.0f, 200.0f, 0xffffffff, L"Host new marketplace (bots)" );
        m_Font.DrawText( 180.0f, 232.0f, 0xffffffff, L"Host new marketplace (no bots)" );
        m_Font.DrawText( 180.0f, 264.0f, 0xffffffff, L"Join existing marketplace" );
        m_Font.DrawText( 180.0f, 296.0f, 0xffffffff, L"Edit bot voices");
        m_Font.DrawText( 150.0f, 200.0f + 32 * m_iCurrentUISelection, 0xffffff00, GLYPH_RIGHT_TICK );
        break;
    case STATE_FIND_HOSTS:
        m_Font.DrawText( 160.0f, 200.0f, 0xffffffff, L"Searching for a marketplace..." );
        m_Font.DrawText( 150.0f, 248.0f, 0xffffffff, L"Press " GLYPH_B_BUTTON L" to cancel" );
        break;
    case STATE_JOINING_GAME:
        m_Font.DrawText( 160.0f, 200.0f, 0xffffffff, L"Attempting to join..." );
        m_Font.DrawText( 150.0f, 248.0f, 0xffffffff, L"Press " GLYPH_B_BUTTON L" to cancel" );
        break;
    case STATE_HOST_GAME:
        m_Font.DrawText( 160.0f, 200.0f, 0xffffffff, L"Registering session..." );
        m_Font.DrawText( 150.0f, 248.0f, 0xffffffff, L"Press " GLYPH_B_BUTTON L" to cancel" );
        break;
    case STATE_PLAYING_GAME:
        DrawMarketplace();
        break;
    case STATE_HOST_MENU:
        m_Font.DrawText( 150.0f, 152.0f, 0xffffffff, L"Host settings:" );
        m_Font.DrawText( 180.0f, 200.0f, 0xffffffff, L"Voice mode:" );
        m_Font.DrawText( 180.0f, 224.0f, 0xffffffff, L"Network rate:" );
        m_Font.DrawText( 180.0f, 248.0f, 0xffffffff, L"Voice rate:" );
        m_Font.DrawText( 180.0f, 272.0f, 0xffffffff, L"Speech timeout:" );      
        m_Font.DrawText( 180.0f, 296.0f, 0xffffffff, L"Downstream peers:" );
        m_Font.DrawText( 180.0f, 320.0f, 0xffffffff, L"Upstream peers:" );

        m_Font.DrawText( 150.0f, 200.0f + 24 * m_iCurrentUISelection, 0xffffff00, GLYPH_RIGHT_TICK );
        
        if ( m_HostProps.byVoiceMode == VM_PEER_TO_PEER )
            m_Font.DrawText( 400.0f, 200.0f, 0xffa0a010, L"Peer to peer");
        else
            m_Font.DrawText( 400.0f, 200.0f, 0xffa0a010, L"Server forward");
        
        swprintf( wszDisplay, L"%d ms", 10 * m_HostProps.byNetworkUpdateRate );
        m_Font.DrawText( 400.0f, 224.0f, 0xffa0a010, wszDisplay );
        swprintf( wszDisplay, L"%d ms", 10 * m_HostProps.byVoiceUpdateRate );
        m_Font.DrawText( 400.0f, 248.0f, 0xffa0a010, wszDisplay );
        swprintf( wszDisplay, L"%2.1f s", 0.1f * m_HostProps.bySpeechTimeout );
        m_Font.DrawText( 400.0f, 272.0f, 0xffa0a010, wszDisplay );
        swprintf( wszDisplay, L"%d", m_HostProps.byNumClientChannelsIn );
        m_Font.DrawText( 400.0f, 296.0f, 0xffa0a010, wszDisplay );
        swprintf( wszDisplay, L"%d", m_HostProps.byNumClientChannelsOut );
        m_Font.DrawText( 400.0f, 320.0f, 0xffa0a010, wszDisplay );

        break;
    case STATE_SOUNDBIT_EDITOR:
        DrawSoundbitEditor();
        break;
    }
    
       
    // Show title, frame rate, and help
    
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    if( m_bDrawHelp )
    {
        if ( m_bHost )
            m_Help.Render( &m_Font, g_HostHelpCallouts, NUM_HOST_HELP_CALLOUTS );
        else
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    
    m_Font.DrawText( 48, 36, 0xffffffff, L"Marketplace" );           

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//------------------------------------------------------------------------------
// Name: Marketplace::DrawMarketplace()
// Desc: renders the marketplace when you are in a game session
//------------------------------------------------------------------------------
void Marketplace::DrawMarketplace()
{
    m_SpriteDraw.DrawSprite( MARKET_X, MARKET_Y, MARKET_SCALE, 0xffffffff, GFX_MARKETPLACE);

    DisplayedObject *pObj;
    for ( pObj = g_IndexObject.Next(); pObj != &g_IndexObject; pObj = pObj->Next() )
    {
        pObj->Render();
    }  
    
    if ( m_bShowStats )
    {
        StatsDraw();
    }

    m_Font.Begin();

    if ( !m_bShowStats )  
    {
        if (g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_ONPRIVATECHANNEL ) )
        {
            m_Font.DrawText( 50.0f, 400.0f, 0xffffffff, GLYPH_X_BUTTON L" Leave private channel" );
            m_Font.DrawText( 320.0f, 400.0f, 0xffffffff, GLYPH_A_BUTTON L" Hold to talk on channel" );    
        }
        else
            m_Font.DrawText( 50.0f, 400.0f, 0xffffffff, GLYPH_X_BUTTON L" Join private channel" );       
    }
   
    m_Font.End();
}




//------------------------------------------------------------------------------
// Name: Marketplace::DrawScaledGlyph()
// Desc: draws text, scaled to the marketplace (x, z shrink as you move back in y)
//------------------------------------------------------------------------------
void Marketplace::DrawScaledGlyph( float fX, float fY, float fZ, float fS, WCHAR *pString, DWORD dwColor )
{
    float ffX, ffY;
    ffX = fX / MAX_COORD_X;
    ffY = fY / MAX_COORD_Y;

    const float fX1 = MARKET_X + MARKET_SCALE * 63.0f;
    const float fX2 = MARKET_X + MARKET_SCALE * 703.0f;
    const float fY1 = MARKET_Y + MARKET_SCALE * 84.0f;
    const float fY2 = MARKET_Y + MARKET_SCALE * 485.0f;
    const float cX = MARKET_X + MARKET_SCALE * 382.0f;
    
    float fScale = 1.0f / (1.45f - 0.45f * ffY);

    float fRy = (ffY * fScale) * (fY2 - fY1) + fY1 + fZ * fScale;
    float fRx = (ffX - 0.5f) * fScale * (fX2 - fX1) + cX;
    m_IconFont.SetScaleFactors( fS, fS );
    m_IconFont.DrawText( fRx, fRy, dwColor, pString, XBFONT_CENTER_X); 
    m_IconFont.SetScaleFactors( 1.0f, 1.0f );
}




//------------------------------------------------------------------------------
// Name: Marketplace::DrawScaledObject()
// Desc: draws a sprite, scaled to the marketplace (x, z shrink as you move back in y)
//------------------------------------------------------------------------------
void Marketplace::DrawScaledObject( D3DXVECTOR3 &vP, DWORD dwColor, int idx )
{
    float ffX, ffY;
    ffX = vP.x / MAX_COORD_X;
    ffY = vP.y / MAX_COORD_Y;

    const float fX1 = MARKET_X + MARKET_SCALE * 63.0f;
    const float fX2 = MARKET_X + MARKET_SCALE * 703.0f;
    const float fY1 = MARKET_Y + MARKET_SCALE * 84.0f;
    const float fY2 = MARKET_Y + MARKET_SCALE * 485.0f;
    const float cX = MARKET_X + MARKET_SCALE * 382.0f;
    
    float fScale = 1.0f / (1.45f - 0.45f * ffY);

    float fRy = (ffY * fScale) * (fY2 - fY1) + fY1;
    float fRx = (ffX - 0.5f) * fScale * (fX2 - fX1) + cX;

    m_SpriteDraw.DrawSprite( fRx, fRy, fScale * MARKET_SCALE, dwColor, idx );
}




//------------------------------------------------------------------------------
// Name: Marketplace::UpdateMarketplace()
// Desc: Updates the marketplace during a session
//------------------------------------------------------------------------------
VOID Marketplace::UpdateMarketplace()
{   
    // update all of the players
    g_PlayerMgr.Update( m_fElapsedTime );

    // update the voice queue - this does not generate network traffic
    g_VoiceMgr.Update();

    // update the network ( this will generate voice network traffic )
    UpdateNetwork();    

    // update the sounds (xact, xhv, etc)
    UpdateSounds();    
    
    if (( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] ) && m_bHost ) 
        m_State = STATE_HOST_MENU;

    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] ) 
    {
        if (g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_ONPRIVATECHANNEL ) ) 
            g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_ONPRIVATECHANNEL );
        else
            g_PlayerMgr.GetLocalPlayer()->SetPlayerFlag( PLAYERFLAG_ONPRIVATECHANNEL );
    }     
    
    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] ) 
    {
        m_bShowStats = !m_bShowStats;
        StatsReset();
    }

    if ( m_DefaultGamepad.bLastAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )    
        m_bShowNames = true;        
    else
        m_bShowNames = false;
}




//------------------------------------------------------------------------------
// Name: Marketplace::UpdateNetwork()
// Desc: Reads network packets, sends out network updates during a session
//------------------------------------------------------------------------------
void Marketplace::UpdateNetwork()
{
    // network update
    INT i, j;
    FLOAT fNetworkRate, fVoiceRate; 
    Packet packet, plrpacket;
    WORD wLocalUserID, wFromID;
    DWORD dwVoiceBytesSent;
    D3DXVECTOR3 vDist;

    // if we are the host, we have to make sure to process the match session
    // so people can join
    if ( m_bHost )
        m_Session.Process();

    m_fLastNetworkUpdate += m_fElapsedTime;
    m_fLastVoiceUpdate += m_fElapsedTime;

    fNetworkRate = ((float)m_HostProps.byNetworkUpdateRate) / 100.0f;
    fVoiceRate = ((float)m_HostProps.byVoiceUpdateRate) / 100.0f ;
       
    g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKPODIUM );    
    g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKPRIVATE );    
    
    // This manages whether PLAYERFLAG_TALKING is set for the local player
    // If they are talking on the podium or on the private channel,
    // PLAYERFLAG_TALKPODIUM and PLAYERFLAG_ONPRIVATECHANNEL will also be set,
    // respectively.

    // Check and see if we've talked within m_HostProps.bySpeechTimeout 10ths of seconds.
    if ( (m_fTime -  m_fLastLocalSpeechTime) < ((float)m_HostProps.bySpeechTimeout) / 10.0f )
    {        
        g_PlayerMgr.GetLocalPlayer()->SetPlayerFlag( PLAYERFLAG_TALKING );
      
        // if we are within PODIUM_RADIUS (squared units) of PODIUM_LOCATION,
        // we are talking on the podium
        
        vDist = g_PlayerMgr.GetLocalPlayer()->Position() - PODIUM_LOCATION;

        if ( D3DXVec3LengthSq( &vDist ) < PODIUM_RADIUS )            
        {
            g_PlayerMgr.GetLocalPlayer()->SetPlayerFlag( PLAYERFLAG_TALKPODIUM );                        
        }
        else
        {
            g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKPODIUM );                
        }
        
        // if we're pressing A and are on the private channel, we're talking on the private channel
        if ( g_PlayerMgr.GetLocalPlayer()->IsPlayerFlagSet( PLAYERFLAG_ONPRIVATECHANNEL ) )
        {
            if ( m_DefaultGamepad.bLastAnalogButtons[ XINPUT_GAMEPAD_A ] )
            {
                g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKPODIUM );
                g_PlayerMgr.GetLocalPlayer()->SetPlayerFlag( PLAYERFLAG_TALKPRIVATE );
            }
            else
                g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKPRIVATE );        
        }        
    }
    else
        g_PlayerMgr.GetLocalPlayer()->ClearPlayerFlag( PLAYERFLAG_TALKING );
        

    // This decouples network traffic from framerate-
    // We send updates every fNetworkRate seconds ( the hostproperty is in 100ths of a second)

    if ( m_fLastNetworkUpdate > fNetworkRate )
    {
        m_fLastNetworkUpdate -= fNetworkRate;        

        if (!m_bHost)
        {
            // if we are not the host, we send just our data to the host

            packet.ResetPacket();
            packet << (WORD)MSG_UPDATE_PLAYER;
            packet << g_PlayerMgr.GetLocalPlayer()->LUID();
            
            // write the player update packet

            g_PlayerMgr.GetLocalPlayer()->WritePlayerUpdate( packet );

            // we append voice data if we can, to make things more efficient
            
            dwVoiceBytesSent = g_VoiceMgr.AppendVoiceDataForHost( packet );
            StatsSendVoiceData( dwVoiceBytesSent );
                        
            m_SockAddr.sin_addr = m_HostInAddr;              
            packet.SendToSocket( &m_VDPSock, &m_SockAddr );
        }
        else
        {                        
            packet.ResetPacket();            
            for ( i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
            {
                if ( g_PlayerMgr.PlayerFromIndex( i )->IsLocal() ) continue;

                // if a player has timed out, we send a remove_player message
                // to all the other players                 

                if ( ( m_fAppTime - g_PlayerMgr.PlayerFromIndex( i )->LastMessageTime() > PLAYER_TIMEOUT ) ||
                     ( XNetGetConnectStatus( g_PlayerMgr.PlayerFromIndex( i )->InAddr() ) == XNET_CONNECT_STATUS_LOST ) )
                {
                    wFromID = g_PlayerMgr.PlayerFromIndex( i )->LUID();
                    g_PlayerMgr.RemovePlayer( wFromID );

                    packet << (WORD) MSG_REMOVE_PLAYER;
                    packet << wFromID;
                        
                    for ( j = 0; j < g_PlayerMgr.NumPlayers(); j++ )
                    {
                        if ( g_PlayerMgr.PlayerFromIndex( j )->IsLocal() ) continue;

                        m_SockAddr.sin_addr = g_PlayerMgr.PlayerFromIndex( j )->InAddr();                        
                        packet.SendToSocket( &m_VDPSock, &m_SockAddr );                    
                    }                    
                    i--; // test the new player in this position
                }
            }

       
            // we need to send the all-players update packet to each client
            // first we build the generic part of it

            packet.ResetPacket();
            packet << (WORD)MSG_UPDATE_ALLPLAYERS;
            packet << (DWORD)m_dwServerFlags;
            g_PlayerMgr.WritePlayerListUpdate( packet );

            if ( m_dwServerFlags & SF_SERVER_PROPS_CHANGED )
                packet.WriteRawBytes( &m_HostProps, sizeof( HostProperties ) );            
            m_dwServerFlags = 0;

            // now we loop through, and add on specific voice for each individual player
            // if there is any in our queue

            for ( i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
            {
                plrpacket = packet;
                            
                // if it's a bot or us, we don't generate traffic
                if ( g_PlayerMgr.PlayerFromIndex( i )->IsLocal() ) continue;
                
                dwVoiceBytesSent = g_VoiceMgr.AppendVoiceDataForPlayer( plrpacket, g_PlayerMgr.PlayerFromIndex( i )->LUID() );
                StatsSendVoiceData( dwVoiceBytesSent );

                m_SockAddr.sin_addr = g_PlayerMgr.PlayerFromIndex( i )->InAddr();                        
                plrpacket.SendToSocket( &m_VDPSock, &m_SockAddr );
            }            
        }
        
    }
    
    INT iResult;
    WORD wMessage;
    XUID xuid;
    IN_ADDR inaddr;
    WCHAR nameBuf[ XONLINE_GAMERTAG_SIZE ];
    Player *pPlayer;
    DWORD dwServerFlags;
    WORD wBytes;


    // Every frame we DO check for incoming network messages to process them as fast
    // as possible- this is the message pump

    do
    {
        packet.ResetPacket();

        iResult = packet.ReceiveFromSocket( &m_VDPSock, &m_RecvSockAddr );
        inaddr = m_RecvSockAddr.sin_addr;

        // If message waiting, process it
        if( iResult != SOCKET_ERROR && iResult > 0 )
        {
            packet >> wMessage;
            switch (wMessage)
            {            
            
            //-----------------------------------------------------------
            // MSG_JOIN_GAME is a request to join the game
            // normally if the game is full you send a REJECT_JOIN type of
            // message, but that's not what this sample is demonstrating

            case MSG_JOIN_GAME:      
                if (!m_bHost) break;

                pPlayer = g_PlayerMgr.CreateNewPlayer();       
                if ( !pPlayer ) return;

                packet >> xuid;
                packet.ReadRawBytes( nameBuf, XONLINE_GAMERTAG_SIZE * sizeof(WCHAR) );

                pPlayer->SetName( nameBuf );                              
                pPlayer->SetOnlineData( NULL, &inaddr, xuid );
                
                // All players start in the same location, in the middle of the market near the top

                pPlayer->SetPosition( D3DXVECTOR3( 50.0f, 20.0f, 0.0f ) );  
                pPlayer->SetLastMessageTime( m_fAppTime );
                
                // Send Response packet

                packet.ResetPacket();
                packet << (WORD)MSG_JOIN_OK;
                packet << pPlayer->LUID();
                
                g_PlayerMgr.WritePlayerListFull( packet );
                
                packet.WriteRawBytes( &m_HostProps, sizeof(HostProperties) );
                
                m_SockAddr.sin_addr = pPlayer->InAddr();                
                packet.SendToSocket( &m_VDPSock, &m_SockAddr );

                // play a chime on the host
                m_pDoorbell->Enable();

                // notify the rest of the palyers                
                packet.ResetPacket();
                packet << (WORD)MSG_ADD_PLAYER;
                packet << pPlayer->LUID();
                pPlayer->WritePlayerFull( packet );
                
                // send a MSG_ADD_PLAYER to all players

                for ( i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
                {                    
                    if ( g_PlayerMgr.PlayerFromIndex( i )->IsLocal() ||
                         ( g_PlayerMgr.PlayerFromIndex( i ) == pPlayer ) ) continue;
                    m_SockAddr.sin_addr = g_PlayerMgr.PlayerFromIndex( i )->InAddr();   
                    packet.SendToSocket( &m_VDPSock, &m_SockAddr );
                }            

                break;

            //-----------------------------------------------------------
            // MSG_UPDATE_PLAYER is received by the host- it's the changes
            // from a single players moves, etc

            case MSG_UPDATE_PLAYER:
                if (!m_bHost) break;

                packet >> wLocalUserID;
                pPlayer = g_PlayerMgr.PlayerFromLUID(wLocalUserID);
                if (!pPlayer) break; // ignore dead players

                pPlayer->SetLastMessageTime( m_fAppTime );
                if (!pPlayer) 
                    OutputDebugStringA("Received an update from a non-existant player\n");
                else
                    pPlayer->ReadPlayerUpdate( packet );

                // if the player attached voice data, make sure we read it
                wBytes = g_VoiceMgr.ReadVoiceData( packet );
                StatsRecvVoiceData( wBytes );

                break;

            //-----------------------------------------------------------
            // MSG_UPDATE_ALLPLAYERS is sent by the host
            // It contains the location, facing, and flags for every player            

            case MSG_UPDATE_ALLPLAYERS:         
                if (m_bHost) break;             
       
                packet >> dwServerFlags;
                g_PlayerMgr.ReadPlayerListUpdate( packet );
                
                // if the server properties have changed, read in the structure
                if ( dwServerFlags & SF_SERVER_PROPS_CHANGED )
                   packet.ReadRawBytes( &m_HostProps, sizeof(HostProperties) );     

                // if voice was attached, process it
                wBytes = g_VoiceMgr.ReadVoiceData( packet );
                StatsRecvVoiceData( wBytes );

                break;
            
            //-----------------------------------------------------------
            // MSG_ADD_PLAYER is sent by the host
            // it contains the info for a new player joining the game

            case MSG_ADD_PLAYER:
                if (m_bHost) break;

                m_pDoorbell->Enable();

                packet >> wFromID;
                g_PlayerMgr.CreateNewPlayerWithLUID( wFromID )->ReadPlayerFull( packet );
                break;

            //-----------------------------------------------------------
            // MSG_VOICE contains voice data only
            // it's used for all peer-to-peer voice traffic
            
            case MSG_VOICE:                   
                wBytes = g_VoiceMgr.ReadVoiceData( packet );   
                
                // don't let network stats be affected by local chatter in peer-to-peer mode
                // as host (ReadVoiceData will set this to 1 for us, we just need to not
                // add the full overhead )

                if ( wBytes > 0 )
                    StatsRecvVoiceData( FULL_PACKET_OVERHEAD + wBytes );
                break;

                
            //-----------------------------------------------------------
            // MSG_REMOVE_PLAYER notifies clients when a player leaves
            // due to timeout

            case MSG_REMOVE_PLAYER:
                if (m_bHost) break;

                packet >> wFromID;
                g_PlayerMgr.RemovePlayer( wFromID );
                break;
            }
        }        
    } while( iResult != SOCKET_ERROR && iResult > 0 );


    // See if we need to do a network run       
    
    if ( m_fLastVoiceUpdate > fVoiceRate ) 
    {
        m_fLastVoiceUpdate -= fVoiceRate;
        
        
        // normally you'd only rebuild routes after someone changed 
        // whether they were talking or not, but since we display
        // a constant indication of who you'd talk to (glowing people)
        // we mark for rebuild every frame
        g_VoiceMgr.MarkForRebuild();
        
        // send network voice
        g_VoiceMgr.SendNetworkVoice();    
        
        StatsAdvanceFrame();    
    }    
}





//------------------------------------------------------------------------------
// Name: Marketplace::SendToPlayer()
// Desc: Sends a packet to a specific player
//------------------------------------------------------------------------------
VOID Marketplace::SendToPlayer( Packet &p, WORD wLUID )
{
    Player *pPlayer;

    pPlayer = g_PlayerMgr.PlayerFromLUID( wLUID );
        
    assert(pPlayer);

    m_SockAddr.sin_addr = pPlayer->InAddr();
    p.SendToSocket( &m_VDPSock, &m_SockAddr );
}




//------------------------------------------------------------------------------
// Name: Marketplace::SendToHost()
// Desc: Sends a packet to the host
//------------------------------------------------------------------------------
VOID Marketplace::SendToHost( Packet &p )
{
    m_SockAddr.sin_addr = m_HostInAddr;
    p.SendToSocket( &m_VDPSock, &m_SockAddr );
}





//------------------------------------------------------------------------------
// Name: Marketplace::HandleCollisions()
// Desc: Simple bounding-circle based collisions
//------------------------------------------------------------------------------
void Marketplace::HandleCollisions( DisplayedObject *pObj, D3DXVECTOR3 *vModifyPos )
{
    DisplayedObject *pSrch;
    int ItersLeft;
    bool bColl;
    float fDist, fRad;
    D3DXVECTOR3 vPush, vPos;

    // go through several iterations, and move the players away from possible
    // collision objects

    for( ItersLeft = COLLISION_ITERATIONS; ItersLeft > 0; ItersLeft-- )
    {
        bColl = 0;
        for( pSrch = g_IndexObject.Next(); pSrch != &g_IndexObject; pSrch = pSrch->Next())
        {
            if (pSrch == pObj) continue;

            vPos = pSrch->GetPosition();

            fDist = ( vPos.x - vModifyPos->x ) *
                    ( vPos.x - vModifyPos->x ) +
                    ( vPos.y - vModifyPos->y ) *
                    ( vPos.y - vModifyPos->y );
            
            if (fDist == 0.0f) continue;
            
            fRad = ( pSrch->GetRadius() + pObj->GetRadius() );
            if( fDist < fRad * fRad )
            {
                bColl = false;
                vPush = *vModifyPos - pSrch->GetPosition();
                *vModifyPos += vPush * ( fRad / sqrtf( fDist ) - 1.0f );    
            }
        }

        if ( vModifyPos->x < pObj->GetRadius() )
            vModifyPos->x = pObj->GetRadius();
        if ( vModifyPos->x > MAX_COORD_X - pObj->GetRadius() )
            vModifyPos->x = MAX_COORD_X - pObj->GetRadius();
        if ( vModifyPos->y < pObj->GetRadius() )
            vModifyPos->y = pObj->GetRadius();
        if ( vModifyPos->y > MAX_COORD_Y - pObj->GetRadius() )
            vModifyPos->y = MAX_COORD_Y - pObj->GetRadius();
        if (!bColl) break;
    }
}





//------------------------------------------------------------------------------
// Name: Marketplace::GetDefaultGamepad()
// Desc: Returns the default gamepad
//------------------------------------------------------------------------------
XBGAMEPAD *Marketplace::GetDefaultGamepad()
{ 
    return &m_DefaultGamepad; 
}




//------------------------------------------------------------------------------
// Name: Marketplace::UpdateSounds()
// Desc: Updates all of the audio in a session
//------------------------------------------------------------------------------
void Marketplace::UpdateSounds()
{
    Player *pPlayer = g_PlayerMgr.GetLocalPlayer();
    int ttlTalking;

    ttlTalking = 0;

    // count the number of talking players
    for ( int i = 0; i < g_PlayerMgr.NumPlayers(); i++ )
    {
        ttlTalking += g_PlayerMgr.PlayerFromIndex( i )->IsPlayerFlagSet( PLAYERFLAG_TALKING );
    }
    
    if ( ttlTalking > 8 ) ttlTalking = 8;

    // adjust the crowd sound volume by how many people are talking at the moment
    m_pCrowdAmbient->SetVolume( -22.0f + (float)ttlTalking );

    g_SoundbitDB.Update();    
    g_AudioMgr.Update( m_fElapsedTime, pPlayer->Position(), pPlayer->Facing() );
}





//------------------------------------------------------------------------------
// Name: Marketplace::DrawSoundbitEditor()
// Desc: Renders the soundbit editor (the soundbit editor is a way to record
//       simulated voice for bots)
//------------------------------------------------------------------------------
VOID Marketplace::DrawSoundbitEditor()
{
    int i, j;
    WCHAR buf[100];

    m_Font.Begin();

    m_Font.DrawText( 80.0f,  80.0f, 0xffffffff, GLYPH_A_BUTTON L" Play" );
    m_Font.DrawText( 210.0f, 80.0f, 0xffffffff, GLYPH_X_BUTTON L" Record" );
    m_Font.DrawText( 340.0f, 80.0f, 0xffffffff, GLYPH_Y_BUTTON L" Save" );
    m_Font.DrawText( 470.0f, 80.0f, 0xffffffff, GLYPH_B_BUTTON L" Cancel" );

    for (i = 0; i < SOUNDBIT_ROWS; i++)
        for (j = 0; j < SOUNDBIT_COLS; j++)
        {
            swprintf(buf, L"%C%1d", i + 'A', j );
            if ( ( g_SoundbitDB.CurSample() == j ) && ( g_SoundbitDB.CurVoice() == i ) &&
                ( g_SoundbitDB.State() == SDB_STATE_RECORDING ) )
            {
                m_Font.DrawText( 100.0f + 50.0f * j, 130.0f + 40.0f * i, 0xffff0000, buf );
            }
            else if ( ( g_SoundbitDB.CurSample() == j ) && ( g_SoundbitDB.CurVoice() == i ) &&
                ( g_SoundbitDB.State() == SDB_STATE_PLAYING ) )
            {
                m_Font.DrawText( 100.0f + 50.0f * j, 130.0f + 40.0f * i, 0xff00ff00, buf );
            }
            else if ( g_SoundbitDB.NumPackets( i, j ) == 0)
                m_Font.DrawText( 100.0f + 50.0f * j, 130.0f + 40.0f * i, 0xffffffff, buf );            
            else
                m_Font.DrawText( 100.0f + 50.0f * j, 130.0f + 40.0f * i, 0xffffff00, buf );
        }

    m_Font.DrawText( 80.0f + 50.0f * m_iUICol, 130.0f + 40.0f * m_iUIRow, 0xffffff00, GLYPH_RIGHT_TICK );

    m_Font.End();
}




//------------------------------------------------------------------------------
// Name: Marketplace::UpdateSoundbitEditor()
// Desc: Updates the soundbit editor (the soundbit editor is a way to record
//       simulated voice for bots)
//------------------------------------------------------------------------------
VOID Marketplace::UpdateSoundbitEditor()
{
    WORD wState;

    g_SoundbitDB.Update();
    g_AudioMgr.Update( m_fElapsedTime );
    
    if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )   m_iUICol--;
    if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )  m_iUICol++;
    if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )     m_iUIRow--;
    if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )   m_iUIRow++;

    m_iUICol = ( m_iUICol + SOUNDBIT_COLS ) % SOUNDBIT_COLS;
    m_iUIRow = ( m_iUIRow + SOUNDBIT_ROWS ) % SOUNDBIT_ROWS;

    wState = g_SoundbitDB.State();

    if ( wState == SDB_STATE_RECORDING )
    {
        g_SoundbitDB.EditSubmitSpeechPacketsToSoundbit( g_VoiceMgr.GetLocalVoiceData().bVoiceData, 
                                                        g_VoiceMgr.GetLocalVoiceData().nPackets );
    }
    
    // clear out any speech that has come in (since we're not recording, or done processing) 
    g_VoiceMgr.GetLocalVoiceData().nPackets = 0;
    
    if ( wState == SDB_STATE_PLAYING )  
    {
        if (! g_SoundbitDB.IsPlaying( g_AudioMgr.SimulationXUID() ) )
            wState = SDB_STATE_READY;
    } 
     
    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        g_SoundbitDB.EditPlaySoundbit( m_iUIRow, m_iUICol );
    }

    if ( ( wState != SDB_STATE_RECORDING )&&
         ( m_DefaultGamepad.bLastAnalogButtons[ XINPUT_GAMEPAD_X ] ) )
    {
        g_SoundbitDB.EditRecordSoundbit( m_iUIRow, m_iUICol );
    }
    else if ( ( wState == SDB_STATE_RECORDING )&&
         (! m_DefaultGamepad.bLastAnalogButtons[ XINPUT_GAMEPAD_X ] ) )
    {
        g_SoundbitDB.EditStop();
    }

    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
        g_SoundbitDB.WriteToFile("D:\\media\\marketplace.sdb");

    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        g_SoundbitDB.ReadFromFile("D:\\media\\marketplace.sdb");

    if (( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] ) || 
        ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] ))
    {
        g_AudioMgr.SetSimulationMode( FALSE );
        m_State = STATE_SELECT_MODE;        
    }      
}




//------------------------------------------------------------------------------
// Name: Marketplace::GetHostProperties()
// Desc: Returns the host properties, like voice timeouts and upstream and
//       downstream channels)
//------------------------------------------------------------------------------
HostProperties &Marketplace::GetHostProperties()
{
    return m_HostProps;
}




//------------------------------------------------------------------------------
// Name: Marketplace::IsHost()
// Desc: Returns whether we are the host or not
//------------------------------------------------------------------------------
BOOL Marketplace::IsHost()
{
    return m_bHost;
}





//------------------------------------------------------------------------------
// Name: Marketplace::StatsSendVoiceData()
// Desc: Records voice data sent as stats
//------------------------------------------------------------------------------
VOID Marketplace::StatsSendVoiceData( DWORD dwBytes )
{
    if (dwBytes == 0) return;
    m_dwSendNumPackets[ m_wCurStatPos ]++;
    m_dwSendVoiceBytes[ m_wCurStatPos ] += dwBytes;
    m_dwTotalSendNumPackets++;
    m_dwTotalSendVoiceBytes += dwBytes;
}




//------------------------------------------------------------------------------
// Name: Marketplace::StatsSendVoiceData()
// Desc: Records voice data received
//------------------------------------------------------------------------------
VOID Marketplace::StatsRecvVoiceData( DWORD dwBytes )
{
    if (dwBytes == 0) return;
    m_dwRecvNumPackets[ m_wCurStatPos ]++;
    m_dwRecvVoiceBytes[ m_wCurStatPos ] += dwBytes;
    m_dwTotalRecvNumPackets++;
    m_dwTotalRecvVoiceBytes += dwBytes;
}




//------------------------------------------------------------------------------
// Name: Marketplace::StatsReset()
// Desc: Reset all of the stats
//------------------------------------------------------------------------------
VOID Marketplace::StatsReset()
{
    int i;
    
    for (i = 0; i < STATS_WINDOW_SIZE; i++ )
    {
        m_dwRecvNumPackets[i] = 0;
        m_dwRecvVoiceBytes[i] = 0;
        m_dwSendNumPackets[i] = 0;
        m_dwSendVoiceBytes[i] = 0;      
    }

    m_dwTotalRecvNumPackets = 0;
    m_dwTotalSendNumPackets = 0;
    m_dwTotalRecvVoiceBytes = 0;
    m_dwTotalRecvNumPackets = 0;

    m_fMaxRecvKbps = 0.0f;
    m_fMaxSendKbps = 0.0f;

    m_wCurStatPos = 0;
    m_fStatsTime = m_fAppTime;
}




//------------------------------------------------------------------------------
// Name: Marketplace::StatsAdvanceFrame()
// Desc: Advance the frame for 'current' stats
//------------------------------------------------------------------------------
VOID Marketplace::StatsAdvanceFrame()
{
    m_wCurStatPos ++;
    m_wCurStatPos %= STATS_WINDOW_SIZE;
    m_dwRecvNumPackets[ m_wCurStatPos ] = 0;
    m_dwRecvVoiceBytes[ m_wCurStatPos ] = 0;
    m_dwSendNumPackets[ m_wCurStatPos ] = 0;
    m_dwSendVoiceBytes[ m_wCurStatPos ] = 0;
}




//------------------------------------------------------------------------------
// Name: Marketplace::StatsAdvanceFrame()
// Desc: Draw the stats graph
//------------------------------------------------------------------------------
VOID Marketplace::StatsDraw()
{   
    int i;
    WCHAR wczBuf[30];

    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,      FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGENABLE,    FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE,     D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_CULLMODE,     D3DCULL_NONE );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );
    
    const float TXWinStartX = 100.0f;
    const float RXWinStartX = 375.0f;
    const float WinWidth = 175.0f;
    const float WinStartY = 320.0f;
    const float WinHeight = 100.0f;
    const float TextRow1Y = WinStartY + WinHeight;
    const float TextRow2Y = WinStartY + WinHeight + 16.0f;
    const float TextCol2Add = WinWidth / 2.0f + 10.0f;
    const float TextNumShift = 36.0f;

    D3DDevice::Begin( D3DPT_QUADLIST );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0x8030a030 );
   
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX,             WinStartY, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX + WinWidth,  WinStartY, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX + WinWidth,  WinStartY + WinHeight, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX,             WinStartY + WinHeight, 1.0f, 1.0f );
    
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX,             WinStartY, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX + WinWidth,  WinStartY, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX + WinWidth,  WinStartY + WinHeight, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX,             WinStartY + WinHeight, 1.0f, 1.0f );
    
    D3DDevice::End(); 

    D3DDevice::Begin( D3DPT_LINELIST );

    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0xffa0ffa0 );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX, WinStartY + (WinHeight / 2.0f ), 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, RXWinStartX + WinWidth, WinStartY + (WinHeight / 2.0f ), 1.0f, 1.0f );
   
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX, WinStartY + (WinHeight / 2.0f ), 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, TXWinStartX + WinWidth, WinStartY + (WinHeight / 2.0f ), 1.0f, 1.0f );

    D3DDevice::End();

    float fSamplesPerSec, fSampleScale;

    fSamplesPerSec = 100.0f / (float)m_HostProps.byVoiceUpdateRate;
    fSampleScale = fSamplesPerSec * 8.0f / 128000.0f;


    D3DDevice::Begin( D3DPT_LINESTRIP );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0xffffffff );
          
    int iPos;
    
    // draw the receive graph 
    for ( i = 0; i < STATS_WINDOW_SIZE; i++ )
    {       
        iPos = ( m_wCurStatPos + 1 + i ) % STATS_WINDOW_SIZE;
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, 
                                    RXWinStartX + WinWidth * ( (float)i / (float)STATS_WINDOW_SIZE ),
                                    WinStartY + WinHeight - ((float)m_dwRecvVoiceBytes[ iPos ]  * fSampleScale ) * WinHeight,
                                    1.0f, 1.0f );                        
    }

    D3DDevice::End();

    D3DDevice::Begin( D3DPT_LINESTRIP );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0xffffffff );
          
    // draw the send graph
    for ( i = 0; i < STATS_WINDOW_SIZE; i++ )
    {       
        iPos = ( m_wCurStatPos + 1 + i ) % STATS_WINDOW_SIZE;
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, 
                                    TXWinStartX + WinWidth * ( (float)i / (float)STATS_WINDOW_SIZE ),
                                    WinStartY + WinHeight - ((float)m_dwSendVoiceBytes[ iPos ]  * fSampleScale ) * WinHeight,
                                    1.0f, 1.0f );                        
    }

    D3DDevice::End();
       
    DWORD dwRecvBytes, dwSendBytes, dwIdx;


    dwRecvBytes = 0; 
    dwSendBytes = 0;
    
    // calculate the data over the last second
    for ( i = 0; i < fSamplesPerSec; i++ )
    {
        dwIdx = ( m_wCurStatPos - i - 1 + STATS_WINDOW_SIZE ) % STATS_WINDOW_SIZE;
        dwRecvBytes += m_dwRecvVoiceBytes[ dwIdx ];
        dwSendBytes += m_dwSendVoiceBytes[ dwIdx ];
    }
    
    if ((float)dwSendBytes / 125.0f > m_fMaxSendKbps )
        m_fMaxSendKbps = (float)dwSendBytes / 125.0f;
    if ((float)dwRecvBytes / 125.0f > m_fMaxRecvKbps )
        m_fMaxRecvKbps = (float)dwRecvBytes / 125.0f;

    // draw the stats data (current, total, max, and average)

    m_Font.Begin();
    m_Font.SetScaleFactors( 0.66f, 0.66f );

    m_Font.DrawText( TXWinStartX - 32.0f, TextRow1Y, 0xffffa0a0, L"TX:" );
    m_Font.DrawText( RXWinStartX - 32.0f, TextRow1Y, 0xffa0a0ff, L"RX:" );

    m_Font.DrawText( TXWinStartX,               TextRow1Y, 0xffffffa0, L"Cur:" );
    m_Font.DrawText( TXWinStartX + TextCol2Add, TextRow1Y, 0xffffffa0, L"Ttl:" );
    m_Font.DrawText( TXWinStartX,               TextRow2Y, 0xffffffa0, L"Max:" );
    m_Font.DrawText( TXWinStartX + TextCol2Add, TextRow2Y, 0xffffffa0, L"Avg:" );
    
    m_Font.DrawText( RXWinStartX,               TextRow1Y, 0xffffffa0, L"Cur:" );
    m_Font.DrawText( RXWinStartX + TextCol2Add, TextRow1Y, 0xffffffa0, L"Ttl:" );
    m_Font.DrawText( RXWinStartX,               TextRow2Y, 0xffffffa0, L"Max:" );
    m_Font.DrawText( RXWinStartX + TextCol2Add, TextRow2Y, 0xffffffa0, L"Avg:" );

    swprintf( wczBuf, L"%.2f", (float)dwSendBytes / 125.0f );
    m_Font.DrawText( TXWinStartX + TextNumShift, TextRow1Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", (float)m_dwTotalSendVoiceBytes / 125.0f );
    m_Font.DrawText( TXWinStartX + TextCol2Add + TextNumShift, TextRow1Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", m_fMaxSendKbps );
    m_Font.DrawText( TXWinStartX + TextNumShift, TextRow2Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", (float)m_dwTotalSendVoiceBytes / 125.0f / ( m_fTime - m_fStatsTime ) );
    m_Font.DrawText( TXWinStartX + TextCol2Add + TextNumShift, TextRow2Y, 0xffffffff, wczBuf );

    swprintf( wczBuf, L"%.2f", (float)dwRecvBytes / 125.0f );
    m_Font.DrawText( RXWinStartX + TextNumShift, TextRow1Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", (float)m_dwTotalRecvVoiceBytes / 125.0f );
    m_Font.DrawText( RXWinStartX + TextCol2Add + TextNumShift, TextRow1Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", m_fMaxRecvKbps );
    m_Font.DrawText( RXWinStartX + TextNumShift, TextRow2Y, 0xffffffff, wczBuf );
    swprintf( wczBuf, L"%.2f", (float)m_dwTotalRecvVoiceBytes / 125.0f / ( m_fTime - m_fStatsTime ) );
    m_Font.DrawText( RXWinStartX + TextCol2Add + TextNumShift, TextRow2Y, 0xffffffff, wczBuf );

    m_Font.End();

}




//------------------------------------------------------------------------------
// Name: Marketplace::IsShowNames()
// Desc: Returns whether show names is toggled (black button)
//------------------------------------------------------------------------------
BOOL Marketplace::IsShowNames()
{
    return m_bShowNames;
}





//------------------------------------------------------------------------------
// Name: Marketplace::SetLastLocalSpeechTime()
// Desc: Sets the last time local speech was recorded
//------------------------------------------------------------------------------
void Marketplace::SetLastLocalSpeechTime( float fTime )
{
    m_fLastLocalSpeechTime = fTime;
}
//-----------------------------------------------------------------------------
// File: Packet.h
//
// Desc: Packet is a generic network datagram class, that lets us stream 
//       data in and out.  It also has support for VDP encryption markers
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

// Packet.h - generic network packet stream class


#define MAX_PACKET_SIZE 4000

// This is a generic packet stream class

// Things that could be encapsulated here
//    Error Checking
//    Compression
//    Reliable vs. unreliable

class Packet
{
public:
    Packet();
    
    VOID    ResetPacket();
    VOID    ResetReadPosition( WORD w = 0 );
    WORD    GetReadPosition();

    INT     ReceiveFromSocket( CXBSocket *pSock, SOCKADDR_IN *pFrom );
    VOID    SendToSocket( CXBSocket *pSock, SOCKADDR_IN *pTo );

    Packet& operator = ( Packet& );     // copy a packet
    
    Packet& operator >> ( FLOAT &f );
    Packet& operator >> ( DWORD &dw );
    Packet& operator >> ( WORD &w );
    Packet& operator >> ( BYTE &b );
    Packet& operator >> ( D3DXVECTOR3 &v );
    Packet& operator >> ( XUID &x );
    Packet& operator >> ( XNADDR &x );
    Packet& ReadRawBytes( VOID *bpDest, WORD Length );
    Packet& ReadSubpacket( Packet &packet, WORD Length );

    Packet& operator << ( const FLOAT &f );
    Packet& operator << ( const DWORD &dw );
    Packet& operator << ( const WORD &w );
    Packet& operator << ( const BYTE &b );
    Packet& operator << ( const D3DXVECTOR3 &v );
    Packet& operator << ( const XUID &x );
    Packet& operator << ( const XNADDR &x );
    
    Packet& WriteRawBytes( VOID *bpSource, WORD Length );
    Packet& WriteSubpacket( Packet &packet );

    BOOL    AtEndOfPacket();
    
    VOID    WriteNoEncryptionMarker();      // data after this point will not be encrypted

    WORD    Size();
    WORD    UnencryptedSize();
    WORD    EncryptedSize();    
    BYTE*   RawBytes();

protected:
    BYTE m_byData[MAX_PACKET_SIZE];         // full data to send or receive inc. encrypted length
    BYTE *m_byPacketData;                   // data after the initial 2 bytes
    BYTE m_bInEncryptedPartOfPacket;        // bool, forced to be 1 byte long
    WORD m_wCurrentReadPosition;            // used only for reading- writing is always done from the end
    WORD m_wSizeInBytes;
    WORD m_wEncryptedSizeInBytes;
};


inline Packet::Packet() 
{
    m_byPacketData = m_byData + 2; 
    ResetPacket(); 
} 

inline BYTE *Packet::RawBytes()         { return m_byPacketData; }
inline WORD  Packet::Size()             { return m_wSizeInBytes; }
inline WORD  Packet::UnencryptedSize()  { return m_wSizeInBytes - m_wEncryptedSizeInBytes; }
inline WORD  Packet::EncryptedSize()    { return m_wEncryptedSizeInBytes; }

inline VOID  Packet::ResetReadPosition( WORD w )  { m_wCurrentReadPosition = w; }
inline WORD  Packet::GetReadPosition()            { return m_wCurrentReadPosition; }


inline INT Packet::ReceiveFromSocket( CXBSocket *pSock, SOCKADDR_IN *pFrom )
{
    INT iData;
    
    iData = pSock->RecvFrom( m_byData, MAX_PACKET_SIZE, pFrom);
       
    if (( iData > 2 ) && ( iData != SOCKET_ERROR ))
    {
        m_wEncryptedSizeInBytes = *((WORD *)m_byData ); 
        m_wCurrentReadPosition = 0;
        m_wSizeInBytes = (WORD)iData - 2; // don't include the header
    }

    return iData;
}

inline VOID  Packet::SendToSocket( CXBSocket *pSock, SOCKADDR_IN *pTo )
{
    *((WORD *)m_byData) = m_wEncryptedSizeInBytes;
    pSock->SendTo( m_byData, m_wSizeInBytes + 2, pTo ); 
}

inline VOID Packet::ResetPacket()
{
    m_wSizeInBytes = m_wEncryptedSizeInBytes = m_wCurrentReadPosition = 0;
    m_bInEncryptedPartOfPacket = 1;
}

inline VOID Packet::WriteNoEncryptionMarker()
{
    m_bInEncryptedPartOfPacket = 0;
}

inline Packet & Packet::operator = ( Packet &p )
{
    m_wSizeInBytes = p.m_wSizeInBytes;
    m_wEncryptedSizeInBytes = p.m_wEncryptedSizeInBytes;
    m_bInEncryptedPartOfPacket = p.m_bInEncryptedPartOfPacket;
    memcpy( m_byPacketData, p.m_byPacketData, m_wSizeInBytes );
    return *this;
}

// all of these are handled the exact same way

#define STANDARD_READ( type ) \
    inline Packet & Packet::operator >> ( type &v )                                          \
    {                                                                                        \
        v = *((type *)( m_byPacketData + m_wCurrentReadPosition ));                          \
        m_wCurrentReadPosition += sizeof(type);                                              \
        return *this;                                                                        \
    }   

#define STANDARD_WRITE( type ) \
    inline Packet & Packet::operator << ( const type &v )                                   \
    {                                                                                        \
        *((type *)( m_byPacketData + m_wSizeInBytes )) = v;                                  \
        m_wSizeInBytes += sizeof(type);                                                      \
        m_wEncryptedSizeInBytes += sizeof(type) * m_bInEncryptedPartOfPacket;                \
        return *this;                                                                        \
    }

STANDARD_READ( FLOAT );
STANDARD_READ( DWORD );
STANDARD_READ( WORD );
STANDARD_READ( BYTE );
STANDARD_READ( D3DXVECTOR3 );
STANDARD_READ( XUID );
STANDARD_READ( XNADDR );

STANDARD_WRITE( FLOAT );
STANDARD_WRITE( DWORD );
STANDARD_WRITE( WORD );
STANDARD_WRITE( BYTE );
STANDARD_WRITE( D3DXVECTOR3 );
STANDARD_WRITE( XUID );
STANDARD_WRITE( XNADDR );

inline Packet& Packet::ReadRawBytes( VOID *bpDest, WORD wLength )
{
    memcpy( bpDest, m_byPacketData + m_wCurrentReadPosition, wLength );
    m_wCurrentReadPosition += wLength;
    return *this;
}


inline Packet& Packet::ReadSubpacket( Packet &p, WORD wLength )
{
    p.ResetPacket();
    p.m_wSizeInBytes = wLength;
    memcpy( p.m_byPacketData, m_byPacketData + m_wCurrentReadPosition, wLength);
    m_wCurrentReadPosition += wLength;
    return *this;
}

inline Packet& Packet::WriteRawBytes( VOID *bpDest, WORD wLength )
{
    memcpy( m_byPacketData + m_wSizeInBytes, bpDest, wLength );
    m_wSizeInBytes += wLength;
    m_wEncryptedSizeInBytes += wLength * m_bInEncryptedPartOfPacket;
    return *this;
}

inline Packet& Packet::WriteSubpacket( Packet &p )
{
    memcpy( m_byPacketData + m_wSizeInBytes, p.m_byPacketData, p.m_wSizeInBytes );
    m_wSizeInBytes += p.m_wSizeInBytes;
    m_wEncryptedSizeInBytes += p.m_wSizeInBytes * m_bInEncryptedPartOfPacket;
    return *this;
}

inline BOOL Packet::AtEndOfPacket()
{
    return ( m_wCurrentReadPosition == m_wSizeInBytes );
}
//-----------------------------------------------------------------------------
// File: WordList.cpp
//
// Desc: Contains implementation of CWordList class, used to load and parse
//          Speech Recognition vocabulary definitions
//
// Hist: 03.14.03 - New for the April 2003 XDK Release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "wordlist.h"
#include "XBUtil.h"



//-----------------------------------------------------------------------------
// Name: CWordList (ctor)
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CWordList::CWordList()
{
    m_pbFileData = NULL;
}



//-----------------------------------------------------------------------------
// Name: ~CWordList (dtor)
// Desc: Cleans up member variables
//-----------------------------------------------------------------------------
CWordList::~CWordList()
{
    free( m_pbFileData );
}



//-----------------------------------------------------------------------------
// Name: IsXXXX
// Desc: Helper functions for parsing out word list text file
//-----------------------------------------------------------------------------
__forceinline BOOL IsNumber( WCHAR ch ) { return ch >= L'0' && ch <= L'9'; }
__forceinline BOOL IsNewLine( WCHAR ch ) { return ch == L'\r' || ch == L'\n'; }
__forceinline BOOL IsSpace( WCHAR ch ) { return IsNewLine( ch ) || ch == L' ' || ch == L'\t'; }
// __forceinline BOOL IsChar( WCHAR ch ) { return ( ch >= L'a' && ch <= L'z' ) || ( ch >= L'A' && ch <= L'Z' ); }
__forceinline BOOL IsChar( WCHAR ch ) { return !( IsSpace( ch ) || IsNumber( ch ) ); }



//-----------------------------------------------------------------------------
// Name: LoadWordList
// Desc: Parses the vocabularies specified in the given file.  Does this by
//          loading the entire file into memory and walking through it, 
//          keeping track of the locations of words and vocabulary names
//-----------------------------------------------------------------------------
HRESULT CWordList::LoadWordList( CHAR* strFilename )
{
    Clear();

    CHAR strFullpath[MAX_PATH + 1];
    if( FAILED( XBUtil_FindMediaFile( strFullpath, strFilename ) ) )
    {
        XBUtil_DebugPrint( "Couldn't find media file %s\n", strFilename  );
        return E_FAIL;
    }

    DWORD dwFileSize;
    if( FAILED( XBUtil_LoadFile( strFullpath, (VOID**)&m_pbFileData, &dwFileSize ) ) )
        return E_FAIL;

    // Start walking a pointer from the beginning of the file
    WCHAR* pchEnd = (WCHAR *)( m_pbFileData + dwFileSize );
    WCHAR* pch = (WCHAR*)m_pbFileData;
    
    SR_VOCAB vocabCurrent;
    while( pch < pchEnd )
    {
        if( *pch == L'[' )
        {
            // Open bracket means we're starting a new vocabulary:
            // If we had any words in the current vocabulary, add that
            // vocabulary to our list
            if( vocabCurrent.words.size() > 0 )
            {
                m_VocabList.push_back( vocabCurrent );
                vocabCurrent.words.clear();
            }

            // Skip the open bracket and remember the vocab name
            vocabCurrent.strName = pch + 1;

            // Find the end of the vocabulary name by looking for a 
            // close bracket
            while( *pch != L']' )
                ++pch;

            // Terminate the vocab name by replacing close bracket w/ 0
            *pch++ = L'\0';
        }
        else if( IsNumber( *pch ) )
        {
            // Number means a word is being defined:
            // Grab the word ID from the beginning of the line
            SR_WORD wordCurrent;
            wordCurrent.dwWordID = _wtol( pch );

            // Skip until we see text
            while( !IsChar( *pch ) )
                ++pch;

            // Grab a pointer to the word and skip to end of line
            wordCurrent.strWord = pch;
            while( !( IsNewLine( *pch ) ) )
                ++pch;

            // Replace newlines with terminators.  Note this may leave
            // trailing space at the end of a word.  We could scan back
            // for space, but it's not really worth it
            while( IsNewLine( *pch ) )
            {
                *pch = L'\0';
                ++pch;
            }

            vocabCurrent.words.push_back( wordCurrent );
        }
        else
        {
            // We're only interested in words and vocabularies
            ++pch;
        }
    }

    // Flush the last vocabulary
    if( vocabCurrent.words.size() > 0 )
    {
        m_VocabList.push_back( vocabCurrent );
        vocabCurrent.words.clear();
    }

    return S_OK;
}

HRESULT CWordList::Clear()
{
    free( m_pbFileData );
    m_pbFileData = NULL;
    m_VocabList.clear();

    return S_OK;
}

WCHAR* CWordList::FindWord( DWORD dwWordID )
{
    for( SR_VOCABLIST::iterator i = m_VocabList.begin(); i < m_VocabList.end(); ++i )
    {
        for( SR_WORDLIST::iterator j = i->words.begin(); j < i->words.end(); ++j )
        {
            if( dwWordID == j->dwWordID )
                return j->strWord;
        }
    }

    return NULL;
}
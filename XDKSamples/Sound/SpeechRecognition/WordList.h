//-----------------------------------------------------------------------------
// File: WordList.h
//
// Desc: Contains definition of CWordList class, used to load and parse
//          Speech Recognition vocabulary definitions
//
// Hist: 03.14.03 - New for the April 2003 XDK Release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef _WORDLIST_H_
#define _WORDLIST_H_

#include <xtl.h>
#include <vector>

struct SR_WORD {
    WCHAR* strWord;
    DWORD  dwWordID;
};

typedef std::vector<SR_WORD> SR_WORDLIST;
struct SR_VOCAB {
    SR_WORDLIST    words;
    WCHAR*      strName;
};
typedef std::vector<SR_VOCAB> SR_VOCABLIST;

//-----------------------------------------------------------------------------
// Name: class CWordList
// Desc: Class that handles loading and parsing a speech recognition bank
//          word list
//-----------------------------------------------------------------------------
class CWordList
{
public:
    CWordList();
    ~CWordList();

    HRESULT LoadWordList( CHAR* strFilename );
    HRESULT Clear();
    WCHAR*  FindWord( DWORD dwWordID );

    SR_VOCABLIST    m_VocabList;
    BYTE*           m_pbFileData;
};

#endif // _WORDLIST_H_
// Scintilla source code edit control
/** @file LexCmake.cxx
 ** Lexer for Cmake
 **/
// Copyright 2007 by Cristian Adam <cristian [dot] adam [at] gmx [dot] net>
// based on the NSIS lexer
// The License.txt file describes the conditions under which this software may be distributed.
// Lexer4 changes by James Michael Armstrong (https://github.com/BlazesRus) based on LexRegistry.cxx and LexCPP.cxx
// and multiline fix mostly by https://github.com/Motyaspr with some adjustments from (https://github.com/BlazesRus)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include <string>
#include <string_view>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

#include "StringCopy.h"
#include "OptionSet.h"
#include "DefaultLexer.h"

using namespace Lexilla;

static const char * const cmakeWordLists[] = {
    "Commands",
    "Parameters",
    "UserDefined",
    0,
    0,};

struct OptionsCMake {
    bool foldCompact;
    bool fold;
    bool foldAtElse;
    OptionsCMake() {
        foldCompact = false;
        fold = false;
        foldAtElse = false;
    }
};

struct OptionSetCMake : public OptionSet<OptionsCMake> {
    OptionSetCMake() {
        DefineProperty("fold.compact", &OptionsCMake::foldCompact, "");
        DefineProperty("fold", &OptionsCMake::fold, "Code is currently folded");
        DefineProperty("fold.at.else", &OptionsCMake::foldAtElse, "");
        DefineWordListSets(cmakeWordLists);
    }
};

class LexerCmake : public DefaultLexer {

    OptionsCMake options;
    OptionSetCMake optSet;
    WordList Commands;
    WordList Parameters;
    WordList UserDefined;

    static bool isCmakeNumber(char ch)
    {
        return(ch >= '0' && ch <= '9');
    }

    static bool isCmakeChar(char ch)
    {
        return(ch == '.' ) || (ch == '_' ) || isCmakeNumber(ch) || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    }

    static bool isCmakeLetter(char ch)
    {
        return(ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    }

    bool CmakeNextLineHasElse(Sci_PositionU start, Sci_PositionU end, LexAccessor &styler)
    {
        Sci_Position nNextLine = -1;
        for ( Sci_PositionU i = start; i < end; ++i ) {
            char cNext = styler.SafeGetCharAt( i );
            if ( cNext == '\n' ) {
                nNextLine = i+1;
                break;
            }
        }

        if ( nNextLine == -1 ) // We never foudn the next line...
            return false;

        for ( Sci_PositionU firstChar = nNextLine; firstChar < end; ++firstChar ) {
            char cNext = styler.SafeGetCharAt( firstChar );
            if ( cNext == ' ' )
                continue;
            if ( cNext == '\t' )
                continue;
            if ( styler.Match(firstChar, "ELSE")  || styler.Match(firstChar, "else"))
                return true;
            break;
        }

        return false;
    }

    int calculateFoldCmake(Sci_PositionU start, Sci_PositionU end, int foldlevel, LexAccessor &styler, bool bElse)
    {
        // If the word is too long, it is not what we are looking for
        if ( end - start > 20 )
            return foldlevel;

        int newFoldlevel = foldlevel;

        char s[20]; // The key word we are looking for has atmost 13 characters
        for (unsigned int i = 0; i < end - start + 1 && i < 19; i++) {
            s[i] = static_cast<char>( styler[ start + i ] );
            s[i + 1] = '\0';
        }

        if ( CompareCaseInsensitive(s, "IF") == 0 || CompareCaseInsensitive(s, "WHILE") == 0
             || CompareCaseInsensitive(s, "MACRO") == 0 || CompareCaseInsensitive(s, "FOREACH") == 0
             || CompareCaseInsensitive(s, "ELSEIF") == 0 )
            ++newFoldlevel;
        else if ( CompareCaseInsensitive(s, "ENDIF") == 0 || CompareCaseInsensitive(s, "ENDWHILE") == 0
                  || CompareCaseInsensitive(s, "ENDMACRO") == 0 || CompareCaseInsensitive(s, "ENDFOREACH") == 0)
            --newFoldlevel;
        else if ( bElse && CompareCaseInsensitive(s, "ELSEIF") == 0 )
            ++newFoldlevel;
        else if ( bElse && CompareCaseInsensitive(s, "ELSE") == 0 )
            ++newFoldlevel;

        return newFoldlevel;
    }

    //static int classifyWordCmake(Sci_PositionU start, Sci_PositionU end, WordList *keywordLists[], LexAccessor &styler )
    int classifyWordCmake(const Sci_PositionU& start, const Sci_PositionU& end, LexAccessor &styler )
    {
        char word[100] = {0};
        char lowercaseWord[100] = {0};

        for (Sci_PositionU i = 0; i < end - start + 1 && i < 99; ++i) {
            word[i] = static_cast<char>( styler[ start + i ] );
            lowercaseWord[i] = static_cast<char>(tolower(word[i]));
        }

        // Check for special words...
        if ( CompareCaseInsensitive(word, "MACRO") == 0 || CompareCaseInsensitive(word, "ENDMACRO") == 0 )
            return SCE_CMAKE_MACRODEF;

        if ( CompareCaseInsensitive(word, "IF") == 0 ||  CompareCaseInsensitive(word, "ENDIF") == 0 )
            return SCE_CMAKE_IFDEFINEDEF;

        if ( CompareCaseInsensitive(word, "ELSEIF") == 0  || CompareCaseInsensitive(word, "ELSE") == 0 )
            return SCE_CMAKE_IFDEFINEDEF;

        if ( CompareCaseInsensitive(word, "WHILE") == 0 || CompareCaseInsensitive(word, "ENDWHILE") == 0)
            return SCE_CMAKE_WHILEDEF;

        if ( CompareCaseInsensitive(word, "FOREACH") == 0 || CompareCaseInsensitive(word, "ENDFOREACH") == 0)
            return SCE_CMAKE_FOREACHDEF;

        if ( Commands.InList(lowercaseWord) )
            return SCE_CMAKE_COMMANDS;

        if ( Parameters.InList(word) )
            return SCE_CMAKE_PARAMETERS;


        if ( UserDefined.InList(word) )
            return SCE_CMAKE_USERDEFINED;

        if ( strlen(word) > 3 ) {
            if ( word[1] == '{' && word[strlen(word)-1] == '}' )
                return SCE_CMAKE_VARIABLE;
        }

        // To check for numbers
        if ( isCmakeNumber( word[0] ) ) {
            bool bHasSimpleCmakeNumber = true;
            for (unsigned int j = 1; j < end - start + 1 && j < 99; ++j) {
                if ( !isCmakeNumber( word[j] ) ) {
                    bHasSimpleCmakeNumber = false;
                    break;
                }
            }

            if ( bHasSimpleCmakeNumber )
                return SCE_CMAKE_NUMBER;
        }

        return SCE_CMAKE_DEFAULT;
    }

    void SCE_CMAKE_STRINGPart02(LexAccessor &styler, int& state, const char& cNextChar, const Sci_Position& i)
    {
        if ( cNextChar == '\r' || cNextChar == '\n' ) {
            Sci_Position nCurLine = styler.GetLine(i+1);
            Sci_Position nBack = i;
            // We need to check if the previous line has a \ in it...
            bool bNextLine = false;
            char cTemp;

            while ( nBack > 0 ) {
                if ( styler.GetLine(nBack) != nCurLine )
                    return;

                cTemp = styler.SafeGetCharAt(nBack, 'a'); // Letter 'a' is safe here

                if ( cTemp == '\\' ) {
                    bNextLine = true;
                    return;
                }
                else if ( cTemp != '\r' && cTemp != '\n' && cTemp != '\t' && cTemp != ' ' )
                    return;

                --nBack;
            }

            if ( bNextLine )
                styler.ColourTo(i+1,state);

            if ( bNextLine == false ) {
                styler.ColourTo(i,state);
                state = SCE_CMAKE_DEFAULT;
            }
        }
    }

public:
    LexerCmake(){}
    virtual ~LexerCmake() {}
    int SCI_METHOD Version() const override {
        return lvRelease4;
    }
    void SCI_METHOD Release() override {
        delete this;
    }
    const char *SCI_METHOD PropertyNames() override {
        return optSet.PropertyNames();
    }
    int SCI_METHOD PropertyType(const char *name) override {
        return optSet.PropertyType(name);
    }
    const char *SCI_METHOD DescribeProperty(const char *name) override {
        return optSet.DescribeProperty(name);
    }
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) override {
        if (optSet.PropertySet(&options, key, val)) {
            return 0;
        }
        return -1;
    }
    const char * SCI_METHOD DescribeWordListSets() override {
        return optSet.DescribeWordListSets();
    }
    Sci_Position SCI_METHOD WordListSet(int n, const char *wl) override;
    void *SCI_METHOD PrivateCall(int, void *) override {
        return 0;
    }
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess) override;
    static ILexer4 *LexerFactoryCmake() {
        return new LexerCmake;
    }
};

Sci_Position SCI_METHOD LexerCmake::WordListSet(int n, const char *wl) {
    WordList *wordListN = nullptr;
    switch (n) {
    case 0:
        wordListN = &Commands;
        break;
    case 1:
        wordListN = &Parameters;
        break;
    case 2:
        wordListN = &UserDefined;
        break;
    }
    Sci_Position firstModification = -1;
    if (wordListN) {
        WordList wlNew;
        wlNew.Set(wl);
        if (*wordListN != wlNew) {
            wordListN->Set(wl);
        }
    }
    return firstModification;
}

//static void ColouriseCmakeDoc(Sci_PositionU startPos, Sci_Position length, int, WordList *keywordLists[], Accessor &styler)
void SCI_METHOD LexerCmake::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    LexAccessor styler(pAccess);
    StyleContext context(startPos, length, initStyle, styler);

    int state = SCE_CMAKE_DEFAULT;
    if ( startPos > 0 )
        state = styler.StyleAt(startPos-1); // Use the style from the previous line, usually default, but could be commentbox

    styler.StartAt( startPos );
    styler.GetLine( startPos );

    Sci_PositionU nLengthDoc = startPos + length;
    styler.StartSegment( startPos );

    char cCurrChar;
    bool bVarInString = false;
    bool bClassicVarInString = false;
    bool bMultiComment = false;
    char cNextChar; char cAfterNextChar; char cPrevChar;

    Sci_PositionU i;
    for ( i = startPos; i < nLengthDoc; ++i ) {
        cCurrChar = styler.SafeGetCharAt( i );
        cNextChar = styler.SafeGetCharAt(i+1);
        cAfterNextChar = styler.SafeGetCharAt(i + 2);
        cPrevChar = styler.SafeGetCharAt(i - 1);
        switch (state) {
        case SCE_CMAKE_DEFAULT: {
            switch (cCurrChar)
            {
                case '#':// we have a comment line
                    styler.ColourTo(i-1, state );
                    state = SCE_CMAKE_COMMENT;
                    if (cNextChar == '[' && cAfterNextChar == '[')
                        bMultiComment = true; // we have multi comment
                    break;
                case '"':
                    styler.ColourTo(i-1, state );
                    bVarInString = false;
                    bClassicVarInString = false;
                    state = SCE_CMAKE_STRINGDQ;
                    break;
                case '\'':
                    styler.ColourTo(i-1, state );
                    bVarInString = false;
                    bClassicVarInString = false;
                    state = SCE_CMAKE_STRINGRQ;
                    break;
                case '`':
                    styler.ColourTo(i-1, state );
                    bVarInString = false;
                    bClassicVarInString = false;
                    state = SCE_CMAKE_STRINGLQ;
                    break;
                default:// CMake Variable
                    if ( cCurrChar == '$' || isCmakeChar(cCurrChar)) {
                        styler.ColourTo(i-1,state);
                        state = SCE_CMAKE_VARIABLE;

                        // If it is a number, we must check and set style here first...
                        if ( isCmakeNumber(cCurrChar) && (cNextChar == '\t' || cNextChar == ' ' || cNextChar == '\r' || cNextChar == '\n' ) )
                            styler.ColourTo( i, SCE_CMAKE_NUMBER);
                    }
            }
        } break;

        case SCE_CMAKE_COMMENT:
            if (bMultiComment == false && ( cCurrChar == '\n' || cCurrChar == '\r' ))
            {
                if ( cPrevChar == '\\' ) {
                    styler.ColourTo(i-2,state);
                    styler.ColourTo(i-1,SCE_CMAKE_DEFAULT);
                }
                else {
                    styler.ColourTo(i-1,state);
                    state = SCE_CMAKE_DEFAULT;
                }
            }
            else if (bMultiComment == true && cCurrChar == ']' && cPrevChar == ']')
            {
                styler.ColourTo(i - 1, state);
                styler.ColourTo(i, state);
                state = SCE_CMAKE_DEFAULT;
                bMultiComment = false;
            }
            break;

        case SCE_CMAKE_STRINGDQ:
            if ( cPrevChar == '\\' && styler.SafeGetCharAt(i-2) == '$' )
            {    // Ignore the next character, even if it is a quote of some sort
            }
            else if ( cCurrChar == '"' ) {
                styler.ColourTo(i,state);
                state = SCE_CMAKE_DEFAULT;
            }
            else
                SCE_CMAKE_STRINGPart02(styler, state, cNextChar, i);
            break;

        case SCE_CMAKE_STRINGLQ:
            if ( cPrevChar == '\\' && styler.SafeGetCharAt(i-2) == '$' )
            {    // Ignore the next character, even if it is a quote of some sort
            }
            else if ( cCurrChar == '`' ) {
                styler.ColourTo(i,state);
                state = SCE_CMAKE_DEFAULT;
            }
            else
                SCE_CMAKE_STRINGPart02(styler, state, cNextChar, i);
            break;

        case SCE_CMAKE_STRINGRQ:
            if ( cPrevChar == '\\' && styler.SafeGetCharAt(i-2) == '$' )
            {    // Ignore the next character, even if it is a quote of some sort
            }
            else if ( cCurrChar == '\'') {
                styler.ColourTo(i,state);
                state = SCE_CMAKE_DEFAULT;
            }
            else
                SCE_CMAKE_STRINGPart02(styler, state, cNextChar, i);
            break;

        case SCE_CMAKE_VARIABLE:

            // CMake Variable:
            if ( cCurrChar == '$' )
                state = SCE_CMAKE_DEFAULT;
            else if ( cCurrChar == '\\' && (cNextChar == 'n' || cNextChar == 'r' || cNextChar == 't' ) )
                state = SCE_CMAKE_DEFAULT;
            else if ( (isCmakeChar(cCurrChar) && !isCmakeChar( cNextChar) && cNextChar != '}') || cCurrChar == '}' ) {
                state = classifyWordCmake( styler.GetStartSegment(), i, styler );
                styler.ColourTo( i, state);
                state = SCE_CMAKE_DEFAULT;
            }
            else if ( !isCmakeChar( cCurrChar ) && cCurrChar != '{' && cCurrChar != '}' ) {
                if ( classifyWordCmake( styler.GetStartSegment(), i-1, styler) == SCE_CMAKE_NUMBER )
                    styler.ColourTo( i-1, SCE_CMAKE_NUMBER );

                state = SCE_CMAKE_DEFAULT;

                if ( cCurrChar == '"' ) {
                    state = SCE_CMAKE_STRINGDQ;
                    bVarInString = false;
                    bClassicVarInString = false;
                }
                else if ( cCurrChar == '`' ) {
                    state = SCE_CMAKE_STRINGLQ;
                    bVarInString = false;
                    bClassicVarInString = false;
                }
                else if ( cCurrChar == '\'' ) {
                    state = SCE_CMAKE_STRINGRQ;
                    bVarInString = false;
                    bClassicVarInString = false;
                }
                else if ( cCurrChar == '#' ) {
                    state = SCE_CMAKE_COMMENT;
                    if (cNextChar == '[' && cAfterNextChar == '[')
                        bMultiComment = true;
                }
            }
            break;
        }

        if ( state == SCE_CMAKE_STRINGDQ || state == SCE_CMAKE_STRINGLQ || state == SCE_CMAKE_STRINGRQ ) {
            bool bIngoreNextDollarSign = false;

            if ( bVarInString && cCurrChar == '$' ) {
                bVarInString = false;
                bIngoreNextDollarSign = true;
            }
            else if ( bVarInString && cCurrChar == '\\' && (cNextChar == 'n' || cNextChar == 'r' || cNextChar == 't' || cNextChar == '"' || cNextChar == '`' || cNextChar == '\'' ) ) {
                styler.ColourTo( i+1, SCE_CMAKE_STRINGVAR);
                bVarInString = false;
                bIngoreNextDollarSign = false;
            }

            else if ( bVarInString && !isCmakeChar(cNextChar) ) {
                int nWordState = classifyWordCmake( styler.GetStartSegment(), i, styler);
                if ( nWordState == SCE_CMAKE_VARIABLE )
                    styler.ColourTo( i, SCE_CMAKE_STRINGVAR);
                bVarInString = false;
            }
            // Covers "${TEST}..."
            else if ( bClassicVarInString && cNextChar == '}' ) {
                styler.ColourTo( i+1, SCE_CMAKE_STRINGVAR);
                bClassicVarInString = false;
            }

            // Start of var in string
            if ( !bIngoreNextDollarSign && cCurrChar == '$' && cNextChar == '{' ) {
                styler.ColourTo( i-1, state);
                bClassicVarInString = true;
                bVarInString = false;
            }
            else if ( !bIngoreNextDollarSign && cCurrChar == '$' ) {
                styler.ColourTo( i-1, state);
                bVarInString = true;
                bClassicVarInString = false;
            }
        }
    }

    // Colourise remaining document
    styler.ColourTo(nLengthDoc-1,state);

    context.Complete();
}

//static void FoldCmakeDoc(Sci_PositionU startPos, Sci_Position length, int, WordList *[], Accessor &styler)
void SCI_METHOD LexerCmake::Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    LexAccessor styler(pAccess);
    StyleContext context(startPos, length, initStyle, styler);

     // No folding enabled, no reason to continue...
    if (!options.fold)
        return;

    bool foldAtElse = options.foldAtElse == 1;

    Sci_Position lineCurrent = styler.GetLine(startPos);
    Sci_PositionU safeStartPos = styler.LineStart( lineCurrent );

    bool bArg1 = true;
    Sci_Position nWordStart = -1;

    Sci_Position levelCurrent = SC_FOLDLEVELBASE;
    if (lineCurrent > 0)
        levelCurrent = styler.LevelAt(lineCurrent-1) >> 16;
    int levelNext = levelCurrent;

    for (Sci_PositionU i = safeStartPos; i < startPos + length; i++) {
        char chCurr = styler.SafeGetCharAt(i);

        if ( bArg1 ) {
            if ( nWordStart == -1 && (isCmakeLetter(chCurr)) ) {
                nWordStart = i;
            }
            else if ( isCmakeLetter(chCurr) == false && nWordStart > -1 ) {
                int newLevel = calculateFoldCmake( nWordStart, i-1, levelNext, styler, foldAtElse);

                if ( newLevel == levelNext ) {
                    if ( foldAtElse ) {
                        if ( CmakeNextLineHasElse(i, startPos + length, styler) )
                            --levelNext;
                    }
                }
                else
                    levelNext = newLevel;
                bArg1 = false;
            }
        }

        if ( chCurr == '\n' ) {
            if ( bArg1 && foldAtElse) {
                if ( CmakeNextLineHasElse(i, startPos + length, styler) )
                    --levelNext;
            }

            // If we are on a new line...
            int levelUse = levelCurrent;
            int lev = levelUse | levelNext << 16;
            if (levelUse < levelNext )
                lev |= SC_FOLDLEVELHEADERFLAG;
            if (lev != styler.LevelAt(lineCurrent))
                styler.SetLevel(lineCurrent, lev);

            ++lineCurrent;
            levelCurrent = levelNext;
            bArg1 = true; // New line, lets look at first argument again
            nWordStart = -1;
        }
    }

    int levelUse = levelCurrent;
    int lev = levelUse | levelNext << 16;
    if (levelUse < levelNext)
        lev |= SC_FOLDLEVELHEADERFLAG;
    if (lev != styler.LevelAt(lineCurrent))
        styler.SetLevel(lineCurrent, lev);

    context.Complete();
}

LexerModule lmCmake(SCLEX_CMAKE, LexerCmake::LexerFactoryCmake, "cmake", cmakeWordLists);

extern const LexerModule lmCmake(SCLEX_CMAKE, ColouriseCmakeDoc, "cmake", FoldCmakeDoc, cmakeWordLists);


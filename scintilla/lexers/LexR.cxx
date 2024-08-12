// This file is part of Notepad4.
// See License.txt for details about distribution and modification.
//! Lexer for R.

#include <cassert>
#include <cstring>

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

using namespace Lexilla;

namespace {

// https://search.r-project.org/R/refmans/base/html/Quotes.html
struct EscapeSequence {
	int outerState = SCE_R_DEFAULT;
	int digitsLeft = 0;
	bool hex = false;
	bool brace = false;

	// highlight any character as escape sequence, unrecognized escape sequence is syntax error.
	void resetEscapeState(int state, int chNext) noexcept {
		outerState = state;
		digitsLeft = 1;
		hex = true;
		brace = false;
		if (chNext == 'x') {
			digitsLeft = 3;
		} else if (chNext == 'u') {
			digitsLeft = 5;
		} else if (chNext == 'U') {
			digitsLeft = 9;
		} else if (IsOctalDigit(chNext)) {
			digitsLeft = 3;
			hex = false;
		}
	}
	bool atEscapeEnd(int ch) noexcept {
		--digitsLeft;
		return digitsLeft <= 0 || !IsOctalOrHex(ch, hex);
	}
};

//KeywordIndex++Autogenerated -- start of section automatically generated
enum {
	KeywordIndex_Keyword = 0,
};
//KeywordIndex--Autogenerated -- end of section automatically generated

constexpr bool IsRawString(int state) noexcept {
	return state >= SCE_R_RAWSTRING_SQ;
}

constexpr int GetStringQuote(int state) noexcept {
	return (state == SCE_R_BACKTICKS) ? '`' : ((state == SCE_R_STRING_SQ) ? '\'' : '\"');
}

inline int CheckRawString(const StyleContext &sc, int &dashCount) noexcept {
	Sci_PositionU pos = sc.currentPos + 2;
	dashCount = 0;
	while (pos < sc.lineStartNext) {
		const char ch = sc.styler[pos++];
		switch (ch) {
		case '-':
			++dashCount;
			break;
		case '(':
			return ')';
		case '[':
			return ']';
		case '{':
			return '}';
		default:
			dashCount = 0;
			return 0;
		}
	}
	dashCount = 0;
	return 0;
}

constexpr bool IsFormatSpecifier(char ch) noexcept {
	// copied from LexAwk
	return AnyOf(ch, 'a', 'A',
					'c',
					'd',
					'e', 'E',
					'f', 'F',
					'g', 'G',
					'i',
					'o',
					's',
					'u',
					'x', 'X');
}

// https://search.r-project.org/R/refmans/base/html/sprintf.html
Sci_Position CheckFormatSpecifier(const StyleContext &sc, LexAccessor &styler, bool insideUrl) noexcept {
	if (sc.chNext == '%') {
		return 2;
	}
	if (insideUrl && IsHexDigit(sc.chNext)) {
		// percent encoded URL string
		return 0;
	}
	if (IsASpaceOrTab(sc.chNext) && IsADigit(sc.chPrev)) {
		// ignore word after percent: "5% x"
		return 0;
	}

	// similar to LexAwk
	Sci_PositionU pos = sc.currentPos + 1;
	char ch = styler[pos];
	// argument
	while (IsADigit(ch)) {
		ch = styler[++pos];
	}
	if (ch == '$') {
		ch = styler[++pos];
	}
	// flags
	while (AnyOf(ch, '-', '+', ' ', '#', '0')) {
		ch = styler[++pos];
	}
	for (int i = 0; i < 2; i++) {
		// width
		const bool argument = ch == '*';
		if (argument) {
			ch = styler[++pos];
		}
		while (IsADigit(ch)) {
			ch = styler[++pos];
		}
		if (argument && ch == '$') {
			ch = styler[++pos];
		}
		// precision
		if (i == 0 && ch == '.') {
			ch = styler[++pos];
		} else {
			break;
		}
	}
	// conversion format specifier
	if (IsFormatSpecifier(ch)) {
		return pos - sc.currentPos + 1;
	}
	return 0;
}

void ColouriseRDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	int lineStateLineComment = 0;
	int chBeforeIdentifier = 0;
	int visibleChars = 0;
	bool insideUrl = false;
	int matchingDelimiter = 0;
	int dashCount = 0;
	EscapeSequence escSeq;

	StyleContext sc(startPos, lengthDoc, initStyle, styler);
	if (sc.currentLine > 0) {
		const int lineState = styler.GetLineState(sc.currentLine - 1);
		matchingDelimiter = (lineState >> 1) & 0x7f;
		dashCount = lineState >> 8;
	}

	while (sc.More()) {
		switch (sc.state) {
		case SCE_R_OPERATOR:
			sc.SetState(SCE_R_DEFAULT);
			break;

		case SCE_R_INFIX:
			if (sc.atLineStart) {
				sc.SetState(SCE_R_DEFAULT);
			} else if (sc.ch == '%') {
				sc.ForwardSetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_NUMBER:
			if (!IsDecimalNumberEx(sc.chPrev, sc.ch, sc.chNext)) {
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_IDENTIFIER:
			if (!IsIdentifierCharEx(sc.ch)) {
				if (sc.ch != '.' && chBeforeIdentifier != '.') {
					char s[128];
					sc.GetCurrent(s, sizeof(s));
					if (keywordLists[KeywordIndex_Keyword].InList(s)) {
						sc.ChangeState(SCE_R_KEYWORD);
					}
				}
				if (sc.state == SCE_R_IDENTIFIER && sc.GetLineNextChar() == '(') {
					sc.ChangeState(SCE_R_FUNCTION);
				}
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_COMMENT:
		case SCE_R_DIRECTIVE:
			if (sc.atLineStart) {
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_BACKTICKS:
		case SCE_R_STRING_SQ:
		case SCE_R_STRING_DQ:
		case SCE_R_RAWSTRING_SQ:
		case SCE_R_RAWSTRING_DQ:
			if (sc.ch == '\\' && !IsRawString(sc.state)) {
				escSeq.resetEscapeState(sc.state, sc.chNext);
				sc.SetState(SCE_R_ESCAPECHAR);
				sc.Forward();
				if (sc.chNext == '{' && escSeq.digitsLeft > 4) {
					escSeq.brace = true;
					sc.Forward();
				} else if (sc.MatchLineEnd()) {
					// don't highlight line ending as escape sequence:
					// escapeSeq.outerState is lost when editing on next line.
					sc.SetState(escSeq.outerState);
				}
			} else if (!IsRawString(sc.state) && sc.ch == GetStringQuote(sc.state)) {
				sc.ForwardSetState(SCE_R_DEFAULT);
			} else if (IsRawString(sc.state) && sc.ch == matchingDelimiter) {
				insideUrl = insideUrl && sc.ch != '}';
				sc.Forward();
				int count = dashCount;
				while (count != 0 && sc.ch == '-') {
					--count;
					sc.Forward();
				}
				if (count == 0 && sc.ch == ((sc.state == SCE_R_RAWSTRING_SQ) ? '\'' : '\"')) {
					matchingDelimiter = 0;
					dashCount = 0;
					sc.ForwardSetState(SCE_R_DEFAULT);
				} else {
					continue;
				}
			} else if (sc.state != SCE_R_BACKTICKS) {
				if (sc.ch == '%') {
					const Sci_Position length = CheckFormatSpecifier(sc, styler, insideUrl);
					if (length != 0) {
						const int state = sc.state;
						sc.SetState(SCE_R_FORMAT_SPECIFIER);
						sc.Advance(length);
						sc.SetState(state);
						continue;
					}
				} else if (sc.Match(':', '/', '/') && IsLowerCase(sc.chPrev)) {
					insideUrl = true;
				} else if (insideUrl && IsInvalidUrlChar(sc.ch)) {
					insideUrl = false;
				}
			}
			break;

		case SCE_R_ESCAPECHAR:
			if (escSeq.atEscapeEnd(sc.ch)) {
				if (escSeq.brace && sc.ch == '}') {
					sc.Forward();
				}
				sc.SetState(escSeq.outerState);
				continue;
			}
			break;
		}

		if (sc.state == SCE_R_DEFAULT) {
			if (sc.ch == '#') {
				if (visibleChars == 0 && sc.Match("#line")) {
					sc.SetState(SCE_R_DIRECTIVE);
				} else {
					sc.SetState(SCE_R_COMMENT);
					if (visibleChars == 0) {
						lineStateLineComment = SimpleLineStateMaskLineComment;
					}
				}
			} else if (UnsafeLower(sc.ch) == 'r' && (sc.chNext == '\'' || sc.chNext == '\"')) {
				const int chNext = sc.chNext;
				insideUrl = false;
				matchingDelimiter = CheckRawString(sc, dashCount);
				if (matchingDelimiter) {
					sc.SetState((chNext == '\'') ? SCE_R_RAWSTRING_SQ : SCE_R_RAWSTRING_DQ);
					sc.Advance(dashCount + 2);
				} else {
					sc.SetState(SCE_R_IDENTIFIER);
					sc.ForwardSetState((chNext == '\'') ? SCE_R_STRING_SQ : SCE_R_STRING_DQ);
				}
			} else if (sc.ch == '\"') {
				insideUrl = false;
				sc.SetState(SCE_R_STRING_DQ);
			} else if (sc.ch == '\'') {
				insideUrl = false;
				sc.SetState(SCE_R_STRING_SQ);
			} else if (sc.ch == '`') {
				sc.SetState(SCE_R_BACKTICKS);
			} else if (IsNumberStartEx(sc.chPrev, sc.ch, sc.chNext)) {
				sc.SetState(SCE_R_NUMBER);
			} else if (IsIdentifierStartEx(sc.ch)) {
				chBeforeIdentifier = sc.chPrev;
				sc.SetState(SCE_R_IDENTIFIER);
			} else if (sc.ch == '%') {
				sc.SetState(SCE_R_INFIX);
			} else if (IsAGraphic(sc.ch) && sc.ch != '\\') {
				sc.SetState(SCE_R_OPERATOR);
			}
		}

		if (visibleChars == 0 && !isspacechar(sc.ch)) {
			visibleChars = 1;
		}
		if (sc.atLineEnd) {
			const int lineState = lineStateLineComment | (matchingDelimiter << 1) | (dashCount << 8);
			styler.SetLineState(sc.currentLine, lineState);
			lineStateLineComment = 0;
			visibleChars = 0;
			insideUrl = false;
		}
		sc.Forward();
	}

	sc.Complete();
}

constexpr int GetLineCommentState(int lineState) noexcept {
	return lineState & SimpleLineStateMaskLineComment;
}

static_assert(SCE_R_OPERATOR == SCE_SIMPLE_OPERATOR);
}

namespace Lexilla {

void FoldSimpleDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int /*initStyle*/, LexerWordList /*keywordLists*/, Accessor &styler) {
	const Sci_PositionU endPos = startPos + lengthDoc;
	Sci_Line lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	int lineCommentPrev = 0;
	if (lineCurrent > 0) {
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
		lineCommentPrev = GetLineCommentState(styler.GetLineState(lineCurrent - 1));
	}

	int levelNext = levelCurrent;
	int lineCommentCurrent = GetLineCommentState(styler.GetLineState(lineCurrent));
	Sci_PositionU lineStartNext = styler.LineStart(lineCurrent + 1);
	lineStartNext = sci::min(lineStartNext, endPos);

	while (startPos < endPos) {
		const int style = styler.StyleAt(startPos);

		if (style == SCE_SIMPLE_OPERATOR) {
			const char ch = styler[startPos];
			if (ch == '{' || ch == '[' || ch == '(') {
				levelNext++;
			} else if (ch == '}' || ch == ']' || ch == ')') {
				levelNext--;
			}
		}

		if (++startPos == lineStartNext) {
			const int lineCommentNext = GetLineCommentState(styler.GetLineState(lineCurrent + 1));
			levelNext = sci::max(levelNext, SC_FOLDLEVELBASE);
			if (lineCommentCurrent) {
				levelNext += lineCommentNext - lineCommentPrev;
			}

			const int levelUse = levelCurrent;
			int lev = levelUse | (levelNext << 16);
			if (levelUse < levelNext) {
				lev |= SC_FOLDLEVELHEADERFLAG;
			}
			styler.SetLevel(lineCurrent, lev);

			lineCurrent++;
			lineStartNext = styler.LineStart(lineCurrent + 1);
			lineStartNext = sci::min(lineStartNext, endPos);
			levelCurrent = levelNext;
			lineCommentPrev = lineCommentCurrent;
			lineCommentCurrent = lineCommentNext;
		}
	}
}

}

extern const LexerModule lmRLang(SCLEX_RLANG, ColouriseRDoc, "r", FoldSimpleDoc);

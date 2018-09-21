#include "EditLexer.h"
#include "EditStyle.h"

// https://en.wikipedia.org/wiki/Go_(programming_language)
// https://golang.org/

static KEYWORDLIST Keywords_Go = {{
"break case chan const continue default defer else fallthrough for func go goto "
"if import interface map package range return select struct switch type var "
"false iota nil true "

, // 1 Type Keyword
"bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 "
"rune string uint uintptr uint8 uint16 uint32 uint64 "

, // 2 Preprocessor
""
, // 3 Directive
""

, // 4 Attribute
"append cap close complex copy imag len make new panic print println real recover "

, // 5 Class
""
, // 6 Interface
""
, // 7 Enumeration
""
, // 8 Constant
""

, "", "", "", "", "", "", ""
}};

GCC_NO_WARNING_MISSING_BRACES_BEGIN

EDITLEXER lexGo = { SCLEX_CPP, NP2LEX_GO, EDITLEXER_HOLE(L"Go Source"), L"go", &Keywords_Go,
{
	{ STYLE_DEFAULT, NP2STYLE_Default, L"Default", L"", L"" },
	{ SCE_C_WORD, NP2STYLE_Keyword, L"Keyword", L"fore:#0000FF", L"" },
	{ SCE_C_WORD2, NP2STYLE_TypeKeyword, L"Type Keyword", L"fore:#0000FF", L"" },
	{ SCE_C_ATTRIBUTE, NP2STYLE_BasicFunction, L"Basic Function", L"fore:#FF8000", L"" },
	{ SCE_C_FUNCTION, NP2STYLE_Function, L"Function", L"fore:#A46000", L"" },
	{ MULTI_STYLE(SCE_C_COMMENT, SCE_C_COMMENTLINE, 0, 0), NP2STYLE_Comment, L"Comment", L"fore:#608060", L"" },
	{ SCE_C_COMMENTDOC_TAG, NP2STYLE_DocCommentTag, L"Doc Comment Tag", L"fore:#408080", L"" },
	{ MULTI_STYLE(SCE_C_COMMENTDOC, SCE_C_COMMENTLINEDOC, SCE_C_COMMENTDOC_TAG_XML, 0), NP2STYLE_DocComment, L"Doc Comment", L"fore:#408040", L"" },
	{ MULTI_STYLE(SCE_C_STRING, SCE_C_CHARACTER, SCE_C_STRINGEOL, 0), NP2STYLE_String, L"String", L"fore:#008000", L"" },
	{ SCE_C_DSTRINGB, NP2STYLE_Backticks, L"Backticks", L"fore:#F08000", L"" },
	{ SCE_C_NUMBER, NP2STYLE_Number, L"Number", L"fore:#FF0000", L"" },
	{ SCE_C_OPERATOR, NP2STYLE_Operator, L"Operator", L"fore:#B000B0", L"" },
	EDITSTYLE_SENTINEL
}
};

GCC_NO_WARNING_MISSING_BRACES_END

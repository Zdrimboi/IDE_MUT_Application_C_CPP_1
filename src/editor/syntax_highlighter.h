#pragma once
#include <string>
#include <vector>
#include "imgui.h"
#include <tree_sitter/api.h>

// Forward declarations
typedef struct TSParser TSParser;
typedef struct TSTree TSTree;
typedef struct TSLanguage TSLanguage;

enum class TokenType : uint16_t {
    None,
    Ident,
    NumberLiteral,
    NumberLiteralDark,
    StringLiteral,
    FormatSpecifier,
    EscapedChar,
    StringSeq,
    PrimitiveType,
    Function,
    FunctionCall,
    IdentSub,
    NewType,
    Null,
    Preproc,
    PreprocErr,
    PreprocWar,
    SystemLibString,
    PreprocIdent,
    PreprocArg,
    PreprocArgCall,
    PreprocIdentFunc,
    PreprocIdentVar,
    PreprocOp,
    Keywords1,
    Keywords2,
    Comment,
    CharLiteral,
    Paren1, Paren2, Paren3, Paren4, Paren5, Paren6, Paren7, Paren8,
    Quote,
    Default
};

struct SyntaxToken {
    int line;
    int column;
    int length;
    TokenType type;
    ImVec4 color;
};

struct TextEdit;  // Forward declaration

class SyntaxHighlighter {
public:
    SyntaxHighlighter(const std::string& language);
    ~SyntaxHighlighter();

    std::string LoadFile(const std::string& path);
    std::vector<SyntaxToken> Highlight(const std::string& code);
    std::vector<SyntaxToken> HighlightIncremental(const std::string& code, const std::vector<TextEdit>& edits);

private:
    struct Impl;
    Impl* impl;
};

ImVec4 GetSemanticColor(const std::string& clangCursorKind);
const char* TokenTypeToString(TokenType type);
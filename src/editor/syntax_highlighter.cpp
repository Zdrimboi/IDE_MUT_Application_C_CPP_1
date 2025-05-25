#include "syntax_highlighter.h"
#include <tree_sitter/api.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_set>
#include <array>
#include <cassert>
#include <functional>
#include <span>
#include <text_editor.h>

// Link to your language grammar here
extern "C" const TSLanguage* tree_sitter_c();
extern "C" const TSLanguage* tree_sitter_cpp();

// Centralized color table with descriptive names
struct TokenColorEntry {
    TokenType type;
    const char* name;
    ImVec4 color;
};

constexpr TokenColorEntry kTokenColorTable[] = {
    {TokenType::Preproc,            "preprocessor",             ImVec4(0.5f, 0.5f, 0.5f, 1.0f)},
    {TokenType::PreprocErr,         "preprocessor_error",       ImVec4(1.0f, 0.0f, 0.0f, 1.0f)},
    {TokenType::PreprocWar,         "preprocessor_warning",     ImVec4(1.0f, 1.0f, 0.0f, 1.0f)},
    {TokenType::SystemLibString,    "system_include_path",      ImVec4(1.0f, 0.55f, 0.0f, 1.0f)},
    {TokenType::Quote,              "string_quote",             ImVec4(1.0f, 0.85f, 0.0f, 1.0f)},
    {TokenType::PreprocIdent,       "preprocessor_macro",       ImVec4(0.9f, 0.5f, 1.0f, 1.0f)},
    {TokenType::PreprocArg,         "preprocessor_arg",         ImVec4(0.8f, 1.0f, 0.5f, 1.0f)},
    {TokenType::PreprocArgCall,     "preprocessor_arg_call",    ImVec4(0.5f, 0.0f, 1.0f, 1.0f)},
    {TokenType::PreprocIdentFunc,   "preprocessor_func",        ImVec4(1.0f, 1.0f, 0.5f, 1.0f)},
    {TokenType::Ident,              "identifier",               ImVec4(0.5f, 0.75f, 1.0f, 1.0f)},
    {TokenType::PreprocIdentVar,    "preprocessor_var",         ImVec4(0.5f, 0.75f, 1.0f, 1.0f)},
    {TokenType::Keywords1,          "keyword_control",          ImVec4(0.9f, 0.5f, 1.0f, 1.0f)},
    {TokenType::Keywords2,          "keyword_type",             ImVec4(0.45f, 0.69f, 0.70f, 1.0f)},
    {TokenType::Comment,            "comment",                  ImVec4(0.0f, 1.0f, 0.0f, 1.0f)},
    {TokenType::NumberLiteral,      "number",                   ImVec4(0.8f, 1.0f, 0.5f, 1.0f)},
    {TokenType::StringLiteral,      "string",                   ImVec4(1.0f, 0.55f, 0.0f, 1.0f)},
    {TokenType::StringSeq,          "string_escape",            ImVec4(0.8f, 1.0f, 0.5f, 1.0f)},
    {TokenType::PrimitiveType,      "primitive_type",           ImVec4(0.45f, 0.69f, 0.70f, 1.0f)},
    {TokenType::Function,           "function",                 ImVec4(1.0f, 1.0f, 0.0f, 1.0f)},
    {TokenType::FunctionCall,       "function_call",            ImVec4(1.0f, 1.0f, 0.5f, 1.0f)},
    {TokenType::NumberLiteralDark,  "number_suffix",            ImVec4(0.5f, 0.8f, 0.3f, 1.0f)},
    {TokenType::CharLiteral,        "char",                     ImVec4(1.0f, 0.85f, 0.0f, 1.0f)},
    {TokenType::IdentSub,           "field_identifier",         ImVec4(0.7f, 0.8f, 1.0f, 1.0f)},
    {TokenType::NewType,            "type_name",                ImVec4(0.4f, 0.7f, 0.2f, 1.0f)},
    {TokenType::Null,               "null_literal",             ImVec4(0.5f, 0.0f, 0.5f, 1.0f)},
    {TokenType::FormatSpecifier,    "string_format",            ImVec4(0.5f, 0.75f, 1.0f, 1.0f)},
    {TokenType::EscapedChar,        "string_escape_char",       ImVec4(1.0f, 0.85f, 0.0f, 1.0f)},
    {TokenType::Paren1,             "rainbow_paren_1",          ImVec4(1.0f, 0.85f, 0.0f, 1.0f)},
    {TokenType::Paren2,             "rainbow_paren_2",          ImVec4(1.0f, 0.5f, 0.5f, 1.0f)},
    {TokenType::Paren3,             "rainbow_paren_3",          ImVec4(1.0f, 0.7f, 0.5f, 1.0f)},
    {TokenType::Paren4,             "rainbow_paren_4",          ImVec4(0.8f, 0.8f, 0.8f, 1.0f)},
    {TokenType::Paren5,             "rainbow_paren_5",          ImVec4(0.5f, 0.5f, 0.8f, 1.0f)},
    {TokenType::Paren6,             "rainbow_paren_6",          ImVec4(1.0f, 0.5f, 0.8f, 1.0f)},
    {TokenType::Paren7,             "rainbow_paren_7",          ImVec4(0.8f, 0.5f, 0.8f, 1.0f)},
    {TokenType::Paren8,             "rainbow_paren_8",          ImVec4(0.8f, 0.8f, 0.5f, 1.0f)},
    {TokenType::Quote,              "string_quote",             ImVec4(1.0f, 0.85f, 0.0f, 1.0f)},
    {TokenType::Default,            "default",                  ImVec4(0.83f, 0.83f, 0.83f, 1.0f)}
};

constexpr size_t kTokenColorTableSize = std::size(kTokenColorTable);

// Utility: get name from TokenType
inline const char* TokenTypeToString(TokenType type) {
    for (const auto& entry : std::span(kTokenColorTable, kTokenColorTableSize))
        if (entry.type == type)
            return entry.name;
    return "unknown";
}

// Utility: get color from TokenType
ImVec4 GetColorForCapture(TokenType type) {
    for (const auto& entry : std::span(kTokenColorTable, kTokenColorTableSize))
        if (entry.type == type)
            return entry.color;
    // fallback to default
    return kTokenColorTable[kTokenColorTableSize - 1].color;
}

// Save color table to file
void SaveTokenColorsToFile(const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) return;
    out << "{\n";
    for (size_t i = 0; i < kTokenColorTableSize; ++i) {
        const auto& entry = kTokenColorTable[i];
        out << "  \"" << entry.name << "\": ["
            << entry.color.x << ", "
            << entry.color.y << ", "
            << entry.color.z << ", "
            << entry.color.w << "]";
        if (i + 1 < kTokenColorTableSize) out << ",";
        out << "\n";
    }
    out << "}\n";
}

ImVec4 GetSemanticColor(const std::string& kind) {
    if (kind == "FunctionDecl")    return ImVec4(1.00f, 0.80f, 0.30f, 1.0f);
    if (kind == "VarDecl")         return ImVec4(0.85f, 0.85f, 0.60f, 1.0f);
    if (kind == "ParmDecl")        return ImVec4(0.70f, 0.90f, 0.90f, 1.0f);
    if (kind == "FieldDecl")       return ImVec4(0.60f, 0.90f, 0.60f, 1.0f);
    if (kind == "MemberRefExpr")   return ImVec4(0.60f, 0.70f, 1.00f, 1.0f);
    return GetColorForCapture(TokenType::Default);
}
// --- Helper: classify_number_literal (unchanged, but returns TokenType) ---
std::vector<std::pair<std::string, TokenType>> classify_number_literal(const std::string& token) {
    static const std::regex hex_regex(R"(^0[xX]([0-9a-fA-F']+)([uUlL]*)$)", std::regex::optimize);
    static const std::regex bin_regex(R"(^0[bB]([01']+)([uUlL]*)$)", std::regex::optimize);
    static const std::regex oct_regex(R"(^0([0-7']+)([uUlL]*)$)", std::regex::optimize);
    static const std::regex float_regex(R"(^([0-9]*\.[0-9]+([eE][+-]?[0-9]+)?)([fFlL]*)$)", std::regex::optimize);
    static const std::regex int_suf_regex(R"(^([0-9][0-9']*)([uUlL]+)$)", std::regex::optimize);
    static const std::regex int_regex(R"(^[0-9][0-9']*$)", std::regex::optimize);

    std::vector<std::pair<std::string, TokenType>> parts;
    std::smatch m;

    if (std::regex_match(token, m, hex_regex)) {
        parts.emplace_back(token.substr(0, 2), TokenType::NumberLiteralDark);
        parts.emplace_back(m[1].str(), TokenType::NumberLiteral);
        if (m[2].length() > 0)
            parts.emplace_back(m[2].str(), TokenType::NumberLiteralDark);
        return parts;
    }
    if (std::regex_match(token, m, bin_regex)) {
        parts.emplace_back(token.substr(0, 2), TokenType::NumberLiteralDark);
        parts.emplace_back(m[1].str(), TokenType::NumberLiteral);
        if (m[2].length() > 0)
            parts.emplace_back(m[2].str(), TokenType::NumberLiteralDark);
        return parts;
    }
    if (std::regex_match(token, m, oct_regex)) {
        parts.emplace_back(token.substr(0, 1), TokenType::NumberLiteralDark);
        parts.emplace_back(m[1].str(), TokenType::NumberLiteral);
        if (m[2].length() > 0)
            parts.emplace_back(m[2].str(), TokenType::NumberLiteralDark);
        return parts;
    }
    if (std::regex_match(token, m, float_regex)) {
        parts.emplace_back(m[1].str(), TokenType::NumberLiteral);
        if (m[3].length() > 0)
            parts.emplace_back(m[3].str(), TokenType::NumberLiteralDark);
        return parts;
    }
    if (std::regex_match(token, m, int_suf_regex)) {
        parts.emplace_back(m[1].str(), TokenType::NumberLiteral);
        parts.emplace_back(m[2].str(), TokenType::NumberLiteralDark);
        return parts;
    }
    if (std::regex_match(token, int_regex)) {
        parts.emplace_back(token, TokenType::NumberLiteral);
        return parts;
    }
    parts.emplace_back(token, TokenType::NumberLiteral);
    return parts;
}

// --- Helper: classify_string_content ---
void classify_string_content(
    const std::string_view& text,
    int line,
    int start_col,
    std::vector<SyntaxToken>& tokens
) {
    static const std::regex special_regex(R"((%[-+#0-9.]*[a-zA-Z])|(\\[\\'\"abfnrtv]))", std::regex::optimize);
    std::string s(text);
    int col = start_col;
    size_t last = 0;
    for (std::sregex_iterator it(s.begin(), s.end(), special_regex), end; it != end; ++it) {
        auto match = *it;
        size_t match_pos = match.position();
        size_t match_len = match.length();
        if (match_pos > last) {
            tokens.push_back({
                line,
                col,
                static_cast<int>(match_pos - last),
                TokenType::StringLiteral,
                GetColorForCapture(TokenType::StringLiteral)
                });
            col += static_cast<int>(match_pos - last);
        }
        TokenType match_type = (match.str()[0] == '%') ? TokenType::FormatSpecifier : TokenType::EscapedChar;
        tokens.push_back({
            line,
            col,
            static_cast<int>(match_len),
            match_type,
            GetColorForCapture(match_type)
            });
        col += static_cast<int>(match_len);
        last = match_pos + match_len;
    }
    if (last < s.size()) {
        tokens.push_back({
            line,
            col,
            static_cast<int>(s.size() - last),
            TokenType::StringLiteral,
            GetColorForCapture(TokenType::StringLiteral)
            });
    }
}

void regex_colorization(
    const std::string& code_fragment,
    int base_line,
    int base_column,
    const std::vector<TokenType>& paren_colors,
    std::vector<SyntaxToken>& tokens
) {
    static const std::regex token_regex(
        R"((\"([^\"\\]|\\.)*\")|(0[xX][0-9a-fA-F']+[uUlL]*|0[bB][01']+[uUlL]*|0[0-7']+[uUlL]*|[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?[fFlL]*|[0-9][0-9']*[uUlL]*|[a-zA-Z_]\w*|[(){}[\]+\-*/%&|^~!=<>?:,.;#]|\\|\.\.\.))",
        std::regex::optimize
    );
    static const std::regex string_regex(R"(^\"([^\"\\]|\\.)*\"$)", std::regex::optimize);
    static const std::regex number_regex(
        R"(^(0[xX][0-9a-fA-F']+[uUlL]*|0[bB][01']+[uUlL]*|0[0-7']+[uUlL]*|[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?[fFlL]*|[0-9][0-9']*[uUlL]*)$)",
        std::regex::optimize
    );
    static const std::regex ident_regex(R"([a-zA-Z_]\w*)", std::regex::optimize);
    static const std::regex hex_prefix(R"(^0[xX])", std::regex::optimize);
    static const std::regex bin_prefix(R"(^0[bB])", std::regex::optimize);
    static const std::regex num_suffix(R"([uUlL]+$)", std::regex::optimize);

    static const std::unordered_set<std::string> keywords_1 = {
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return", "goto"
    };
    static const std::unordered_set<std::string> keywords_2 = {
        "static", "const", "extern", "register", "auto", "volatile", "inline", "restrict", "typedef"
    };

    std::vector<TokenType> local_paren_stack;
    std::vector<TokenType> local_brace_stack;

    int line = base_line;
    int column = base_column;
    size_t last_token_end = 0;

    auto words_begin = std::sregex_iterator(code_fragment.begin(), code_fragment.end(), token_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        const std::smatch& match = *i;
        const std::string& token = match.str();
        int token_offset = static_cast<int>(match.position());
        int length = static_cast<int>(match.length());

        for (size_t idx = last_token_end; idx < static_cast<size_t>(token_offset); ++idx) {
            if (code_fragment[idx] == '\n') {
                ++line;
                column = 0;
            }
            else {
                ++column;
            }
        }
        last_token_end = token_offset + length;

        TokenType colorType = TokenType::Default;
        if (std::regex_match(token, string_regex)) {
            colorType = TokenType::StringLiteral;
        }
        else if (token == "(") {
            int color_idx = static_cast<int>(local_paren_stack.size()) % paren_colors.size();
            colorType = paren_colors[color_idx];
            local_paren_stack.push_back(colorType);
        }
        else if (token == ")") {
            TokenType color = local_paren_stack.empty() ? paren_colors[0] : local_paren_stack.back();
            colorType = color;
            if (!local_paren_stack.empty()) local_paren_stack.pop_back();
        }
        else if (token == "{") {
            int color_idx = static_cast<int>(local_brace_stack.size()) % paren_colors.size();
            colorType = paren_colors[color_idx];
            local_brace_stack.push_back(colorType);
        }
        else if (token == "}") {
            TokenType color = local_brace_stack.empty() ? paren_colors[0] : local_brace_stack.back();
            colorType = color;
            if (!local_brace_stack.empty()) local_brace_stack.pop_back();
        }
        else if (std::regex_match(token, number_regex)) {
            if (
                std::regex_search(token, hex_prefix) ||
                std::regex_search(token, bin_prefix) ||
                std::regex_search(token, num_suffix)
                ) {
                colorType = TokenType::NumberLiteralDark;
            }
            else {
                colorType = TokenType::NumberLiteral;
            }
        }
        else if (std::regex_match(token, ident_regex)) {
            bool is_func = false;
            size_t after = match.position() + match.length();
            while (after < code_fragment.size() && isspace(static_cast<unsigned char>(code_fragment[after]))) ++after;
            if (after < code_fragment.size() && code_fragment[after] == '(') is_func = true;

            if (keywords_1.count(token)) colorType = TokenType::Keywords1;
            else if (keywords_2.count(token)) colorType = TokenType::Keywords2;
            else if (is_func) colorType = TokenType::PreprocIdentFunc;
            else colorType = TokenType::PreprocIdentVar;
        }
        else {
            colorType = TokenType::PreprocOp;
        }

        tokens.push_back({
            line,
            column,
            length,
            colorType,
            GetColorForCapture(colorType)
            });

        column += length;
    }
}

struct SyntaxHighlighter::Impl {
    TSParser* parser = nullptr;
    TSTree* tree = nullptr;
    const TSLanguage* language = nullptr;
    std::string Llang;
    std::string last_code;  // Cache last code for incremental updates

    Impl(const std::string& lang) {
        parser = ts_parser_new();
        if (lang == "c") language = tree_sitter_c();
        else if (lang == "cpp") language = tree_sitter_cpp();
        ts_parser_set_language(parser, language);
        Llang = lang;

    }

    ~Impl() {
        if (tree) ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    std::string LoadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buf;
        buf << file.rdbuf();
        std::string txt = buf.str();
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        if (txt.size() >= 3 &&
            (unsigned char)txt[0] == bom[0] &&
            (unsigned char)txt[1] == bom[1] &&
            (unsigned char)txt[2] == bom[2])
        {
            txt.erase(0, 3);
        }
        return txt;
    }

    std::vector<TokenType> paren_colors = {
        TokenType::Paren1, TokenType::Paren2, TokenType::Paren3, TokenType::Paren4,
        TokenType::Paren5, TokenType::Paren6, TokenType::Paren7, TokenType::Paren8
    };

    std::vector<SyntaxToken> Highlight(const std::string& code) {
        // Reserve a reasonable amount to avoid reallocations
        std::vector<SyntaxToken> tokens;
        tokens.reserve(code.size() / 4);

        if (tree) ts_tree_delete(tree);
        tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
        if (!tree) return tokens;
        TSNode root = ts_tree_root_node(tree);

        std::vector<TokenType> paren_stack;
        std::vector<TokenType> brace_stack;

        // Helper: map type string to TokenType (for fast dispatch)
        static const std::unordered_map<std::string_view, TokenType> type_map = {
            {"identifier", TokenType::Ident},
            {"number_literal", TokenType::NumberLiteral},
            {"string_literal", TokenType::StringLiteral},
            {"string_content", TokenType::StringLiteral},
            {"escape_sequence", TokenType::StringSeq},
            {"typedef", TokenType::PrimitiveType},
            {"primitive_type", TokenType::PrimitiveType},
            {"type_identifier", TokenType::NewType},
            {"character", TokenType::CharLiteral},
            {"comment", TokenType::Comment},
            {"field_identifier", TokenType::IdentSub},
            {"NULL", TokenType::Null},
            {"sizeof", TokenType::FunctionCall},
            {"statement_identifier", TokenType::Keywords1},
            {"preproc_directive", TokenType::Preproc},
            {"defined", TokenType::Preproc},
            {"system_lib_string", TokenType::SystemLibString},
            {"preproc_arg", TokenType::PreprocArg},
            {"preproc_function_def", TokenType::PreprocIdentFunc},
            {"preproc_def", TokenType::PreprocIdent},
            {"preproc_ifdef", TokenType::PreprocIdent},
            {"preproc_defined", TokenType::PreprocIdent},
            {"preproc_arg_call", TokenType::PreprocArgCall},
            {"preproc_ident_func", TokenType::PreprocIdentFunc},
            {"preproc_ident_var", TokenType::PreprocIdentVar},
            {"preproc_op", TokenType::PreprocOp},
            {"preproc_err", TokenType::PreprocErr},
            {"preproc_war", TokenType::PreprocWar},
            {"function", TokenType::Function},
            {"function_call", TokenType::FunctionCall},
            {"Keywords_1", TokenType::Keywords1},
            {"Keywords_2", TokenType::Keywords2},
            {"'", TokenType::StringLiteral},
            {"\"", TokenType::Quote}
        };

        // Helper: keyword sets
        static const std::unordered_set<std::string_view> keywords_1 = {
            "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return", "goto", "default", "_Generic"
        };
        static const std::unordered_set<std::string_view> keywords_2 = {
            "static", "const", "extern", "register", "auto", "volatile", "inline", "restrict", "typedef", "struct", "enum", "union", "unsigned", "long", "_Noreturn", "_Alignof"
        };

        // Recursive visit
        std::function<void(TSNode)> visit = [&](TSNode node) {
            uint32_t child_count = ts_node_child_count(node);
            std::string_view type(ts_node_type(node));
            TSNode parent = ts_node_parent(node);
            std::string_view parent_type;
            if (parent.id) {
                parent_type = ts_node_type(parent);
            }
            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            uint32_t start_byte = ts_node_start_byte(node);
            uint32_t end_byte = ts_node_end_byte(node);

            std::string_view text;
            if (end_byte > start_byte)
                text = std::string_view(code.data() + start_byte, end_byte - start_byte);

            if (child_count == 0) {
                if (type.empty() || (text.find_first_not_of(" \t\r\n") == std::string_view::npos))
                    return;

                TokenType colorType = TokenType::Default;

                // Fast path for identifier
                if (type == "identifier") {
                    if (parent_type == "function_declarator")
                        colorType = TokenType::Function;
                    else if (parent_type == "call_expression")
                        colorType = TokenType::FunctionCall;
                    else if (parent_type == "preproc_function_def")
                        colorType = TokenType::PreprocIdentFunc;
                    else if (parent_type == "preproc_def" || parent_type == "preproc_ifdef" || parent_type == "preproc_defined")
                        colorType = TokenType::PreprocIdent;
                    else
                        colorType = TokenType::Ident;
                }
                // Number literal
                else if (type == "number_literal") {
                    int col = static_cast<int>(start.column);
                    auto parts = classify_number_literal(std::string(text));
                    for (const auto& [part_text, part_type] : parts) {
                        tokens.push_back({
                            static_cast<int>(start.row) + 1,
                            col,
                            static_cast<int>(part_text.length()),
                            part_type,
                            GetColorForCapture(part_type)
                            });
                        col += static_cast<int>(part_text.length());
                    }
                    return;
                }
                // Comment
                else if (type == "comment") {
                    size_t pos = 0;
                    int line = static_cast<int>(start.row) + 1;
                    int col = static_cast<int>(start.column);
                    std::string_view comment_text = text;
                    size_t next;
                    while ((next = comment_text.find('\n', pos)) != std::string_view::npos) {
                        int len = static_cast<int>(next - pos);
                        tokens.push_back({
                            line,
                            col,
                            len,
                            TokenType::Comment,
                            GetColorForCapture(TokenType::Comment)
                            });
                        pos = next + 1;
                        line++;
                        col = 0;
                    }
                    if (pos < comment_text.size()) {
                        int len = static_cast<int>(comment_text.size() - pos);
                        tokens.push_back({
                            line,
                            col,
                            len,
                            TokenType::Comment,
                            GetColorForCapture(TokenType::Comment)
                            });
                    }
                    return;
                }
                // String content (with format/escape highlighting)
                else if (type == "string_content") {
                    classify_string_content(
                        text,
                        static_cast<int>(start.row) + 1,
                        static_cast<int>(start.column),
                        tokens
                    );
                    return;
                }
                // String literal (the quotes themselves)
                else if (type == "string_literal") {
                    colorType = TokenType::StringLiteral;
                }
                // Preprocessor directives
                else if (type == "#include" || type == "#define" || type == "#undef" || type == "#ifdef" ||
                    type == "#ifndef" || type == "#endif" || type == "#else" || type == "#if" || type == "#elif")
                    colorType = TokenType::Preproc;
                else if (type == "preproc_directive" && text == "#warning")
                    colorType = TokenType::PreprocWar;
                else if (type == "preproc_directive" && text == "#error")
                    colorType = TokenType::PreprocErr;
                else if (type == "preproc_directive")
                    colorType = TokenType::Preproc;
                else if (type == "defined")
                    colorType = TokenType::Preproc;
                else if (type == "system_lib_string")
                    colorType = TokenType::SystemLibString;
                else if (type == "string_content" && parent.id && std::string_view(ts_node_type(ts_node_parent(parent))) == "preproc_include")
                    colorType = TokenType::SystemLibString;
                else if (type == "preproc_arg" && parent_type == "preproc_def")
                    colorType = TokenType::PreprocArg;
                else if (type == "preproc_arg") {
                    if (!text.empty() && text.find_first_not_of(" \t\n\r") != std::string_view::npos) {
                        regex_colorization(
                            std::string(text),
                            static_cast<int>(start.row) + 1,
                            static_cast<int>(start.column),
                            paren_colors,
                            tokens
                        );
                        return;
                    }
                    return;
                }
                else if (type == "field_identifier" && (parent_type == "field_expression" || parent_type == "field_designator"))
                    colorType = TokenType::IdentSub;
                else if (type == "escape_sequence")
                    colorType = TokenType::StringSeq;
                else if (type == "typedef" || type == "primitive_type")
                    colorType = TokenType::PrimitiveType;
                else if (type == "type_identifier")
                    colorType = TokenType::NewType;
                else if (type == "character" && parent_type == "char_literal")
                    colorType = TokenType::CharLiteral;
                else if (type == "'")
                    colorType = TokenType::StringLiteral;
                else if (type == "NULL")
                    colorType = TokenType::Null;
                else if (keywords_1.count(type))
                    colorType = TokenType::Keywords1;
                else if (keywords_2.count(type))
                    colorType = TokenType::Keywords2;
                else if (type == "sizeof")
                    colorType = TokenType::FunctionCall;
                else if (type == "statement_identifier")
                    colorType = TokenType::Keywords1;
                // Rainbow parentheses/braces
                else if (type == "(") {
                    int color_idx = static_cast<int>(paren_stack.size()) % paren_colors.size();
                    colorType = paren_colors[color_idx];
                    paren_stack.push_back(colorType);
                }
                else if (type == ")") {
                    TokenType color = paren_stack.empty() ? paren_colors[0] : paren_stack.back();
                    colorType = color;
                    if (!paren_stack.empty()) paren_stack.pop_back();
                }
                else if (type == "{") {
                    int color_idx = static_cast<int>(brace_stack.size()) % paren_colors.size();
                    colorType = paren_colors[color_idx];
                    brace_stack.push_back(colorType);
                }
                else if (type == "}") {
                    TokenType color = brace_stack.empty() ? paren_colors[0] : brace_stack.back();
                    colorType = color;
                    if (!brace_stack.empty()) brace_stack.pop_back();
                }
                else if (type == "\"")
                    colorType = TokenType::Quote;
                else
                    colorType = TokenType::Default;

                tokens.push_back({
                    static_cast<int>(start.row) + 1,
                    static_cast<int>(start.column),
                    static_cast<int>(end.column - start.column),
                    colorType,
                    GetColorForCapture(colorType)
                    });
            }
            else {
                for (uint32_t i = 0; i < child_count; ++i)
                    visit(ts_node_child(node, i));
            }
            };

        visit(root);
        return tokens;
    }

    std::vector<SyntaxToken> HighlightIncremental(const std::string& code, const std::vector<TextEdit>& edits) {
        // If we have a tree and edits, update incrementally
        if (tree && !edits.empty() && code.size() > 0 && last_code.size() > 0) {
            // Apply edits to the tree
            for (const auto& edit : edits) {
                TSInputEdit ts_edit;
                ts_edit.start_byte = edit.start_byte;
                ts_edit.old_end_byte = edit.old_end_byte;
                ts_edit.new_end_byte = edit.new_end_byte;
                ts_edit.start_point = edit.start_point;
                ts_edit.old_end_point = edit.old_end_point;
                ts_edit.new_end_point = edit.new_end_point;

                ts_tree_edit(tree, &ts_edit);
            }

            // Reparse incrementally
            TSTree* new_tree = ts_parser_parse_string(parser, tree, code.c_str(), code.size());
            if (new_tree) {
                ts_tree_delete(tree);
                tree = new_tree;
            }
        }
        else {
            // Full reparse
            if (tree) ts_tree_delete(tree);
            tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
        }

        last_code = code;

        // Now perform highlighting with the updated tree
        if (!tree) return {};

        std::vector<SyntaxToken> tokens;
        tokens.reserve(code.size() / 4);

        TSNode root = ts_tree_root_node(tree);

        // Reuse the existing highlighting logic
        std::vector<TokenType> paren_stack;
        std::vector<TokenType> brace_stack;

        // Your existing highlighting logic here...
        // (Just call the regular Highlight method since the tree is already updated)
        return Highlight(code);
    }
};

SyntaxHighlighter::SyntaxHighlighter(const std::string& language) {
    impl = new Impl(language);
}

SyntaxHighlighter::~SyntaxHighlighter() {
    delete impl;
}

std::string SyntaxHighlighter::LoadFile(const std::string& path) {
    return impl->LoadFile(path);
}

std::vector<SyntaxToken> SyntaxHighlighter::Highlight(const std::string& code) {
    return impl->Highlight(code);
}
std::vector<SyntaxToken> SyntaxHighlighter::HighlightIncremental(const std::string& code, const std::vector<TextEdit>& edits) {
    return impl->HighlightIncremental(code, edits);
}

class StringInterner {
    std::unordered_map<std::string_view, std::shared_ptr<std::string>> interned_;
public:
    const std::string* intern(std::string_view str) {
        auto it = interned_.find(str);
        if (it != interned_.end()) {
            return it->second.get();
        }
        auto shared = std::make_shared<std::string>(str);
        interned_[*shared] = shared;
        return shared.get();
    }
};

//// Token pool for reduced allocations
//class TokenPool {
//    std::vector<std::vector<SyntaxToken>> pool_;
//    size_t current_pool_ = 0;
//    size_t current_index_ = 0;
//    static constexpr size_t POOL_SIZE = 1000;
//
//public:
//    TokenPool() {
//        pool_.push_back(std::vector<SyntaxToken>());
//        pool_[0].reserve(POOL_SIZE);
//    }
//
//    SyntaxToken* allocate() {
//        if (current_index_ >= pool_[current_pool_].size()) {
//            pool_[current_pool_].resize(pool_[current_pool_].size() + 1);
//        }
//        return &pool_[current_pool_][current_index_++];
//    }
//
//    void reset() {
//        current_pool_ = 0;
//        current_index_ = 0;
//    }
//
//    std::vector<SyntaxToken> harvest() {
//        std::vector<SyntaxToken> result;
//        result.reserve(current_index_);
//        for (size_t i = 0; i < current_index_; ++i) {
//            result.push_back(pool_[current_pool_][i]);
//        }
//        return result;
//    }
//};
//
//// Optimized regex matching with caching
//class RegexCache {
//    struct CachedMatch {
//        size_t text_hash;
//        std::vector<std::pair<size_t, size_t>> matches; // position, length
//    };
//
//    std::unordered_map<size_t, CachedMatch> cache_;
//    static constexpr size_t MAX_CACHE_SIZE = 100;
//
//public:
//    bool get_matches(const std::string& text, const std::regex& regex,
//        std::vector<std::pair<size_t, size_t>>& matches) {
//        size_t hash = std::hash<std::string>{}(text);
//        auto it = cache_.find(hash);
//        if (it != cache_.end()) {
//            matches = it->second.matches;
//            return true;
//        }
//
//        // Compute matches
//        matches.clear();
//        auto words_begin = std::sregex_iterator(text.begin(), text.end(), regex);
//        auto words_end = std::sregex_iterator();
//
//        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
//            matches.push_back({ i->position(), i->length() });
//        }
//
//        // Cache result
//        if (cache_.size() >= MAX_CACHE_SIZE) {
//            cache_.clear();
//        }
//        cache_[hash] = { hash, matches };
//
//        return false;
//    }
//};
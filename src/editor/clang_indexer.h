#pragma once
#include <string>
#include <vector>

struct Symbol {
    std::string name;
    int line;
    int column;
    std::string kind;
};

class ClangIndexer {
public:
    std::vector<Symbol> Index(const std::string& filepath, const std::string& code);
    static void Cleanup();  // Add static cleanup method
};
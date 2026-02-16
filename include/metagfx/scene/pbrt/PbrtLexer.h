// ============================================================================
// include/metagfx/scene/pbrt/PbrtLexer.h
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <fstream>

namespace metagfx {

enum class PbrtTokenType {
    Ident,      // identifier / keyword  (e.g. WorldBegin, Shape)
    String,     // quoted string content (e.g. "trianglemesh")
    Number,     // numeric literal       (e.g. 39.3, -1.5, 128)
    LBracket,   // [
    RBracket,   // ]
    LBrace,     // {
    RBrace,     // }
    Eof         // end of all input
};

struct PbrtToken {
    PbrtTokenType type  = PbrtTokenType::Eof;
    std::string   value;
    int           line  = 0;
    std::string   file;
};

/**
 * @brief Tokenizer for PBRT v4 scene files.
 *
 * Supports a file stack for recursive Include directives.
 * The lexer is consumed by PbrtParser; users only call Next()/Peek().
 */
class PbrtLexer {
public:
    explicit PbrtLexer(const std::string& filepath);
    ~PbrtLexer();

    // Non-copyable, non-movable (owns open file streams)
    PbrtLexer(const PbrtLexer&)            = delete;
    PbrtLexer& operator=(const PbrtLexer&) = delete;

    /** Consume and return the next token. */
    PbrtToken Next();

    /** Return the next token without consuming it. */
    PbrtToken Peek();

    /** Push a new file onto the include stack (called by parser on Include). */
    void PushFile(const std::string& filepath);

    /** Return "filepath:line" string for error messages. */
    std::string CurrentLocation() const;

private:
    struct FileState {
        std::ifstream stream;
        int           line     = 1;
        std::string   path;
        bool          hasUnget = false;
        char          ungetCh  = 0;

        explicit FileState(const std::string& p)
            : stream(p), path(p) {}
    };

    std::vector<std::unique_ptr<FileState>> m_Stack;
    std::optional<PbrtToken>                m_Lookahead;

    PbrtToken ReadNext();

    // Character-level helpers operating on top of m_Stack
    char GetChar();
    void UngetChar(char c);
};

} // namespace metagfx

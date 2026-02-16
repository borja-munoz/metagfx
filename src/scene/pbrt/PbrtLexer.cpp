// ============================================================================
// src/scene/pbrt/PbrtLexer.cpp
// ============================================================================
#include "metagfx/scene/pbrt/PbrtLexer.h"
#include "metagfx/core/Logger.h"

#include <cctype>
#include <stdexcept>

namespace metagfx {

PbrtLexer::PbrtLexer(const std::string& filepath) {
    auto fs = std::make_unique<FileState>(filepath);
    if (!fs->stream.is_open()) {
        throw std::runtime_error("PbrtLexer: cannot open file: " + filepath);
    }
    m_Stack.push_back(std::move(fs));
}

PbrtLexer::~PbrtLexer() = default;

// ─────────────────────────────────────────────────────────────────────────────

std::string PbrtLexer::CurrentLocation() const {
    if (m_Stack.empty()) return "<eof>";
    const auto& fs = *m_Stack.back();
    return fs.path + ":" + std::to_string(fs.line);
}

void PbrtLexer::PushFile(const std::string& filepath) {
    auto fs = std::make_unique<FileState>(filepath);
    if (!fs->stream.is_open()) {
        METAGFX_WARN << "PbrtLexer: cannot open included file: " << filepath;
        return;
    }
    m_Stack.push_back(std::move(fs));
}

// ─── Character helpers ────────────────────────────────────────────────────────

char PbrtLexer::GetChar() {
    while (!m_Stack.empty()) {
        auto& fs = *m_Stack.back();

        if (fs.hasUnget) {
            fs.hasUnget = false;
            return fs.ungetCh;
        }

        int c = fs.stream.get();
        if (c != std::char_traits<char>::eof()) {
            if (c == '\n') fs.line++;
            return static_cast<char>(c);
        }

        // Current file exhausted — pop back to parent
        m_Stack.pop_back();
    }
    return '\0';  // All files exhausted
}

void PbrtLexer::UngetChar(char c) {
    if (m_Stack.empty()) return;
    auto& fs = *m_Stack.back();
    fs.ungetCh  = c;
    fs.hasUnget = true;
    if (c == '\n' && fs.line > 1) fs.line--;
}

// ─── Tokenizer ────────────────────────────────────────────────────────────────

PbrtToken PbrtLexer::ReadNext() {
    // Skip whitespace and # comments
    while (true) {
        char c = GetChar();
        if (c == '\0') {
            return { PbrtTokenType::Eof, "", 0, "" };
        }
        if (c == '#') {
            // Line comment — skip to end of line
            while (c != '\n' && c != '\0') c = GetChar();
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) continue;

        // Record location from the top of the stack
        int lineNo = m_Stack.empty() ? 0 : m_Stack.back()->line;
        std::string filePath = m_Stack.empty() ? "" : m_Stack.back()->path;

        // ── Single-character tokens ──────────────────────────────────────────
        if (c == '[') return { PbrtTokenType::LBracket, "[", lineNo, filePath };
        if (c == ']') return { PbrtTokenType::RBracket, "]", lineNo, filePath };
        if (c == '{') return { PbrtTokenType::LBrace,   "{", lineNo, filePath };
        if (c == '}') return { PbrtTokenType::RBrace,   "}", lineNo, filePath };

        // ── Quoted string ────────────────────────────────────────────────────
        if (c == '"') {
            std::string s;
            while (true) {
                char d = GetChar();
                if (d == '"' || d == '\0') break;
                if (d == '\n') {
                    METAGFX_WARN << "PbrtLexer: newline inside quoted string at "
                                 << filePath << ":" << lineNo;
                    break;
                }
                s += d;
            }
            return { PbrtTokenType::String, s, lineNo, filePath };
        }

        // ── Identifier (letter or underscore start) ──────────────────────────
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string s;
            s += c;
            while (true) {
                char d = GetChar();
                if (std::isalnum(static_cast<unsigned char>(d)) || d == '_') {
                    s += d;
                } else {
                    UngetChar(d);
                    break;
                }
            }
            return { PbrtTokenType::Ident, s, lineNo, filePath };
        }

        // ── Number: digit, '-', or '.' ───────────────────────────────────────
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '.') {
            std::string s;
            s += c;
            while (true) {
                char d = GetChar();
                if (std::isdigit(static_cast<unsigned char>(d)) ||
                    d == '.' || d == 'e' || d == 'E' ||
                    d == '+' || d == '-')
                {
                    // Allow sign only immediately after exponent marker
                    if ((d == '+' || d == '-') && !s.empty() &&
                        s.back() != 'e' && s.back() != 'E')
                    {
                        UngetChar(d);
                        break;
                    }
                    s += d;
                } else {
                    UngetChar(d);
                    break;
                }
            }
            return { PbrtTokenType::Number, s, lineNo, filePath };
        }

        // Unknown character — skip with warning
        METAGFX_WARN << "PbrtLexer: unexpected character '" << c
                     << "' at " << filePath << ":" << lineNo << " — skipping";
    }
}

PbrtToken PbrtLexer::Next() {
    if (m_Lookahead.has_value()) {
        PbrtToken t = *m_Lookahead;
        m_Lookahead.reset();
        return t;
    }
    return ReadNext();
}

PbrtToken PbrtLexer::Peek() {
    if (!m_Lookahead.has_value()) {
        m_Lookahead = ReadNext();
    }
    return *m_Lookahead;
}

} // namespace metagfx

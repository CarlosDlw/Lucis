#include "lsp/DocComment.h"

#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <cctype>

// ═══════════════════════════════════════════════════════════════════════
//  Internal helpers
// ═══════════════════════════════════════════════════════════════════════

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Tag name → kind mapping.
static const std::unordered_map<std::string, DocTagKind>& tagKindMap() {
    static const std::unordered_map<std::string, DocTagKind> m = {
        {"param",      DocTagKind::Param},
        {"property",   DocTagKind::Property},
        {"field",      DocTagKind::Field},
        {"return",     DocTagKind::Return},
        {"returns",    DocTagKind::Returns},
        {"brief",      DocTagKind::Brief},
        {"deprecated", DocTagKind::Deprecated},
        {"version",    DocTagKind::Version},
        {"author",     DocTagKind::Author},
        {"see",        DocTagKind::See},
        {"since",      DocTagKind::Since},
        {"throws",     DocTagKind::Throws},
        {"todo",       DocTagKind::Todo},
        {"example",    DocTagKind::Example},
        {"remarks",    DocTagKind::Remarks},
        {"note",       DocTagKind::Note},
        {"warning",    DocTagKind::Warning},
        {"private",    DocTagKind::Private},
        {"public",     DocTagKind::Public},
        {"protected",  DocTagKind::Protected},
        {"internal",   DocTagKind::Internal},
        {"struct",     DocTagKind::Struct},
        {"namespace",  DocTagKind::Namespace},
    };
    return m;
}

// Classify tag kind into parsing category.
enum class TagParse { NameDesc, Text, Block, Flag };

static TagParse tagParseKind(DocTagKind kind) {
    switch (kind) {
        case DocTagKind::Param:
        case DocTagKind::Property:
        case DocTagKind::Field:
            return TagParse::NameDesc;

        case DocTagKind::Return:
        case DocTagKind::Returns:
        case DocTagKind::Brief:
        case DocTagKind::Deprecated:
        case DocTagKind::Version:
        case DocTagKind::Author:
        case DocTagKind::See:
        case DocTagKind::Since:
        case DocTagKind::Throws:
        case DocTagKind::Todo:
            return TagParse::Text;

        case DocTagKind::Example:
        case DocTagKind::Remarks:
        case DocTagKind::Note:
        case DocTagKind::Warning:
            return TagParse::Block;

        default:
            return TagParse::Flag;
    }
}

// Extract the next word from `s` starting at `pos`, advancing pos past it.
static std::string nextWord(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
    size_t start = pos;
    while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') ++pos;
    return s.substr(start, pos - start);
}

// Rest of the string from pos, trimmed.
static std::string restOfLine(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
    if (pos >= s.size()) return "";
    std::string r = s.substr(pos);
    // trim trailing whitespace
    size_t end = r.find_last_not_of(" \t\r\n");
    if (end == std::string::npos) return "";
    return r.substr(0, end + 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  Parse doc-comments from source
// ═══════════════════════════════════════════════════════════════════════

// Strip leading ` * ` or ` *` from doc-comment body lines.
static std::string stripStarPrefix(const std::string& line) {
    std::string s = line;
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i < s.size() && s[i] == '*') {
        ++i;
        if (i < s.size() && s[i] == ' ') ++i;
        return s.substr(i);
    }
    return s.substr(i);  // no star prefix, just trimmed leading whitespace
}

std::vector<DocComment> parseDocComments(const std::string& source) {
    std::vector<DocComment> result;

    size_t pos = 0;
    size_t line = 0;
    size_t lineStart = 0;

    // Build line offsets for quick line/col mapping
    std::vector<size_t> lineOffsets;
    lineOffsets.push_back(0);
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n')
            lineOffsets.push_back(i + 1);
    }

    pos = 0;
    while (pos < source.size()) {
        // Skip string literals (avoid false positives)
        if (source[pos] == '"') {
            ++pos;
            while (pos < source.size() && source[pos] != '"' && source[pos] != '\n') {
                if (source[pos] == '\\') ++pos;
                ++pos;
            }
            if (pos < source.size() && source[pos] == '"') ++pos;
            continue;
        }
        if (source[pos] == '\'') {
            ++pos;
            while (pos < source.size() && source[pos] != '\'' && source[pos] != '\n') {
                if (source[pos] == '\\') ++pos;
                ++pos;
            }
            if (pos < source.size() && source[pos] == '\'') ++pos;
            continue;
        }

        // Skip line comments
        if (pos + 1 < source.size() && source[pos] == '/' && source[pos + 1] == '/') {
            while (pos < source.size() && source[pos] != '\n') ++pos;
            continue;
        }

        // Check for doc-comment: /** (but not /***... which is just a block comment)
        if (pos + 2 < source.size() &&
            source[pos] == '/' && source[pos + 1] == '*' && source[pos + 2] == '*') {
            // Make sure it's not /*** (3+ stars = regular block comment)
            if (pos + 3 < source.size() && source[pos + 3] == '*') {
                // Skip regular block comment
                pos += 2;
                while (pos < source.size()) {
                    if (pos + 1 < source.size() && source[pos] == '*' && source[pos + 1] == '/') {
                        pos += 2;
                        break;
                    }
                    ++pos;
                }
                continue;
            }

            // Found a doc-comment start
            // Determine start line
            size_t startLine = 0;
            for (size_t i = 1; i < lineOffsets.size(); ++i) {
                if (lineOffsets[i] > pos) {
                    startLine = i - 1;
                    break;
                }
                if (i == lineOffsets.size() - 1) startLine = i;
            }

            size_t commentStart = pos;
            pos += 3;  // skip /**

            // Find closing */
            size_t bodyStart = pos;
            while (pos < source.size()) {
                if (pos + 1 < source.size() && source[pos] == '*' && source[pos + 1] == '/') {
                    break;
                }
                ++pos;
            }
            size_t bodyEnd = pos;
            if (pos + 1 < source.size()) pos += 2;  // skip */

            // Determine end line
            size_t endLine = startLine;
            for (size_t i = startLine + 1; i < lineOffsets.size(); ++i) {
                if (lineOffsets[i] > pos - 1) {
                    endLine = i - 1;
                    break;
                }
                if (i == lineOffsets.size() - 1) endLine = i;
            }

            // Extract the body text between /** and */
            std::string body = source.substr(bodyStart, bodyEnd - bodyStart);

            // Split body into lines
            std::vector<std::string> lines;
            std::istringstream ss(body);
            std::string ln;
            while (std::getline(ss, ln)) {
                lines.push_back(stripStarPrefix(ln));
            }

            // Parse: summary (before first @tag) + tags
            DocComment doc;
            doc.startLine = startLine;
            doc.endLine = endLine;

            std::ostringstream summaryBuf;
            bool inTags = false;
            DocTag* currentTag = nullptr;

            for (auto& rawLine : lines) {
                std::string trimmedLine = trim(rawLine);

                // Check if this line starts a new @tag
                if (!trimmedLine.empty() && trimmedLine[0] == '@') {
                    size_t tagEnd = 1;
                    while (tagEnd < trimmedLine.size() &&
                           std::isalpha(static_cast<unsigned char>(trimmedLine[tagEnd])))
                        ++tagEnd;

                    std::string tagName = trimmedLine.substr(1, tagEnd - 1);
                    // Convert to lowercase for lookup
                    std::string tagNameLower = tagName;
                    std::transform(tagNameLower.begin(), tagNameLower.end(),
                                   tagNameLower.begin(), ::tolower);

                    auto& km = tagKindMap();
                    auto it = km.find(tagNameLower);
                    if (it != km.end()) {
                        inTags = true;
                        doc.tags.emplace_back();
                        currentTag = &doc.tags.back();
                        currentTag->kind = it->second;

                        std::string afterTag = trimmedLine.substr(tagEnd);
                        size_t apos = 0;

                        switch (tagParseKind(it->second)) {
                            case TagParse::NameDesc: {
                                currentTag->name = nextWord(afterTag, apos);
                                currentTag->text = restOfLine(afterTag, apos);
                                break;
                            }
                            case TagParse::Text: {
                                currentTag->text = restOfLine(afterTag, 0);
                                break;
                            }
                            case TagParse::Block: {
                                // First line after the tag is start of block
                                std::string first = restOfLine(afterTag, 0);
                                if (!first.empty())
                                    currentTag->text = first;
                                break;
                            }
                            case TagParse::Flag: {
                                // No arguments expected
                                break;
                            }
                        }
                        continue;
                    }
                }

                // Continuation line
                if (inTags && currentTag) {
                    // Append to current tag's text
                    if (!currentTag->text.empty())
                        currentTag->text += "\n";
                    currentTag->text += rawLine;
                } else {
                    // Part of summary
                    if (summaryBuf.tellp() > 0)
                        summaryBuf << "\n";
                    summaryBuf << rawLine;
                }
            }

            doc.summary = trim(summaryBuf.str());
            result.push_back(std::move(doc));
            continue;
        }

        // Skip regular block comments
        if (pos + 1 < source.size() && source[pos] == '/' && source[pos + 1] == '*') {
            pos += 2;
            while (pos < source.size()) {
                if (pos + 1 < source.size() && source[pos] == '*' && source[pos + 1] == '/') {
                    pos += 2;
                    break;
                }
                ++pos;
            }
            continue;
        }

        ++pos;
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  Find doc-comment for a declaration
// ═══════════════════════════════════════════════════════════════════════

const DocComment* findDocForLine(const std::vector<DocComment>& docs,
                                  size_t declLine) {
    const DocComment* best = nullptr;
    for (auto& doc : docs) {
        // The doc-comment must end just before the declaration line.
        // Allow up to 1 blank line between doc-comment end and declaration.
        if (doc.endLine < declLine && (declLine - doc.endLine) <= 2) {
            if (!best || doc.endLine > best->endLine)
                best = &doc;
        }
    }
    return best;
}

// ═══════════════════════════════════════════════════════════════════════
//  Format doc-comment as Markdown
// ═══════════════════════════════════════════════════════════════════════

static std::string tagLabel(DocTagKind kind) {
    switch (kind) {
        case DocTagKind::Param:      return "@param";
        case DocTagKind::Property:   return "@property";
        case DocTagKind::Field:      return "@field";
        case DocTagKind::Return:     return "@return";
        case DocTagKind::Returns:    return "@returns";
        case DocTagKind::Brief:      return "@brief";
        case DocTagKind::Deprecated: return "@deprecated";
        case DocTagKind::Version:    return "@version";
        case DocTagKind::Author:     return "@author";
        case DocTagKind::See:        return "@see";
        case DocTagKind::Since:      return "@since";
        case DocTagKind::Throws:     return "@throws";
        case DocTagKind::Todo:       return "@todo";
        case DocTagKind::Example:    return "@example";
        case DocTagKind::Remarks:    return "@remarks";
        case DocTagKind::Note:       return "@note";
        case DocTagKind::Warning:    return "@warning";
        case DocTagKind::Private:    return "@private";
        case DocTagKind::Public:     return "@public";
        case DocTagKind::Protected:  return "@protected";
        case DocTagKind::Internal:   return "@internal";
        case DocTagKind::Struct:     return "@struct";
        case DocTagKind::Namespace:  return "@namespace";
    }
    return "@unknown";
}

std::string formatDocComment(const DocComment& doc) {
    std::ostringstream ss;

    // ── Summary ─────────────────────────────────────────────────────
    if (!doc.summary.empty()) {
        ss << doc.summary << "\n\n";
    }

    // ── Visibility / deprecation flags (rendered first) ─────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Deprecated) {
            ss << "> **" << tagLabel(tag.kind) << "**";
            if (!tag.text.empty()) ss << " " << tag.text;
            ss << "\n\n";
        }
    }

    // ── @param / @property / @field ─────────────────────────────────
    bool hasParams = false;
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Param ||
            tag.kind == DocTagKind::Property ||
            tag.kind == DocTagKind::Field) {
            if (!hasParams) {
                hasParams = true;
            }
            ss << "**" << tagLabel(tag.kind) << "** ";
            if (!tag.name.empty()) ss << "`" << tag.name << "`";
            if (!tag.text.empty()) ss << " — " << tag.text;
            ss << "  \n";
        }
    }
    if (hasParams) ss << "\n";

    // ── @return / @returns ──────────────────────────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Return || tag.kind == DocTagKind::Returns) {
            ss << "**" << tagLabel(tag.kind) << "** " << tag.text << "  \n\n";
        }
    }

    // ── @throws ─────────────────────────────────────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Throws) {
            ss << "**" << tagLabel(tag.kind) << "** " << tag.text << "  \n\n";
        }
    }

    // ── @example (code block) ───────────────────────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Example) {
            ss << "**" << tagLabel(tag.kind) << "**\n";
            ss << "```lucis\n";
            ss << trim(tag.text) << "\n";
            ss << "```\n\n";
        }
    }

    // ── @remarks / @note / @warning (block tags) ────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Remarks) {
            ss << "**" << tagLabel(tag.kind) << "**  \n" << tag.text << "\n\n";
        } else if (tag.kind == DocTagKind::Note) {
            ss << "> **" << tagLabel(tag.kind) << "** " << tag.text << "\n\n";
        } else if (tag.kind == DocTagKind::Warning) {
            ss << "> ⚠ **" << tagLabel(tag.kind) << "** " << tag.text << "\n\n";
        }
    }

    // ── @brief / @todo ──────────────────────────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Brief) {
            ss << tag.text << "\n\n";
        } else if (tag.kind == DocTagKind::Todo) {
            ss << "**" << tagLabel(tag.kind) << "** " << tag.text << "  \n\n";
        }
    }

    // ── Metadata tags (@since, @version, @author, @see) ────────────
    bool hasMeta = false;
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Since  || tag.kind == DocTagKind::Version ||
            tag.kind == DocTagKind::Author || tag.kind == DocTagKind::See) {
            if (!hasMeta) { hasMeta = true; }
            ss << "*" << tagLabel(tag.kind) << "* " << tag.text << "  \n";
        }
    }
    if (hasMeta) ss << "\n";

    // ── Visibility flags ────────────────────────────────────────────
    for (auto& tag : doc.tags) {
        if (tag.kind == DocTagKind::Private || tag.kind == DocTagKind::Public ||
            tag.kind == DocTagKind::Protected || tag.kind == DocTagKind::Internal ||
            tag.kind == DocTagKind::Struct || tag.kind == DocTagKind::Namespace) {
            ss << "*" << tagLabel(tag.kind) << "*  \n";
        }
    }

    std::string result = ss.str();
    // Trim trailing whitespace/newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == ' '))
        result.pop_back();

    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  Append doc to hover
// ═══════════════════════════════════════════════════════════════════════

std::string appendDocToHover(const std::string& hoverMd,
                             const std::vector<DocComment>& docs,
                             size_t declLine) {
    auto* doc = findDocForLine(docs, declLine);
    if (!doc) return hoverMd;

    std::string formatted = formatDocComment(*doc);
    if (formatted.empty()) return hoverMd;

    return hoverMd + "\n\n---\n\n" + formatted;
}

// ═══════════════════════════════════════════════════════════════════════
//  Check if cursor is inside a doc-comment
// ═══════════════════════════════════════════════════════════════════════

bool isInsideDocComment(const std::string& source, size_t line, size_t col) {
    // Convert 0-based line:col to byte offset
    size_t offset = 0;
    size_t currentLine = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        if (currentLine == line) {
            offset = i + col;
            break;
        }
        if (source[i] == '\n') ++currentLine;
    }
    if (offset > source.size()) offset = source.size();

    // Scan source from the beginning, tracking whether `offset` falls
    // inside a /** ... */ block.
    size_t pos = 0;
    while (pos < source.size()) {
        // Skip string literals
        if (source[pos] == '"') {
            ++pos;
            while (pos < source.size() && source[pos] != '"' && source[pos] != '\n') {
                if (source[pos] == '\\') ++pos;
                ++pos;
            }
            if (pos < source.size() && source[pos] == '"') ++pos;
            continue;
        }
        if (source[pos] == '\'') {
            ++pos;
            while (pos < source.size() && source[pos] != '\'' && source[pos] != '\n') {
                if (source[pos] == '\\') ++pos;
                ++pos;
            }
            if (pos < source.size() && source[pos] == '\'') ++pos;
            continue;
        }

        // Line comment
        if (pos + 1 < source.size() && source[pos] == '/' && source[pos + 1] == '/') {
            while (pos < source.size() && source[pos] != '\n') ++pos;
            continue;
        }

        // Doc-comment: /**
        if (pos + 2 < source.size() &&
            source[pos] == '/' && source[pos + 1] == '*' && source[pos + 2] == '*' &&
            !(pos + 3 < source.size() && source[pos + 3] == '*')) {
            size_t docStart = pos;
            pos += 3;
            while (pos < source.size()) {
                if (pos + 1 < source.size() && source[pos] == '*' && source[pos + 1] == '/') {
                    pos += 2;
                    // Check if offset falls inside [docStart, pos)
                    if (offset >= docStart && offset < pos) return true;
                    break;
                }
                ++pos;
            }
            // If we hit end of file without closing */, check if offset is inside
            if (offset >= docStart && offset <= pos) return true;
            continue;
        }

        // Regular block comment
        if (pos + 1 < source.size() && source[pos] == '/' && source[pos + 1] == '*') {
            pos += 2;
            while (pos < source.size()) {
                if (pos + 1 < source.size() && source[pos] == '*' && source[pos + 1] == '/') {
                    pos += 2;
                    break;
                }
                ++pos;
            }
            continue;
        }

        ++pos;
    }

    return false;
}

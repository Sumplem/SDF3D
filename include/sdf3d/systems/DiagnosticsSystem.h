#pragma once

#include <string>
#include <vector>

namespace sdf3d {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct DiagnosticEntry {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::string source;
    std::string message;
};

/// Stores runtime diagnostics for UI display.
class DiagnosticsSystem {
public:
    /// Removes all diagnostics.
    void clear();

    /// Appends one diagnostic message.
    void add(std::string message);

    /// Appends one typed diagnostic message.
    void add(DiagnosticSeverity severity, std::string source, std::string message);

    /// Appends multiple messages with a source prefix.
    void addPrefixed(const std::string& prefix, const std::vector<std::string>& messages);

    /// Returns current diagnostics as display strings.
    const std::vector<std::string>& entries() const;

    /// Returns typed diagnostics.
    const std::vector<DiagnosticEntry>& typedEntries() const;

private:
    void rebuildDisplayEntries();

    std::vector<DiagnosticEntry> m_typedEntries;
    std::vector<std::string> m_entries;
};

} // namespace sdf3d

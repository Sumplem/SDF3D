#include "sdf3d/systems/DiagnosticsSystem.h"

#include <sstream>
#include <utility>

namespace sdf3d {
namespace {

std::string sourceFromPrefix(const std::string& prefix)
{
    if (prefix.size() >= 3 && prefix.front() == '[') {
        const size_t end = prefix.find(']');
        if (end != std::string::npos && end > 1) {
            return prefix.substr(1, end - 1);
        }
    }

    return prefix;
}

} // namespace

void DiagnosticsSystem::clear()
{
    m_typedEntries.clear();
    m_entries.clear();
}

void DiagnosticsSystem::add(std::string message)
{
    add(DiagnosticSeverity::Error, "Runtime", std::move(message));
}

void DiagnosticsSystem::add(DiagnosticSeverity severity, std::string source, std::string message)
{
    m_typedEntries.push_back({severity, std::move(source), std::move(message)});
    rebuildDisplayEntries();
}

void DiagnosticsSystem::addPrefixed(const std::string& prefix, const std::vector<std::string>& messages)
{
    const std::string source = sourceFromPrefix(prefix);
    for (const std::string& message : messages) {
        add(DiagnosticSeverity::Error, source, message);
    }
}

const std::vector<std::string>& DiagnosticsSystem::entries() const
{
    return m_entries;
}

const std::vector<DiagnosticEntry>& DiagnosticsSystem::typedEntries() const
{
    return m_typedEntries;
}

void DiagnosticsSystem::rebuildDisplayEntries()
{
    m_entries.clear();
    m_entries.reserve(m_typedEntries.size());
    for (const DiagnosticEntry& entry : m_typedEntries) {
        std::ostringstream text;
        if (!entry.source.empty()) {
            text << '[' << entry.source << "] ";
        }
        text << entry.message;
        m_entries.push_back(text.str());
    }
}

} // namespace sdf3d

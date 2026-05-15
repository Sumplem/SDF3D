#include "sdf3d/systems/DiagnosticsSystem.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testAddClear(std::vector<TestFailure>& failures)
{
    const std::string testName = "add clear";
    sdf3d::DiagnosticsSystem diagnostics;

    diagnostics.add("one");
    diagnostics.addPrefixed("[Compiler] ", {"two", "three"});

    expect(diagnostics.entries().size() == 3, testName, "Expected three diagnostics.", failures);
    if (diagnostics.entries().size() == 3) {
        expect(diagnostics.entries()[1] == "[Compiler] two", testName, "Expected prefixed diagnostic.", failures);
    }
    diagnostics.clear();
    expect(diagnostics.entries().empty(), testName, "Expected clear diagnostics.", failures);
    expect(diagnostics.typedEntries().empty(), testName, "Expected clear typed diagnostics.", failures);
}

void testTypedEntries(std::vector<TestFailure>& failures)
{
    const std::string testName = "typed entries";
    sdf3d::DiagnosticsSystem diagnostics;

    diagnostics.add(sdf3d::DiagnosticSeverity::Warning, "Shader", "line 12");

    expect(diagnostics.typedEntries().size() == 1, testName, "Expected one typed diagnostic.", failures);
    if (diagnostics.typedEntries().size() == 1) {
        const sdf3d::DiagnosticEntry& entry = diagnostics.typedEntries()[0];
        expect(entry.severity == sdf3d::DiagnosticSeverity::Warning, testName, "Expected warning severity.", failures);
        expect(entry.source == "Shader", testName, "Expected source.", failures);
        expect(entry.message == "line 12", testName, "Expected message.", failures);
    }
    expect(diagnostics.entries().size() == 1, testName, "Expected display diagnostic.", failures);
    if (diagnostics.entries().size() == 1) {
        expect(diagnostics.entries()[0] == "[Shader] line 12", testName, "Expected formatted display diagnostic.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testAddClear(failures);
    testTypedEntries(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All DiagnosticsSystem tests passed.\n";
    return 0;
}

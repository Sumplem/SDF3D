#include "sdf3d/math/Math.h"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#else
#include <unistd.h>
#endif

namespace sdf3d {

std::filesystem::path executableDir()
{
#if defined(_WIN32)
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        return std::filesystem::current_path();
    }

    // AGENT: Native wide paths preserve non-ASCII Windows install locations.
    return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return std::filesystem::current_path();
    }

    return std::filesystem::weakly_canonical(buffer.data()).parent_path();
#else
    std::array<char, 4096> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return std::filesystem::current_path();
    }

    buffer[static_cast<size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#endif
}

} // namespace sdf3d

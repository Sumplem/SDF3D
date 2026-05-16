#include "sdf3d/systems/GraphSerializer.h"

#include <utility>

namespace sdf3d {

const std::string& GraphSerializer::lastError() const
{
    return m_lastError;
}

void GraphSerializer::setLastError(std::string error)
{
    m_lastError = std::move(error);
}

void GraphSerializer::clearLastError()
{
    m_lastError.clear();
}

} // namespace sdf3d

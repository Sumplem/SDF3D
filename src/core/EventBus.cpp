#include "sdf3d/core/EventBus.h"

namespace sdf3d {

void EventBus::clear()
{
    m_subscribers.clear();
}

} // namespace sdf3d

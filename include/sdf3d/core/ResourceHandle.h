#pragma once

#include <memory>
#include <utility>

namespace sdf3d {

/// Typed weak reference to a resource cached by ResourceManager.
template<typename T>
class ResourceHandle {
public:
    ResourceHandle() = default;
    explicit ResourceHandle(std::weak_ptr<T> resource)
        : m_resource(std::move(resource))
    {
    }

    /// Returns a shared pointer when the cached resource is still alive.
    std::shared_ptr<T> lock() const
    {
        return m_resource.lock();
    }

    /// Returns true when the cached resource has been unloaded.
    bool expired() const
    {
        return m_resource.expired();
    }

private:
    std::weak_ptr<T> m_resource;
};

} // namespace sdf3d

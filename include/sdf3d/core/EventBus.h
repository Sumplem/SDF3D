#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sdf3d {

/// Emitted when scene data changed and compiled render data is stale.
struct SceneDirtyEvent {
};

/// Emitted when material uniforms changed without shader topology changes.
struct MaterialDirtyEvent {
};

/// Requests duplication of selected graph nodes.
struct DuplicateSelectionEvent {
    std::vector<std::uint64_t> selectedNodeIds;
};

/// Emitted when graph selection changes through decoupled systems.
struct SelectionEvent {
    std::vector<std::uint64_t> nodeIds;
    std::uint64_t primaryNodeId = 0;
};

/// Requests graph save to a concrete path.
struct SaveGraphEvent {
    std::string path;
};

/// Requests graph load from a concrete path.
struct LoadGraphEvent {
    std::string path;
};

/// Typed in-process pub/sub bus for decoupled runtime systems.
class EventBus {
public:
    using SubscriptionId = std::size_t;

    /// Registers a typed event handler and returns its subscription ID.
    template <typename Event>
    SubscriptionId subscribe(std::function<void(const Event&)> handler)
    {
        const SubscriptionId id = m_nextSubscriptionId++;
        auto wrapped = [callback = std::move(handler)](const void* event) {
            callback(*static_cast<const Event*>(event));
        };

        m_subscribers[std::type_index(typeid(Event))].push_back({id, std::move(wrapped)});
        return id;
    }

    /// Removes one typed event handler by subscription ID.
    template <typename Event>
    bool unsubscribe(SubscriptionId id)
    {
        auto found = m_subscribers.find(std::type_index(typeid(Event)));
        if (found == m_subscribers.end()) {
            return false;
        }

        std::vector<Subscription>& subscriptions = found->second;
        const auto oldSize = subscriptions.size();
        subscriptions.erase(
            std::remove_if(subscriptions.begin(), subscriptions.end(), [id](const Subscription& subscription) {
                return subscription.id == id;
            }),
            subscriptions.end()
        );

        return subscriptions.size() != oldSize;
    }

    /// Emits one event to all current subscribers of its exact type.
    template <typename Event>
    void emit(const Event& event)
    {
        auto found = m_subscribers.find(std::type_index(typeid(Event)));
        if (found == m_subscribers.end()) {
            return;
        }

        // AGENT: Copy callback wrappers before dispatch so handlers can safely
        // unsubscribe or subscribe without invalidating this emit traversal.
        const std::vector<Subscription> subscriptions = found->second;
        for (const Subscription& subscription : subscriptions) {
            subscription.callback(&event);
        }
    }

    /// Removes all subscriptions.
    void clear();

private:
    struct Subscription {
        SubscriptionId id = 0;
        std::function<void(const void*)> callback;
    };

    std::unordered_map<std::type_index, std::vector<Subscription>> m_subscribers;
    SubscriptionId m_nextSubscriptionId = 1;
};

} // namespace sdf3d

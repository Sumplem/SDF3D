#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sdf3d {

/// Emitted when scene data changed and compiled render data is stale.
struct SceneDirtyEvent {
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

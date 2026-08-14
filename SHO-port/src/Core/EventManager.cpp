#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <iostream>

namespace SHO {
namespace Core {

EventManager& EventManager::GetInstance() {
    static EventManager instance;
    return instance;
}

uint32_t EventManager::HashString(const std::string& str) const {
    // FNV-1a 32-bit hash for fast message ID mapping
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash == 0 ? 1 : hash; // 0 is reserved for invalid
}

EventId EventManager::RegisterMsg(const std::string& name, const std::string& /*ownerClassName*/) {
    if (name.empty()) return EventId{};

    auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
        return it->second;
    }

    uint32_t id = HashString(name);
    EventId eventId{ id, name };
    m_nameToId[name] = eventId;
    m_idToName[id] = name;
    return eventId;
}

EventId EventManager::FindMsg(const std::string& name) const {
    auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
        return it->second;
    }
    return EventId{};
}

uint32_t EventManager::LinkMsg(const EventId& eventId, EventCallback callback, uint16_t priority) {
    if (!eventId.IsValid() || !callback) return 0;

    uint32_t handle = m_nextHandle++;
    EventSubscription sub{ handle, priority, callback };

    auto& list = m_listeners[eventId.id];
    list.push_back(sub);

    // Sort by priority (higher priority runs first)
    std::stable_sort(list.begin(), list.end(), [](const EventSubscription& a, const EventSubscription& b) {
        return a.priority > b.priority;
    });

    return handle;
}

uint32_t EventManager::LinkMsg(const std::string& name, EventCallback callback, uint16_t priority) {
    EventId id = RegisterMsg(name);
    return LinkMsg(id, callback, priority);
}

void EventManager::UnlinkMsg(uint32_t handle) {
    if (handle == 0) return;

    for (auto& pair : m_listeners) {
        auto& list = pair.second;
        list.erase(std::remove_if(list.begin(), list.end(), [handle](const EventSubscription& sub) {
            return sub.handle == handle;
        }), list.end());
    }
}

void EventManager::SendMsg(const Msg& msg) {
    if (!msg.id.IsValid()) return;

    auto it = m_listeners.find(msg.id.id);
    if (it == m_listeners.end()) return;

    // Dispatch a copy of the listeners in case a callback unlinks/links
    auto listeners = it->second;
    for (const auto& sub : listeners) {
        if (sub.callback) {
            sub.callback(msg);
        }
    }
}

void EventManager::SendMsg(const std::string& eventName, void* sender, void* data, size_t size) {
    EventId id = RegisterMsg(eventName);
    Msg msg(id, sender, data, size);
    SendMsg(msg);
}

void EventManager::QueueMsg(const Msg& msg) {
    if (msg.id.IsValid()) {
        m_queuedMessages.push_back(msg);
    }
}

void EventManager::FlushQueuedMessages() {
    std::vector<Msg> pending;
    pending.swap(m_queuedMessages);

    for (const auto& msg : pending) {
        SendMsg(msg);
    }
}

void EventManager::Clear() {
    m_nameToId.clear();
    m_idToName.clear();
    m_listeners.clear();
    m_queuedMessages.clear();
    m_nextHandle = 1;
}

} // namespace Core
} // namespace SHO

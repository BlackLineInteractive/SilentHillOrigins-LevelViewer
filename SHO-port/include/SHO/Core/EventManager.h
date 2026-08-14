#pragma once

#include "SHO/Core/Types.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SHO {
namespace Core {

// Identifier for a registered message
struct EventId {
    uint32_t id = 0;
    std::string name;

    bool IsValid() const { return id != 0; }
    bool operator==(const EventId& o) const { return id == o.id; }
    bool operator<(const EventId& o) const { return id < o.id; }
};

// Generic message payload
struct Msg {
    EventId id;
    void* sender = nullptr;
    void* data = nullptr;
    size_t dataSize = 0;

    Msg() = default;
    Msg(const EventId& inId, void* inSender = nullptr, void* inData = nullptr, size_t inSize = 0)
        : id(inId), sender(inSender), data(inData), dataSize(inSize) {}
};

// Callback listener function
using EventCallback = std::function<void(const Msg&)>;

struct EventSubscription {
    uint32_t handle = 0;
    uint16_t priority = 0;
    EventCallback callback;
};

// Central Event Manager (translated from RWS::CEventHandler & RWS::_SendMsg)
class EventManager {
public:
    static EventManager& GetInstance();

    // Register a message by name (e.g. "camLanding01", "camStairway01S", "DoorOpen")
    EventId RegisterMsg(const std::string& name, const std::string& ownerClassName = "");

    // Find an existing registered event ID by name
    EventId FindMsg(const std::string& name) const;

    // Link (subscribe) to a message by EventId
    uint32_t LinkMsg(const EventId& eventId, EventCallback callback, uint16_t priority = 0);

    // Link (subscribe) to a message by string name
    uint32_t LinkMsg(const std::string& name, EventCallback callback, uint16_t priority = 0);

    // Unsubscribe from a message
    void UnlinkMsg(uint32_t handle);

    // Send a message immediately to all listeners (synchronous dispatch)
    void SendMsg(const Msg& msg);

    // Convenience method to send an event by name with optional sender and data
    void SendMsg(const std::string& eventName, void* sender = nullptr, void* data = nullptr, size_t size = 0);

    // Queue a message for delayed dispatch (e.g. next frame update)
    void QueueMsg(const Msg& msg);

    // Process all queued messages
    void FlushQueuedMessages();

    // Clear all registrations and subscribers
    void Clear();

private:
    EventManager() = default;
    ~EventManager() = default;

    uint32_t HashString(const std::string& str) const;

    std::map<std::string, EventId> m_nameToId;
    std::map<uint32_t, std::string> m_idToName;
    std::map<uint32_t, std::vector<EventSubscription>> m_listeners;
    std::vector<Msg> m_queuedMessages;
    uint32_t m_nextHandle = 1;
};

} // namespace Core
} // namespace SHO

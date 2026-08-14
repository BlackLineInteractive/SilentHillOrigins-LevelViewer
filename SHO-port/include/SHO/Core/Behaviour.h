#pragma once

#include "SHO/Core/SystemCommands.h"
#include "SHO/Core/EventManager.h"
#include <memory>

namespace SHO {
namespace Core {

// Base component class translated from CBaseBehaviour
class Behaviour : public SystemCommands {
public:
    Behaviour() = default;
    virtual ~Behaviour();

    // Lifecycle methods
    virtual void OnInit() {}
    virtual void OnUpdate(float dt) { (void)dt; }
    virtual void OnFixedUpdate(float fixedDt) { (void)fixedDt; }
    virtual void OnEvent(const Msg& msg) { (void)msg; }
    virtual void OnDestroy() {}

    // Subscribe to event with automatic cleanup on destroy
    void ListenToEvent(const std::string& eventName, uint16_t priority = 0);
    void ListenToEvent(const EventId& eventId, uint16_t priority = 0);

protected:
    std::vector<uint32_t> m_eventHandles;
};

} // namespace Core
} // namespace SHO

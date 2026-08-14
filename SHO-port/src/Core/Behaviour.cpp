#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Core {

Behaviour::~Behaviour() {
    auto& em = EventManager::GetInstance();
    for (uint32_t handle : m_eventHandles) {
        em.UnlinkMsg(handle);
    }
    m_eventHandles.clear();
}

void Behaviour::ListenToEvent(const std::string& eventName, uint16_t priority) {
    auto& em = EventManager::GetInstance();
    uint32_t handle = em.LinkMsg(eventName, [this](const Msg& msg) {
        if (this->m_active) {
            this->OnEvent(msg);
        }
    }, priority);

    if (handle != 0) {
        m_eventHandles.push_back(handle);
    }
}

void Behaviour::ListenToEvent(const EventId& eventId, uint16_t priority) {
    auto& em = EventManager::GetInstance();
    uint32_t handle = em.LinkMsg(eventId, [this](const Msg& msg) {
        if (this->m_active) {
            this->OnEvent(msg);
        }
    }, priority);

    if (handle != 0) {
        m_eventHandles.push_back(handle);
    }
}

} // namespace Core
} // namespace SHO

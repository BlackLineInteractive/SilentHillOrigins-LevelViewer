#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Triggers {

class TriggerBase : public Core::Behaviour {
public:
    TriggerBase() = default;
    virtual ~TriggerBase() = default;

    virtual bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) = 0;
    virtual void OnTriggerEnter(void* actor) { (void)actor; }
    virtual void OnTriggerExit(void* actor) { (void)actor; }

    bool IsTriggerEnabled() const { return m_triggerEnabled; }
    void SetTriggerEnabled(bool enabled) { m_triggerEnabled = enabled; }

protected:
    bool m_triggerEnabled = true;
};

} // namespace Triggers
} // namespace SHO

#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Progression {

// Translated from SavePoint (red symbols on walls)
class SavePoint : public Triggers::TriggerBase {
public:
    SavePoint();
    virtual ~SavePoint() = default;

    float GetInteractionRadius() const { return m_radius; }
    void SetInteractionRadius(float r) { m_radius = r; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;
    void TriggerSave();

private:
    float m_radius = 1.6f;
    bool  m_playerInRange = false;
};

} // namespace Progression
} // namespace SHO

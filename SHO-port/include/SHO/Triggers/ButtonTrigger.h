#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Triggers {

// Translated from ButtonTrigger & ButtonBoxTrigger (interactable examination prompts & switches)
class ButtonTrigger : public TriggerBase {
public:
    ButtonTrigger();
    virtual ~ButtonTrigger() = default;

    // Interaction radius or volume
    float GetRadius() const { return m_radius; }
    void SetRadius(float r) { m_radius = r; }

    const Core::AABB& GetBoxBounds() const { return m_boxBounds; }
    void SetBoxBounds(const Core::AABB& aabb) { m_boxBounds = aabb; m_useBox = true; }

    // Prompt string ID (for localized UI text lookup)
    const std::string& GetPromptStringId() const { return m_promptStringId; }
    void SetPromptStringId(const std::string& strId) { m_promptStringId = strId; }

    // Action event fired when player presses the action button (Cross)
    const std::string& GetActionEventName() const { return m_actionEvent; }
    void SetActionEventName(const std::string& evt) { m_actionEvent = evt; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;
    void Interact();

private:
    float       m_radius = 1.5f;
    bool        m_useBox = false;
    Core::AABB  m_boxBounds;
    std::string m_promptStringId;
    std::string m_actionEvent;
    bool        m_playerInRange = false;
};

} // namespace Triggers
} // namespace SHO

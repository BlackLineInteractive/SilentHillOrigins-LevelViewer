#pragma once

#include "SHO/Camera/BaseCamera.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SHO {
namespace Camera {

class CameraManager {
public:
    static CameraManager& GetInstance();

    void RegisterCamera(BaseCamera* cam);
    void UnregisterCamera(BaseCamera* cam);

    BaseCamera* FindCamera(const std::string& name) const;
    BaseCamera* GetActiveCamera() const { return m_activeCamera; }

    // Switch to a new camera (triggers camera cut)
    void SetActiveCamera(BaseCamera* cam);
    void SetActiveCamera(const std::string& name);

    // Update all registered cameras and active camera tracking
    void Update(float dt, const Core::Vec3& playerTargetPos);

    // Matrices from current active camera
    Core::Mat4 GetViewMatrix() const;
    Core::Mat4 GetProjectionMatrix(float aspect) const;

    // Camera Cut Detection
    bool DidCutThisFrame() const { return m_didCut; }

    void Clear();

private:
    CameraManager() = default;
    ~CameraManager() = default;

    std::vector<BaseCamera*> m_cameras;
    std::map<std::string, BaseCamera*> m_nameLookup;
    BaseCamera* m_activeCamera = nullptr;
    bool m_didCut = false;
};

} // namespace Camera
} // namespace SHO

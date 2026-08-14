#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Audio {

// Translated from AudioSound3D (3D positional sound emitter)
class AudioSound3D : public Core::Behaviour {
public:
    AudioSound3D();
    virtual ~AudioSound3D() = default;

    const std::string& GetCueName() const { return m_cueName; }
    void SetCueName(const std::string& name) { m_cueName = name; }

    float GetVolume() const { return m_volume; }
    void SetVolume(float vol) { m_volume = vol; }

    float GetMinDistance() const { return m_minDist; }
    void SetMinDistance(float minD) { m_minDist = minD; }

    float GetMaxDistance() const { return m_maxDist; }
    void SetMaxDistance(float maxD) { m_maxDist = maxD; }

    bool IsLooping() const { return m_looping; }
    void SetLooping(bool loop) { m_looping = loop; }

    void Play();
    void Stop();

private:
    std::string m_cueName;
    float       m_volume = 1.0f;
    float       m_minDist = 1.0f;
    float       m_maxDist = 15.0f;
    bool        m_looping = true;
    bool        m_isPlaying = false;
};

// Translated from AudioStream (background music & ambient streaming track)
class AudioStream : public Core::Behaviour {
public:
    AudioStream();
    virtual ~AudioStream() = default;

    const std::string& GetStreamPath() const { return m_streamPath; }
    void SetStreamPath(const std::string& path) { m_streamPath = path; }

    float GetVolume() const { return m_volume; }
    void SetVolume(float vol) { m_volume = vol; }

    void Play();
    void Stop();
    void FadeOut(float duration);

private:
    std::string m_streamPath;
    float       m_volume = 1.0f;
    bool        m_isPlaying = false;
};

// Translated from AudioReverb (environmental reverb zone)
class AudioReverb : public Core::Behaviour {
public:
    AudioReverb();
    virtual ~AudioReverb() = default;

    int GetReverbPreset() const { return m_preset; }
    void SetReverbPreset(int preset) { m_preset = preset; }

    float GetWetDryMix() const { return m_mix; }
    void SetWetDryMix(float mix) { m_mix = mix; }

private:
    int   m_preset = 0; // 0=Room, 1=Hallway, 2=Cave, 3=LargeHall
    float m_mix = 0.35f;
};

} // namespace Audio
} // namespace SHO

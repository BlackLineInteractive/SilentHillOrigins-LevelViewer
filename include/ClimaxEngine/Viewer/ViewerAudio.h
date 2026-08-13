#pragma once

#include "ClimaxEngine/Platform/PS2/RwsAudio.h"
#include "ClimaxEngine/Core/Types.h"
#include <string>
#include <vector>

namespace ClimaxEngine {
namespace Viewer {

// Returns the active audio clip.
const AudioClip& CurrentAudioClip();

// Starts playing a loaded clip.
void PlayAudioClip(const AudioClip& clip);

// Loads and plays a clip from the library by index.
void PlayLibraryEntry(int index);

// Toggles playback pause state.
void ToggleAudioPlayback();

// Stops audio playback.
void StopAudio();

// Sets audio progress (0.0 to 1.0).
void SetAudioProgress(float progress);

// Retrieves audio health metrics.
void AudioHealth(int& calls, int& late, double& worstMs, double& bufferMs);

// Scans the mounted archive and loose files to populate the library.
void ScanAudioLibrary();

// Expose the global audio library for UI.
extern std::vector<AudioSourceRef> g_AudioLibrary;

} // namespace Viewer
} // namespace ClimaxEngine

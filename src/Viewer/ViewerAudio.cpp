#include "ClimaxEngine/Viewer/ViewerAudio.h"
#include "ClimaxEngine/Render/ViewerState.h" // For state.showAudioPlayer
#include "ClimaxEngine/Core/RWS/FileSystem/CArchiveManager.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace ClimaxEngine {
namespace Viewer {

// Expose the global audio library for UI.
std::vector<AudioSourceRef> g_AudioLibrary;

// We declare g_IgcArc here so that ViewerAudio owns it.
RWS::FileSystem::CArchive g_IgcArc;

const AudioClip& CurrentAudioClip() { 
    return Audio::CAudioRelay::GetInstance().CurrentAudioClip(); 
}

void PlayAudioClip(const AudioClip& clip) {
    Audio::CAudioRelay::GetInstance().PlayAudioClip(clip);
    state.showAudioPlayer = true;
}

void PlayLibraryEntry(int index) {
    if (index < 0 || index >= (int)g_AudioLibrary.size()) return;
    const AudioSourceRef& ref = g_AudioLibrary[(size_t)index];
    AudioClip clip;
    if (ref.arcIndex >= 0 && g_IgcArc.IsOpen()) {
        std::vector<uint8_t> blob;
        if (!g_IgcArc.Read((size_t)ref.arcIndex, blob) || blob.empty()) return;
        clip.name = ref.name;
        if (!::Audio::LoadBuffer(blob.data(), blob.size(), clip)) return;
        clip.name = ref.name;
    } else if (!::Audio::LoadFile(ref.path, clip)) {
        return;
    }
    clip.name = ref.name;
    PlayAudioClip(clip);
}

void ToggleAudioPlayback() { 
    Audio::CAudioRelay::GetInstance().ToggleAudioPlayback(); 
}

void StopAudio() { 
    Audio::CAudioRelay::GetInstance().StopAudio(); 
}

void SetAudioProgress(float progress) { 
    Audio::CAudioRelay::GetInstance().SetAudioProgress(progress); 
}

void AudioHealth(int& calls, int& late, double& worstMs, double& bufferMs) { 
    calls = late = 0; worstMs = bufferMs = 0.0; 
}

void ScanAudioLibrary() {
    g_AudioLibrary.clear();
    if (!RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()) return;

    std::error_code ec;
    const fs::path root = fs::path(RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Path()).parent_path();

    for (const char* dirName : {"MUSIC", "Music", "music"}) {
        const fs::path dir = root / dirName;
        if (!fs::is_directory(dir, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(dir, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec)) continue;
            std::string ext = it->path().extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (ext != ".rws" && ext != ".vag" && ext != ".ads") continue;
            AudioSourceRef ref;
            ref.name  = it->path().stem().string();
            ref.group = "Music";
            ref.path  = it->path().string();
            ref.arcIndex = -1;
            g_AudioLibrary.push_back(std::move(ref));
        }
        break;
    }

    for (const char* arcName : {"IGC.ARC", "igc.arc", "Igc.arc"}) {
        const fs::path p = root / arcName;
        if (!fs::is_regular_file(p, ec)) continue;
        if (!g_IgcArc.Open(p.string())) break;
        const auto& entries = g_IgcArc.Entries();
        for (size_t i = 0; i < entries.size(); ++i) {
            AudioSourceRef ref;
            ref.name = entries[i].name;
            const size_t dot = ref.name.find_last_of('.');
            if (dot != std::string::npos) ref.name.resize(dot);
            ref.group = "Cutscenes";
            ref.path = p.string();
            ref.arcIndex = (int)i;
            g_AudioLibrary.push_back(std::move(ref));
        }
        break;
    }

    std::cout << "[audio] library: " << g_AudioLibrary.size() << " tracks beside "
              << fs::path(RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Path()).filename().string() << " (looked in "
              << root.string() << ")\n";
    if (!g_AudioLibrary.empty() && !state.audioAutoOpened) {
        state.audioAutoOpened = true;
        state.showAudioPlayer = true;
    }
}

} // namespace Viewer
} // namespace ClimaxEngine

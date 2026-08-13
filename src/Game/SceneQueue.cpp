#include "ClimaxEngine/Game/SceneQueue.h"

namespace ClimaxEngine {
namespace Game {

const char *SceneCmdName(SceneCmd c) {
    switch (c) {
    case SceneCmd::Begin:          return "Begin";
    case SceneCmd::OpenArchive:    return "OpenArchive";
    case SceneCmd::ReadHeader:     return "ReadHeader";
    case SceneCmd::Prepare:        return "Prepare";
    case SceneCmd::Step4:          return "Step4";
    case SceneCmd::ClearWorld:     return "ClearWorld";
    case SceneCmd::ReleaseRes:     return "ReleaseResources";
    case SceneCmd::Instantiate:    return "Instantiate";
    case SceneCmd::FinishTeardown: return "FinishTeardown";
    case SceneCmd::HandOver:       return "HandOver";
    case SceneCmd::ResetSystems:   return "ResetSystems";
    case SceneCmd::FadeOut:        return "FadeOut";
    case SceneCmd::FadeIn:         return "FadeIn";
    case SceneCmd::StepE:          return "StepE";
    case SceneCmd::StepF:          return "StepF";
    case SceneCmd::FreeTemp:       return "FreeTemporaries";
    case SceneCmd::LevelAudio:     return "LevelAudio";
    }
    return "?";
}

// The order the queue is filled in.
//
// Down through black first, then the old scene goes away, then the new one is
// read, built and made audible, and only then the picture comes back and
// control is handed over. Teardown before read is what keeps the peak memory
// down on a console with 32 MB, and it is why a level change on the PS2 shows
// black rather than the old room.
void SceneQueue::LoadScene(const std::string &name) {
    m_scene = name;
    m_nodes.clear();
    const SceneCmd order[] = {
        SceneCmd::FadeOut,
        SceneCmd::ClearWorld,
        SceneCmd::ReleaseRes,
        SceneCmd::FinishTeardown,
        SceneCmd::OpenArchive,
        SceneCmd::ReadHeader,
        SceneCmd::Prepare,
        SceneCmd::Instantiate,
        SceneCmd::FreeTemp,
        SceneCmd::ResetSystems,
        SceneCmd::LevelAudio,
        SceneCmd::FadeIn,
        SceneCmd::HandOver,
    };
    for (SceneCmd c : order)
        m_nodes.push_back(SceneNode{c, name});
}

SceneCmd SceneQueue::Current() const {
    return m_nodes.empty() ? SceneCmd::HandOver : m_nodes.front().cmd;
}

// FUN_00179D60. Run nodes until one is not finished; that one stays at the
// head and is tried again next frame.
void SceneQueue::Update(SceneQueueHost &host, float dt) {
    (void)dt;
    while (!m_nodes.empty()) {
        const SceneNode &n = m_nodes.front();
        bool done = true;
        switch (n.cmd) {
        case SceneCmd::FadeOut:        done = host.FadeOut(fadeSeconds); break;
        case SceneCmd::FadeIn:         done = host.FadeIn(fadeSeconds); break;
        case SceneCmd::ClearWorld:     done = host.ClearWorld(); break;
        case SceneCmd::ReleaseRes:     done = host.ReleaseResources(); break;
        case SceneCmd::FinishTeardown: done = host.FinishTeardown(); break;
        case SceneCmd::OpenArchive:    done = host.OpenArchive(n.arg); break;
        case SceneCmd::ReadHeader:     done = host.ReadHeader(); break;
        case SceneCmd::Prepare:        done = host.Prepare(); break;
        case SceneCmd::Instantiate:    done = host.Instantiate(n.arg); break;
        case SceneCmd::FreeTemp:       done = host.FreeTemporaries(); break;
        case SceneCmd::ResetSystems:   done = host.ResetSystems(); break;
        case SceneCmd::LevelAudio:     done = host.StartLevelAudio(); break;
        case SceneCmd::HandOver:       done = host.HandOver(); break;
        case SceneCmd::Begin:
        case SceneCmd::Step4:
        case SceneCmd::StepE:
        case SceneCmd::StepF:          done = host.Unread(n.cmd); break;
        }
        if (!done)
            return;   // the handler returned 0: stop here, retry next frame
        m_nodes.pop_front();
    }
}

} // namespace Game
} // namespace ClimaxEngine

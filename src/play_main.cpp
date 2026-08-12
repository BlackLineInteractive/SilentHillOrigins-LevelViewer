// ─────────────────────────────────────────────────────────────────────────────
// climax-play — the game, without the toolkit.
//
// This exists to make the boundary real. It links climax-core and climax-game
// and nothing else of ours; if anyone puts an ImGui include into either, this
// target stops linking, and the rule stops being a comment in a header.
//
// It is small on purpose. A window, a 2D pass, the boot sequence, and the main
// menu read out of the retail archive: the four XML screens, their button
// textures, the string table and the font. What it draws is not a mock-up of
// the menu; it is the menu, at the coordinates the game ships.
// ─────────────────────────────────────────────────────────────────────────────
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ClimaxEngine/Core/RWS/FileSystem/CArchive.h"
#include "ClimaxEngine/Core/UI/Font.h"
#include "ClimaxEngine/Core/UI/ScreenDef.h"
#include "ClimaxEngine/Core/UI/StringTable.h"
#include "ClimaxEngine/Game/FrontEnd.h"
#include "ClimaxEngine/Platform/PS2/AudioParser.h"
#include "ClimaxEngine/Platform/PS2/PS2Texture.h"
#include "ClimaxEngine/Platform/PS2/RwsAudio.h"

#ifdef CLIMAX_HAVE_FFMPEG
#include "ClimaxEngine/Rendering/VideoPlayer.h"
#endif

using namespace ClimaxEngine;

// The game authors its screens at 1280x720 for widescreen; every xpos/ypos in
// the XML is in that space, and the 4:3 pair is for 640x480.
static const float kAuthorW = 1280.0f;
static const float kAuthorH = 720.0f;

// ── a 2D pass, which is all a menu needs ─────────────────────────────────────

namespace {

const char *kVert = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform vec2 uScreen;
out vec2 vUV;
void main() {
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0,
                    1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
})";

const char *kFrag = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec4 uColour;
uniform int uTextured;
out vec4 o;
void main() {
    o = uTextured != 0 ? texture(uTex, vUV) * uColour : uColour;
})";

GLuint Compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[play] shader: %s\n", log);
    }
    return s;
}

class Painter {
public:
    bool Init() {
        m_prog = glCreateProgram();
        glAttachShader(m_prog, Compile(GL_VERTEX_SHADER, kVert));
        glAttachShader(m_prog, Compile(GL_FRAGMENT_SHADER, kFrag));
        glLinkProgram(m_prog);
        m_uScreen = glGetUniformLocation(m_prog, "uScreen");
        m_uColour = glGetUniformLocation(m_prog, "uColour");
        m_uTextured = glGetUniformLocation(m_prog, "uTextured");

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void *)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void *)8);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        return m_prog != 0;
    }

    void Begin(int w, int h) {
        glUseProgram(m_prog);
        glUniform2f(m_uScreen, (float)w, (float)h);
        glBindVertexArray(m_vao);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
    }

    void Quad(float x, float y, float w, float h, GLuint tex, float r, float g,
              float b, float a, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
        const float v[24] = {
            x,     y,     u0, v0,  x + w, y,     u1, v0,  x + w, y + h, u1, v1,
            x,     y,     u0, v0,  x + w, y + h, u1, v1,  x,     y + h, u0, v1,
        };
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
        glUniform4f(m_uColour, r, g, b, a);
        glUniform1i(m_uTextured, tex ? 1 : 0);
        if (tex) glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

private:
    GLuint m_prog = 0, m_vao = 0, m_vbo = 0;
    GLint m_uScreen = -1, m_uColour = -1, m_uTextured = -1;
};

// A font baked into one texture, with where each glyph landed.
//
// The shipped font stores each character as its own run-length block, which is
// right for a console streaming from disc and wrong for a GPU: 137 draw calls
// with 137 texture binds. Expanding them once into an atlas turns a line of
// text into one bind and a quad per character.
class TextAtlas {
public:
    struct Placed { int x, y, w, h; };

    bool Build(const UI::Font &font) {
        // A row packer is enough: the glyphs are all under 40 px and sorted
        // roughly by height already.
        const int kW = 512;
        int penX = 0, penY = 0, rowH = 0;
        std::vector<uint8_t> pixels((size_t)kW * 512, 0);

        std::vector<uint8_t> bmp;
        for (const UI::Glyph &g : font.Glyphs()) {
            int w = 0, h = 0;
            if (!font.Rasterise(g.code, bmp, w, h) || w == 0 || h == 0) continue;
            if (penX + w + 1 > kW) { penX = 0; penY += rowH + 1; rowH = 0; }
            if (penY + h > 512) break;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    pixels[(size_t)(penY + y) * kW + penX + x] = bmp[(size_t)y * w + x];
            m_where[g.code] = {penX, penY, w, h};
            penX += w + 1;
            if (h > rowH) rowH = h;
        }

        // One channel of coverage, expanded to RGBA so the one shader can draw
        // both text and button art without a second path.
        std::vector<uint8_t> rgba((size_t)kW * 512 * 4);
        for (size_t i = 0; i < (size_t)kW * 512; ++i) {
            rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = pixels[i];
        }
        glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kW, 512, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_size = 512.0f;
        return m_tex != 0;
    }

    GLuint Texture() const { return m_tex; }
    const Placed *Where(uint16_t code) const {
        auto it = m_where.find(code);
        return it == m_where.end() ? nullptr : &it->second;
    }
    float AtlasSize() const { return m_size; }

private:
    GLuint m_tex = 0;
    float m_size = 512.0f;
    std::map<uint16_t, Placed> m_where;
};

// Resolves a texture name as the XML writes it.
//
// Two things have to happen. The extension is dropped: the archive stores
// "sho_arrow", the XML asks for "sho_arrow.png". And `**` is a placeholder for
// the display mode -- "sho_inv_bd_**.jpg" is "sho_inv_bd_pw" in UiDataPW, where
// p/n is PAL or NTSC and w/4 is the aspect. Nineteen of the thirty-six textures
// the UI references are written that way, so without the substitution half the
// backgrounds are simply absent.
std::string ResolveTextureName(std::string name, const char *mode) {
    const size_t star = name.find("**");
    if (star != std::string::npos)
        name.replace(star, 2, mode);
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos)
        name.erase(dot);
    return name;
}

// Scans a raw Startup container blob for JPEG data (FF D8 … FF D9).
// For each JPEG, looks back up to 128 bytes for a null-terminated name string
// and uploads the decoded image into `textures` using the name without its
// extension, so ResolveTextureName("sho_aspect_pw.jpg") finds it.
// Decodes one JPEG blob into a GL texture. Shared by the Startup container scan
// and by the standalone entries -- splash_p4.jpg is its own entry in SH.ARC, not
// part of Startup, so it needs the same decode without the name-scanning.
static GLuint MakeTextureFromJpeg(const uint8_t *data, size_t len,
                                  int &wOut, int &hOut) {
    SDL_RWops *rw = SDL_RWFromConstMem(data, (int)len);
    SDL_Surface *surf = IMG_Load_RW(rw, 1);
    if (!surf) return 0;
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!rgba) return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Read before freeing -- reading rgba->w after SDL_FreeSurface printed
    // "0x0" for every background until it was fixed.
    wOut = rgba->w; hOut = rgba->h;
    SDL_FreeSurface(rgba);
    return id;
}

static void LoadStartupJpegs(const std::vector<uint8_t> &buf,
                              std::map<std::string, GLuint> &textures,
                              bool checkOnly) {
    const uint8_t *d = buf.data();
    const size_t sz = buf.size();

    size_t pos = 0;
    while (pos + 1 < sz) {
        if (d[pos] != 0xFF || d[pos + 1] != 0xD8) { ++pos; continue; }

        // Find EOI
        size_t end = pos + 2;
        while (end + 1 < sz && !(d[end] == 0xFF && d[end + 1] == 0xD9))
            ++end;
        if (end + 1 >= sz) break;
        end += 2;

        const size_t jpegLen = end - pos;
        if (jpegLen < 1024) { pos = end; continue; } // flags/icons only, skip

        // Look back up to 256 bytes for a "sho_*.jpg" null-terminated string.
        // The container stores the name right before the JPEG data block.
        std::string key;
        const size_t lookback = std::min(pos, (size_t)256);
        for (size_t off = 1; off <= lookback; ++off) {
            const uint8_t *p = d + pos - off;
            if (*p != 0) continue; // looking for null terminator

            // Walk back to find string start
            const uint8_t *q = p - 1;
            while (q > d && *q >= 0x20 && *q < 0x7F) --q;
            ++q;
            const size_t len = (size_t)(p - q);

            // Must start with "sho_" and end with ".jpg"
            if (len >= 12 && len < 64) {
                std::string s((const char *)q, len);
                if (s.rfind("sho_", 0) == 0 &&
                    s.size() > 4 && s.substr(s.size() - 4) == ".jpg") {
                    // Strip extension → lookup key
                    key = s.substr(0, s.size() - 4);
                    break;
                }
            }
        }

        if (!key.empty()) {
            if (!checkOnly && textures.find(key) == textures.end()) {
                int jw = 0, jh = 0;
                const GLuint id = MakeTextureFromJpeg(d + pos, jpegLen, jw, jh);
                if (id) {
                    textures[key] = id;
                    std::fprintf(stderr, "[play] bg '%s' %dx%d\n",
                                 key.c_str(), jw, jh);
                } else {
                    std::fprintf(stderr, "[play] IMG failed '%s': %s\n",
                                 key.c_str(), IMG_GetError());
                }
            } else if (checkOnly) {
                textures[key] = 1;
            }
        }
        pos = end;
    }
}

// Walks a UTF-8 string, yielding code points.
template <typename F>
void ForEachCodePoint(const std::string &s, F &&fn) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        uint32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0)      { cp = c & 0x07; len = 4; }
        else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
        else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
        if (i + len > s.size()) break;
        for (size_t k = 1; k < len; ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        i += len;
        fn(cp);
    }
}

// Reads an entry out of the archive.
bool ReadEntry(RWS::FileSystem::CArchive &arc, const char *name,
               std::vector<uint8_t> &out) {
    const int i = arc.Find(name);
    return i >= 0 && arc.Read((size_t)i, out) && !out.empty();
}

// Walks a container's sections looking for one of a given RenderWare type.
const uint8_t *FindSection(const std::vector<uint8_t> &buf, const char *type,
                           size_t &sizeOut) {
    const uint32_t RW = 0x1C020065;
    size_t off = 0;
    while (off + 12 <= buf.size()) {
        uint32_t t, s, v;
        std::memcpy(&t, &buf[off], 4);
        std::memcpy(&s, &buf[off + 4], 4);
        std::memcpy(&v, &buf[off + 8], 4);
        if (v != RW || s == 0 || off + 12 + s > buf.size()) break;
        const size_t inner = off + 12;
        uint32_t hdr, tagLen;
        std::memcpy(&hdr, &buf[inner], 4);
        std::memcpy(&tagLen, &buf[inner + 4], 4);
        const size_t g = inner + 8 + tagLen;
        if (g + 20 < buf.size() &&
            std::strncmp((const char *)&buf[g + 20], type, std::strlen(type)) == 0) {
            const size_t data = inner + 4 + hdr;
            sizeOut = buf.size() - data;
            return &buf[data];
        }
        off += 12 + s;
    }
    sizeOut = 0;
    return nullptr;
}

#ifdef CLIMAX_HAVE_FFMPEG
bool FileExists(const std::string &p) {
    std::ifstream f(p);
    return f.good();
}

// Letter suffix a language adds to a movie's base name -- LOGOWF, GOMOVNS --
// matching the disc's own naming (F/G/I/S; no suffix is English). Not every
// base name has every language: MENU ships only MENUW/MENUN.
std::string MovieLangSuffix(Game::Language l) {
    switch (l) {
    case Game::Language::French:  return "F";
    case Game::Language::German:  return "G";
    case Game::Language::Italian: return "I";
    case Game::Language::Spanish: return "S";
    default: return "";
    }
}

// Resolves `base` ("LOGO", "MENU") plus aspect and language to a converted
// clip under moviesDir, mirroring the disc's own <first letter>/<name>.mp4
// layout (see tools/convert_movies.py). Falls back to the unsuffixed, then to
// the opposite-aspect file, so a still-incomplete conversion degrades instead
// of leaving the stage with nothing to show.
std::string ResolveMoviePath(const std::string &moviesDir, const std::string &base,
                             bool wide, Game::Language lang) {
    const std::string aspect = wide ? "W" : "N";
    auto pathFor = [&](const std::string &name) {
        return moviesDir + "/" + name.substr(0, 1) + "/" + name + ".mp4";
    };
    std::string name = base + aspect + MovieLangSuffix(lang);
    std::string path = pathFor(name);
    if (FileExists(path)) return path;

    name = base + aspect;   // no per-language cut for this base (e.g. MENU)
    path = pathFor(name);
    if (FileExists(path)) return path;

    name = base + (wide ? "N" : "W");   // whatever aspect was actually converted
    path = pathFor(name);
    if (FileExists(path)) return path;

    return {};
}
#endif

} // namespace

int main(int argc, char **argv) {
    const char *arcPath = "game-iso/SHO/SH.ARC";
    std::string moviesDir = "SHO-port/MOVIES";   // tools/convert_movies.py's output
    // --check loads everything and reports, without opening a window. The data
    // half is worth testing on its own: it is the half that can be wrong
    // quietly, and it is the half a build machine can run.
    bool checkOnly = false;
    std::string dumpEntry;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--check") == 0) checkOnly = true;
        else if (std::strcmp(argv[i], "--movies") == 0 && i + 1 < argc) moviesDir = argv[++i];
        // Writes one archive entry to stdout verbatim. The game reads its
        // screens out of SH.ARC, so anything worth looking at is in there.
        else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) dumpEntry = argv[++i];
        else arcPath = argv[i];
    }

    RWS::FileSystem::CArchive arc;
    if (!arc.Open(arcPath)) {
        std::fprintf(stderr, "[play] cannot open %s\n", arcPath);
        return 1;
    }

    if (!checkOnly && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[play] SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (!checkOnly) {
        const int imgFlags = IMG_INIT_JPG;
        if ((IMG_Init(imgFlags) & imgFlags) != imgFlags)
            std::fprintf(stderr, "[play] SDL_image JPEG support: %s\n", IMG_GetError());
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *win = nullptr;
    SDL_GLContext ctx = nullptr;
    Painter painter;
    if (!checkOnly) {
        win = SDL_CreateWindow("Silent Hill Origins", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, 1280, 720,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
                                   SDL_WINDOW_RESIZABLE);
        ctx = SDL_GL_CreateContext(win);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::fprintf(stderr, "[play] no GL\n");
            return 1;
        }
        if (!painter.Init()) return 1;
    }

    // ── textures: the decoder hands pixels here, this sink uploads them ──────
    std::map<std::string, GLuint> textures;
    SetTextureExists([&](const std::string &n) { return textures.count(n) != 0; });
    SetTextureSink([&](RawTexture &raw, const std::vector<uint8_t> &rgba, int w,
                       int h) {
        if (checkOnly) { textures[raw.name] = 1; return; }
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        textures[raw.name] = id;
        raw.glID = id;
    });

    // ── the data the menu is made of ────────────────────────────────────────
    std::vector<uint8_t> buf;

    UI::StringTable strings;
    if (ReadEntry(arc, "Strings.Eng", buf))
        strings.Load(buf.data(), buf.size());

    UI::Font font;
    TextAtlas atlas;
    std::vector<uint8_t> fontBuf;
    if (ReadEntry(arc, "FontEUR", fontBuf)) {
        size_t n = 0;
        if (const uint8_t *k = FindSection(fontBuf, "rwID_KFONT", n))
            font.Load(k, n);
        ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(fontBuf, {}, true);
        if (!checkOnly) atlas.Build(font);
    }

    // Two sets of art: the shared one, and the per-language buttons. UiData
    // comes in four variants -- PAL and NTSC, widescreen and 4:3 -- and they
    // differ only in the backgrounds' aspect, so the widescreen PAL set is
    // loaded and the 4:3 layout simply uses the same images.
    // GlobalStream carries the shared widgets -- arrows, frames, cursors.
    // Startup holds the boot screens: the two backgrounds and the six flags,
    // each with a highlighted twin and a selection frame.
    for (const char *name : {"GlobalStream", "UiDataPW", "LocaleUIEng"})
        if (ReadEntry(arc, name, buf))
            ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(buf, {}, true);

    // Startup is a SHO container that embeds both PS2 textures (the flags) and
    // raw JPEG blobs (the full-screen backgrounds). The PS2 decoder handles the
    // flags; a separate pass pulls out the JPEGs by scanning for FF D8 markers.
    if (ReadEntry(arc, "Startup", buf)) {
        ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(buf, {}, true);
        LoadStartupJpegs(buf, textures, checkOnly);
    }

    // The copyright plate is a single JPEG, its own entry in SH.ARC rather than
    // part of Startup: the logo and the whole copyright block are baked into the
    // picture, so there is no wording to supply. Only 4:3 variants ship -- p4
    // for PAL, n4 for NTSC, n4Jap -- because it is shown before the player has
    // been asked about the aspect.
    for (const char *n : {"splash_p4", "splash_n4"}) {
        const std::string key = n;
        if (textures.count(key) || !ReadEntry(arc, (key + ".jpg").c_str(), buf))
            continue;
        // The entry wraps the JPEG in a section header; the picture starts at
        // the SOI marker.
        size_t a = 0;
        while (a + 1 < buf.size() && !(buf[a] == 0xFF && buf[a + 1] == 0xD8)) ++a;
        if (a + 1 >= buf.size()) continue;
        if (checkOnly) { textures[key] = 1; std::fprintf(stderr, "[play] splash '%s'\n", key.c_str()); continue; }
        int jw = 0, jh = 0;
        if (const GLuint id = MakeTextureFromJpeg(buf.data() + a, buf.size() - a, jw, jh)) {
            textures[key] = id;
            std::fprintf(stderr, "[play] splash '%s' %dx%d\n", key.c_str(), jw, jh);
        }
    }

    if (!dumpEntry.empty()) {
        if (!ReadEntry(arc, dumpEntry.c_str(), buf)) {
            std::fprintf(stderr, "[play] %s: not in the archive\n", dumpEntry.c_str());
            return 1;
        }
        std::fwrite(buf.data(), 1, buf.size(), stdout);
        return 0;
    }

    std::vector<std::unique_ptr<UI::Element>> owned;
    std::vector<std::pair<std::string, const UI::Element *>> screens;
    // `bootmenu` is the screen the game puts up first. It is an entry in
    // SH.ARC like every other screen -- there is nothing to reconstruct and
    // nothing to type in by hand.
    const UI::Element *bootScreen = nullptr;
    for (const char *name : {"bootmenu", "mainmenu", "newgame", "gameoptions",
                             "pausemenu"}) {
        // Every step here used to fail silently, so a screen that did not load
        // was indistinguishable from one that loaded empty.
        if (!ReadEntry(arc, (std::string(name) + ".xml").c_str(), buf)) {
            std::fprintf(stderr, "[play] %s.xml: not in the archive\n", name);
            continue;
        }
        auto root = std::make_unique<UI::Element>();
        std::string err;
        if (!UI::ParseXml((const char *)buf.data(), buf.size(), *root, &err)) {
            std::fprintf(stderr, "[play] %s.xml: %s\n", name, err.c_str());
            continue;
        }
        const UI::Element *scr = root->Find("SCREEN");
        if (!scr) {
            std::fprintf(stderr, "[play] %s.xml: parsed, but no <SCREEN> (root <%s>, %zu children)\n",
                         name, root->tag.c_str(), root->children.size());
        } else {
            screens.emplace_back(name, scr);
            if (std::string(name) == "bootmenu")
                bootScreen = scr;
        }
        owned.push_back(std::move(root));
    }
    std::fprintf(stderr, "[play] %zu screens, %zu strings, %zu glyphs, %zu textures\n",
                 screens.size(), strings.Count(), font.Glyphs().size(), textures.size());
    // The front end asks for text at sizes 16 and 14 (DAT_00338A3C /
    // DAT_00338A40). Those are heights in the 512x448 space, so the scale that
    // turns a glyph into one of them depends on what the font's own glyphs
    // measure -- take it from the font instead of assuming a number.
    float kFontNativeH = 16.0f;
    {
        int tallest = 0;
        for (const UI::Glyph &g : font.Glyphs())
            if ((int)g.height > tallest) tallest = (int)g.height;
        if (tallest > 0) kFontNativeH = (float)tallest;
        std::fprintf(stderr, "[play] font native height %d px\n", tallest);
    }

    // Draws a line and returns its width, so the same code can centre it by
    // measuring first. Kerning comes from the font's own table.
    auto drawText = [&](float x, float y, float scale, const std::string &text,
                        float r, float g, float b, float a, bool measureOnly) {
        float pen = x;
        uint16_t prev = 0;
        ForEachCodePoint(text, [&](uint32_t cp) {
            if (cp == 1 || cp == 3 || cp > 0xFFFF) return;   // markers, controls
            const UI::Glyph *gl = font.Find((uint16_t)cp);
            if (!gl) return;
            if (prev) pen += font.Kerning(prev, (uint16_t)cp) * scale;
            if (!measureOnly) {
                if (const TextAtlas::Placed *w = atlas.Where((uint16_t)cp)) {
                    const float S = atlas.AtlasSize();
                    painter.Quad(pen + gl->xOffset * scale, y + gl->yOffset * scale,
                                 w->w * scale, w->h * scale, atlas.Texture(),
                                 r, g, b, a,
                                 w->x / S, w->y / S,
                                 (w->x + w->w) / S, (w->y + w->h) / S);
                }
            }
            pen += gl->advance * scale;
            prev = (uint16_t)cp;
        });
        return pen - x;
    };
    auto measure = [&](const std::string &t, float sc) {
        return drawText(0, 0, sc, t, 0, 0, 0, 0, true);
    };
    auto centre = [&](float cx, float y, float sc, const std::string &t,
                      float r, float g, float b, float a) {
        drawText(cx - measure(t, sc) * 0.5f, y, sc, t, r, g, b, a, false);
    };

    Game::FrontEnd front;
    front.Menu().SetScreens(screens);

    if (checkOnly) {
        // Walk the boot sequence with a synthetic keypress each frame, so the
        // stage order and the menu wiring are exercised rather than assumed.
        Game::MenuInput press;
        press.anyKey = press.accept = true;
        Game::BootStage last = front.Stage();
        std::fprintf(stderr, "[check] stage %s\n", Game::BootStageName(last));
        for (int i = 0; i < 600 && front.Stage() != Game::BootStage::MainMenu; ++i) {
            front.Update(0.05f, i % 20 == 19 ? press : Game::MenuInput{});
            if (front.Stage() != last) {
                last = front.Stage();
                std::fprintf(stderr, "[check] stage %s\n", Game::BootStageName(last));
            }
        }
        std::fprintf(stderr, "[check] screen '%s', active '%s'\n",
                     front.Menu().ScreenId().c_str(), front.Menu().ActiveId().c_str());
        Game::MenuInput down; down.down = true;
        for (int i = 0; i < 4; ++i) {
            front.Update(0.016f, down);
            std::fprintf(stderr, "[check]   down -> '%s'\n",
                         front.Menu().ActiveId().c_str());
        }
        Game::MenuInput ok; ok.accept = true;
        const std::string cmd = front.Update(0.016f, ok);
        std::fprintf(stderr, "[check] accept -> screen '%s'%s\n",
                     front.Menu().ScreenId().c_str(),
                     cmd.empty() ? "" : (" command '" + cmd + "'").c_str());
        return 0;
    }

    // ── video ───────────────────────────────────────────────────────────────
    // The idents, the content notice and the menu background are movies in the
    // retail game (`bgmovie="Menu"` in mainmenu.xml; LOGOW/LOGON covers both the
    // idents and the notice as one 16.76 s clip -- there is no separate warning
    // asset anywhere in the archive). Played from tools/convert_movies.py's
    // output, which corrects the aspect the PS2 stretches on output and the
    // .PSS files do not record.
#ifdef CLIMAX_HAVE_FFMPEG
    VideoPlayer logoVideo, menuVideo;
    bool haveVideoSupport = true;
#else
    bool haveVideoSupport = false;
    std::fprintf(stderr, "[play] built without FFmpeg -- boot movies will not play\n");
#endif
    Game::BootStage lastStage = front.Stage();
    bool lastWide = true;
    bool musicStarted = false;

    // ── menu music ──────────────────────────────────────────────────────────
    // MENU.RWS sits in MUSIC/M/ relative to the disc root (one dir up from
    // the SH.ARC directory).
    AudioClip menuClip;
    {
        // Derive MUSIC path from arcPath: strip filename, go up one level.
        std::string arcDir;
        const size_t sl = std::string(arcPath).find_last_of("/\\");
        arcDir = sl != std::string::npos ? std::string(arcPath).substr(0, sl + 1) : "";
        const std::string musicPath = arcDir + "MUSIC/M/MENU.RWS";
        if (!checkOnly && ::Audio::LoadFile(musicPath, menuClip))
            std::fprintf(stderr, "[play] menu music: %s (%.0f s)\n",
                         musicPath.c_str(), menuClip.Seconds());
        else if (!checkOnly)
            std::fprintf(stderr, "[play] menu music not found at: %s\n",
                         musicPath.c_str());
    }

    // ── loop ────────────────────────────────────────────────────────────────
    bool run = true;
    uint64_t last = SDL_GetPerformanceCounter();
    while (run) {
        Game::MenuInput in;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            if (e.type != SDL_KEYDOWN) continue;
            in.anyKey = true;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:  in.cancel = true; break;
            case SDLK_UP:      in.up = true; break;
            case SDLK_DOWN:    in.down = true; break;
            case SDLK_LEFT:    in.left = true; break;
            case SDLK_RIGHT:   in.right = true; break;
            case SDLK_RETURN:
            case SDLK_SPACE:   in.accept = true; break;
            default: break;
            }
        }

        const uint64_t now = SDL_GetPerformanceCounter();
        const float dt = (float)((now - last) / (double)SDL_GetPerformanceFrequency());
        last = now;

        int w, h;
        SDL_GL_GetDrawableSize(win, &w, &h);
        // Author-space to window-space. The game ships both a widescreen and a
        // 4:3 layout, and picks by display mode; this picks the same way.
        const bool wide = (float)w / (float)h > 1.5f;

#ifdef CLIMAX_HAVE_FFMPEG
        // Advance whichever clip is current *before* FrontEnd::Update, so a
        // clip that just finished can end the Logo stage this same frame
        // instead of one frame late.
        if (front.Stage() == Game::BootStage::Logo && haveVideoSupport) {
            if (!logoVideo.IsOpen() || lastWide != wide) {
                const std::string p = ResolveMoviePath(moviesDir, "LOGO", wide, front.language);
                if (!p.empty() && logoVideo.Open(p))
                    std::fprintf(stderr, "[play] logo video: %s (%dx%d)\n",
                                 p.c_str(), logoVideo.Width(), logoVideo.Height());
                else
                    std::fprintf(stderr, "[play] no logo video at '%s' for base LOGO\n",
                                 moviesDir.c_str());
            }
            if (logoVideo.IsOpen()) {
                logoVideo.Update(dt);
                in.mediaEnded = logoVideo.Finished();
            }
        }
#endif

        const std::string cmd = front.Update(dt, in);
        if (!cmd.empty())
            std::fprintf(stderr, "[play] command: %s\n", cmd.c_str());

#ifdef CLIMAX_HAVE_FFMPEG
        if (haveVideoSupport) {
            // Enter the menu's looping background the moment the stage
            // changes, rather than waiting to be asked -- mainmenu.xml itself
            // says loop_movie="true".
            if (front.Stage() == Game::BootStage::MainMenu &&
                (lastStage != Game::BootStage::MainMenu || !menuVideo.IsOpen())) {
                const std::string p = ResolveMoviePath(moviesDir, "MENU", wide, front.language);
                if (!p.empty() && menuVideo.Open(p))
                    std::fprintf(stderr, "[play] menu video: %s (%dx%d)\n",
                                 p.c_str(), menuVideo.Width(), menuVideo.Height());
            }
            if (front.Stage() == Game::BootStage::MainMenu && menuVideo.IsOpen()) {
                menuVideo.Update(dt);
                if (menuVideo.Finished())
                    menuVideo.Restart();
            }
        }
#endif
        if (front.Stage() != lastStage) {
            std::fprintf(stderr, "[play] stage %s → %s\n",
                         Game::BootStageName(lastStage), Game::BootStageName(front.Stage()));

            // Start menu music on first transition to LanguageSelect or MainMenu.
            const bool wantMusic =
                front.Stage() == Game::BootStage::LanguageSelect ||
                front.Stage() == Game::BootStage::MainMenu;
            if (wantMusic && !musicStarted && menuClip.Valid()) {
                auto& relay = ClimaxEngine::Audio::CAudioRelay::GetInstance();
                relay.PlayMusic(menuClip, 0.65f);
                musicStarted = true;
            }
        }
        lastStage = front.Stage();
        lastWide = wide;

        glViewport(0, 0, w, h);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        painter.Begin(w, h);

        // Only the PAL sets are loaded, so the mode suffix picks between its
        // two aspects. An NTSC build would load UiDataN* and use "nw"/"n4".
        const char *texMode = wide ? "pw" : "p4";
        const float sx = (float)w / (wide ? kAuthorW : 640.0f);
        const float sy = (float)h / (wide ? kAuthorH : 480.0f);

        const UI::Element *scr = nullptr;
        std::string activeId;
        if (front.Stage() == Game::BootStage::MainMenu) {
            scr = front.Menu().Screen();
            activeId = front.Menu().ActiveId();
        } else if (front.Stage() == Game::BootStage::Loading) {
            scr = bootScreen;
        }

        if (scr) {
            {
                // `bgmovie="Menu"` is what the screen actually asks for; the
                // static `bgtexture` (when a screen has one at all -- mainmenu
                // does not) is the fallback for when no decoder is built in.
                bool drewBg = false;
#ifdef CLIMAX_HAVE_FFMPEG
                if (haveVideoSupport && menuVideo.IsOpen()) {
                    painter.Quad(0, 0, (float)w, (float)h, menuVideo.Texture(),
                                1, 1, 1, 1);
                    drewBg = true;
                }
#endif
                if (!drewBg) {
                    if (const std::string bg = scr->Attr("bgtexture"); !bg.empty()) {
                        const std::string n = ResolveTextureName(bg, texMode);
                        if (auto it = textures.find(n); it != textures.end()) {
                            painter.Quad(0, 0, (float)w, (float)h, it->second,
                                        1, 1, 1, 1);
                            drewBg = true;
                        }
                    }
                }
                if (!drewBg)
                    painter.Quad(0, 0, (float)w, (float)h, 0, 0.02f, 0.02f, 0.03f, 1.0f);

                for (const UI::Element &b : scr->children) {
                    const float bx = (wide ? b.Float("xpos") : b.Float("xpos4x3")) * sx;
                    const float by = (wide ? b.Float("ypos") : b.Float("ypos4x3")) * sy;
                    const float bw = b.Float("width") * sx;
                    const float bh = b.Float("height") * sy;

                    const bool active = !activeId.empty() && b.Attr("id") == activeId;
                    const float k = active ? 1.0f : 0.5f;

                    // Art, if the element has any. Drawing a filled rectangle
                    // when it has none was the bug behind the white bars: a
                    // TEXTBOX is text, not a box.
                    const std::string tex = ResolveTextureName(b.Attr("bgtexture"), texMode);
                    GLuint id = 0;
                    if (!tex.empty())
                        if (auto it = textures.find(tex); it != textures.end())
                            id = it->second;
                    if (id)
                        painter.Quad(bx, by, bw, bh, id, k, k, k, 1.0f,
                                     b.Float("textureu", 0.0f), b.Float("texturev", 0.0f),
                                     b.Float("texturew", 1.0f), b.Float("textureh", 1.0f));

                    // Text, if it names a string. Never the element id: that is
                    // a name for the designer, not a label for the player.
                    const std::string sid = b.Attr("string");
                    if (!sid.empty()) {
                        const std::string label = strings.Text(sid);
                        if (!label.empty()) {
                            const float tscale = sy * 1.1f;
                            const float tx = bx + b.Float("textoffx") * sx;
                            const float ty = by + b.Float("textoffy") * sy + bh * 0.8f;
                            drawText(tx, ty, tscale, label, k, k, k, 1.0f, false);
                        }
                    }
                }
            }
        } else {
            // The idents and the notice are movies in the retail game and are
            // not decoded yet, so each stage says what it is and what it wants.
            // The text is the game's own, out of Strings.Eng where there is one.
            const float cx = w * 0.5f;
            const float sc = (wide ? sy : sy) * 1.6f;
            auto tex = [&](const std::string &n) -> GLuint {
                auto it = textures.find(n);
                return it == textures.end() ? 0 : it->second;
            };
            // The aspect these images are *meant* to be seen at, which is not
            // the one they are stored at.
            //
            // Measured: sho_aspect_pw, sho_lang_bd_pw and sho_inv_bd_pw are all
            // 512x512, and MENUW.PSS is 512x512 with SAR and DAR both 1:1. The
            // PS2 stretches them on output -- the pixel is not square and
            // nothing in the file says so. Preserving the stored 1:1 is what
            // pillarboxed a widescreen image into a square, and it is the same
            // reason the movies look squeezed in any ordinary player.
            const float displayAR = wide ? 16.0f / 9.0f : 4.0f / 3.0f;

            auto fullscreen = [&](const std::string &n, float texAR = 0.0f) {
                const GLuint id = tex(n);
                if (!id) return false;
                if (texAR <= 0.0f) texAR = displayAR;
                // Fit that aspect into the window, letterboxing whichever way
                // the window differs.
                const float winAR = (float)w / (float)h;
                float dw, dh, dx, dy;
                if (winAR >= texAR) {
                    // window wider than texture — fit by height
                    dh = (float)h;
                    dw = dh * texAR;
                    dy = 0.0f;
                    dx = ((float)w - dw) * 0.5f;
                } else {
                    // window narrower — fit by width
                    dw = (float)w;
                    dh = dw / texAR;
                    dx = 0.0f;
                    dy = ((float)h - dh) * 0.5f;
                }
                painter.Quad(dx, dy, dw, dh, id, 1, 1, 1, 1);
                return true;
            };

            // The rect `fullscreen` just drew into, so that anything laid on top
            // of a backdrop lands in the same place the backdrop did.
            auto uiRect = [&](float &rx, float &ry, float &rw, float &rh) {
                const float winAR = (float)w / (float)h;
                if (winAR >= displayAR) {
                    rh = (float)h; rw = rh * displayAR;
                    ry = 0.0f;     rx = ((float)w - rw) * 0.5f;
                } else {
                    rw = (float)w; rh = rw / displayAR;
                    rx = 0.0f;     ry = ((float)h - rh) * 0.5f;
                }
            };

            // Maps the coordinates the game's own drawing code uses onto that
            // rect. The front end is authored against the PS2's 512x448 PAL
            // framebuffer -- 512 is not a guess, the flag row in FUN_00151918
            // runs from x=32 to x=480 and is visibly centred.
            auto uiQuad = [&](float ux, float uy, float uw, float uh, GLuint id,
                              float r, float g, float b, float a) {
                if (!id) return;
                float rx, ry, rw, rh;
                uiRect(rx, ry, rw, rh);
                const float kx = rw / 512.0f, ky = rh / 448.0f;
                painter.Quad(rx + ux * kx, ry + uy * ky, uw * kx, uh * ky,
                             id, r, g, b, a);
            };

            // Centres a string inside a rect given in the game's 512x448 space,
            // at one of the font sizes the front end asks for.
            auto uiText = [&](float ux, float uy, float uw, float uh, float size,
                              const std::string &t, float r, float g, float b, float a) {
                if (t.empty()) return;
                float rx, ry, rw, rh;
                uiRect(rx, ry, rw, rh);
                const float px = rw / 512.0f, py = rh / 448.0f;
                const float scale = size * py / kFontNativeH;
                const float wpx = drawText(0, 0, scale, t, 0, 0, 0, 0, true);
                drawText(rx + (ux + uw * 0.5f) * px - wpx * 0.5f,
                         ry + (uy + uh * 0.5f) * py + size * py * 0.35f,
                         scale, t, r, g, b, a, false);
            };

            // Same fit as `fullscreen`, but for a decoded video frame -- its
            // own GLuint, not one looked up in `textures` by name -- letterboxed
            // to the clip's real decoded size rather than the display aspect,
            // since a video's aspect is a property of the frame, already
            // correct, and not of the display mode the way a stretched still is.
            auto fullscreenVideo = [&](GLuint id, int texW, int texH) {
                if (!id || texW <= 0 || texH <= 0) return false;
                const float texAR = (float)texW / (float)texH;
                const float winAR = (float)w / (float)h;
                float dw, dh, dx, dy;
                if (winAR >= texAR) {
                    dh = (float)h; dw = dh * texAR;
                    dy = 0.0f; dx = ((float)w - dw) * 0.5f;
                } else {
                    dw = (float)w; dh = dw / texAR;
                    dx = 0.0f; dy = ((float)h - dh) * 0.5f;
                }
                painter.Quad(dx, dy, dw, dh, id, 1, 1, 1, 1);
                return true;
            };

            switch (front.Stage()) {
            case Game::BootStage::Logo: {
                // LOGOW/LOGON is one clip covering both the publisher/developer
                // idents and the content notice -- there is no separate warning
                // asset in the archive, so there is no separate drawing path
                // for it either.
                bool drew = false;
#ifdef CLIMAX_HAVE_FFMPEG
                if (haveVideoSupport && logoVideo.IsOpen())
                    drew = fullscreenVideo(logoVideo.Texture(), logoVideo.Width(),
                                           logoVideo.Height());
#endif
                if (!drew) {
                    // No video decoder in this build, or the clip failed to
                    // open: say so rather than drawing an invented logo, which
                    // read as the toolkit's own branding rather than the game's.
                    centre(cx, h * 0.48f, sc, "Silent Hill Origins",
                          0.55f, 0.55f, 0.6f, 1.0f);
                    centre(cx, h * 0.58f, sc * 0.6f, "(LOGOW.mp4 not found)",
                          0.35f, 0.35f, 0.4f, 1.0f);
                }
                break;
            }
            case Game::BootStage::Loading:
                // splash_p4.jpg carries the logo and the copyright block baked
                // in; "loading" is not part of the picture, it is the string
                // `loading` drawn over it. Its position is the one thing here
                // still eyeballed -- this screen's drawing code has not been
                // found, so there are no coordinates to copy.
                fullscreen("splash_p4", 4.0f / 3.0f);
                uiText(0, 268, 512, 32, 16.0f, strings.Text("loading"),
                       1.0f, 1.0f, 1.0f, 1.0f);
                break;

            case Game::BootStage::LanguageSelect: {
                fullscreen(std::string("sho_lang_bd_") + texMode);

                // The layout is the game's, out of FUN_00151918: five 80x60
                // quads on one line, first at (32,192), 92 apart. Selected is
                // drawn white and the rest at 0xC8505050 -- the original tints
                // the flag itself rather than putting a frame around it, so
                // there is no sho_flg_sel here.
                const Game::FlagRowLayout row = Game::LanguageRow();
                const int n = Game::LanguageCount();
                for (int i = 0; i < n; ++i) {
                    const auto lang = (Game::Language)i;
                    const bool sel = i == front.languageIndex;
                    GLuint id = tex(Game::LanguageFlag(lang));
                    // Some sets ship a pre-lit "_h" variant; prefer it when the
                    // entry is selected, and fall back to tinting.
                    if (sel)
                        if (const GLuint hi = tex(std::string(Game::LanguageFlag(lang)) + "_h"))
                            id = hi;
                    const float k = sel ? 1.0f : 0x50 / 255.0f;
                    const float a = sel ? 1.0f : 0xC8 / 255.0f;
                    uiQuad(row.x + i * row.step, row.y, row.w, row.h, id, k, k, k, a);
                }
                break;
            }

            case Game::BootStage::AspectSelect: {
                // Two entries, one under the other, and they are *text*:
                // FUN_00151fb8 passes `display_4x3` and `display_ws` through the
                // string table rather than looking up a texture. Same two
                // colours as the flag row.
                fullscreen(std::string("sho_aspect_") + texMode);
                // Every word here is the game's. FUN_00151fb8 passes the string
                // id at 0x6929E0 -- `display_title` -- for the heading, and the
                // two entries are `display_4x3` and `display_ws` through the
                // same string table. Typing "4:3" and "16:9" in by hand was
                // wrong even when the letters happened to come out the same.
                // Coordinates out of .data, not out of my head: the heading
                // rect is 0x00338A48 = (0,128,512,32) at size 0x00338A3C = 16,
                // the list rect is 0x00338AA8 = (0,192,512,32) at size
                // 0x00338A40 = 14, and FUN_00151fb8 advances y by that size
                // twice per row -- 28.
                uiText(0, 128, 512, 32, 16.0f, strings.Text("display_title"),
                       1.0f, 1.0f, 1.0f, 1.0f);
                for (int i = 0; i < 2; ++i) {
                    const bool sel = (i == 1) == front.widescreen;
                    const float k = sel ? 1.0f : 0x50 / 255.0f;
                    uiText(0, 192.0f + i * 28.0f, 512, 32, 14.0f,
                           strings.Text(i == 0 ? "display_4x3" : "display_ws"),
                           k, k, k, sel ? 1.0f : 0xC8 / 255.0f);
                }
                break;
            }
            default:
                break;
            }
        }

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

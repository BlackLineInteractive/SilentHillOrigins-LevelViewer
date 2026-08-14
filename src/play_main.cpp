// ─────────────────────────────────────────────────────────────────────────────
// The front end — the first half of the game.
//
// A window, a 2D pass, the boot sequence, and the main menu read out of the
// retail archive: the four XML screens, their button textures, the string
// table and the font. What it draws is not a mock-up of the menu; it is the
// menu, at the coordinates the game ships.
//
// This was `climax-play`, a separate executable, and the split was artificial:
// the boot sequence, the menu and the level are one game. It is now the front
// half of `sho-game` and ends where the game's own scene queue says it should,
// at the handover command. See ClimaxEngine/Game/PlayMain.h.
//
// It still takes no ImGui dependency, and neither does anything it calls. That
// rule now lives in `climax-game`, which links neither SDL nor GL.
// ─────────────────────────────────────────────────────────────────────────────
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
#include "ClimaxEngine/Game/PlayMain.h"
#include "ClimaxEngine/Game/SceneQueue.h"
#include "ClimaxEngine/Game/SceneObjects.h"
#include "ClimaxEngine/Loader/Loader.h"
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
            float b, float a, float u0 = 0, float v0 = 0, float u1 = 1,
            float v1 = 1) {
    const float v[24] = {
        x, y, u0, v0, x + w, y,     u1, v0, x + w, y + h, u1, v1,
        x, y, u0, v0, x + w, y + h, u1, v1, x,     y + h, u0, v1,
    };
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glUniform4f(m_uColour, r, g, b, a);
    glUniform1i(m_uTextured, tex ? 1 : 0);
    if (tex)
      glBindTexture(GL_TEXTURE_2D, tex);
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
  struct Placed {
    int x, y, w, h;
  };

  bool Build(const UI::Font &font) {
    // A row packer is enough: the glyphs are all under 40 px and sorted
    // roughly by height already.
    const int kW = 512;
    int penX = 0, penY = 0, rowH = 0;
    std::vector<uint8_t> pixels((size_t)kW * 512, 0);

    std::vector<uint8_t> bmp;
    for (const UI::Glyph &g : font.Glyphs()) {
      int w = 0, h = 0;
      if (!font.Rasterise(g.code, bmp, w, h) || w == 0 || h == 0)
        continue;
      if (penX + w + 1 > kW) {
        penX = 0;
        penY += rowH + 1;
        rowH = 0;
      }
      if (penY + h > 512)
        break;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          pixels[(size_t)(penY + y) * kW + penX + x] = bmp[(size_t)y * w + x];
      m_where[g.code] = {penX, penY, w, h};
      penX += w + 1;
      if (h > rowH)
        rowH = h;
    }

    // One channel of coverage, expanded to RGBA so the one shader can draw
    // both text and button art without a second path.
    std::vector<uint8_t> rgba((size_t)kW * 512 * 4);
    for (size_t i = 0; i < (size_t)kW * 512; ++i) {
      rgba[i * 4 + 0] = 255;
      rgba[i * 4 + 1] = 255;
      rgba[i * 4 + 2] = 255;
      rgba[i * 4 + 3] = pixels[i];
    }
    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kW, 512, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
// and by the standalone entries -- splash_p4.jpg is its own entry in SH.ARC,
// not part of Startup, so it needs the same decode without the name-scanning.
static GLuint MakeTextureFromJpeg(const uint8_t *data, size_t len, int &wOut,
                                  int &hOut) {
  SDL_RWops *rw = SDL_RWFromConstMem(data, (int)len);
  SDL_Surface *surf = IMG_Load_RW(rw, 1);
  if (!surf)
    return 0;
  SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(surf);
  if (!rgba)
    return 0;
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba->pixels);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Read before freeing -- reading rgba->w after SDL_FreeSurface printed
  // "0x0" for every background until it was fixed.
  wOut = rgba->w;
  hOut = rgba->h;
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
    if (d[pos] != 0xFF || d[pos + 1] != 0xD8) {
      ++pos;
      continue;
    }

    // Find EOI
    size_t end = pos + 2;
    while (end + 1 < sz && !(d[end] == 0xFF && d[end + 1] == 0xD9))
      ++end;
    if (end + 1 >= sz)
      break;
    end += 2;

    const size_t jpegLen = end - pos;
    if (jpegLen < 1024) {
      pos = end;
      continue;
    } // flags/icons only, skip

    // Look back up to 256 bytes for a "sho_*.jpg" null-terminated string.
    // The container stores the name right before the JPEG data block.
    std::string key;
    const size_t lookback = std::min(pos, (size_t)256);
    for (size_t off = 1; off <= lookback; ++off) {
      const uint8_t *p = d + pos - off;
      if (*p != 0)
        continue; // looking for null terminator

      // Walk back to find string start
      const uint8_t *q = p - 1;
      while (q > d && *q >= 0x20 && *q < 0x7F)
        --q;
      ++q;
      const size_t len = (size_t)(p - q);

      // Must start with "sho_" and end with ".jpg"
      if (len >= 12 && len < 64) {
        std::string s((const char *)q, len);
        if (s.rfind("sho_", 0) == 0 && s.size() > 4 &&
            s.substr(s.size() - 4) == ".jpg") {
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
          std::fprintf(stderr, "[play] bg '%s' %dx%d\n", key.c_str(), jw, jh);
        } else {
          std::fprintf(stderr, "[play] IMG failed '%s': %s\n", key.c_str(),
                       IMG_GetError());
        }
      } else if (checkOnly) {
        textures[key] = 1;
      }
    }
    pos = end;
  }
}

// Walks a UTF-8 string, yielding code points.
template <typename F> void ForEachCodePoint(const std::string &s, F &&fn) {
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = s[i];
    uint32_t cp = c;
    size_t len = 1;
    if (c >= 0xF0) {
      cp = c & 0x07;
      len = 4;
    } else if (c >= 0xE0) {
      cp = c & 0x0F;
      len = 3;
    } else if (c >= 0xC0) {
      cp = c & 0x1F;
      len = 2;
    }
    if (i + len > s.size())
      break;
    for (size_t k = 1; k < len; ++k)
      cp = (cp << 6) | (s[i + k] & 0x3F);
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
    if (v != RW || s == 0 || off + 12 + s > buf.size())
      break;
    const size_t inner = off + 12;
    uint32_t hdr, tagLen;
    std::memcpy(&hdr, &buf[inner], 4);
    std::memcpy(&tagLen, &buf[inner + 4], 4);
    const size_t g = inner + 8 + tagLen;
    if (g + 20 < buf.size() && std::strncmp((const char *)&buf[g + 20], type,
                                            std::strlen(type)) == 0) {
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
// FUN_001F3048 appends **one** character -- the first of the language's
// three-letter code (Eng, Fre, Ita, Ger, Spa) -- and falls back to the bare
// name when that file is missing. English therefore has no suffix, because
// "...E" does not exist on the disc and the fallback catches it.
std::string MovieLangSuffix(Game::Language l) {
  switch (l) {
  case Game::Language::French:
    return "F";
  case Game::Language::German:
    return "G";
  case Game::Language::Italian:
    return "I";
  case Game::Language::Spanish:
    return "S";
  default:
    return "";
  }
}

// Resolves `base` ("LOGO", "MENU") plus aspect and language to a converted
// clip under moviesDir, mirroring the disc's own <first letter>/<name>.mp4
// layout (see tools/convert_movies.py). Falls back to the unsuffixed, then to
// the opposite-aspect file, so a still-incomplete conversion degrades instead
// of leaving the stage with nothing to show.
std::string ResolveMoviePath(const std::string &moviesDir,
                             const std::string &base, bool wide,
                             Game::Language lang) {
  const std::string aspect = wide ? "W" : "N";
  auto pathFor = [&](const std::string &name) {
    return moviesDir + "/" + name.substr(0, 1) + "/" + name + ".mp4";
  };
  std::string name = base + aspect + MovieLangSuffix(lang);
  std::string path = pathFor(name);
  if (FileExists(path))
    return path;

  name = base + aspect; // no per-language cut for this base (e.g. MENU)
  path = pathFor(name);
  if (FileExists(path))
    return path;

  name = base + (wide ? "N" : "W"); // whatever aspect was actually converted
  path = pathFor(name);
  if (FileExists(path))
    return path;

  return {};
}
#endif

} // namespace

int ClimaxEngine::Game::RunFrontEnd(int argc, char **argv,
                                    ClimaxEngine::Game::FrontEndExit &out) {
  const char *arcPath = "game-iso/SHO/SH.ARC";
  std::string moviesDir = "SHO-port/MOVIES"; // tools/convert_movies.py's output
  std::string musicDir = "SHO-port/MUSIC";   // converted MUSIC/*.RWS
  // --check loads everything and reports, without opening a window. The data
  // half is worth testing on its own: it is the half that can be wrong
  // quietly, and it is the half a build machine can run.
  bool checkOnly = false;
  std::string dumpEntry;
  // --shot renders one frame of a screen to a PNG and exits. This is not a
  // convenience: without it every change to the layout is made blind, and
  // "make it bigger" degenerates into guessing at coefficients.
  std::string shotPath, shotScreen = "mainmenu";
  // --keys feeds presses to the menu before the shot, so behaviour can be
  // photographed instead of asserted. e.g. --keys down,right
  std::string shotKeys;
  // How many rows tall the front end's own framebuffer is.
  //
  // The horizontal side is read from the code: FUN_00151918 places the flag row
  // at x=32..480 and passes 0 as FUN_001B38C0's last argument, so no squeeze is
  // applied and the buffer is 512 wide. The height is not stated anywhere I
  // have found, but it is pinned by measurement rather than by preference: in
  // the retail language screen the flag row occupies 43-56% of the frame, and
  // y=192..252 over 448 rows is 42.9-56.3%. 512 puts it at 37.5-49.2% (too
  // high) and 368 at 46.7-58.5% (too low). --uirows is here to re-check that
  // against a capture, not to be tuned by eye.
  float frontEndRows = 380.0f;
  // Multiplies the XML screens' text only. The scale itself is
  // textsize / designHeight from FUN_00215190, and designHeight is the one
  // number in that formula I picked (12, the KFONT header word at 0x14)
  // without confirming which offset the runtime font struct uses. This knob is
  // that uncertainty, made visible instead of buried in a magic constant.
  float textScale = 1.3f;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--check") == 0)
      checkOnly = true;
    else if (std::strcmp(argv[i], "--movies") == 0 && i + 1 < argc)
      moviesDir = argv[++i];
    else if (std::strcmp(argv[i], "--music") == 0 && i + 1 < argc)
      musicDir = argv[++i];
    else if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc)
      shotPath = argv[++i];
    else if (std::strcmp(argv[i], "--screen") == 0 && i + 1 < argc)
      shotScreen = argv[++i];
    else if (std::strcmp(argv[i], "--keys") == 0 && i + 1 < argc)
      shotKeys = argv[++i];
    else if (std::strcmp(argv[i], "--uirows") == 0 && i + 1 < argc)
      frontEndRows = (float)std::atof(argv[++i]);
    else if (std::strcmp(argv[i], "--textscale") == 0 && i + 1 < argc)
      textScale = (float)std::atof(argv[++i]);
    // Writes one archive entry to stdout verbatim. The game reads its
    // screens out of SH.ARC, so anything worth looking at is in there.
    else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
      dumpEntry = argv[++i];
    else
      arcPath = argv[i];
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
    const int imgFlags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags)
      std::fprintf(stderr, "[play] SDL_image JPEG support: %s\n",
                   IMG_GetError());
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
    if (!painter.Init())
      return 1;
  }

  // ── textures: the decoder hands pixels here, this sink uploads them ──────
  std::map<std::string, GLuint> textures;
  SetTextureExists(
      [&](const std::string &n) { return textures.count(n) != 0; });
  SetTextureSink(
      [&](RawTexture &raw, const std::vector<uint8_t> &rgba, int w, int h) {
        if (checkOnly) {
          textures[raw.name] = 1;
          return;
        }
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        textures[raw.name] = id;
        raw.glID = id;
      });

  // ── the very first screen ───────────────────────────────────────────────
  //
  // `FUN_001D78E0` is the engine's own init, and the first thing it does --
  // before it mounts sh.arc, before any module, before the front end object
  // exists -- is resolve "splash_p4.jpg", make a texture of it, blit it at
  // (0,0) and present it *twice*, once into each framebuffer. Then it frees
  // the texture and never draws it again: the picture just stays on screen
  // for however long the initialisation takes.
  //
  // So it is not a stage and it has no timer, which is why it is not in the
  // state table at 0x00338A00. It is the frame the console is left holding.
  // Doing the same here means drawing it before the archive is read, not
  // after. Nothing is written over it -- the string `loading` at y=268 was
  // invented and is gone. The game's loading indicator is a separate texture,
  // "LoadingIcon" out of Startup, registered here by FUN_001B44A0 and spun by
  // FUN_001F0678; it is not part of this picture.
  if (!checkOnly) {
    std::vector<uint8_t> sp;
    if (ReadEntry(arc, "splash_p4.jpg", sp)) {
      size_t a = 0;
      while (a + 1 < sp.size() && !(sp[a] == 0xFF && sp[a + 1] == 0xD8))
        ++a;
      int jw = 0, jh = 0;
      const GLuint id =
          a + 1 < sp.size()
              ? MakeTextureFromJpeg(sp.data() + a, sp.size() - a, jw, jh)
              : 0;
      if (id) {
        textures["splash_p4"] = id;
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(win, &w, &h);
        // FUN_001D78E0 blits it at (0,0) straight into the
        // framebuffer -- it fills the screen, it is not fitted to an
        // aspect and not letterboxed.
        const float dx = 0.0f, dy = 0.0f, dw = (float)w, dh = (float)h;
        // Twice, into both buffers, exactly as FUN_001D78E0 does.
        for (int i = 0; i < 2; ++i) {
          glViewport(0, 0, w, h);
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT);
          painter.Begin(w, h);
          painter.Quad(dx, dy, dw, dh, id, 1, 1, 1, 1);
          SDL_GL_SwapWindow(win);
        }
        std::fprintf(stderr,
                     "[play] splash 'splash_p4' %dx%d (held while loading)\n",
                     jw, jh);

        // Hold the splash screen for 1 seconds so the user can see it
        uint64_t splashStart = SDL_GetPerformanceCounter();
        while (true) {
          uint64_t now = SDL_GetPerformanceCounter();
          if ((now - splashStart) / (double)SDL_GetPerformanceFrequency() >
              1.0) {
            break;
          }
          SDL_Event e;
          while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
              exit(0);
          }
          glViewport(0, 0, w, h);
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT);
          painter.Begin(w, h);
          painter.Quad(dx, dy, dw, dh, id, 1, 1, 1, 1);
          SDL_GL_SwapWindow(win);
        }
      }
    }
  }

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
    ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(fontBuf, {},
                                                                    true);
    if (!checkOnly)
      atlas.Build(font);
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
      ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(buf, {},
                                                                      true);

  // Startup is a SHO container that embeds both PS2 textures (the flags) and
  // raw JPEG blobs (the full-screen backgrounds). The PS2 decoder handles the
  // flags; a separate pass pulls out the JPEGs by scanning for FF D8 markers.
  if (ReadEntry(arc, "Startup", buf)) {
    ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(buf, {},
                                                                    true);
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
    while (a + 1 < buf.size() && !(buf[a] == 0xFF && buf[a + 1] == 0xD8))
      ++a;
    if (a + 1 >= buf.size())
      continue;
    if (checkOnly) {
      textures[key] = 1;
      std::fprintf(stderr, "[play] splash '%s'\n", key.c_str());
      continue;
    }
    int jw = 0, jh = 0;
    if (const GLuint id =
            MakeTextureFromJpeg(buf.data() + a, buf.size() - a, jw, jh)) {
      textures[key] = id;
      std::fprintf(stderr, "[play] splash '%s' %dx%d\n", key.c_str(), jw, jh);
    }
  }

  if (!dumpEntry.empty()) {
    if (!ReadEntry(arc, dumpEntry.c_str(), buf)) {
      std::fprintf(stderr, "[play] %s: not in the archive\n",
                   dumpEntry.c_str());
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
  for (const char *name :
       {"bootmenu", "mainmenu", "newgame", "gameoptions", "pausemenu"}) {
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
      std::fprintf(
          stderr,
          "[play] %s.xml: parsed, but no <SCREEN> (root <%s>, %zu children)\n",
          name, root->tag.c_str(), root->children.size());
    } else {
      screens.emplace_back(name, scr);
      if (std::string(name) == "bootmenu")
        bootScreen = scr;
    }
    owned.push_back(std::move(root));
  }
  std::fprintf(
      stderr, "[play] %zu screens, %zu strings, %zu glyphs, %zu textures\n",
      screens.size(), strings.Count(), font.Glyphs().size(), textures.size());
  // The front end asks for text at sizes 16 and 14 (DAT_00338A3C /
  // DAT_00338A40). Those are heights in the 910x512 authored space, so the scale that
  // turns a glyph into one of them depends on what the font's own glyphs
  // measure -- take it from the font instead of assuming a number.
  //
  // It has to be the height the font is *designed* at, because `textsize="12"`
  // means "twelve rows tall" and the scale is textsize / design height.
  //
  // Taking the tallest glyph was wrong: FontEUR carries accented capitals
  // (A-grave, A-umlaut, E-acute) and they overshoot the cap height by about a
  // third, so every string came out ~1.4x too small. That is exactly what the
  // hand-tuned `fontScaleAdjust = 1.5f` was quietly correcting, in two places.
  // A capital H *is* the cap height, so measure that and let the accents
  // overshoot the way they do on paper.
  // The divisor is the font's **ascent**, and that is not a guess any more:
  // FUN_002158B0 lays a line out as
  //
  //     scale    = size / *(ushort *)(font + 4)
  //     baseline = rect.y + size
  //
  // so `size` is exactly the distance from the top of the line down to the
  // baseline. That distance is the ascent, so the scale that makes a request
  // for `size` come out right is size / ascent.
  //
  // Glyph::yOffset is measured from the baseline with negative meaning up, so
  // the ascent is the largest -yOffset in the face. Two earlier answers were
  // wrong: the tallest glyph (17 px -- accented capitals overshoot) and then
  // the cap height (13 px, a capital H), both of which are taller than the
  // ascent and so made every string too small.
  // FUN_00215190 lays a glyph out as
  //
  //     scale  = size / *(ushort *)(font + 4)
  //     glyphH = glyph.height * scale
  //     glyphY = baseline - glyph.yOffset * scale
  //
  // and FUN_002158B0 puts the baseline at rect.y + size. So the divisor is one
  // 16-bit field in the font, and everything else follows from it.
  //
  // In the KFONT header that field reads 12 (offset 0x14, followed by 0xFFFF).
  // It is not any of the three things guessed at before -- the tallest glyph
  // (17, accented capitals overshoot), the cap height (13, a capital H), or the
  // ascent (17) -- and all three of those made the text too small, which is
  // what the hand-tuned 1.5x was compensating for.
  float kFontNativeH = 12.0f;
  {
    int ascent = 0, tallest = 0, cap = 0;
    for (const UI::Glyph &g : font.Glyphs()) {
      if (-(int)g.yOffset > ascent) ascent = -(int)g.yOffset;
      if ((int)g.height > tallest) tallest = (int)g.height;
    }
    if (const UI::Glyph *h = font.Find((uint16_t)'H')) cap = (int)h->height;
    std::fprintf(stderr,
                 "[play] font design height %.0f (ascent %d, cap %d, tallest %d)\n",
                 kFontNativeH, ascent, cap, tallest);
  }

  // Draws a line and returns its width, so the same code can centre it by
  // measuring first. Kerning comes from the font's own table.
  auto drawText = [&](float x, float y, float scale, const std::string &text,
                      float r, float g, float b, float a, bool measureOnly) {
    auto drawInner = [&](float ox, float oy, float cr, float cg, float cb,
                         float ca) {
      float pen = x;
      uint16_t prev = 0;
      ForEachCodePoint(text, [&](uint32_t cp) {
        if (cp == 1 || cp == 3 || cp > 0xFFFF)
          return;
        const UI::Glyph *gl = font.Find((uint16_t)cp);
        if (!gl)
          return;
        if (prev)
          pen += font.Kerning(prev, (uint16_t)cp) * scale;
        if (!measureOnly) {
          if (const TextAtlas::Placed *w = atlas.Where((uint16_t)cp)) {
            const float S = atlas.AtlasSize();
            painter.Quad(
                pen + gl->xOffset * scale + ox, y + gl->yOffset * scale + oy,
                w->w * scale, w->h * scale, atlas.Texture(), cr, cg, cb, ca,
                w->x / S, w->y / S, (w->x + w->w) / S, (w->y + w->h) / S);
          }
        }
        pen += gl->advance * scale;
        prev = (uint16_t)cp;
      });
      return pen - x;
    };

    if (measureOnly)
      return drawInner(0, 0, 0, 0, 0, 0);

    // Drop shadow (drawn first so it is behind)
    drawInner(1.5f * scale, 1.5f * scale, 0, 0, 0, a);

    // Bold effect (draw slightly shifted, e.g. 0.5px right)
    drawInner(0.5f * scale, 0.0f, r, g, b, a);

    // Main text
    return drawInner(0, 0, r, g, b, a);
  };
  auto measure = [&](const std::string &t, float sc) {
    return drawText(0, 0, sc, t, 0, 0, 0, 0, true);
  };
  auto centre = [&](float cx, float y, float sc, const std::string &t, float r,
                    float g, float b, float a) {
    drawText(cx - measure(t, sc) * 0.5f, y, sc, t, r, g, b, a, false);
  };

  // ── video ───────────────────────────────────────────────────────────────
  // Every background in the front end is a movie, not a still. The state
  // machine asks for them by bare name -- "Back" behind the language and
  // aspect screens, "Logo" as a state of its own -- and FUN_00180798 turns
  // that name into <movies>/<first letter>/<name><N|W>.pss, the N/W picked by
  // the display-mode global this same front end sets. Here they come from
  // tools/convert_movies.py's output, which also corrects the aspect the PS2
  // stretches on output and the .PSS files do not record.
#ifdef CLIMAX_HAVE_FFMPEG
  VideoPlayer bgVideo, menuVideo;
  bool haveVideoSupport = true;
#else
  bool haveVideoSupport = false;
  std::fprintf(stderr,
               "[play] built without FFmpeg -- boot movies will not play\n");
#endif
  bool bgLooping = false;
  std::string bgName;

  // ── the two clips the boot sequence needs ───────────────────────────────
  //
  // MUSIC/*.RWS is interleaved ADPCM and stutters through the current
  // decoder, so prefer the converted WAVs under SHO-port/MUSIC and only fall
  // back to the disc if they are not there.
  //
  // LUMBRYRD is the logo's audio: it runs with LOGO*, and the menu's own
  // ambient starts the moment the logo ends -- there is no gap and no fade
  // built into the audio itself.
  AudioClip menuClip, logoClip, clickClip, selectClip;
  bool haveAudioSupport =
      ClimaxEngine::Audio::CAudioRelay::GetInstance().Init();
  if (!checkOnly) {
    std::string arcDir;
    const size_t sl = std::string(arcPath).find_last_of("/\\");
    arcDir =
        sl != std::string::npos ? std::string(arcPath).substr(0, sl + 1) : "";
    auto loadTrack = [&](const char *letter, const char *stem,
                         AudioClip &into) {
      const std::string wav = musicDir + "/" + letter + "/" + stem + ".WAV";
      const std::string rws = arcDir + "MUSIC/" + letter + "/" + stem + ".RWS";
      if (::Audio::LoadFile(wav, into))
        std::fprintf(stderr, "[play] audio %s: %s (%.1f s)\n", stem,
                     wav.c_str(), into.Seconds());
      else if (::Audio::LoadFile(rws, into))
        std::fprintf(stderr, "[play] audio %s: %s (%.1f s, RWS fallback)\n",
                     stem, rws.c_str(), into.Seconds());
      else
        std::fprintf(stderr, "[play] audio %s: not found (%s)\n", stem,
                     wav.c_str());
    };
    // Background movies and audio
    loadTrack("L", "LUMBRYRD", logoClip);
    loadTrack("M", "MENU", menuClip);

    // UI sounds (placeholders, will load if these WAVs exist in the directory)
    loadTrack("S", "CursorMove", clickClip);
    loadTrack("S", "Select", selectClip);
  }
  bool wide = true; // updated per frame from the window, seeded widescreen

  // ── the host side of the front end ──────────────────────────────────────
  //
  // One method per function in the executable. The state machine in
  // Game::FrontEnd is a transcription and knows nothing about GL, SDL or
  // FFmpeg; everything platform-shaped lives here.
  struct PlayHost : Game::FrontEndHost {
    std::function<bool(const char *, bool)> playMovie;
    std::function<void()> stopMovie;
    std::function<void(int)> loadStringsFor;
    int language = 0;    // 0x0033F044
    int displayMode = 1; // base+0x198; 1 = widescreen
    bool left = false;

    bool PlayMovie(const char *n, bool loop) override {
      return playMovie ? playMovie(n, loop) : false;
    }
    void StopMovie() override {
      if (stopMovie)
        stopMovie();
    }
    void SetLanguage(int id) override { language = id; }
    int CurrentLanguage() const override { return language; }
    void LoadStrings() override {
      if (loadStringsFor)
        loadStringsFor(language);
    }
    void SetDisplayMode(int mode) override { displayMode = mode; }
    void LoadLocaleUI() override {}
    // There is no memory card here and nothing to read off one, so the
    // card task is never busy and the state completes on its first frame --
    // which is what the original does too when the card holds no PlayerData.
    void MemoryCardCheck() override {}
    bool MemoryCardBusy() const override { return false; }
    void Leave() override { left = true; }
  } host;

  host.loadStringsFor = [&](int id) {
    // FUN_001F31D8: the string table is rebuilt from Strings.<code> for the
    // language that was just accepted.
    const char *entry = Game::LanguageStrings((Game::Language)id);
    std::vector<uint8_t> sb;
    if (ReadEntry(arc, entry, sb)) {
      strings.Load(sb.data(), sb.size());
      std::fprintf(stderr, "[play] strings: %s (%zu)\n", entry,
                   strings.Count());
    } else {
      std::fprintf(stderr, "[play] strings: %s not in the archive\n", entry);
    }
  };
  // Every movie's audio is a separate track under SHO-port/MUSIC, named after
  // the clip's base -- MENU -> MUSIC/M/MENU.WAV, SCN01 -> MUSIC/S/SCN01.WAV --
  // because on the disc it is a separate MUSIC/*.RWS, not a stream inside the
  // .PSS. LOGO is the one clip whose track is not named after it: LUMBRYRD.
  auto movieTrack = [&](const std::string &base) -> std::string {
    const std::string stem = (base == "LOGO") ? std::string("LUMBRYRD") : base;
    return musicDir + "/" + stem.substr(0, 1) + "/" + stem + ".WAV";
  };
  host.playMovie = [&](const char *name, bool loop) -> bool {
#ifdef CLIMAX_HAVE_FFMPEG
    // --check has no GL context, and the decoder uploads to a texture.
    if (haveVideoSupport && !checkOnly) {
      bgVideo.Close();
      const std::string base = name; // "Back", "Logo"
      std::string upper;
      for (char c : base)
        upper += (char)std::toupper((unsigned char)c);
      const std::string p =
          ResolveMoviePath(moviesDir, upper, host.displayMode != 0,
                           (Game::Language)host.language);
      if (!p.empty() && bgVideo.Open(p)) {
        bgLooping = loop;
        bgName = upper;
        // Start the clip's own track with the picture. A looping background
        // goes to the music voice so it loops by itself; a one-shot goes to
        // the sfx voice and ends with the clip.
        AudioClip track;
        const std::string tp = movieTrack(upper);
        if (::Audio::LoadFile(tp, track) && track.Valid()) {
          auto &relay = ClimaxEngine::Audio::CAudioRelay::GetInstance();
          if (loop)
            relay.PlayMusic(track, 0.65f);
          else
            relay.PlayAudioClip(track);
          std::fprintf(stderr, "[play] track %s (%.1f s)\n", tp.c_str(),
                       track.Seconds());
        } else {
          std::fprintf(stderr, "[play] track %s: not found\n", tp.c_str());
        }
        std::fprintf(stderr, "[play] movie '%s' -> %s (%dx%d)%s\n", name,
                     p.c_str(), bgVideo.Width(), bgVideo.Height(),
                     loop ? " looping" : "");
        return true;
      }
      std::fprintf(stderr, "[play] movie '%s': nothing under %s\n", name,
                   moviesDir.c_str());
    }
#else
    (void)loop;
    std::fprintf(stderr, "[play] movie '%s': no decoder in this build\n", name);
#endif
    // The return is not cosmetic: FUN_00151E00 passes it back as "did this
    // state start", so answering false here would make the game skip the
    // language screen. A still backdrop is a screen, so say yes when one
    // exists and let the drawing code fall back to it.
    bgName = name;
    bgLooping = loop;
    return true;
  };
  host.stopMovie = [&]() {
#ifdef CLIMAX_HAVE_FFMPEG
    bgVideo.Close();
#endif
    bgName.clear();
    bgLooping = false;
  };

  Game::FrontEnd front;
  front.Menu().SetScreens(screens);
  front.Start(&host);

  if (checkOnly) {
    // Walk the table with cross held down every 20th frame, so the record
    // order and the menu wiring are exercised rather than assumed.
    int last = -1;
    for (int i = 0; i < 600 && !host.left; ++i) {
      if (front.StateIndex() != last) {
        last = front.StateIndex();
        std::fprintf(stderr, "[check] state %d %s\n", last,
                     Game::BootKindName(front.Kind()));
      }
      // The Logo record is not skippable, so end it the way the movie
      // player would.
      if (front.Kind() == Game::BootKind::Movie && i % 20 == 19)
        front.NotifyMovieEnded();
      front.Update(i % 20 == 19 ? Game::Pad::Cross : 0u);
    }
    std::fprintf(stderr, "[check] language %s, display %s\n",
                 Game::LanguageOwnName((Game::Language)host.language),
                 host.displayMode ? "widescreen" : "4:3");
    front.Menu().Open("mainmenu");
    std::fprintf(stderr, "[check] screen '%s', active '%s'\n",
                 front.Menu().ScreenId().c_str(),
                 front.Menu().ActiveId().c_str());
    Game::MenuInput down;
    down.down = true;
    for (int i = 0; i < 4; ++i) {
      front.Menu().Update(down);
      std::fprintf(stderr, "[check]   down -> '%s'\n",
                   front.Menu().ActiveId().c_str());
    }
    // The toggles are settings, not buttons: left and right change them and
    // cross must not (ignorecross="true"). Exercise that rather than assume it.
    if (front.Menu().Open("newgame")) {
      auto show = [&](const char *what) {
        std::fprintf(stderr, "[check] %s: subtitles %s, vibration %s\n", what,
                     front.Menu().Toggle("subtitles") ? "on" : "off",
                     front.Menu().Toggle("vibration") ? "on" : "off");
      };
      show("newgame defaults");
      Game::MenuInput right; right.right = true;
      front.Menu().Update(right);
      show("subtitles right");
      Game::MenuInput cross; cross.accept = true;
      front.Menu().Update(cross);
      show("then cross");
      Game::MenuInput left; left.left = true;
      front.Menu().Update(left);
      show("vibration left");
      front.Menu().Open("mainmenu");
    }
    Game::MenuInput ok;
    ok.accept = true;
    const std::string cmd = front.Menu().Update(ok);
    std::fprintf(stderr, "[check] accept -> screen '%s'%s\n",
                 front.Menu().ScreenId().c_str(),
                 cmd.empty() ? "" : (" command '" + cmd + "'").c_str());
    return 0;
  }

  bool inMenu = false; // set once the terminal record has been reached
  int shotFrames = 0;
  bool playingIntro = false;   // SCN01 is up, covering the menu
  // The menu does not cut to the intro, it goes through black: half a second
  // down with the music, then the clip starts and half a second back up.
  float introFade = 0.0f;      // 1 = fully black
  bool introFadingOut = false; // going down; false while coming back up
  bool introPending = false;   // accepted, waiting for black

  // ── the scene queue ─────────────────────────────────────────────────────
  //
  // FUN_001CF718 ends with FUN_00179FB8(world, "IntroRoad", ...), so when the
  // intro clip is over the game is already asking for its first scene. The
  // queue below is the transcription of that; this host is what it drives.
  Game::SceneQueue sceneQueue;
  struct PlaySceneHost : Game::SceneQueueHost {
      RWS::FileSystem::CArchive *arc = nullptr;
      float *fade = nullptr;          // the same black the intro uses
      std::vector<uint8_t> raw;                    // the scene container
      std::vector<NamedBlob> txds;   // its texture dictionaries
      std::string scene;
      float t = 0.0f;

      bool FadeOut(float secs) override {
          t += 1.0f / 60.0f;
          *fade = std::min(1.0f, t / std::max(secs, 0.01f));
          if (*fade < 1.0f) return false;
          t = 0.0f;
          return true;
      }
      bool FadeIn(float secs) override {
          t += 1.0f / 60.0f;
          *fade = 1.0f - std::min(1.0f, t / std::max(secs, 0.01f));
          if (*fade > 0.0f) return false;
          t = 0.0f;
          return true;
      }
      bool ClearWorld() override { raw.clear(); return true; }
      bool ReleaseResources() override { return true; }
      bool FinishTeardown() override { return true; }
      bool LoadTextures(const std::string &name, const std::string &from) override {
          scene = name;
          // FUN_0017B398: the room's own dictionary, and -- when there is a
          // room being left -- the one the two share. Both are plain entries
          // of SH.ARC on this disc.
          txds.clear();
          auto take = [&](const std::string &entry) {
              std::vector<uint8_t> b;
              if (arc && ReadEntry(*arc, entry.c_str(), b)) {
                  std::fprintf(stderr, "[scene] %s: %zu bytes\n", entry.c_str(), b.size());
                  txds.emplace_back(entry, std::move(b));
              }
          };
          if (!from.empty())
              take(from + "-" + name + ".txd");
          take(name + ".txd");
          if (txds.empty())
              std::fprintf(stderr, "[scene] %s: no texture dictionary\n", name.c_str());

          // The scene's own objects are a separate entry under the bare name.
          if (arc && ReadEntry(*arc, name.c_str(), raw))
              std::fprintf(stderr, "[scene] %s: %zu bytes\n", name.c_str(), raw.size());
          else
              std::fprintf(stderr, "[scene] %s: not in the archive\n", name.c_str());
          return true;
      }
      bool ReadHeader() override { return true; }
      bool Prepare() override { return true; }
      std::vector<Game::SceneObject> objects;
      glm::vec3 spawn = glm::vec3(0.0f);
      bool haveSpawn = false;

      bool Instantiate(const std::string &name) override {
          // The same loader the toolkit uses. It is linked into the game now,
          // so the level is decoded here rather than only in the editor.
          if (!raw.empty())
              ::LoadLevelData(name, raw, txds);
          objects = Game::ParseSceneObjects(raw.data(), raw.size());
          haveSpawn = false;
          int cameras = 0, triggers = 0, lights = 0;
          for (const auto &o : objects) {
              if (o.className == "CPlayerSpawner" && o.placed && !haveSpawn) {
                  spawn = o.position;
                  haveSpawn = true;
              }
              if (o.className.find("Camera") != std::string::npos) ++cameras;
              if (o.className.find("Trigger") != std::string::npos) ++triggers;
              if (o.className.find("Light") != std::string::npos) ++lights;
          }
          std::fprintf(stderr,
                       "[scene] %s: %zu objects -- %d cameras, %d triggers, %d lights\n",
                       name.c_str(), objects.size(), cameras, triggers, lights);
          if (haveSpawn)
              std::fprintf(stderr, "[scene] player spawn (%.2f %.2f %.2f)\n",
                           spawn.x, spawn.y, spawn.z);
          else
              std::fprintf(stderr, "[scene] no CPlayerSpawner placed\n");
          return true;
      }
      bool FreeTemporaries() override { return true; }
      bool ResetSystems() override { return true; }
      bool StartLevelAudio() override { return true; }

      // Where the front end ends. Command 0x0A is FUN_0017B780, and it is the
      // last node the load sequence pushes -- the point the original hands the
      // room to the player. The window, the archive and the decoded level stay
      // up; the caller takes over the frame from here.
      bool *handedOver = nullptr;
      bool HandOver() override {
          std::fprintf(stderr, "[scene] %s up, control handed over\n", scene.c_str());
          if (handedOver)
              *handedOver = true;
          return true;
      }
      bool ResetSubsystems() override {
          std::fprintf(stderr, "[scene] subsystems reset\n");
          return true;
      }
  } sceneHost;
  sceneHost.arc = &arc;
  sceneHost.fade = &introFade;
  bool handedOver = false;
  sceneHost.handedOver = &handedOver;
  bool sceneStarted = false;
  // Screen-to-screen inside the menu goes through black too, the same way the
  // menu-to-intro handover does -- just faster, because it is a step sideways
  // rather than a handover to another part of the game.
  std::string lastScreenId;
  float screenFade = 0.0f;
  // A name the XML does not have (say "frontend") leaves the boot sequence
  // running, so the language and aspect screens can be shot too.
  if (!shotPath.empty() && front.Menu().Open(shotScreen)) {
    inMenu = true;
    size_t at = 0;
    while (at <= shotKeys.size()) {
      const size_t comma = shotKeys.find(',', at);
      const std::string k = shotKeys.substr(at, comma == std::string::npos
                                                    ? std::string::npos
                                                    : comma - at);
      if (!k.empty()) {
        Game::MenuInput mi;
        if (k == "up") mi.up = true;
        else if (k == "down") mi.down = true;
        else if (k == "left") mi.left = true;
        else if (k == "right") mi.right = true;
        else if (k == "accept") mi.accept = true;
        else if (k == "cancel") mi.cancel = true;
        front.Menu().Update(mi);
        std::fprintf(stderr, "[play] key %s -> active '%s'\n", k.c_str(),
                     front.Menu().ActiveId().c_str());
      }
      if (comma == std::string::npos) break;
      at = comma + 1;
    }
  }
  float menuFadeTimer = 0.0f;
  int lastState = -1;
  float bootFadeTimer = 0.0f;

  // ── loop ────────────────────────────────────────────────────────────────
  bool run = true;
  uint64_t last = SDL_GetPerformanceCounter();
  while (run) {
    // Edges, for the XML menu -- that machine is written against presses.
    Game::MenuInput in;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        run = false;
      if (e.type != SDL_KEYDOWN)
        continue;
      switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        in.cancel = true;
        break;
      case SDLK_UP:
        in.up = true;
        break;
      case SDLK_DOWN:
        in.down = true;
        break;
      case SDLK_LEFT:
        in.left = true;
        break;
      case SDLK_RIGHT:
        in.right = true;
        break;
      case SDLK_RETURN:
      case SDLK_SPACE:
        in.accept = true;
        break;
      default:
        break;
      }
    }

    // Levels, for the front end. FUN_001515D0 asks the pad for its current
    // mask and takes the rising edge itself, so handing it edges would run
    // the edge detector twice and lose presses.
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    uint32_t buttons = 0;
    if (keys[SDL_SCANCODE_UP])
      buttons |= Game::Pad::Up;
    if (keys[SDL_SCANCODE_DOWN])
      buttons |= Game::Pad::Down;
    if (keys[SDL_SCANCODE_LEFT])
      buttons |= Game::Pad::Left;
    if (keys[SDL_SCANCODE_RIGHT])
      buttons |= Game::Pad::Right;
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE])
      buttons |= Game::Pad::Cross;
    if (keys[SDL_SCANCODE_ESCAPE])
      buttons |= Game::Pad::Circle;
    if (keys[SDL_SCANCODE_TAB])
      buttons |= Game::Pad::Start;

    const uint64_t now = SDL_GetPerformanceCounter();
    float dt = (float)((now - last) / (double)SDL_GetPerformanceFrequency());
    if (dt > 0.05f)
      dt = 0.05f; // Clamp to avoid video skipping ahead of audio on lag spikes
    last = now;

    int w, h;
    SDL_GL_GetDrawableSize(win, &w, &h);
    // Author-space to window-space. Which layout the *art* uses follows the
    // window; which movie is loaded follows the display mode the player
    // picked, the same global FUN_00180798 reads for its N/W suffix.
    wide = (float)w / (float)h > 1.5f;

#ifdef CLIMAX_HAVE_FFMPEG
    // Advance the front end's clip *before* the state machine, so a clip
    // that just finished ends its record this same frame rather than one
    // frame late. A looping background restarts itself; a one-shot sends
    // the "Ended" message, which is exactly what FUN_00151810 acts on.
    if (haveVideoSupport && !inMenu && bgVideo.IsOpen()) {
      bgVideo.Update(dt);
      if (bgVideo.Finished()) {
        if (bgLooping)
          bgVideo.Restart();
        else
          front.NotifyMovieEnded();
      }
    }
#endif

    if (!inMenu) {
      front.Update(buttons);
      if (front.StateIndex() != lastState) {
        lastState = front.StateIndex();
        bootFadeTimer = 1.0f;
        std::fprintf(stderr, "[play] state %d %s\n", lastState,
                     Game::BootKindName(front.Kind()));
      }
      if (host.left) {
        // The terminal record: the front end object is gone and the
        // main menu is the next game state.
        inMenu = true;
        menuFadeTimer = 1.0f; // Start fade from black for 1 second
        front.Menu().Open("mainmenu");
        if (menuClip.Valid())
          ClimaxEngine::Audio::CAudioRelay::GetInstance().PlayMusic(
              menuClip, 0.0f); // start at 0 volume
        std::fprintf(stderr, "[play] front end done -- language %s, %s\n",
                     Game::LanguageOwnName((Game::Language)host.language),
                     host.displayMode ? "widescreen" : "4:3");
      }
    } else {
      const std::string prevScreen = front.Menu().ScreenId();
      const std::string cmd = front.Menu().Update(in);
      if (front.Menu().ScreenId() != lastScreenId) {
        lastScreenId = front.Menu().ScreenId();
        screenFade = 1.0f;   // the new screen comes up out of black
      }
      if (!cmd.empty())
        std::fprintf(stderr, "[play] command: %s\n", cmd.c_str());

      // Accepting on the new-game options screen starts the game, and the
      // first thing the game does is the intro. `newgame.xml` has no
      // `onaccept` on the vibration row, so this transition is not in the
      // screen data -- it is code, and this is the port's stand-in for it
      // until that code is read.
      //
      // The clip is SCN01, and its variants on the disc are exactly what
      // FUN_001F3048 resolves: SCN01W / SCN01N for the aspect, plus one
      // language letter (SCN01WF, WG, WI, WS), English falling through to the
      // bare name. Subtitles are not part of the file name.
      if (in.accept && cmd.empty() && !playingIntro &&
          (prevScreen == "newgame" ||
           prevScreen == "new_game_menu_screen")) {
        introPending = true;
        introFadingOut = true;
        introFade = 0.0f;
      }

      if (in.up || in.down || in.left || in.right) {
        if (clickClip.Valid())
          ClimaxEngine::Audio::CAudioRelay::GetInstance().PlayAudioClip(
              clickClip);
      } else if (in.accept || in.cancel) {
        if (selectClip.Valid())
          ClimaxEngine::Audio::CAudioRelay::GetInstance().PlayAudioClip(
              selectClip);
      }
    }

    // The fade, and the handover in the middle of it.
    if (introFadingOut) {
      introFade += dt * 2.0f;                 // half a second to black
      if (introFade >= 1.0f) {
        introFade = 1.0f;
        introFadingOut = false;
        if (introPending) {
          introPending = false;
          if (host.playMovie("SCN01", false)) {
            playingIntro = true;
            std::fprintf(stderr, "[play] intro: SCN01 (%s, %s, subtitles %s)\n",
                         host.displayMode ? "widescreen" : "4:3",
                         Game::LanguageOwnName((Game::Language)host.language),
                         front.Menu().Toggle("subtitles") ? "on" : "off");
          }
        }
      }
    } else if (introFade > 0.0f) {
      introFade -= dt * 2.0f;                 // and half a second back up
      if (introFade < 0.0f)
        introFade = 0.0f;
    }

    // Cutscenes can be skipped, and the engine says so itself: `IGCStart`,
    // `IGCActive`, `IGCSkipRequest`, `IGCSkip` and `IGCEnd` are five of the
    // 206 registered messages (docs/executables/MESSAGES.md). A request is a
    // separate message from the skip, so something arbitrates -- but the
    // player-facing half is plain: a button ends the clip.
    //
    // Which button comes from the boot table's own movie record,
    // `FUN_00152218`, which tests the remapped mask against 0x120 -- cross or
    // start. That record also carries a skippable flag, and the Logo record's
    // is 0, which is why the logo cannot be skipped and this can.
    bool skipIntro = false;
    if (playingIntro && (in.accept || in.cancel))
      skipIntro = true;

#ifdef CLIMAX_HAVE_FFMPEG
    // The intro runs on the same player the boot movies use, and ends the same
    // way they do -- when the clip runs out, not on a timer.
    if (haveVideoSupport && playingIntro && bgVideo.IsOpen())
      bgVideo.Update(dt);
    if (playingIntro && (skipIntro || (bgVideo.IsOpen() && bgVideo.Finished()))) {
#else
    if (playingIntro && skipIntro) {
#endif
      host.stopMovie();
      playingIntro = false;
      std::fprintf(stderr, "[play] intro %s\n",
                   skipIntro ? "skipped" : "finished");
      // FUN_001CF718: new game -> the first scene.
      if (!sceneStarted) {
        sceneStarted = true;
        sceneQueue.LoadScene("IntroRoad");
        std::fprintf(stderr, "[scene] queue: IntroRoad\n");
      }
    }
#ifdef CLIMAX_HAVE_FFMPEG
    if (haveVideoSupport && inMenu && !sceneStarted) {
      const UI::Element *scr = front.Menu().Screen();
      std::string requestedMovie = scr ? scr->Attr("bgmovie") : "";

      static std::string currentMenuMovie;
      if (requestedMovie.empty() && currentMenuMovie.empty()) {
        requestedMovie = "MENU"; // Initial fallback
      }

      if (!requestedMovie.empty() && currentMenuMovie != requestedMovie) {
        menuVideo.Close();
        currentMenuMovie = requestedMovie;
        const std::string p =
            ResolveMoviePath(moviesDir, requestedMovie, host.displayMode != 0,
                             (Game::Language)host.language);
        if (!p.empty() && menuVideo.Open(p))
          std::fprintf(stderr, "[play] menu video: %s (%dx%d)\n", p.c_str(),
                       menuVideo.Width(), menuVideo.Height());
      }
      if (menuVideo.IsOpen()) {
        menuVideo.Update(dt);
        if (menuVideo.Finished())
          menuVideo.Restart();
      }
    }
#endif

    // The menu loop is part of the menu; once the queue is running it has to
    // stop with everything else, or the level comes up over menu music.
#ifdef CLIMAX_HAVE_FFMPEG
    if (sceneStarted && menuVideo.IsOpen())
      menuVideo.Close();
#endif

    static float currentMusicVolume = 0.0f;
    if (inMenu && menuFadeTimer > 0.0f) {
      menuFadeTimer -= dt;
      if (menuFadeTimer < 0.0f)
        menuFadeTimer = 0.0f;
    }
    if (inMenu) {
        float target = (!sceneStarted && front.Menu().ScreenId() == "mainmenu") ? 1.0f : 0.0f;
        if (currentMusicVolume < target) {
            currentMusicVolume += dt * 0.5f; // Fade in over 2s
            if (currentMusicVolume > target) currentMusicVolume = target;
        } else if (currentMusicVolume > target) {
            currentMusicVolume -= dt * 1.5f; // Fade out slightly faster
            if (currentMusicVolume < target) currentMusicVolume = target;
        }
        if (currentMusicVolume <= 0.0f) {
            ClimaxEngine::Audio::CAudioRelay::GetInstance().PauseMusic(true);
        } else {
            ClimaxEngine::Audio::CAudioRelay::GetInstance().PauseMusic(false);
            ClimaxEngine::Audio::CAudioRelay::GetInstance().SetMusicVolume(
                0.65f * currentMusicVolume);
        }
    }

    // FUN_00179D60: one drain per frame.
    if (sceneQueue.Busy())
      sceneQueue.Update(sceneHost, dt);

    // The queue reached its handover node. Stop drawing the front end and let
    // the caller run the level on the window this loop has been using.
    if (handedOver) {
      out.startGame = true;
      out.firstScene = sceneHost.scene;
      out.archive = arcPath;
      out.widescreen = host.displayMode != 0;
      out.language = host.language;
      run = false;
    }

    glViewport(0, 0, w, h);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    painter.Begin(w, h);

    // Only the PAL sets are loaded, so the mode suffix picks between its
    // two aspects. An NTSC build would load UiDataN* and use "nw"/"n4".
    const char *texMode = wide ? "pw" : "p4";

    // Where the UI lands, out of `FUN_001B38C0` -- the one function every
    // 2D widget goes through. Its two magic constants say what the space
    // is:
    //
    //     widescreen:  x *= 0.5626374          = 512/910
    //     4:3:         x  = (x - 114.0) * 0.75073314 = 512/682,
    //                  and 114 = (910 - 682) / 2
    //
    // So the screens are authored **910 wide** -- the same 910x512 the
    // background movies are -- and the PS2 squeezes that into its 512-wide
    // framebuffer; 4:3 crops the middle 682 of the 910 first. **Only x and
    // width are scaled.** y and height are passed through untouched, which
    // means they are already in framebuffer rows.
    //
    // 1280x720 was an assumption and it was wrong on both axes.
    const float kUiSqueeze = wide ? 512.0f / 910.0f : 512.0f / 682.0f;
    const float kUiCrop = wide ? 0.0f : 114.0f;
    float uiX, uiY, uiW, uiH;
    {
      const float ar = wide ? 16.0f / 9.0f : 4.0f / 3.0f;
      const float winAR = (float)w / (float)h;
      if (winAR >= ar) {
        uiH = (float)h;
        uiW = uiH * ar;
        uiY = 0.0f;
        uiX = ((float)w - uiW) * 0.5f;
      } else {
        uiW = (float)w;
        uiH = uiW / ar;
        uiX = 0.0f;
        uiY = ((float)h - uiH) * 0.5f;
      }
    }
    const float uiPX = uiW / 512.0f, uiPY = uiH / 512.0f;

    // Only the main menu is an XML screen. Nothing in the boot table draws
    // one: `bootmenu.xml` exists in the archive but the front-end object
    // never opens it, and an earlier version of this file put it up as a
    // "Loading" stage that the state table does not have.
    const UI::Element *scr = nullptr;
    std::string activeId;
    // Once the scene queue is running the front end is over, whatever the menu
    // state machine still thinks. Drawing the screen here put the new-game
    // options back on top of the load -- the clip ended, the queue started, and
    // for the seconds it takes to read four megabytes the player was looking at
    // "subtitles / vibration" again instead of at black.
    if (inMenu && !sceneStarted) {
      scr = front.Menu().Screen();
      activeId = front.Menu().ActiveId();
    }
    (void)bootScreen;

    if (scr) {
      {
        // `bgmovie="Menu"` is what the screen actually asks for; the
        // static `bgtexture` (when a screen has one at all -- mainmenu
        // does not) is the fallback for when no decoder is built in.
        bool drewBg = false;
#ifdef CLIMAX_HAVE_FFMPEG
        if (haveVideoSupport && menuVideo.IsOpen()) {
          // Into the same rect the widgets map onto: the movie *is*
          // the 910x512 space they are authored over, so they only
          // line up if both go through the same fit.
          painter.Quad(uiX, uiY, uiW, uiH, menuVideo.Texture(), 1, 1, 1, 1);
          drewBg = true;
        }
#endif
        if (!drewBg) {
          if (const std::string bg = scr->Attr("bgtexture"); !bg.empty()) {
            const std::string n = ResolveTextureName(bg, texMode);
            if (auto it = textures.find(n); it != textures.end()) {
              painter.Quad(0, 0, (float)w, (float)h, it->second, 1, 1, 1, 1);
              drewBg = true;
            }
          }
        }
        if (!drewBg)
          painter.Quad(0, 0, (float)w, (float)h, 0, 0.02f, 0.02f, 0.03f, 1.0f);

        for (const UI::Element &b : scr->children) {
          // `xpos4x3`/`ypos4x3` are an override, not a second unit:
          // FUN_001B5410 seeds them from `xpos`/`ypos` and only
          // replaces them when the attribute is present.
          const float ax = wide ? b.Float("xpos") : b.Float("xpos4x3");
          const float ay = wide ? b.Float("ypos") : b.Float("ypos4x3");
          const float bx = uiX + (ax - kUiCrop) * kUiSqueeze * uiPX;
          
          // Hack: the gameoptions menu is authored very bottom-heavy in the XML.
          // The user specifically requested it to be vertically centered on screen.
          float globalYOffset = 0.0f;
          if (scr && scr->Attr("id") == "gameoptions") {
              globalYOffset = -32.5f * uiPY;
          }
          const float by = uiY + ay * uiPY + globalYOffset;
          
          const float bw = b.Float("width") * kUiSqueeze * uiPX;
          const float bh = b.Float("height") * uiPY;

          const bool active = !activeId.empty() && b.Attr("id") == activeId;
          const float k = active ? 1.0f : 0.5f;

          // Art, if the element has any. Drawing a filled rectangle
          // when it has none was the bug behind the white bars: a
          // TEXTBOX is text, not a box.
          const std::string tex =
              ResolveTextureName(b.Attr("bgtexture"), texMode);
          GLuint id = 0;
          if (!tex.empty())
            if (auto it = textures.find(tex); it != textures.end())
              id = it->second;
          if (id) {
            float u0 = b.Float("textureu", 0.0f),
                  v0 = b.Float("texturev", 0.0f);
            float u1 = b.Float("texturew", 1.0f),
                  v1 = b.Float("textureh", 1.0f);
            // `rotation` is radians. The only value the screens use
            // is -pi -- the right-hand arrow is the left-hand one
            // turned round -- and a half turn is the UVs swapped on
            // both axes, no matrix needed.
            if (std::fabs(b.Float("rotation", 0.0f)) > 3.0f) {
              // Let's just flip the X coordinates of the quad itself, instead
              // of UVs? Actually, let's just see if UVs were the problem. I'll
              // leave this empty to test if rotation caused the noise.
              std::swap(u0, u1);
              // std::swap(v0, v1); // Try just swapping U, maybe swapping V
              // breaks it?
            }
            painter.Quad(bx, by, bw, bh, id, k, k, k, 1.0f, u0, v0, u1, v1);
          }

          // Text, if it names a string. Never the element id: that is
          // a name for the designer, not a label for the player.
          //
          // A TEXTBOX named by some TOGGLEBUTTON's `toggletextbox` does not
          // show its own `string` at all -- it shows that toggle's `ontext` or
          // `offtext`. Its own `string` is only the value the screen ships in,
          // which is why `ON` sat there forever.
          std::string sid = b.Attr("string");
          if (const std::string *tt =
                  front.Menu().ToggleTypeForTextbox(b.Attr("id"))) {
            const bool on = front.Menu().Toggle(*tt);
            for (const UI::Element &t : scr->children)
              if (t.tag == "TOGGLEBUTTON" && t.Attr("toggletype") == *tt) {
                const std::string s2 = t.Attr(on ? "ontext" : "offtext");
                if (!s2.empty())
                  sid = s2;
                break;
              }
          }
          if (!sid.empty()) {
            const std::string label = strings.Text(sid);
            if (!label.empty()) {
              // One rule for text, the same one the front end's own screens
              // use: the element is the text's box, and the line is centred in
              // it. FUN_001B38C0 builds every box as y .. y+height, so a
              // TEXTBOX with height="1.00" is centred on `ypos` -- which is the
              // same `ypos` its two arrows carry, so they end up on one row.
              //
              // What was here instead: textsize scaled by 1.5, height under 20
              // replaced by a literal 30, and a 0.45 nudge. Three numbers that
              // are in no file and in no function, and between them they put
              // `NO` and `ON` about fifteen rows below their arrows.
              const float tscale =
                  b.Float("textsize", 16.0f) * textScale * uiPY / kFontNativeH;
              float tx = bx + b.Float("textoffx") * kUiSqueeze * uiPX;
              // `ty` is a baseline: Glyph::yOffset is "negative is up from
              // the baseline", so the glyphs hang above it. Putting the cap
              // box's centre on the element box's centre is half a cap height
              // below that centre -- no nudge factor involved.
              // Measured off a --shot: centring the line inside the element's
              // own box put `NO` seven and a half rows above the centre of the
              // arrows that share its `ypos` -- exactly half a cap height,
              // because a TEXTBOX declares height="1.00" and has no box to
              // centre in.
              //
              // `ypos` is the top of the line, not its centre. `ty` is a
              // baseline (Glyph::yOffset is "negative is up from the
              // baseline"), so the baseline is one cap height below `ypos` and
              // the capitals start exactly on it.
              const float capPx = b.Float("textsize", 16.0f) * uiPY;
              const float ty = by + b.Float("textoffy") * uiPY + capPx;
              // `justification` is an attribute too, and ignoring
              // it is what pushed the newgame labels left into
              // the logo.
              const std::string just = b.Attr("justification");
              if (just == "centre" || just == "center")
                tx += (bw - measure(label, tscale)) * 0.5f;
              else if (just == "right")
                tx += bw - measure(label, tscale);
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
      const float sc = uiPY * 1.6f; // only the "no clip" fallback text
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
        if (!id)
          return false;
        if (texAR <= 0.0f)
          texAR = displayAR;
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
          rh = (float)h;
          rw = rh * displayAR;
          ry = 0.0f;
          rx = ((float)w - rw) * 0.5f;
        } else {
          rw = (float)w;
          rh = rw / displayAR;
          rx = 0.0f;
          ry = ((float)h - rh) * 0.5f;
        }
      };

      // Maps the coordinates the game's own drawing code uses onto that
      // rect. The front end is authored against the 910x512 authored space, squeezed into the PS2's 512-wide
      // framebuffer -- 512 is not a guess, the flag row in FUN_00151918
      // runs from x=32 to x=480 and is visibly centred.
      auto uiQuad = [&](float ux, float uy, float uw, float uh, GLuint id,
                        float r, float g, float b, float a) {
        if (!id)
          return;
        float rx, ry, rw, rh;
        uiRect(rx, ry, rw, rh);
        // The front end is not a widget: FUN_00151918 calls FUN_001B38C0 with
        // its last argument 0, so neither the 910->512 squeeze nor the /512
        // normalisation in FUN_001B3EC0 is applied to it. Its coordinates are
        // already framebuffer pixels, and the PAL framebuffer is 512 x 448.
        // Only the XML widgets are authored 910 x 512.
        const float kx = rw / 512.0f, ky = rh / frontEndRows;
        painter.Quad(rx + ux * kx, ry + uy * ky, uw * kx, uh * ky, id, r, g, b,
                     a);
      };

      // Centres a string inside a rect given in the game's 910x512 authored space,
      // at one of the font sizes the front end asks for.
      auto uiText = [&](float ux, float uy, float uw, float uh, float size,
                        const std::string &t, float r, float g, float b,
                        float a) {
        if (t.empty())
          return;
        float rx, ry, rw, rh;
        uiRect(rx, ry, rw, rh);
        const float px = rw / 512.0f, py = rh / frontEndRows;
        const float scale = size * py / kFontNativeH;
        const float wpx = drawText(0, 0, scale, t, 0, 0, 0, 0, true);
        drawText(rx + (ux + uw * 0.5f) * px - wpx * 0.5f,
                 ry + (uy + uh * 0.5f) * py + size * py * 0.5f, scale, t, r, g,
                 b, a, false);
      };

      // Same fit as `fullscreen`, but for a decoded video frame -- its
      // own GLuint, not one looked up in `textures` by name -- letterboxed
      // to the clip's real decoded size rather than the display aspect,
      // since a video's aspect is a property of the frame, already
      // correct, and not of the display mode the way a stretched still is.
      auto fullscreenVideo = [&](GLuint id, int texW, int texH) {
        if (!id || texW <= 0 || texH <= 0)
          return false;
        const float texAR = (float)texW / (float)texH;
        const float winAR = (float)w / (float)h;
        float dw, dh, dx, dy;
        if (winAR >= texAR) {
          dh = (float)h;
          dw = dh * texAR;
          dy = 0.0f;
          dx = ((float)w - dw) * 0.5f;
        } else {
          dw = (float)w;
          dh = dw / texAR;
          dx = 0.0f;
          dy = ((float)h - dh) * 0.5f;
        }
        painter.Quad(dx, dy, dw, dh, id, 1, 1, 1, 1);
        return true;
      };

      // The background under the language, memory-card and aspect states
      // is one looping movie -- "Back" -- put up when the first of them
      // is entered and taken down when the aspect state exits. It is not
      // per-screen art, which is why nothing here reopens it.
      auto backdrop = [&](const char *still) {
        bool drew = false;
#ifdef CLIMAX_HAVE_FFMPEG
        if (haveVideoSupport && bgVideo.IsOpen())
          drew = fullscreenVideo(bgVideo.Texture(), bgVideo.Width(),
                                 bgVideo.Height());
#endif
        if (!drew && still)
          drew = fullscreen(std::string(still) + texMode);
        return drew;
      };

      const Game::MenuTint on = Game::SelectedTint();
      const Game::MenuTint off = Game::UnselectedTint();
      const Game::TextRect head = Game::HeadingRect();

      switch (front.Kind()) {
      case Game::BootKind::Language: {
        backdrop("sho_lang_bd_");

        // FUN_00151918 draws two things and nothing else: the heading,
        // and the row.
        //
        // The heading is *not* a string-table id. It is descriptor
        // word 0 of the language under the cursor -- the language's own
        // name, written in that language -- which is why it changes as
        // the cursor moves, and why FUN_00151E98 re-points the current
        // language on the move rather than on accept.
        uiText(head.x, head.y, head.w, head.h, head.size,
               Game::LanguageOwnName(front.SelectedLanguage()), on.r, on.g,
               on.b, on.a);

        // Five 80x60 quads on one line, first at (32,192), 92 apart.
        // Selected is drawn white and the rest at 0xC8505050 -- the
        // original tints the flag itself rather than putting a frame
        // around it, so there is no sho_flg_sel here.
        const Game::FlagRowLayout row = Game::LanguageRow();
        const int n = Game::LanguageCount();
        for (int i = 0; i < n; ++i) {
          const auto lang = (Game::Language)i;
          const bool sel = i == front.Cursor();
          GLuint id = tex(Game::LanguageFlag(lang));
          // Some sets ship a pre-lit "_h" variant; prefer it when the
          // entry is selected, and fall back to tinting.
          if (sel)
            if (const GLuint hi =
                    tex(std::string(Game::LanguageFlag(lang)) + "_h"))
              id = hi;
          const Game::MenuTint t = sel ? on : off;
          uiQuad(row.x + i * row.step, row.y, row.w, row.h, id, t.r, t.g, t.b,
                 t.a);
        }
        break;
      }

      case Game::BootKind::MemoryCard:
        // FUN_001D3AD0 queues the card check and draws nothing of its
        // own; the "Back" movie is still up behind it. With no card to
        // read this record completes on its first frame, so this branch
        // is normally never seen.
        backdrop("sho_lang_bd_");
        break;

      case Game::BootKind::Aspect: {
        // Two entries, one under the other, and they are *text*:
        // FUN_00151FB8 passes `display_4x3` and `display_ws` through the
        // string table rather than looking up a texture. Same two
        // colours as the flag row.
        backdrop("sho_aspect_");
        // Every word here is the game's. FUN_00151FB8 passes the string
        // id at 0x6929E0 -- `display_title` -- for the heading, and the
        // two entries come from the 8-byte records at 0x00338A98.
        // Coordinates out of .data: the heading rect is 0x00338A48 =
        // (0,128,512,32) at size 0x00338A3C = 16, the list rect is
        // 0x00338AA8 = (0,192,512,32) at size 0x00338A40 = 14, and the
        // loop advances y by that size twice per row -- 28.
        uiText(head.x, head.y, head.w, head.h, head.size,
               strings.Text("display_title"), on.r, on.g, on.b, on.a);
        const Game::TextRect rowRect = Game::AspectRowRect();
        for (int i = 0; i < Game::AspectCount(); ++i) {
          const Game::MenuTint t = i == front.Cursor() ? on : off;
          uiText(rowRect.x, rowRect.y + i * Game::AspectRowStep(), rowRect.w,
                 rowRect.h, rowRect.size, strings.Text(Game::AspectStringId(i)),
                 t.r, t.g, t.b, t.a);
        }
        break;
      }

      case Game::BootKind::Movie: {
        // The "Logo" record. LOGOW/LOGON is one clip covering both the
        // publisher/developer idents and the content notice -- there is
        // no separate warning asset in the archive, so there is no
        // separate drawing path for it either.
        bool drew = false;
#ifdef CLIMAX_HAVE_FFMPEG
        if (haveVideoSupport && bgVideo.IsOpen())
          drew = fullscreenVideo(bgVideo.Texture(), bgVideo.Width(),
                                 bgVideo.Height());
#endif
        if (!drew) {
          // No decoder in this build, or the clip failed to open: say
          // so rather than drawing an invented logo, which read as the
          // toolkit's own branding rather than the game's.
          centre(cx, h * 0.48f, sc, "Silent Hill Origins", 0.55f, 0.55f, 0.6f,
                 1.0f);
          centre(cx, h * 0.58f, sc * 0.6f, "(LOGOW.mp4 not found)", 0.35f,
                 0.35f, 0.4f, 1.0f);
        }
        break;
      }
      case Game::BootKind::Leave:
        break;
      }
    }

#ifdef CLIMAX_HAVE_FFMPEG
    if (playingIntro && bgVideo.IsOpen() && bgVideo.Width() > 0) {
      const float texAR = (float)bgVideo.Width() / (float)bgVideo.Height();
      const float winAR = (float)w / (float)h;
      float dw, dh, dx, dy;
      if (winAR >= texAR) { dh = (float)h; dw = dh * texAR; dy = 0.0f; dx = ((float)w - dw) * 0.5f; }
      else                { dw = (float)w; dh = dw / texAR; dx = 0.0f; dy = ((float)h - dh) * 0.5f; }
      painter.Quad(0, 0, (float)w, (float)h, 0, 0, 0, 0, 1.0f);
      painter.Quad(dx, dy, dw, dh, bgVideo.Texture(), 1, 1, 1, 1);
    }
#endif
    if (screenFade > 0.0f) {
      screenFade -= dt * 3.0f;   // a third of a second
      if (screenFade < 0.0f)
        screenFade = 0.0f;
      if (!playingIntro)
        painter.Quad(0, 0, (float)w, (float)h, 0, 0, 0, 0, screenFade);
    }
    if (introFade > 0.0f)
      painter.Quad(0, 0, (float)w, (float)h, 0, 0, 0, 0, introFade);
    if (inMenu && menuFadeTimer > 0.0f) {
      // Draw a black overlay fading out
      painter.Quad(0, 0, w, h, 0, 0, 0, 0, menuFadeTimer);
    }
    
    if (!inMenu && bootFadeTimer > 0.0f) {
      bootFadeTimer -= dt * 2.0f;
      if (bootFadeTimer < 0.0f) bootFadeTimer = 0.0f;
      painter.Quad(0, 0, w, h, 0, 0, 0, 0, bootFadeTimer);
    }

    SDL_GL_SwapWindow(win);

    if (!shotPath.empty() && ++shotFrames >= 120) {
      std::vector<uint8_t> px((size_t)w * h * 4);
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
      // glReadPixels hands back bottom-up; PNG wants top-down.
      std::vector<uint8_t> flip((size_t)w * h * 4);
      for (int y = 0; y < h; ++y)
        std::memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4],
                    (size_t)w * 4);
      SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
          flip.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
      if (surf && IMG_SavePNG(surf, shotPath.c_str()) == 0)
        std::fprintf(stderr, "[play] shot: %s (%dx%d, screen '%s')\n",
                     shotPath.c_str(), w, h, shotScreen.c_str());
      else
        std::fprintf(stderr, "[play] shot failed: %s\n", IMG_GetError());
      if (surf) SDL_FreeSurface(surf);
      run = false;
    }
  }

  // Handing over: the window, the context and SDL itself stay up, and the
  // caller keeps drawing into them. Tearing them down here and building a
  // second window is what makes a port feel like two programs stitched
  // together, which is exactly what this is not any more.
  if (out.startGame) {
    out.window = win;
    out.context = ctx;
    return 0;
  }

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
  return 0;
}

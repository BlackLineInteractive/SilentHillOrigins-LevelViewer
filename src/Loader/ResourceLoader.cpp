#include "GeometryDecoder.h"
#include <array>
#include <cstring>
#include <vector>
#include "ClimaxEngine/SG/SceneObject.h"
#include "ClimaxEngine/Loader/ResourceLoader.h"
#include "ClimaxEngine/Platform/PS2/PS2Texture.h"
#include "ClimaxEngine/Core/Common.h"


#include <iostream>

namespace ClimaxEngine {
namespace ResourceLoader {

bool RwChunk::Read(const uint8_t* d, size_t dataSize, size_t offset) {
    if (offset + 12 > dataSize) return false;
    type = (uint32_t)d[offset] | ((uint32_t)d[offset + 1] << 8) | ((uint32_t)d[offset + 2] << 16) | ((uint32_t)d[offset + 3] << 24);
    size = (uint32_t)d[offset + 4] | ((uint32_t)d[offset + 5] << 8) | ((uint32_t)d[offset + 6] << 16) | ((uint32_t)d[offset + 7] << 24);
    version = (uint32_t)d[offset + 8] | ((uint32_t)d[offset + 9] << 8) | ((uint32_t)d[offset + 10] << 16) | ((uint32_t)d[offset + 11] << 24);
    
    if (offset + 12 + size > dataSize) return false;
    payload = d + offset + 12;
    return true;
}

bool RwChunk::Read(RWS::RwStream* stream) {
    uint32_t header[3];
    if (stream->Read(header, 12) != 12) {
        return false;
    }
    type = header[0];
    size = header[1];
    version = header[2];
    
    // Payload pointer cannot be used directly with RwStream if it's not memory stream, 
    // but we can assume memory stream for now and use GetCurrentPointer.
    if (auto* memStream = dynamic_cast<RWS::RwMemoryStream*>(stream)) {
        payload = memStream->GetCurrentPointer();
    }
    return true;
}

CResourceHandler::CResourceHandler() {
    // Automatically register standard loaders
    RegisterLoader(std::make_shared<CWorldStreamLoader>());
    RegisterLoader(std::make_shared<CClumpStreamLoader>());
    RegisterLoader(std::make_shared<CTexDictionaryStreamLoader>());
    RegisterLoader(std::make_shared<CAudioCuesStreamLoader>());
    auto shoLoader = std::make_shared<CSHOSceneStreamLoader>();
    m_loaders[0x071C] = shoLoader;
    m_loaders[0x0716] = shoLoader;
}

CResourceHandler& CResourceHandler::GetInstance() {
    static CResourceHandler instance;
    return instance;
}

void CResourceHandler::RegisterLoader(std::shared_ptr<IStreamLoader> loader) {
    m_loaders[loader->GetTypeID()] = loader;
}

std::shared_ptr<IStreamLoader> CResourceHandler::GetLoader(uint32_t typeId) const {
    auto it = m_loaders.find(typeId);
    if (it != m_loaders.end()) {
        return it->second;
    }
    return nullptr;
}

void CResourceHandler::ProcessStream(const char* streamName, RWS::RwStream* stream, uint32_t streamSize) {
    size_t startPos = stream->Tell();
    std::cout << "[ProcessStream] Starting stream '" << streamName << "' at pos " << startPos << " size " << streamSize << "\n";
    while (stream->Tell() + 12 <= startPos + streamSize && !stream->IsEOF()) {
        RwChunk chunk;
        size_t chunkStart = stream->Tell();
        if (!chunk.Read(stream)) {
            std::cout << "[ProcessStream] Failed to read chunk at " << chunkStart << "\n";
            break;
        }
        std::cout << "[ProcessStream] Found chunk type 0x" << std::hex << chunk.type << std::dec << " size " << chunk.size << " at " << chunkStart << "\n";
        
        auto loader = GetLoader(chunk.type);
        if (loader) {
            // The engine's own loop (RWS::CStreamHandler::ProcessStream at
            // 0x002667B8 in Ghost Rider) never repositions the stream after a
            // handler: every handler is required to consume exactly its chunk,
            // and anything else desyncs the walk.
            //
            // We keep the corrective skip, because we run this recursively over
            // sub-ranges rather than over real sub-streams, but a handler that
            // does not honour the contract is now loud instead of silent. Every
            // container bug found so far was exactly this kind of divergence.
            const size_t beforeRead = stream->Tell();
            loader->Read(streamName, stream, chunk.size);
            const size_t bytesRead = stream->Tell() - beforeRead;
            if (bytesRead != chunk.size) {
                std::cout << "[stream] handler for chunk 0x" << std::hex << chunk.type
                          << std::dec << " read " << bytesRead << " of " << chunk.size
                          << " bytes at " << chunkStart << " in '" << streamName
                          << "'\n";
            }
            if (bytesRead < chunk.size) {
                stream->Skip(chunk.size - bytesRead);
            } else if (bytesRead > chunk.size) {
                // An over-read cannot be undone by *skipping*, but it can be
                // undone by seeking: this is a range over a buffer, not a real
                // sequential stream, and the chunk header already said where
                // the next one starts.
                //
                // Abandoning the container here was costing real data. A 0x716
                // shell whose payload is a JPEG -- HO_1_ExamRoom has four --
                // reads as a chunk of type 0xE0FFD8FF (the JFIF marker) with a
                // nonsense size, so the shell handler walks off the end and the
                // whole walk stopped at that point. In HO_1_ExamRoom that is
                // offset 832350 of 1597782: the four JPEGs, both rwID_RWS audio
                // streams and the rwID_WORLD at 1047742 were never read at all.
                std::cout << "[stream] handler over-read chunk 0x" << std::hex
                          << chunk.type << std::dec << " at " << chunkStart
                          << " in '" << streamName << "'; resyncing\n";
                stream->Seek(chunkStart + 12 + chunk.size);
            }
        } else {
            // Unhandled chunk, skip it
            // std::cout << "[ResourceLoader] Unhandled chunk type: 0x" << std::hex << chunk.type << std::dec << "\n";
            stream->Skip(chunk.size);
        }
    }
}


// --- Specific Loaders Stub ---

// Every level container ships its room more than once.
//
// Measured over SH.ARC: no container has one rwID_WORLD. 33 have two and 181
// have three, and in 98 of them a pair has a byte-identical 64-byte world
// Struct -- the same bounding box, the same everything the header records --
// while the payloads differ and the second is consistently about 12% larger.
// They are not a platform pair: both carry rwID_NATIVEDATAPLG (0x510), so both
// are PS2 native geometry. They are not different areas: HO_1_ExamRoom's two
// share 53 of their 55 materials, differing only in one wall tile and a
// `GreyAlpha_` variant of one slider.
//
// So it is the same room, built twice, and drawing both means every surface is
// drawn twice. Opaque surfaces z-fight; everything blended composites twice
// and comes out at roughly double strength, which is what a map, a save point
// and a mannequin "glowing" look like.
//
// Which of the two the engine actually instantiates is not established -- that
// needs the scene queue's resource pick read out of SLES_551.47 -- so this does
// the one thing that is safe without knowing: it keeps the first and skips a
// later world whose Struct header is identical to one already taken. A world
// with a different header (the small third one, 3574 bytes here) is untouched.
static std::vector<std::array<uint8_t, 64>> s_worldStructs;

void ResetWorldDedupe() { s_worldStructs.clear(); }

bool CWorldStreamLoader::Read(const char* name, RWS::RwStream* stream, uint32_t length) {
    std::cout << "[ResourceLoader] Found CWorldStreamLoader chunk (size: " << length << ")\n";
    std::vector<uint8_t> data(length);
    if (stream->Read(data.data(), length) == length) {
        // The world's Struct is its first child: 12 bytes of header, then the
        // 64 bytes that describe the world itself.
        if (length >= 76) {
            uint32_t st = 0;
            std::memcpy(&st, data.data(), 4);
            if (st == 0x01) {
                std::array<uint8_t, 64> key{};
                std::memcpy(key.data(), data.data() + 12, 64);
                for (const auto& seen : s_worldStructs) {
                    if (seen == key) {
                        std::cout << "[ResourceLoader] world '" << name
                                  << "' repeats a world already loaded in this "
                                     "container; skipped\n";
                        return true;
                    }
                }
                s_worldStructs.push_back(key);
            }
        }
        auto obj = std::make_shared<::ClimaxEngine::SG::CWorldObject>(name);
        DecodeRenderWareGeometry(name, data.data(), length, true, obj.get());
        ::ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().RegisterObject(obj);
    }
    return true;
}

bool CClumpStreamLoader::Read(const char* name, RWS::RwStream* stream, uint32_t length) {
    std::cout << "[ResourceLoader] Found CClumpStreamLoader chunk (size: " << length << ")\n";
    std::vector<uint8_t> data(length);
    if (stream->Read(data.data(), length) == length) {
        auto obj = std::make_shared<::ClimaxEngine::SG::CClumpObject>(name);
        DecodeRenderWareGeometry(name, data.data(), length, false, obj.get());
        ::ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().RegisterObject(obj);
    }
    return true;
}

bool CSHOSceneStreamLoader::Read(const char* name, RWS::RwStream* stream, uint32_t length) {
    if (length < 20) { stream->Skip(length); return false; }

    size_t chunkStart = stream->Tell() - 12;

    uint32_t raw[2];
    if (stream->Read(raw, 8) != 8) return false;

    auto swap = [](uint32_t v) {
        return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) |
               ((v << 24) & 0xFF000000);
    };

    // The section header is little-endian on PlayStation 2 and big-endian on
    // Wii, so the order has to come from the data rather than from a constant.
    // Measured on the retail PS2 archive: headerSize = 200 and tagLen = 4 read
    // little-endian, which become 3355443200 and 67108864 the other way round.
    // Reading a PS2 container as big-endian therefore trips the tagLen guard
    // and the whole section is skipped -- which is exactly a container that
    // loads with no geometry at all.
    uint32_t headerSize = raw[0], tagLen = raw[1];
    bool bigEndian = false;
    if (tagLen > 1024 || headerSize >= length) {
        const uint32_t sHdr = swap(raw[0]), sTag = swap(raw[1]);
        if (sTag <= 1024 && sHdr < length) {
            headerSize = sHdr;
            tagLen = sTag;
            bigEndian = true;
        }
    }
    auto rd = [&](uint32_t v) { return bigEndian ? swap(v) : v; };

    if (tagLen > 1024 || headerSize >= length) { stream->Skip(length - 8); return false; }

    stream->Skip(tagLen);
    uint8_t guidBuf[16];
    if (stream->Read(guidBuf, 16) != 16) return false;

    uint32_t nameLenRaw;
    if (stream->Read(&nameLenRaw, 4) != 4) return false;
    const uint32_t nameLen = rd(nameLenRaw);

    std::string secName;
    if (nameLen > 0 && nameLen < 1024) {
        std::vector<char> nameBuf(nameLen);
        stream->Read(nameBuf.data(), nameLen);
        secName.assign(nameBuf.data(), strnlen(nameBuf.data(), nameLen));
    } else if (nameLen >= 1024) {
        stream->Skip(length - 8 - tagLen - 20);
        return false;
    }

    // Skip forward to the payload: the section header states its own length,
    // so the build-path strings never have to be walked.
    const uint32_t bytesRead = 8 + tagLen + 20 + nameLen;
    const uint32_t targetOffset = 4 + headerSize;
    if (targetOffset > bytesRead) stream->Skip(targetOffset - bytesRead);
    else if (targetOffset < bytesRead) return false;

    uint32_t payloadRaw;
    if (stream->Read(&payloadRaw, 4) != 4) return false;
    const uint32_t payloadSize = rd(payloadRaw);

    if (targetOffset + 4 > length) return false;
    const uint32_t remaining = length - targetOffset - 4;
    const uint32_t take = (payloadSize > 0 && payloadSize <= remaining) ? payloadSize
                                                                       : remaining;

    // Named by the section's own chunk offset, which is what LoadLevelData
    // matches ShoSection::offset against when it places the instances.
    const std::string uniqueName = std::to_string(chunkStart);
    CResourceHandler::GetInstance().ProcessStream(uniqueName.c_str(), stream, take);
    return true;
}


bool CTexDictionaryStreamLoader::Read(const char* name, RWS::RwStream* stream, uint32_t length) {
    std::cout << "[ResourceLoader] Found CTexDictionaryStreamLoader chunk (size: " << length << ")\n";
    
    std::vector<uint8_t> data(length + 12);
    // Rewind back 12 bytes because PS2TextureDecoder expects the chunk header
    stream->Seek(stream->Tell() - 12);
    stream->Read(data.data(), length + 12);
    
    // Attempt to load dictionary for all remaining missing materials, or just blindly
    std::vector<std::string> missing;
    for (const auto &mat : ::g_MaterialNames) {
        if (::g_TextureMap.find(mat) == ::g_TextureMap.end()) {
            missing.push_back(mat);
        }
    }
    
    if (!missing.empty() || ::g_MaterialNames.empty()) {
        ::ClimaxEngine::Platform::PS2::PS2TextureDecoder().LoadDictionary(data, missing.empty() ? ::g_MaterialNames : missing, true);
    }
    
    return true;
}

bool CAudioCuesStreamLoader::Read(const char* name, RWS::RwStream* stream, uint32_t length) {
    std::cout << "[ResourceLoader] Found CAudioCuesStreamLoader chunk (size: " << length << ")\n";
    stream->Skip(length);
    return true;
}


} // namespace ResourceLoader
} // namespace ClimaxEngine

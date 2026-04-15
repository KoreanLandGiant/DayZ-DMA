#define NOMINMAX
#include "overlay.h"
#include <thread>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <vector>
#include <set>
#include <memory>
#include <unordered_map>
#include <Windows.h>
#include "SDK.h"
#include "settings.h"
#include "xorstr.h"
#include <xmmintrin.h>
#include <DirectXMath.h>
#include <iosfwd>
#include <iostream>
#include <sstream>

#define UENGINE __stdcall

// Single-thread entity storage — no mutex needed, updated inline before render
std::vector<player_t> g_entities;
std::vector<item_t> items = {};

class USQAnimInstanceSoldier1P1
{
public:
    void UENGINE RecoilCameraOffsetFactor(int zoom)
    {
        *(int*)(this) = zoom;

    }

};
class USQAnimInstanceSoldier1P
{
public:

};

const int screenWidth = 1440;
const int screenHeight = 900;
const float fovRadians = 0.785398; // Aproximadamente 45 graus em radianos

// Single-thread: no mutexes needed

std::vector<uintptr_t*> valid_players;
std::vector<uintptr_t*> valid_LootTP;
std::vector<uintptr_t*> valid_players2;
std::vector<uintptr_t*> valid_players4inventory;
std::vector<uintptr_t*> valid_players4Loot;

int addr = 10000000;

void keyboard_listener();

// Defined in main.cpp
extern void Log(const char* msg);
extern void Log(const std::string& msg);

// Forward declarations
void UpdateItemCache(uintptr_t pUWorld);

void init()
{
    Log("[ 1 ] init() entered");

    Log("[ 2 ] Creating VmmManager...");
    globals.vmmManager = new DMAMem::VmmManager();

    Log("[ 3 ] Calling getVmm() to connect to DMA card...");
    VMM_HANDLE vmm = globals.vmmManager->getVmm();
    if (!vmm) {
        Log("[ ! ] Failed to initialize DMA/VMM. Check FPGA connection.");
        return;
    }
    Log("[ 4 ] DMA connected successfully");

    globals.staticManager = new DMAMem::StaticManager(globals.vmmManager);

    Log("[ 5 ] Finding DayZ_x64.exe...");
    globals.process_id = globals.staticManager->getPid(xorstr_("DayZ_x64.exe"));
    if (!globals.process_id) {
        Log("[ ! ] Failed to find DayZ_x64.exe. Is the game running?");
        return;
    }
    Log("[ 6 ] Found PID: " + std::to_string(globals.process_id));

    Log("[ 7 ] Getting module base (NOT dumping full module)...");
    // Use VMMDLL_ProcessGetModuleBaseU directly instead of dumping the whole module
    ULONG64 baseAddr = VMMDLL_ProcessGetModuleBaseU(vmm, globals.process_id, LPSTR("DayZ_x64.exe"));
    if (!baseAddr) {
        Log("[ ! ] Failed to get module base address");
        return;
    }
    globals.Base = baseAddr;
    Log("[ 8 ] Module base: 0x" + ([&]{ std::stringstream ss; ss << std::hex << globals.Base; return ss.str(); })());

    Log("[ 9 ] Reading World pointer...");
    globals.World = ReadData<uint64_t>(globals.Base + OFF_World);
    Log("[10 ] World: 0x" + ([&]{ std::stringstream ss; ss << std::hex << globals.World; return ss.str(); })());

    if (!globals.World) {
        Log("[ ! ] World pointer is null - offsets may be outdated");
        return;
    }

    Log("[11 ] Starting keyboard listener...");
    std::thread(keyboard_listener).detach();

    Log("[12 ] init() complete — entity updates run inline with render");
}

void DrawBoxFilledmod(const DirectX::XMFLOAT2& from, const DirectX::XMFLOAT2& size, DirectX::XMFLOAT4 color, float rounding)
{
    auto* window = ImGui::GetOverlayDrawList();
    window->AddRectFilled(ImVec2(from.x, from.y), ImVec2(size.x, size.y), ImGui::GetColorU32(ImVec4(color.x / 255, color.y / 255, color.z / 255, color.w / 255)), rounding);
}

void DrawInventoryList(std::vector<std::string> items, std::string name_str, DirectX::XMFLOAT2 screen_resolution, DirectX::XMFLOAT4 tag_color)
{
    float x = 20.f;
    float y = 20.f;
    float width = 200.f;

    float itemHeight = 30.f;
    float itemPadding = 5.f;
    float headerHeight = 40.f;

    float extraMargin = 10.f;
    float height = headerHeight + (items.size() * (itemHeight + itemPadding)) - itemPadding + extraMargin;

    // Fundo da lista
    DrawBoxFilledmod({ x, y }, { x + width, y + height }, { 24.f, 24.f, 24.f, 255.f }, 10.0f);

    auto* window = ImGui::GetOverlayDrawList();
    ImVec2 nameTextSize = ImGui::CalcTextSize(name_str.c_str());
    float namePosX = x + (width / 2) - (nameTextSize.x / 2);
    window->AddText({ namePosX, y + 10 }, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), name_str.c_str());
    float itemY = y + headerHeight;

    for (const auto& item : items)
    {
        // Caixa para cada item
        DrawBoxFilledmod({ x + 10, itemY }, { x + width - 10, itemY + itemHeight }, { 32.f, 32.f, 32.f, 255.f }, 5.0f);

        ImVec2 itemTextSize = ImGui::CalcTextSize(item.c_str());
        float itemTextPosX = x + (width / 2) - (itemTextSize.x / 2);
        window->AddText({ itemTextPosX, itemY + (itemHeight / 2) - (itemTextSize.y / 2) }, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), item.c_str());

        itemY += itemHeight + itemPadding;
    }
}

float sqrtf1(float _X) {
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(_X)));
}

// Zero-DMA version: uses cached bone positions (single-thread, direct access)
uintptr_t* get_closest_player(std::vector<uintptr_t*> list, float field_of_view)
{
    ImGuiIO& io = ImGui::GetIO();

    uintptr_t* resultant_target_entity_temp = nullptr;
    float closestTocenter = FLT_MAX;

    for (auto curr_entity : list)
    {
        const player_t* cached = nullptr;
        for (const auto& e : g_entities) {
            if (e.EntityPtr == (uint64_t)curr_entity) {
                cached = &e;
                break;
            }
        }
        if (!cached || !cached->bonesCached) continue;

        int targetBone = 18;
        if (!cached->boneValid[targetBone]) continue;

        Vector3 currentworld;
        Game::WorldToScreen(cached->bonePositions[targetBone], currentworld);
        auto dx = currentworld.x - (io.DisplaySize.x / 2);
        auto dy = currentworld.y - (io.DisplaySize.y / 2);
        auto dist = sqrtf1(dx * dx + dy * dy);

        if (dist < field_of_view && dist < closestTocenter) {
            closestTocenter = dist;
            resultant_target_entity_temp = curr_entity;
        }
    }

    return resultant_target_entity_temp;
}

// Zero-DMA version: uses cached bone positions (single-thread, direct access)
uintptr_t* get_closest_zombie(std::vector<uintptr_t*> list, float field_of_view)
{
    ImGuiIO& io = ImGui::GetIO();

    uintptr_t* resultant_target_entity_temp = nullptr;
    float closestTocenter = FLT_MAX;

    for (auto curr_entity : list)
    {
        const player_t* cached = nullptr;
        for (const auto& e : g_entities) {
            if (e.EntityPtr == (uint64_t)curr_entity) {
                cached = &e;
                break;
            }
        }
        if (!cached || !cached->bonesCached) continue;

        // Bone 31 for zombie head... use closest available
        int targetBone = 31;
        if (targetBone >= player_t::MAX_BONE_IDX || !cached->boneValid[targetBone]) {
            // Fallback to bone 8 if 31 not cached
            targetBone = 8;
            if (!cached->boneValid[targetBone]) continue;
        }

        Vector3 currentworld;
        Game::WorldToScreen(cached->bonePositions[targetBone], currentworld);
        auto dx = currentworld.x - (io.DisplaySize.x / 2);
        auto dy = currentworld.y - (io.DisplaySize.y / 2);
        auto dist = sqrtf1(dx * dx + dy * dy);

        if (dist < field_of_view && dist < closestTocenter) {
            closestTocenter = dist;
            resultant_target_entity_temp = curr_entity;
        }
    }

    return resultant_target_entity_temp;
}

std::string get_player_name(uint64_t entity) {
    std::string playerName = "BOT";

    bool isDead = ReadData<bool>(entity + OFF_playerIsDead);
    if (isDead) {
        return "(DEAD)";
    }

    uint64_t network_client = ReadData<uint64_t>(globals.Base + OFF_Network_Manager + OFF_Network_Client);
    if (!Game::IsValidPtr2((void*)network_client))
        return playerName;

    uint32_t entity_network_id = ReadData<uint32_t>(entity + OFF_Network_ID);
    if (entity_network_id < 1)
        return playerName;

    uint64_t scoreboard = ReadData<uint64_t>(network_client + OFF_Network_Table);
    uint32_t scoreboard_size = ReadData<uint32_t>(network_client + OFF_Network_Table_Size);
    if (scoreboard_size < 1)
        return playerName;

    for (uint32_t i = 0; i < scoreboard_size; i++) {
        uint64_t current_identity = ReadData<uint64_t>(scoreboard + (i * sizeof(uint64_t)));
        if (!Game::IsValidPtr2((void*)current_identity))
            return playerName;

        uint32_t current_id = ReadData<uint32_t>(current_identity + OFF_Network_Table_ID);
        if (current_id == entity_network_id) {
            uint64_t namePtr = ReadData<uint64_t>(current_identity + OFF_PlayerName);
            playerName = ReadArmaString(namePtr);
            return playerName;
        }
    }

    return playerName;
}

void SetTerrainGrid(float value) {
    WriteData<float>(globals.World + OFF_grass, value);
}

void SetEyeAcom(float value) {
    WriteData<float>(globals.World + OFF_Day, value);
}

void SetWorldTime(float value) {
    WriteData<float>(globals.World + OFF_Hour, value);
}

void SetTick(int value) {
    WriteData<int>(globals.Base + OFF_Tickness, value);
}

void SetThirdPerson(bool value)
{
    uintptr_t ThirdPerson = Game::GetNetworkClient();
    WriteData<bool>(ThirdPerson + OFF_ThirdPerson, value);
}



void DrawLine(float x1, float y1, float x2, float y2, ImColor color)
{
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color);
}


// Scatter-batched skeleton drawing: uses cached pointer chain, only 1 scatter for bone data
// Uses pre-cached bone world positions from background thread (0 DMA reads)
void DrawSkeletonScatter(const player_t& ent, ImVec4 color, int skeletonType, float lineThickness = 1.5f)
{
    if (!ent.bonesCached) return;

    static int vBonePlayer[][2] = {
        { 21, 61 }, { 61, 63 }, { 63, 65 },
        { 21, 94 }, { 94, 97 }, { 97, 99 },
        { 21, 18 }, { 18, 1 }, { 18, 9 },
        { 1, 4 }, { 4, 6 }, { 6, 7 },
        { 9, 11 }, { 11, 14 }, { 14, 15 }
    };
    static int vBoneZombie[][2] = {
        { 19, 24 }, { 24, 53 }, { 53, 27 },
        { 19, 56 }, { 56, 59 }, { 59, 60 },
        { 19, 0 }, { 0, 1 }, { 0, 8 },
        { 1, 3 }, { 3, 6 }, { 6, 7 },
        { 8, 10 }, { 10, 13 }, { 13, 14 }
    };

    auto (*vBone)[2] = (skeletonType == 1) ? vBonePlayer : vBoneZombie;
    int pairCount = 15;

    auto* window = ImGui::GetOverlayDrawList();
    for (int i = 0; i < pairCount; ++i) {
        int b1 = vBone[i][0], b2 = vBone[i][1];
        if (b1 >= player_t::MAX_BONE_IDX || b2 >= player_t::MAX_BONE_IDX) continue;
        if (!ent.boneValid[b1] || !ent.boneValid[b2]) continue;

        Vector3 s1, s2;
        if (Game::WorldToScreen(ent.bonePositions[b1], s1) && Game::WorldToScreen(ent.bonePositions[b2], s2)) {
            window->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), ImGui::GetColorU32(color), lineThickness);
        }
    }
}

// Wrappers matching old signatures
void DrawSkeleton(const player_t& ent, ImVec4 color, int s1)
{
    DrawSkeletonScatter(ent, color, 1, 1.5f);
}

void DrawSkeletonZ(const player_t& ent, ImVec4 color, int s1)
{
    DrawSkeletonScatter(ent, color, 2, 1.0f);
}

void DrawBoneIDs(uintptr_t Entity, ImVec4 color, int s1)
{

    const int maxBones = 140;
    int vBone[maxBones];


    for (int i = 0; i < maxBones; ++i)
    {
        vBone[i] = i + 1;
    }

    auto* window = ImGui::GetOverlayDrawList();


    for (int boneIndex : vBone)
    {
        Vector3 bonePosition;

        if (!Game::GetBonePosition(Game::GetSkeleton(Entity, s1), Game::GeVisualState(Entity), boneIndex, &bonePosition))
            continue;

        Vector3 screenPosition;

        if (Game::WorldToScreen(bonePosition, screenPosition))
        {

            std::string boneIDText = std::to_string(boneIndex);
            window->AddText(ImVec2(screenPosition.x, screenPosition.y), ImGui::GetColorU32(color), boneIDText.c_str());
        }
    }
}

void DrawBoneIDsZ(uintptr_t Entity, ImVec4 color, int s1)
{

    const int maxBones = 140;
    int vBone[maxBones];


    for (int i = 0; i < maxBones; ++i)
    {
        vBone[i] = i + 1;
    }

    auto* window = ImGui::GetOverlayDrawList();


    for (int boneIndex : vBone)
    {
        Vector3 bonePosition;

        if (!Game::GetBonePosition(Game::GetSkeleton(Entity, 2), Game::GeVisualState(Entity), boneIndex, &bonePosition))
            continue;

        Vector3 screenPosition;

        if (Game::WorldToScreen(bonePosition, screenPosition))
        {

            std::string boneIDText = std::to_string(boneIndex);
            window->AddText(ImVec2(screenPosition.x, screenPosition.y), ImGui::GetColorU32(color), boneIDText.c_str());
        }
    }
}

ImVec4 GetHealthColor(int health) {
    if (health >= 255) {
        return ImVec4(0.0f, 1.0f, 0.0f, 0.6f); // Verde desbotado
    }
    else if (health >= 100) {
        int t = (health - 100.0f) / (255.0f - 100.0f); // Transição entre 100 e 255
        return ImVec4(1.0f - t * 0.5f, 1.0f - t * 0.3f, 0.0f, 0.6f); // Verde -> Amarelo desbotado
    }
    else if (health == 1) {
        return ImVec4(1.0f, 0.7f, 0.0f, 0.6f); // Amarelo avermelhado desbotado
    }
    else if (health == 2) {
        return ImVec4(1.0f, 0.5f, 0.0f, 0.6f); // Laranja desbotado
    }
    else if (health == 3) {
        return ImVec4(1.0f, 0.0f, 0.0f, 0.6f); // Vermelho desbotado
    }
    else if (health == 4) {
        return ImVec4(0.0f, 0.0f, 0.0f, 0.6f); // Preto desbotado
    }
    else {
        return ImVec4(0.0f, 0.0f, 0.0f, 0.6f); // Preto (Invalido ou Saúde Baixa Não Definida)
    }
}

float MapHealthToBarHeight(int health, float boxHeight) {
    if (health >= 255) {
        return boxHeight; // 100% da altura para vida máxima
    }
    else if (health >= 100) {
        return boxHeight * 0.7f + (boxHeight * 0.3f * ((health - 100) / 155.0f));
    }
    else if (health == 1) {
        return boxHeight * 0.7f + (boxHeight * 0.2f * ((health - 50) / 50.0f));
    }
    else if (health == 2) {
        return boxHeight * 0.45f + (boxHeight * 0.25f * ((health - 25) / 25.0f));
    }
    else if (health == 3) {
        return boxHeight * 0.1f + (boxHeight * 0.2f * ((health - 4) / 21.0f));
    }
    else if (health == 4) {
        return boxHeight * 0.05f + (boxHeight * 0.2f * ((health - 4) / 21.0f));
    }
    else {
        return boxHeight * 0.05f;
    }
}

void DrawHealthBar(uintptr_t Entity, const ImVec2& topLeft, const ImVec2& bottomRight) {
    auto* window = ImGui::GetOverlayDrawList();

    float boxHeight = bottomRight.y - topLeft.y;

    int health = static_cast<int>(Game::get_health(Entity));

    float barHeight = MapHealthToBarHeight(health, boxHeight);

    ImVec2 barTopLeft = { topLeft.x - 8.0f, bottomRight.y - barHeight }; // Base ajustada pela altura da barra
    ImVec2 barBottomRight = { topLeft.x - 4.0f, bottomRight.y };         // Base fixa na parte inferior

    window->AddRectFilled(ImVec2(topLeft.x - 8.0f, topLeft.y), ImVec2(topLeft.x - 4.0f, bottomRight.y), ImColor(0, 0, 0, 120), 100.0f);  // 4.0f é o raio das bordas

    ImVec4 healthColor = GetHealthColor(health);
    window->AddRectFilled(barTopLeft, barBottomRight, ImGui::GetColorU32(healthColor), 100.0f);  // 4.0f é o raio das bordas
}

// Uses pre-cached bone world positions (0 DMA reads)
bool GetBoneScreenBounds(const player_t& ent, const std::vector<int>& boneIndices, ImVec2& topLeft, ImVec2& bottomRight)
{
    if (!ent.bonesCached) return false;

    std::vector<ImVec2> screenPoints;
    for (int boneIdx : boneIndices) {
        if (boneIdx >= player_t::MAX_BONE_IDX || !ent.boneValid[boneIdx]) continue;

        Vector3 screenPos;
        if (Game::WorldToScreen(ent.bonePositions[boneIdx], screenPos))
            screenPoints.emplace_back(screenPos.x, screenPos.y);
    }

    if (screenPoints.empty()) return false;

    topLeft = screenPoints[0];
    bottomRight = screenPoints[0];
    for (const auto& point : screenPoints) {
        if (point.x < topLeft.x) topLeft.x = point.x;
        if (point.y < topLeft.y) topLeft.y = point.y;
        if (point.x > bottomRight.x) bottomRight.x = point.x;
        if (point.y > bottomRight.y) bottomRight.y = point.y;
    }
    return true;
}

void DrawDynamicBox(const player_t& ent, ImVec4 color, float lineThickness = 2.0f, float expansionFactor = 1.0f)
{
    static std::vector<int> boneIndices = { 49, 8, 75, 113, 16, 97, 63, 58, 48 };
    ImVec2 topLeft, bottomRight;
    if (!GetBoneScreenBounds(ent, boneIndices, topLeft, bottomRight))
        return;

    topLeft.x -= expansionFactor;
    topLeft.y -= expansionFactor;
    bottomRight.x += expansionFactor;
    bottomRight.y += expansionFactor;

    auto* window = ImGui::GetOverlayDrawList();
    ImVec4 outlineColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    float outlineThickness = lineThickness + 2.0f;
    window->AddLine(ImVec2(topLeft.x, topLeft.y), ImVec2(bottomRight.x, topLeft.y), ImGui::GetColorU32(outlineColor), outlineThickness);
    window->AddLine(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x, bottomRight.y), ImGui::GetColorU32(outlineColor), outlineThickness);
    window->AddLine(ImVec2(bottomRight.x, bottomRight.y), ImVec2(topLeft.x, bottomRight.y), ImGui::GetColorU32(outlineColor), outlineThickness);
    window->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x, topLeft.y), ImGui::GetColorU32(outlineColor), outlineThickness);

    window->AddLine(ImVec2(topLeft.x, topLeft.y), ImVec2(bottomRight.x, topLeft.y), ImGui::GetColorU32(color), lineThickness);
    window->AddLine(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x, bottomRight.y), ImGui::GetColorU32(color), lineThickness);
    window->AddLine(ImVec2(bottomRight.x, bottomRight.y), ImVec2(topLeft.x, bottomRight.y), ImGui::GetColorU32(color), lineThickness);
    window->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x, topLeft.y), ImGui::GetColorU32(color), lineThickness);
}

void DrawHealth(const player_t& ent, float lineThickness = 2.0f, float expansionFactor = 1.0f)
{
    static std::vector<int> boneIndices = { 49, 8, 75, 113, 16, 97, 63, 58, 48 };
    ImVec2 topLeft, bottomRight;
    if (!GetBoneScreenBounds(ent, boneIndices, topLeft, bottomRight))
        return;

    topLeft.x -= expansionFactor;
    topLeft.y -= expansionFactor;
    bottomRight.x += expansionFactor;
    bottomRight.y += expansionFactor;

    DrawHealthBar(ent.EntityPtr, topLeft, bottomRight);
}

void DrawCornerBox(const player_t& ent, ImVec4 color, float lineThickness = 2.0f, float expansionFactor = 1.0f, float cornerLength = 10.0f)
{
    static std::vector<int> boneIndices = { 49, 8, 75, 113, 16, 97, 63, 58, 48 };
    ImVec2 topLeft, bottomRight;
    if (!GetBoneScreenBounds(ent, boneIndices, topLeft, bottomRight))
        return;

    topLeft.x -= expansionFactor;
    topLeft.y -= expansionFactor;
    bottomRight.x += expansionFactor;
    bottomRight.y += expansionFactor;

    float boxWidth = bottomRight.x - topLeft.x;
    float boxHeight = bottomRight.y - topLeft.y;
    cornerLength = std::min(cornerLength, std::min(boxWidth / 2, boxHeight / 2));

    auto* window = ImGui::GetOverlayDrawList();
    auto drawCorner = [&](ImVec2 start, ImVec2 end) {
        window->AddLine(start, end, ImGui::GetColorU32(color), lineThickness);
        };

    drawCorner(topLeft, ImVec2(topLeft.x + cornerLength, topLeft.y));
    drawCorner(topLeft, ImVec2(topLeft.x, topLeft.y + cornerLength));
    drawCorner(ImVec2(bottomRight.x - cornerLength, topLeft.y), ImVec2(bottomRight.x, topLeft.y));
    drawCorner(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x, topLeft.y + cornerLength));
    drawCorner(ImVec2(topLeft.x, bottomRight.y - cornerLength), ImVec2(topLeft.x, bottomRight.y));
    drawCorner(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x + cornerLength, bottomRight.y));
    drawCorner(ImVec2(bottomRight.x - cornerLength, bottomRight.y), ImVec2(bottomRight.x, bottomRight.y));
    drawCorner(ImVec2(bottomRight.x, bottomRight.y - cornerLength), ImVec2(bottomRight.x, bottomRight.y));
}

void DrawLineESPFromCachedPos(ImVec4 color, Vector3 worldPos, float lineWidth)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    Vector3 screenPos;
    if (!Game::WorldToScreen(worldPos, screenPos))
        return;

    ImVec2 from;
    if (GameVars.selectedLinePos == 0) from = ImVec2(io.DisplaySize.x / 2, 0);
    else if (GameVars.selectedLinePos == 1) from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2);
    else from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y);

    window->AddLine(from, ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(color), lineWidth);
}

void DrawLineESP(uintptr_t Entity, ImVec4 color, int s1)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    Vector3 playerPos;
    if (!Game::GetBonePosition(Game::GetSkeleton(Entity, s1), Game::GeVisualState(Entity), 8, &playerPos))
        return;

    Vector3 screenPos;
    if (!Game::WorldToScreen(playerPos, screenPos))
        return;

    ImVec2 from;
    if (GameVars.selectedLinePos == 0) from = ImVec2(io.DisplaySize.x / 2, 0);
    else if (GameVars.selectedLinePos == 1) from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2);
    else from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y);

    window->AddLine(from, ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(color), GameVars.linep);
}

void DrawLineZESP(uintptr_t Entity, ImVec4 color, int s2)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    Vector3 playerPos;
    if (!Game::GetBonePosition(Game::GetSkeleton(Entity, 2), Game::GeVisualState(Entity), 8, &playerPos))
        return;

    Vector3 screenPos;
    if (!Game::WorldToScreen(playerPos, screenPos))
        return;

    ImVec2 from;
    if (GameVars.selectedLinePos == 0) from = ImVec2(io.DisplaySize.x / 2, 0);
    else if (GameVars.selectedLinePos == 1) from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2);
    else from = ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y);

    window->AddLine(from, ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(color), GameVars.linez);
}

void DrawNameESP(uintptr_t Entity, ImVec4 color)
{
    auto* window = ImGui::GetOverlayDrawList();

    Vector3 playerPos;
    if (!Game::GetBonePosition(Game::GetSkeleton(Entity, 1), Game::GeVisualState(Entity), 8, &playerPos))
        return;

    Vector3 screenPos;
    if (!Game::WorldToScreen(playerPos, screenPos))
        return;

    char* playerName = ReadData<char*>(Entity + OFF_PlayerName);

    std::string nameText = "Name: " + std::string(playerName);
    window->AddText(ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(color), nameText.c_str());
}

// Refresh camera + local player between scatter passes so ESP tracks smoothly
static void RefreshCamera()
{
    Game::UpdateCameraCache();
    Game::UpdateLocalPlayerCache();
}

// Single-thread: called inline before each render frame
void update_entities()
{
    // Static state persists across calls (replaces loop-local variables)
    static int scanCycle = 0;
    static std::unordered_map<uint64_t, std::pair<std::string, std::string>> cachedPlayerInfo;
    static std::unordered_map<uint64_t, std::pair<std::string, std::string>> cachedEntityNames;

    struct CachedPtrChain {
        uint64_t entityTypePtr;
        uint64_t playerSkeletonPtr, zmSkeletonPtr;
        uint64_t playerAnimClass, zmAnimClass;
        uint64_t playerMatrixClass, zmMatrixClass;
    };
    static std::unordered_map<uint64_t, CachedPtrChain> cachedPtrChains;

    static LARGE_INTEGER perfFreq = {};
    if (!perfFreq.QuadPart) QueryPerformanceFrequency(&perfFreq);
    auto hpc_now = []() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; };
    double toMs = 1000.0 / perfFreq.QuadPart;

    {
        bool fullScan = (scanCycle % 5 == 0);
        scanCycle++;

        auto cycleStart = hpc_now();
        auto passStart = cycleStart;

        // Lambda to measure pass duration
        struct PassTime { const char* name; double ms; };
        std::vector<PassTime> passTimes;
        auto markPass = [&](const char* name) {
            auto now = hpc_now();
            passTimes.push_back({ name, (now - passStart) * toMs });
            passStart = now;
        };

        // No manual cache refresh — using -norefresh + VMMDLL_FLAG_NOCACHE for all reads.
        // TLB cache stays warm from init. DayZ page tables don't change during gameplay.

        std::vector<player_t> tmp_entities{};
        std::vector<item_t> tmp_items{};

        RefreshCamera(); // camera update at top of every cycle
        markPass("RefreshCam0");

        // === Pass 0: Read table sizes + table pointers in one scatter ===
        uint32_t NearTableSize = 0, FarTableSize = 0, itemTableSize = 0;
        uint64_t NearTable = 0, FarTable = 0, ItemTablePtr = 0;
        {
            auto s = ScatterBegin();
            if (s) {
                ScatterAdd(s, globals.World + OFF_NEAR_ENTITY_TABLE + 0x8, &NearTableSize);
                ScatterAdd(s, globals.World + OFF_FAR_ENTITY_TABLE + 0x8, &FarTableSize);
                ScatterAdd(s, globals.World + OFF_ItemTableSize, &itemTableSize);
                ScatterAdd(s, globals.World + OFF_NEAR_ENTITY_TABLE, &NearTable);
                ScatterAdd(s, globals.World + OFF_FAR_ENTITY_TABLE, &FarTable);
                ScatterAdd(s, globals.World + OFF_ItemTable, &ItemTablePtr);
                ScatterExecute(s);
            }
        }

        markPass("Pass0_Tables");

        // Clamp table sizes to sane limits
        if (NearTableSize > 512) NearTableSize = 512;
        if (FarTableSize > 512) FarTableSize = 512;
        if (itemTableSize > 2048) itemTableSize = 2048;

        // === Pass 1: Scatter-read all entity pointers from both tables ===
        int totalEntities = NearTableSize + FarTableSize;
        std::vector<uint64_t> entityPtrs(totalEntities, 0);
        std::vector<uint64_t> entityTableSrc(totalEntities, 0); // track which table

        if (totalEntities > 0) {
            auto s = ScatterBegin();
            if (s) {
                for (uint32_t i = 0; i < NearTableSize; ++i) {
                    ScatterAdd(s, NearTable + (i * 0x8), &entityPtrs[i]);
                    entityTableSrc[i] = NearTable;
                }
                for (uint32_t i = 0; i < FarTableSize; ++i) {
                    ScatterAdd(s, FarTable + (i * 0x8), &entityPtrs[NearTableSize + i]);
                    entityTableSrc[NearTableSize + i] = FarTable;
                }
                ScatterExecute(s);
            }
        }
        markPass("Pass1_EntityPtrs");

        // Build entity list from valid pointers
        for (int i = 0; i < totalEntities; ++i) {
            if (!entityPtrs[i]) continue;
            player_t Player{};
            Player.EntityPtr = entityPtrs[i];
            Player.TableEntry = entityTableSrc[i];
            Player.dataCached = false;
            tmp_entities.push_back(Player);
        }

        // === Pass 2: Scatter-read core data for all entities ===
        if (!tmp_entities.empty()) {
            if (fullScan) {
                // Full scan: read everything including static pointer chains
                auto s = ScatterBegin();
                if (s) {
                    for (auto& e : tmp_entities) {
                        ScatterAdd(s, e.EntityPtr + OFF_VisualState, &e.visualStatePtr);
                        ScatterAdd(s, e.EntityPtr + OFF_EntityTypePtr, &e.entityTypePtr);
                        ScatterAdd(s, e.EntityPtr + OFF_PlayerSkeleton, &e.playerSkeletonPtr);
                        ScatterAdd(s, e.EntityPtr + OFF_ZmSkeleton, &e.zmSkeletonPtr);
                        ScatterAdd(s, e.EntityPtr + OFF_playerIsDead, &e.isDead);
                        ScatterAdd(s, e.EntityPtr + OFF_playerNetworkID, &e.NetworkID);
                    }
                    ScatterExecute(s);
                }
            } else {
                // Fast tick: only read volatile data (visualState, isDead, networkID)
                // Restore static pointer chains from cache
                auto s = ScatterBegin();
                if (s) {
                    for (auto& e : tmp_entities) {
                        ScatterAdd(s, e.EntityPtr + OFF_VisualState, &e.visualStatePtr);
                        ScatterAdd(s, e.EntityPtr + OFF_playerIsDead, &e.isDead);
                        ScatterAdd(s, e.EntityPtr + OFF_playerNetworkID, &e.NetworkID);
                    }
                    ScatterExecute(s);
                }
                // Restore cached static pointers
                for (auto& e : tmp_entities) {
                    auto it = cachedPtrChains.find(e.EntityPtr);
                    if (it != cachedPtrChains.end()) {
                        e.entityTypePtr = it->second.entityTypePtr;
                        e.playerSkeletonPtr = it->second.playerSkeletonPtr;
                        e.zmSkeletonPtr = it->second.zmSkeletonPtr;
                        e.playerAnimClass = it->second.playerAnimClass;
                        e.zmAnimClass = it->second.zmAnimClass;
                        e.playerMatrixClass = it->second.playerMatrixClass;
                        e.zmMatrixClass = it->second.zmMatrixClass;
                    }
                }
            }

            // === Pass 3: Scatter-read coordinates from visual states ===
            struct CoordTemp { float x, y, z; };
            std::vector<CoordTemp> coords(tmp_entities.size(), {});
            {
                auto s2 = ScatterBegin();
                if (s2) {
                    for (size_t i = 0; i < tmp_entities.size(); ++i) {
                        if (tmp_entities[i].visualStatePtr)
                            ScatterAdd(s2, tmp_entities[i].visualStatePtr + OFF_GetCoordinate, &coords[i]);
                    }
                    ScatterExecute(s2);
                }
            }
            for (size_t i = 0; i < tmp_entities.size(); ++i) {
                tmp_entities[i].coordX = coords[i].x;
                tmp_entities[i].coordY = coords[i].y;
                tmp_entities[i].coordZ = coords[i].z;
                tmp_entities[i].dataCached = true;
            }

            if (fullScan) {
                // === Pass 3b: Scatter-read animClass from skeleton pointers ===
                {
                    auto sa = ScatterBegin();
                    if (sa) {
                        for (auto& e : tmp_entities) {
                            if (e.playerSkeletonPtr)
                                ScatterAdd(sa, e.playerSkeletonPtr + OFF_AnimClass, &e.playerAnimClass);
                            if (e.zmSkeletonPtr)
                                ScatterAdd(sa, e.zmSkeletonPtr + OFF_AnimClass, &e.zmAnimClass);
                        }
                        ScatterExecute(sa);
                    }
                }

                // === Pass 3c: Scatter-read matrixClass from animClass pointers ===
                {
                    auto sb = ScatterBegin();
                    if (sb) {
                        for (auto& e : tmp_entities) {
                            if (e.playerAnimClass)
                                ScatterAdd(sb, e.playerAnimClass + OFF_MatrixClass, &e.playerMatrixClass);
                            if (e.zmAnimClass)
                                ScatterAdd(sb, e.zmAnimClass + OFF_MatrixClass, &e.zmMatrixClass);
                        }
                        ScatterExecute(sb);
                    }
                }

                // Update pointer chain cache
                cachedPtrChains.clear();
                for (auto& e : tmp_entities) {
                    cachedPtrChains[e.EntityPtr] = {
                        e.entityTypePtr,
                        e.playerSkeletonPtr, e.zmSkeletonPtr,
                        e.playerAnimClass, e.zmAnimClass,
                        e.playerMatrixClass, e.zmMatrixClass
                    };
                }
            }
            // else: fast tick — static pointers already restored from cache above

            markPass("Pass2-3c_CoreData");

            // === Pass 4+5: Entity type names (full scan only, cached on fast ticks) ===
            if (fullScan) {
                std::vector<uint64_t> typeNamePtrs(tmp_entities.size(), 0);
                std::vector<uint64_t> realNamePtrs(tmp_entities.size(), 0);
                {
                    auto s3 = ScatterBegin();
                    if (s3) {
                        for (size_t i = 0; i < tmp_entities.size(); ++i) {
                            if (tmp_entities[i].entityTypePtr) {
                                ScatterAdd(s3, tmp_entities[i].entityTypePtr + OFF_EntityTypeName, &typeNamePtrs[i]);
                                ScatterAdd(s3, tmp_entities[i].entityTypePtr + OFF_RealName, &realNamePtrs[i]);
                            }
                        }
                        ScatterExecute(s3);
                    }
                }
                struct NameBuf { char typeName[256]; char realName[256]; };
                std::vector<NameBuf> nameBufs(tmp_entities.size(), {});
                {
                    auto s4 = ScatterBegin();
                    if (s4) {
                        for (size_t i = 0; i < tmp_entities.size(); ++i) {
                            if (typeNamePtrs[i])
                                ScatterAdd(s4, typeNamePtrs[i] + OFF_TEXT, nameBufs[i].typeName, 256);
                            if (realNamePtrs[i])
                                ScatterAdd(s4, realNamePtrs[i] + OFF_TEXT, nameBufs[i].realName, 256);
                        }
                        ScatterExecute(s4);
                    }
                }
                cachedEntityNames.clear();
                for (size_t i = 0; i < tmp_entities.size(); ++i) {
                    nameBufs[i].typeName[255] = '\0';
                    nameBufs[i].realName[255] = '\0';
                    tmp_entities[i].entityTypeName = nameBufs[i].typeName;
                    tmp_entities[i].entityRealName = nameBufs[i].realName;
                    if (tmp_entities[i].entityTypePtr)
                        cachedEntityNames[tmp_entities[i].entityTypePtr] = { tmp_entities[i].entityTypeName, tmp_entities[i].entityRealName };
                }
                // (camera refreshed at top of cycle, skip mid-pass DMA)
            } else {
                // Fast tick: reuse cached entity names
                for (size_t i = 0; i < tmp_entities.size(); ++i) {
                    auto it = cachedEntityNames.find(tmp_entities[i].entityTypePtr);
                    if (it != cachedEntityNames.end()) {
                        tmp_entities[i].entityTypeName = it->second.first;
                        tmp_entities[i].entityRealName = it->second.second;
                    }
                }
            }
            markPass("Pass4-5_Names");

            // === Pass 6: Cache player names via scatter-based bulk scoreboard lookup ===
            if (fullScan) {
                cachedPlayerInfo.clear();

                // 6a: Read network client + scoreboard pointer + size (1 scatter)
                uint64_t network_client = 0;
                uint64_t scoreboard = 0;
                uint32_t scoreboard_size = 0;
                {
                    auto s = ScatterBegin();
                    if (s) {
                        ScatterAdd(s, globals.Base + OFF_Network_Manager + OFF_Network_Client, &network_client);
                        ScatterExecute(s);
                    }
                }
                if (Game::IsValidPtr2((void*)network_client)) {
                    auto s = ScatterBegin();
                    if (s) {
                        ScatterAdd(s, network_client + OFF_Network_Table, &scoreboard);
                        ScatterAdd(s, network_client + OFF_Network_Table_Size, &scoreboard_size);
                        ScatterExecute(s);
                    }
                }

                // 6b: Read ALL scoreboard identity pointers in one scatter
                if (scoreboard_size > 0 && scoreboard_size <= 120 && Game::IsValidPtr2((void*)scoreboard)) {
                    std::vector<uint64_t> identityPtrs(scoreboard_size, 0);
                    {
                        auto s = ScatterBegin();
                        if (s) {
                            for (uint32_t i = 0; i < scoreboard_size; i++)
                                ScatterAdd(s, scoreboard + (i * sizeof(uint64_t)), &identityPtrs[i]);
                            ScatterExecute(s);
                        }
                    }

                    // 6c: Read ALL identity IDs + name pointers in one scatter
                    std::vector<uint32_t> identityIDs(scoreboard_size, 0);
                    std::vector<uint64_t> identityNamePtrs(scoreboard_size, 0);
                    {
                        auto s = ScatterBegin();
                        if (s) {
                            for (uint32_t i = 0; i < scoreboard_size; i++) {
                                if (Game::IsValidPtr2((void*)identityPtrs[i])) {
                                    ScatterAdd(s, identityPtrs[i] + OFF_Network_Table_ID, &identityIDs[i]);
                                    ScatterAdd(s, identityPtrs[i] + OFF_PlayerName, &identityNamePtrs[i]);
                                }
                            }
                            ScatterExecute(s);
                        }
                    }
                    // (camera refreshed at top of cycle, skip mid-pass DMA)

                    // 6d: Read ALL name strings in one scatter
                    struct NameStr { char buf[64]; };
                    std::vector<NameStr> nameStrings(scoreboard_size, {});
                    {
                        auto s = ScatterBegin();
                        if (s) {
                            for (uint32_t i = 0; i < scoreboard_size; i++) {
                                if (identityNamePtrs[i])
                                    ScatterAdd(s, identityNamePtrs[i] + OFF_TEXT, nameStrings[i].buf, 64);
                            }
                            ScatterExecute(s);
                        }
                    }

                    // Build ID → name map (0 DMA, local lookup)
                    std::unordered_map<uint32_t, std::string> idToName;
                    for (uint32_t i = 0; i < scoreboard_size; i++) {
                        if (identityIDs[i] > 0) {
                            nameStrings[i].buf[63] = '\0';
                            idToName[identityIDs[i]] = nameStrings[i].buf;
                        }
                    }

                    // Assign names to entities using cached NetworkID from Pass 2
                    for (size_t i = 0; i < tmp_entities.size(); ++i) {
                        if (tmp_entities[i].entityTypeName != "dayzplayer") {
                            tmp_entities[i].playerName = "";
                            continue;
                        }
                        auto nameIt = idToName.find((uint32_t)tmp_entities[i].NetworkID);
                        if (nameIt != idToName.end())
                            tmp_entities[i].playerName = nameIt->second;
                        else
                            tmp_entities[i].playerName = "BOT";

                        cachedPlayerInfo[tmp_entities[i].EntityPtr] = { tmp_entities[i].playerName, tmp_entities[i].itemInHands };
                    }
                }

                // 6e: Batch item-in-hands reads for players (scatter-based)
                // Step 1: read inventory pointers
                std::vector<size_t> playerIndices;
                std::vector<uint64_t> inventoryPtrs;
                for (size_t i = 0; i < tmp_entities.size(); ++i) {
                    if (tmp_entities[i].entityTypeName == "dayzplayer") {
                        playerIndices.push_back(i);
                        inventoryPtrs.push_back(0);
                    }
                }
                if (!playerIndices.empty()) {
                    auto s = ScatterBegin();
                    if (s) {
                        for (size_t j = 0; j < playerIndices.size(); j++)
                            ScatterAdd(s, tmp_entities[playerIndices[j]].EntityPtr + OFF_Inventory, &inventoryPtrs[j]);
                        ScatterExecute(s);
                    }
                    // Step 2: read inhands pointers
                    std::vector<uint64_t> inhandsPtrs(playerIndices.size(), 0);
                    {
                        auto s2 = ScatterBegin();
                        if (s2) {
                            for (size_t j = 0; j < playerIndices.size(); j++) {
                                if (inventoryPtrs[j])
                                    ScatterAdd(s2, inventoryPtrs[j] + OFF_Inhands, &inhandsPtrs[j]);
                            }
                            ScatterExecute(s2);
                        }
                    }
                    // Step 3: read entity type pointers from inhands
                    std::vector<uint64_t> inhandsTypePtrs(playerIndices.size(), 0);
                    {
                        auto s3 = ScatterBegin();
                        if (s3) {
                            for (size_t j = 0; j < playerIndices.size(); j++) {
                                if (inhandsPtrs[j])
                                    ScatterAdd(s3, inhandsPtrs[j] + OFF_EntityTypePtr, &inhandsTypePtrs[j]);
                            }
                            ScatterExecute(s3);
                        }
                    }
                    // Step 4: read clean name pointers
                    std::vector<uint64_t> cleanNamePtrs(playerIndices.size(), 0);
                    {
                        auto s4 = ScatterBegin();
                        if (s4) {
                            for (size_t j = 0; j < playerIndices.size(); j++) {
                                if (inhandsTypePtrs[j])
                                    ScatterAdd(s4, inhandsTypePtrs[j] + OFF_CleanName, &cleanNamePtrs[j]);
                            }
                            ScatterExecute(s4);
                        }
                    }
                    // Step 5: read the actual item name strings
                    struct ItemNameBuf { char buf[128]; };
                    std::vector<ItemNameBuf> itemNames(playerIndices.size(), {});
                    {
                        auto s5 = ScatterBegin();
                        if (s5) {
                            for (size_t j = 0; j < playerIndices.size(); j++) {
                                if (cleanNamePtrs[j])
                                    ScatterAdd(s5, cleanNamePtrs[j] + OFF_TEXT, itemNames[j].buf, 128);
                            }
                            ScatterExecute(s5);
                        }
                    }
                    for (size_t j = 0; j < playerIndices.size(); j++) {
                        itemNames[j].buf[127] = '\0';
                        tmp_entities[playerIndices[j]].itemInHands = itemNames[j].buf;
                        cachedPlayerInfo[tmp_entities[playerIndices[j]].EntityPtr].second = itemNames[j].buf;
                    }
                }
            } else {
                // Fast tick: reuse cached names from last full scan
                for (size_t i = 0; i < tmp_entities.size(); ++i) {
                    if (tmp_entities[i].entityTypeName != "dayzplayer") {
                        tmp_entities[i].playerName = "";
                        continue;
                    }
                    auto it = cachedPlayerInfo.find(tmp_entities[i].EntityPtr);
                    if (it != cachedPlayerInfo.end()) {
                        tmp_entities[i].playerName = it->second.first;
                        tmp_entities[i].itemInHands = it->second.second;
                    }
                }
            }
        }
            markPass(fullScan ? "Pass6_FullScan" : "Pass6_FastTick");

            // === Pass 7: Cache bone world positions for all entities ===
            {
                // All bone indices needed by skeleton + box + health draws
                static const int playerBones[] = { 1, 4, 6, 7, 8, 9, 11, 14, 15, 16, 18, 21, 48, 49, 58, 61, 63, 65, 75, 94, 97, 99, 113 };
                static const int zombieBones[] = { 0, 1, 3, 6, 7, 8, 10, 13, 14, 19, 24, 27, 53, 56, 59, 60 };
                static const int playerBoneCount = sizeof(playerBones) / sizeof(playerBones[0]);
                static const int zombieBoneCount = sizeof(zombieBones) / sizeof(zombieBones[0]);

                // Read matrix_a for all entities in one scatter
                std::vector<Game::matrix4x4> matrixAs(tmp_entities.size(), {});
                {
                    auto s = ScatterBegin();
                    if (s) {
                        for (size_t i = 0; i < tmp_entities.size(); ++i) {
                            if (tmp_entities[i].visualStatePtr)
                                ScatterAdd(s, tmp_entities[i].visualStatePtr + 0x8, &matrixAs[i]);
                        }
                        ScatterExecute(s);
                    }
                }

                // Read bone vectors for all entities in one scatter
                struct BoneRead { size_t entityIdx; int boneIdx; Vector3 vec; };
                std::vector<BoneRead> boneReads;
                boneReads.reserve(tmp_entities.size() * playerBoneCount);

                {
                    auto s = ScatterBegin();
                    if (s) {
                        for (size_t i = 0; i < tmp_entities.size(); ++i) {
                            auto& e = tmp_entities[i];
                            bool isPlayer = (e.entityTypeName == "dayzplayer");
                            uint64_t mc = isPlayer ? e.playerMatrixClass : e.zmMatrixClass;
                            if (!Game::IsValidPtr2((void*)mc)) continue;

                            const int* bones = isPlayer ? playerBones : zombieBones;
                            int count = isPlayer ? playerBoneCount : zombieBoneCount;

                            for (int b = 0; b < count; ++b) {
                                BoneRead br = { i, bones[b], {} };
                                boneReads.push_back(br);
                                ScatterAdd(s, mc + OFF_Matrixb + bones[b] * sizeof(Game::matrix4x4), &boneReads.back().vec);
                            }
                        }
                        ScatterExecute(s);
                    }
                }

                // Compute world positions from matrix_a * bone vectors
                for (auto& br : boneReads) {
                    auto& ma = matrixAs[br.entityIdx];
                    auto& mb = br.vec;
                    Vector3 world;
                    world.x = (ma.m[0] * mb.x) + (ma.m[3] * mb.y) + (ma.m[6] * mb.z) + ma.m[9];
                    world.y = (ma.m[1] * mb.x) + (ma.m[4] * mb.y) + (ma.m[7] * mb.z) + ma.m[10];
                    world.z = (ma.m[2] * mb.x) + (ma.m[5] * mb.y) + (ma.m[8] * mb.z) + ma.m[11];
                    if (br.boneIdx < player_t::MAX_BONE_IDX) {
                        tmp_entities[br.entityIdx].bonePositions[br.boneIdx] = world;
                        tmp_entities[br.entityIdx].boneValid[br.boneIdx] = true;
                        tmp_entities[br.entityIdx].bonesCached = true;
                    }
                }
            }
            markPass("Pass7_Bones");

        // === Scatter-read item pointers (only on full scan) ===
        if (fullScan && itemTableSize > 0 && ItemTablePtr) {
            std::vector<uint64_t> itemPtrs(itemTableSize, 0);
            auto s = ScatterBegin();
            if (s) {
                for (uint32_t i = 0; i < itemTableSize; ++i) {
                    ScatterAdd(s, ItemTablePtr + (i * 0x8), &itemPtrs[i]);
                }
                ScatterExecute(s);
            }
            for (uint32_t i = 0; i < itemTableSize; ++i) {
                if (!itemPtrs[i]) continue;
                item_t ItemStruct{};
                ItemStruct.ItemPtr = itemPtrs[i];
                tmp_items.push_back(ItemStruct);
            }
        }

        // Single-thread: direct assignment, no mutex needed
        g_entities = std::move(tmp_entities);
        if (fullScan) {
            items = std::move(tmp_items);
        }
        markPass("Snapshot");

        // Update item cache only on full scan
        if (fullScan) {
            UpdateItemCache(globals.World);
            markPass("UpdateItemCache");
        }

        // Log timing — always log full scans, slow cycles, and every 10th cycle
        double totalCycleMs = (hpc_now() - cycleStart) * toMs;
        bool isSlow = totalCycleMs > 50.0;
        if (isSlow || fullScan || scanCycle % 10 == 0) {
            std::string prefix = isSlow ? "[ !!! STUTTER cycle " : (fullScan ? "[ FULL cycle " : "[ BG cycle ");
            std::string timingLog = prefix + std::to_string(scanCycle) + " ] TOTAL=" + std::to_string((int)totalCycleMs) + "ms | entities=" + std::to_string((int)g_entities.size()) + " | ";
            for (auto& pt : passTimes) {
                timingLog += std::string(pt.name) + "=" + std::to_string((int)pt.ms) + " ";
            }
            Log(timingLog);
        }

    }
}

float CalculateAngle(Vector3 from, Vector3 to)
{
    return atan2(to.y - from.y, to.x - from.x);
}

float CalculateDistance(Vector3 from, Vector3 to)
{
    return sqrt(pow(to.x - from.x, 2) + pow(to.y - from.y, 2));
}

uintptr_t* get_closest_Item(std::vector<uintptr_t*> list, float field_of_view)
{
    ImGuiIO& io = ImGui::GetIO();

    uintptr_t* resultant_target_entity_temp = nullptr;
    float closestTocenter = FLT_MAX;

    for (auto curr_entity : list)
    {

        Vector3 current = Game::GetItemCoordinate((uint64_t)curr_entity);
        Vector3 currentworld;

        if (!Game::WorldToScreen(current, currentworld))
            continue;

        auto dx = currentworld.x - (io.DisplaySize.x / 2);

        auto dy = currentworld.y - (io.DisplaySize.y / 2);
        auto dist = sqrtf1(dx * dx + dy * dy);

        if (dist < field_of_view && dist < closestTocenter) {
            closestTocenter = dist;
            resultant_target_entity_temp = curr_entity;
        }
    }

    return resultant_target_entity_temp;
}

void SilentAim1(uint64_t Entity)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!Entity)
        return;

    // Skip friend check during aim — too expensive per frame via DMA
    // Friends are already filtered in get_closest_player target selection

    int index = 0;
    switch (GameVars.bonechoose) {
    case 0: index = 24; break;
    case 1: index = 22; break;
    case 2: index = 19; break;
    case 3: index = 17; break;
    case 4: index = 5; break;
    case 5: index = 13; break;
    default: index = 24; break;
    }

    // Use scatter to read skeleton + visualState in one pass
    uint64_t playerSkeleton = 0, zmSkeleton = 0, visualState = 0;
    {
        auto s = ScatterBegin();
        if (!s) return;
        ScatterAdd(s, Entity + OFF_PlayerSkeleton, &playerSkeleton);
        ScatterAdd(s, Entity + OFF_ZmSkeleton, &zmSkeleton);
        ScatterAdd(s, Entity + OFF_VisualState, &visualState);
        ScatterExecute(s);
    }

    uint64_t Skeleton = playerSkeleton;
    if (!Game::IsValidPtr2((void*)Skeleton)) {
        // Zombie bone indices
        switch (GameVars.bonechoose) {
        case 0: index = 21; break;
        case 1: index = 20; break;
        case 2: index = 17; break;
        case 3: index = 15; break;
        case 4: index = 5; break;
        case 5: index = 12; break;
        default: index = 21; break;
        }
        Skeleton = zmSkeleton;
    }
    if (!Game::IsValidPtr2((void*)Skeleton) || !Game::IsValidPtr2((void*)visualState))
        return;

    // Get bone position for the target
    Vector3 current;
    Game::GetBonePosition(Skeleton, visualState, index, &current);

    Vector3 currentworld;
    Game::WorldToScreen(current, currentworld);

    ImU32 color = ImColor(GameVars.colorfovaim[0], GameVars.colorfovaim[1], GameVars.colorfovaim[2], GameVars.colorfovaim[3]);
    window->AddLine(ImVec2{ (float)io.DisplaySize.x / 2, (float)io.DisplaySize.y / 2 }, ImVec2{ currentworld.x, currentworld.y }, color, 1.5f);

    // === Scatter-read bullet table pointer + size in one pass ===
    uint64_t bulletTable = 0;
    uint32_t bulletCount = 0;
    {
        auto s = ScatterBegin();
        if (!s) return;
        ScatterAdd(s, globals.World + OFF_Bullets, &bulletTable);
        ScatterAdd(s, globals.World + OFF_Bulletsize, &bulletCount);
        ScatterExecute(s);
    }

    if (!Game::IsValidPtr2((void*)bulletTable) || bulletCount == 0 || bulletCount > 256)
        return;

    // === Scatter-read all bullet entity pointers at once ===
    std::vector<uint64_t> bulletPtrs(bulletCount, 0);
    {
        auto s = ScatterBegin();
        if (!s) return;
        for (uint32_t i = 0; i < bulletCount; i++) {
            ScatterAdd(s, bulletTable + (i * 0x8), &bulletPtrs[i]);
        }
        ScatterExecute(s);
    }

    // === Scatter-read all bullet visualState pointers at once ===
    std::vector<uint64_t> bulletVisualStates(bulletCount, 0);
    {
        auto s = ScatterBegin();
        if (!s) return;
        for (uint32_t i = 0; i < bulletCount; i++) {
            if (Game::IsValidPtr2((void*)bulletPtrs[i]))
                ScatterAdd(s, bulletPtrs[i] + OFF_VisualState, &bulletVisualStates[i]);
        }
        ScatterExecute(s);
    }

    // === Boost bullet speed via ammo type chain to bypass 50m server correction ===
    {
        uint64_t localPlayer = Game::GetLocalPlayer();
        if (localPlayer) {
            uint64_t inv = 0;
            {
                auto s = ScatterBegin();
                if (s) {
                    ScatterAdd(s, localPlayer + OFF_Inventory, &inv);
                    ScatterExecute(s);
                }
            }
            if (Game::IsValidPtr2((void*)inv)) {
                uint64_t hands = 0;
                {
                    auto s = ScatterBegin();
                    if (s) {
                        ScatterAdd(s, inv + OFF_Inhands, &hands);
                        ScatterExecute(s);
                    }
                }
                if (Game::IsValidPtr2((void*)hands)) {
                    uint64_t ammo1 = 0;
                    {
                        auto s = ScatterBegin();
                        if (s) {
                            ScatterAdd(s, hands + OFF_AMMO_TYPE1, &ammo1);
                            ScatterExecute(s);
                        }
                    }
                    if (Game::IsValidPtr2((void*)ammo1)) {
                        uint64_t ammo2 = 0;
                        {
                            auto s = ScatterBegin();
                            if (s) {
                                ScatterAdd(s, ammo1 + OFF_AMMO_TYPE2, &ammo2);
                                ScatterExecute(s);
                            }
                        }
                        if (Game::IsValidPtr2((void*)ammo2)) {
                            // Fixed 12000 m/s — covers any range up to 1000m, no per-frame distance calc needed
                            WriteData<float>(ammo2 + OFF_AMMOTYPE_INITSPEED, 12000.0f);
                        }
                    }
                }
            }
        }
    }

    // === Write all bullet positions as fast as possible ===
    for (uint32_t i = 0; i < bulletCount; i++) {
        if (!Game::IsValidPtr2((void*)bulletPtrs[i])) continue;
        if (!Game::IsValidPtr2((void*)bulletVisualStates[i])) continue;
        WriteData<Vector3>(bulletVisualStates[i] + OFF_GetCoordinate, current);
    }
}

void PushLootTP(uint64_t Entity, int ss)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!Entity)
        return;

    uint64_t localPlayer = Game::GetLocalPlayer();
    if (!localPlayer)
        return;
    Vector3 worldPosition = Game::GetCoordinate((uintptr_t)localPlayer);


    Game::SetPosition(Entity, worldPosition);
}

inline void __stdcall main_LootTp(int ss)
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!GetAsyncKeyState(GameVars.lootTPHotkey))
        GameVars.resultant_target_entity1 = nullptr;

    if (GetAsyncKeyState(GameVars.lootTPHotkey))
    {
        if (!GameVars.resultant_target_entity1)
            GameVars.resultant_target_entity1 = get_closest_Item(valid_LootTP, GameVars.tpfov);

        if (!GameVars.resultant_target_entity1)
            return;

        Vector3 worldPosition = Game::GetCoordinate((uint64_t)GameVars.resultant_target_entity1);

        int dist = Game::GetDistanceToMe(worldPosition);


        Vector3 currentworld;

        Game::WorldToScreen(worldPosition, currentworld);

        window->AddLine(ImVec2{ (float)io.DisplaySize.x / 2, (float)io.DisplaySize.y }, ImVec2{ currentworld.x, currentworld.y }, ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 0, 255)), 1.5f);

        PushLootTP((uintptr_t)GameVars.resultant_target_entity1, ss);

    }
}

inline void __stdcall main_aimboo2()
{
    auto* window = ImGui::GetOverlayDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!GetAsyncKeyState(GameVars.silentAimHotkey))
        GameVars.resultant_target_entity = nullptr;

    if (GetAsyncKeyState(GameVars.silentAimHotkey))
    {
        if (!GameVars.resultant_target_entity)
            GameVars.resultant_target_entity = get_closest_player(valid_players, GameVars.aimfov);
        if (!GameVars.resultant_target_entity)
            return;

        // SilentAim needs to do its own DMA write — that's the actual aimbot action
        SilentAim1((uintptr_t)GameVars.resultant_target_entity);
    }
}

// Zero-DMA inventory view: uses cached player name + item-in-hands from background thread
void inventoryPlayers() {
    ImGuiIO& io = ImGui::GetIO();

    if (!GameVars.inventory) return;

    uint64_t ff = (uint64_t)get_closest_player(valid_players4inventory, GameVars.invfov);
    if (!ff) return;

    // Find this entity in cached data (single-thread, direct access)
    const player_t* cached = nullptr;
    for (const auto& e : g_entities) {
        if (e.EntityPtr == (uint64_t)ff) {
            cached = &e;
            break;
        }
    }
    if (!cached || !cached->bonesCached) return;

    // Use cached bone 24 position
    if (!cached->boneValid[24]) return;
    Vector3 currentworld;
    Game::WorldToScreen(cached->bonePositions[24], currentworld);

    // Show player name + item in hands from cached data
    std::vector<std::string> item_names1;
    if (!cached->itemInHands.empty())
        item_names1.push_back(cached->itemInHands);

    if (item_names1.empty()) return;

    DrawInventoryList(item_names1, cached->playerName, { (float)io.DisplaySize.x, (float)io.DisplaySize.y }, { 19.f, 19.f, 19.f, 255.f });
}

uintptr_t* get_closest_Item1(std::vector<uintptr_t*> list, float field_of_view)
{
    ImGuiIO& io = ImGui::GetIO();

    uintptr_t* resultant_target_entity_temp = nullptr;
    float closestTocenter = FLT_MAX;

    for (auto curr_entity : list)
    {
        Vector3 currentworld;
        Vector3 Pos = Game::GetObjectVisualState((uintptr_t)curr_entity);

        Game::WorldToScreen(Pos, currentworld);
        auto dx = currentworld.x - (io.DisplaySize.x / 2);
        auto dy = currentworld.y - (io.DisplaySize.y / 2);
        auto dist = sqrtf1(dx * dx + dy * dy);

        if (dist < field_of_view && dist < closestTocenter) {
            closestTocenter = dist;
            resultant_target_entity_temp = curr_entity;
        }


    }

    return resultant_target_entity_temp;
}

void get_quality(uint64_t Entity, ImVec2 circlePosition, float circleRadius, ImDrawList* window)
{
    auto quality = ReadData<int>(Entity + OFF_Quality);
    ImU32 qualityColor;

    if (quality == 0) {
        qualityColor = IM_COL32(0, 255, 0, 235);  // Verde Pristine 
    }
    else if (quality == 1) {
        qualityColor = IM_COL32(127, 204, 34, 255);  // Amarelo Worn 
    }
    else if (quality == 2) {
        qualityColor = IM_COL32(255, 255, 0, 235);  // Amarelo Damaged 
    }
    else if (quality == 3) {
        qualityColor = IM_COL32(255, 165, 0, 235);  // Laranja Badly 
    }
    else if (quality == 4) {
        qualityColor = IM_COL32(255, 0, 0, 235);  // Vermelho Ruined 
    }
    else {
        qualityColor = IM_COL32(128, 128, 128, 235);  // Cinza
    }

    window->AddCircleFilled(circlePosition, circleRadius, qualityColor);
}

// === Cached item data for flicker-free rendering ===
struct CachedItemData {
    uintptr_t entity;
    Vector3 worldPos;
    std::string text;   // name from cleanNamePtr
    std::string text2;  // name from cleanName2Ptr (category)
    std::string text3;  // name from cleanName5Ptr (subcategory)
    int quality;
};
static std::vector<CachedItemData> cachedItemsForDraw;

// Fetch item data via DMA (called on a throttled timer)
void UpdateItemCache(uintptr_t pUWorld)
{
    // Read item table pointer + size in one scatter (avoid sequential DMA)
    int objectTableSz = 0;
    uintptr_t entityTable = 0;
    {
        auto s = ScatterBegin();
        if (s) {
            ScatterAdd(s, pUWorld + OFF_ItemTable + 0x8, &objectTableSz);
            ScatterAdd(s, pUWorld + OFF_ItemTable, &entityTable);
            ScatterExecute(s);
        }
    }
    if (!Game::IsValidPtr2((void*)entityTable) || objectTableSz <= 0 || objectTableSz > 2048)
        return;

    // === Scatter Pass 1: Read all item entries (flag + entity ptr) in one batch ===
    struct ItemEntry { uint32_t flag; uint32_t pad; uint64_t entityPtr; };
    std::vector<ItemEntry> rawEntries(objectTableSz, {});
    {
        auto s = ScatterBegin();
        if (s) {
            for (int i = 0; i < objectTableSz; i++) {
                ScatterAdd(s, entityTable + i * 0x18, &rawEntries[i].flag, 4);
                ScatterAdd(s, entityTable + i * 0x18 + 0x8, &rawEntries[i].entityPtr, 8);
            }
            ScatterExecute(s);
        }
    }

    // Filter valid items
    struct ValidItem { uintptr_t entity; };
    std::vector<ValidItem> validItems;
    validItems.reserve(objectTableSz);
    for (int i = 0; i < objectTableSz; i++) {
        if (rawEntries[i].flag != 1) continue;
        if (!Game::IsValidPtr2((void*)rawEntries[i].entityPtr)) continue;
        validItems.push_back({ rawEntries[i].entityPtr });
    }

    if (validItems.empty()) {
        cachedItemsForDraw.clear();
        return;
    }

    // === Scatter Pass 2: Read visualState + objectBase for all valid items ===
    std::vector<uint64_t> visualStatePtrs(validItems.size(), 0);
    std::vector<uint64_t> objectBasePtrs(validItems.size(), 0);
    {
        auto s = ScatterBegin();
        if (s) {
            for (size_t i = 0; i < validItems.size(); i++) {
                ScatterAdd(s, validItems[i].entity + OFF_VisualState, &visualStatePtrs[i]);
                ScatterAdd(s, validItems[i].entity + OFF_ObjectBase, &objectBasePtrs[i]);
            }
            ScatterExecute(s);
        }
    }

    // === Scatter Pass 3: Read coordinates from visual states + name ptrs from objectBase ===
    struct ItemCoord { float x, y, z; };
    std::vector<ItemCoord> itemCoords(validItems.size(), {});
    std::vector<uint64_t> cleanNamePtrs(validItems.size(), 0);
    std::vector<uint64_t> cleanName2Ptrs(validItems.size(), 0);
    std::vector<uint64_t> cleanName5Ptrs(validItems.size(), 0);
    std::vector<int> itemQualities(validItems.size(), -1);
    {
        auto s = ScatterBegin();
        if (s) {
            for (size_t i = 0; i < validItems.size(); i++) {
                if (visualStatePtrs[i])
                    ScatterAdd(s, visualStatePtrs[i] + OFF_GetCoordinate, &itemCoords[i]);
                if (Game::IsValidPtr2((void*)objectBasePtrs[i])) {
                    ScatterAdd(s, objectBasePtrs[i] + OFF_CleanName, &cleanNamePtrs[i]);
                    ScatterAdd(s, objectBasePtrs[i] + OFF_CleanName2, &cleanName2Ptrs[i]);
                    ScatterAdd(s, objectBasePtrs[i] + OFF_CleanName5, &cleanName5Ptrs[i]);
                }
                ScatterAdd(s, validItems[i].entity + OFF_Quality, &itemQualities[i]);
            }
            ScatterExecute(s);
        }
    }

    // === Scatter Pass 4: Bulk-read all item name strings (replaces sequential getNameFromId) ===
    // Each item has 3 name pointers. Read name data (up to 256 bytes) from each in one scatter.
    struct ItemNameData { char buf[256]; };
    std::vector<ItemNameData> nameData1(validItems.size(), {}); // from cleanNamePtr
    std::vector<ItemNameData> nameData2(validItems.size(), {}); // from cleanName2Ptr
    std::vector<ItemNameData> nameData3(validItems.size(), {}); // from cleanName5Ptr
    {
        auto s = ScatterBegin();
        if (s) {
            for (size_t i = 0; i < validItems.size(); i++) {
                if (Game::IsValidPtr2((void*)cleanNamePtrs[i]))
                    ScatterAdd(s, cleanNamePtrs[i] + OFF_TEXT, nameData1[i].buf, 256);
                if (Game::IsValidPtr2((void*)cleanName2Ptrs[i]))
                    ScatterAdd(s, cleanName2Ptrs[i] + OFF_TEXT, nameData2[i].buf, 256);
                if (Game::IsValidPtr2((void*)cleanName5Ptrs[i]))
                    ScatterAdd(s, cleanName5Ptrs[i] + OFF_TEXT, nameData3[i].buf, 256);
            }
            ScatterExecute(s);
        }
    }

    // Build cached item list from scatter results (0 DMA)
    std::vector<CachedItemData> newCache;
    newCache.reserve(validItems.size());

    for (size_t i = 0; i < validItems.size(); i++)
    {
        // Skip items with no valid visual state (would have 0,0,0 coordinates)
        if (!visualStatePtrs[i]) continue;
        if (itemCoords[i].x == 0.0f && itemCoords[i].y == 0.0f && itemCoords[i].z == 0.0f) continue;

        if (!Game::IsValidPtr2((void*)cleanNamePtrs[i])) continue;
        if (!Game::IsValidPtr2((void*)cleanName5Ptrs[i])) continue;
        if (!Game::IsValidPtr2((void*)cleanName2Ptrs[i])) continue;

        nameData1[i].buf[255] = '\0';
        nameData2[i].buf[255] = '\0';
        nameData3[i].buf[255] = '\0';

        std::string text(nameData1[i].buf);
        std::string text2(nameData2[i].buf);
        std::string text3(nameData3[i].buf);

        // Filter out unwanted entity types (same logic as getNameFromId)
        if (text.find("Animal") != std::string::npos || text.find("Zmb") != std::string::npos ||
            text.find("Firewood") != std::string::npos || text.find("Barrel") != std::string::npos ||
            text.find("Watchtower") != std::string::npos || text.find("Wood Pillar") != std::string::npos ||
            text.find("Roof") != std::string::npos || text.find("Wall") != std::string::npos ||
            text.find("Floor") != std::string::npos || text.find("Fireplace") != std::string::npos ||
            text.find("Wire Mesh Barrier") != std::string::npos || text.find("Fence") != std::string::npos)
            continue;

        if (text2.empty() || std::all_of(text2.begin(), text2.end(), [](unsigned char c){ return std::isspace(c); })) continue;
        if (text3.empty() || std::all_of(text3.begin(), text3.end(), [](unsigned char c){ return std::isspace(c); })) continue;

        CachedItemData item;
        item.entity = validItems[i].entity;
        item.worldPos = Vector3(itemCoords[i].x, itemCoords[i].y, itemCoords[i].z);
        item.text = std::move(text);
        item.text2 = std::move(text2);
        item.text3 = std::move(text3);
        item.quality = itemQualities[i];
        newCache.push_back(std::move(item));
    }

    cachedItemsForDraw = std::move(newCache);
}

// Draw items from cache (called every frame, no DMA)
void DrawCachedItems()
{
    auto* window = ImGui::GetOverlayDrawList();

    // Single-thread: read directly, no copy needed
    const auto& localCache = cachedItemsForDraw;

    float spacing = 20.0f;
    std::map<std::pair<int, int>, bool> textPositionsitems;

    for (size_t i = 0; i < localCache.size(); i++)
    {
        const auto& ci = localCache[i];

        Vector3 outPos;
        if (!Game::WorldToScreen(ci.worldPos, outPos))
            continue;

        ImVec2 textPosition(outPos.x, outPos.y);
        while (textPositionsitems[std::make_pair((int)textPosition.x, (int)textPosition.y)]) {
            textPosition.y += spacing;
        }
        textPositionsitems[std::make_pair((int)textPosition.x, (int)textPosition.y)] = true;

        int dist = Game::GetDistanceToMe(ci.worldPos);
        if (dist < 1) continue;
        if (dist > GameVars.espDistanceitems) continue;

        if (GameVars.itemlistm) {
            GameVars.itemList.push_back(ci.text);
            GameVars.itemDistances.push_back(dist);
        }

        auto get = ((" [ ") + std::to_string(dist) + (" ]"));
        auto get2 = ci.text + get;

        ImVec2 circlePosition(textPosition.x - 12, textPosition.y + 7);
        float circleRadius = 3.0f;

        int itemQuality = ci.quality;
        bool showItem = false;

        if (itemQuality == 0 && GameVars.sPristine) showItem = true;
        else if (itemQuality == 1 && GameVars.sWorn) showItem = true;
        else if (itemQuality == 2 && GameVars.sDamaged) showItem = true;
        else if (itemQuality == 3 && GameVars.sBadly) showItem = true;
        else if (itemQuality == 4 && GameVars.sRuined) showItem = true;

        if (!showItem) continue;

        if (GameVars.itemsESP) {

            valid_LootTP.push_back((uintptr_t*)ci.entity);

            // Helper lambda to draw quality circle
            auto drawQuality = [&]() {
                if (GameVars.qality) {
                    // Use cached quality to draw color circle (no DMA)
                    ImU32 qualityColor;
                    if (itemQuality == 0) qualityColor = IM_COL32(0, 255, 0, 235);
                    else if (itemQuality == 1) qualityColor = IM_COL32(127, 204, 34, 255);
                    else if (itemQuality == 2) qualityColor = IM_COL32(255, 255, 0, 235);
                    else if (itemQuality == 3) qualityColor = IM_COL32(255, 165, 0, 235);
                    else if (itemQuality == 4) qualityColor = IM_COL32(255, 0, 0, 235);
                    else qualityColor = IM_COL32(128, 128, 128, 235);
                    window->AddCircleFilled(circlePosition, circleRadius, qualityColor);
                }
            };

            if (GameVars.weapon) {
                if (ci.text2.find(xorstr_("Weapon")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorweapon2[0], GameVars.colorweapon2[1], GameVars.colorweapon2[2], GameVars.colorweapon2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(ImVec2((int)textPosition.x, (int)textPosition.y), color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.BackPack) {
                if (ci.text3.find(xorstr_("backpacks")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorbackpack2[0], GameVars.colorbackpack2[1], GameVars.colorbackpack2[2], GameVars.colorbackpack2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.Containers) {
                if (ci.text3.find(xorstr_("containers")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorcont2[0], GameVars.colorcont2[1], GameVars.colorcont2[2], GameVars.colorcont2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
                if (ci.text3.find(xorstr_("cooking")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorcont2[0], GameVars.colorcont2[1], GameVars.colorcont2[2], GameVars.colorcont2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.berrel) {
                if (ci.text3.find(xorstr_("itembarrel")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorcont2[0], GameVars.colorcont2[1], GameVars.colorcont2[2], GameVars.colorcont2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                }
            }
            if (GameVars.comida) {
                if (ci.text3.find(xorstr_("food")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorfood2[0], GameVars.colorfood2[1], GameVars.colorfood2[2], GameVars.colorfood2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.medicine) {
                if (ci.text3.find(xorstr_("medical")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colormedcine2[0], GameVars.colormedcine2[1], GameVars.colormedcine2[2], GameVars.colormedcine2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }

            if (GameVars.drinks) {
                if (ci.text3.find(xorstr_("drinks")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colordrnks2[0], GameVars.colordrnks2[1], GameVars.colordrnks2[2], GameVars.colordrnks2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.clothing) {
                if (ci.text2.find(xorstr_("clothing")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorcloting2[0], GameVars.colorcloting2[1], GameVars.colorcloting2[2], GameVars.colorcloting2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.inventoryItem) {
                if (ci.text.find(xorstr_("Cerca")) != std::string::npos) continue;
                if (ci.text3.find(xorstr_("food")) != std::string::npos) continue;
                if (ci.text3.find(xorstr_("medical")) != std::string::npos) continue;
                if (ci.text3.find(xorstr_("drinks")) != std::string::npos) continue;
                if (ci.text3.find(xorstr_("Fogueira")) != std::string::npos) continue;

                if (ci.text2.find(xorstr_("inventoryItem")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.colorothers2[0], GameVars.colorothers2[1], GameVars.colorothers2[2], GameVars.colorothers2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.ProxyMagazines) {
                if (ci.text2.find(xorstr_("ProxyMagazines")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.coloriMagazines2[0], GameVars.coloriMagazines2[1], GameVars.coloriMagazines2[2], GameVars.coloriMagazines2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.Ammunation) {
                if (ci.text3.find(xorstr_("ammunition")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.coloriAmmu2[0], GameVars.coloriAmmu2[1], GameVars.coloriAmmu2[2], GameVars.coloriAmmu2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.Miras) {
                if (ci.text2.find(xorstr_("itemoptics")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.coloriScoop2[0], GameVars.coloriScoop2[1], GameVars.coloriScoop2[2], GameVars.coloriScoop2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.rodas) {
                if (ci.text2.find(xorstr_("carwheel")) != std::string::npos) {
                    ImU32 color = ImColor(GameVars.corrodas2[0], GameVars.corrodas2[1], GameVars.corrodas2[2], GameVars.corrodas2[3]);
                    ImGui::PushFont(GameVars.smallFont);
                    window->AddText(textPosition, color, get2.c_str());
                    ImGui::PopFont();
                    drawQuality();
                }
            }
            if (GameVars.inventoryItem12) {
                ImU32 color = ImColor(GameVars.coloriMagazines2[0], GameVars.coloriMagazines2[1], GameVars.coloriMagazines2[2], GameVars.coloriMagazines2[3]);
                ImGui::PushFont(GameVars.smallFont);
                window->AddText(textPosition, color, get2.c_str());
                ImGui::PopFont();
                drawQuality();
            }
        }
        if (GameVars.Filtrarlloot) {
            bool itemMatchesFilter = false;
            for (const auto& filter : GameVars.lootFilters) {
                if (!filter.empty() && get2.find(filter) != std::string::npos) {
                    itemMatchesFilter = true;
                    break;
                }
            }

            if (itemMatchesFilter) {
                ImU32 color = ImColor(GameVars.coloriMagazines2[0], GameVars.coloriMagazines2[1], GameVars.coloriMagazines2[2], GameVars.coloriMagazines2[3]);
                ImGui::PushFont(GameVars.smallFont);
                window->AddText(textPosition, color, get2.c_str());
                ImGui::PopFont();
                if (GameVars.qality) {
                    ImU32 qualityColor;
                    if (itemQuality == 0) qualityColor = IM_COL32(0, 255, 0, 235);
                    else if (itemQuality == 1) qualityColor = IM_COL32(127, 204, 34, 255);
                    else if (itemQuality == 2) qualityColor = IM_COL32(255, 255, 0, 235);
                    else if (itemQuality == 3) qualityColor = IM_COL32(255, 165, 0, 235);
                    else if (itemQuality == 4) qualityColor = IM_COL32(255, 0, 0, 235);
                    else qualityColor = IM_COL32(128, 128, 128, 235);
                    window->AddCircleFilled(circlePosition, circleRadius, qualityColor);
                }
            }
        }
    }
}

std::vector<uintptr_t> corpses;

bool IsCorpse(uintptr_t entity) {
    bool isDead = Game::is_dead(entity);
    std::string quality = Game::GetQuality(entity);
    return isDead && quality == "Quality(Ruined)";
}

void UpdateCorpses(uintptr_t entity, bool isDead) {
    // Use cached isDead from scatter read to avoid extra DMA
    if (!isDead) return;
    if (std::find(corpses.begin(), corpses.end(), entity) != corpses.end()) return;
    // Only do the quality check (1 DMA read) if actually dead
    std::string quality = Game::GetQuality(entity);
    if (quality == "Quality(Ruined)") {
        corpses.push_back(entity);
    }
}

void DrawCorpses() {
    uint64_t localPlayer = Game::cachedLocalPlayer;
    if (!localPlayer || !GameVars.deadESP)
        return;

    // Limit corpse processing to avoid DMA flooding
    int maxCorpses = 20;

    ImGuiIO& io = ImGui::GetIO();
    auto* window = ImGui::GetOverlayDrawList();

    int corpseCount = 0;
    for (auto it = corpses.begin(); it != corpses.end(); ) {
        if (corpseCount >= maxCorpses) break;
        const auto& corpse = *it;

        if (!Game::is_dead(corpse)) {
            it = corpses.erase(it);
            continue;
        }

        Vector3 worldPosition = Game::GetCoordinate(corpse);
        Vector3 screenPosition;
        if (!Game::WorldToScreen(worldPosition, screenPosition)) {
            ++it;
            continue;
        }

        float distanceToCenter = std::sqrt(
            std::pow(screenPosition.x - io.DisplaySize.x / 2, 2) +
            std::pow(screenPosition.y - io.DisplaySize.y / 2, 2)
        );

        // Build temporary player_t with cached pointer chain for corpse draws
        player_t corpseEnt = {};
        corpseEnt.EntityPtr = corpse;
        corpseEnt.visualStatePtr = ReadData<uint64_t>(corpse + OFF_VisualState);
        corpseEnt.playerSkeletonPtr = ReadData<uint64_t>(corpse + OFF_PlayerSkeleton);
        if (corpseEnt.playerSkeletonPtr) {
            corpseEnt.playerAnimClass = ReadData<uint64_t>(corpseEnt.playerSkeletonPtr + OFF_AnimClass);
            if (corpseEnt.playerAnimClass)
                corpseEnt.playerMatrixClass = ReadData<uint64_t>(corpseEnt.playerAnimClass + OFF_MatrixClass);
        }

        // Compute bone world positions for corpse (DMA but capped at 20 corpses)
        if (corpseEnt.visualStatePtr && corpseEnt.playerMatrixClass) {
            static const int corpseBones[] = { 1, 4, 6, 7, 8, 9, 11, 14, 15, 16, 18, 21, 48, 49, 58, 61, 63, 65, 75, 94, 97, 99, 113 };
            Game::matrix4x4 ma = ReadData<Game::matrix4x4>(corpseEnt.visualStatePtr + 0x8);
            for (int b : corpseBones) {
                Vector3 mb = ReadData<Vector3>(corpseEnt.playerMatrixClass + OFF_Matrixb + b * sizeof(Game::matrix4x4));
                Vector3 world;
                world.x = (ma.m[0] * mb.x) + (ma.m[3] * mb.y) + (ma.m[6] * mb.z) + ma.m[9];
                world.y = (ma.m[1] * mb.x) + (ma.m[4] * mb.y) + (ma.m[7] * mb.z) + ma.m[10];
                world.z = (ma.m[2] * mb.x) + (ma.m[5] * mb.y) + (ma.m[8] * mb.z) + ma.m[11];
                corpseEnt.bonePositions[b] = world;
                corpseEnt.boneValid[b] = true;
            }
            corpseEnt.bonesCached = true;
        }

        if (GameVars.skeletonESP) {
            ImVec4 skeletonColorVec4 = ImVec4(0, 0, 0, 255);
            DrawSkeleton(corpseEnt, skeletonColorVec4, 1);
        }

        if (GameVars.boxESP) {
            ImVec4 boxColorVec4 = ImVec4(0, 0, 0, 255);
            DrawDynamicBox(corpseEnt, boxColorVec4);
        }

        if (GameVars.CorpseTeleport) {

            if (distanceToCenter > GameVars.tpfov) {
                ++it;
                continue;
            }

            if (GameVars.deadESP && GetAsyncKeyState(GameVars.lootTPHotkey)) {
                PushLootTP(corpse, localPlayer);
                break;
            }
        }

        corpseCount++;
        ++it;
    }
}

void draw_esp()
{
    // === Frame timing profiler — logs any section taking >50ms ===
    static LARGE_INTEGER perfFreq = {};
    if (!perfFreq.QuadPart) QueryPerformanceFrequency(&perfFreq);
    auto now_hpc = []() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; };
    double toMs = 1000.0 / perfFreq.QuadPart;

    auto frameStart = now_hpc();
    auto sectionStart = frameStart;

    auto checkSection = [&](const char* name) {
        auto elapsed = now_hpc() - sectionStart;
        double ms = elapsed * toMs;
        if (ms > 16.0) {
            Log("[ PERF ] " + std::string(name) + " took " + std::to_string((int)ms) + "ms");
        }
        sectionStart = now_hpc();
    };

    // Single-thread: update all entity data inline before rendering
    update_entities();
    checkSection("UpdateEntities");

    if (GameVars.itemsESP) {
        DrawCachedItems();
    }
    checkSection("DrawCachedItems");
    uint64_t localPlayer = Game::cachedLocalPlayer;

    for (const auto& Entities : g_entities)
    {
        if (!GameVars.inlocal) {
            if (Entities.EntityPtr == localPlayer)
                continue;
        }

        // Use cached data from update_list scatter reads (0 DMA reads here)
        if (!Entities.dataCached) continue; // Skip uncached entities — never do DMA on render thread
        Vector3 worldPosition(Entities.coordX, Entities.coordY, Entities.coordZ);
        Vector3 screenPosition;

        if (!Game::WorldToScreen(worldPosition, screenPosition))
            continue;

        int distance = Game::GetDistanceToMe(worldPosition);

        // Use cached entity names (0 DMA reads here)
        const std::string& entity = Entities.entityTypeName;
        const std::string& entityRealName = Entities.entityRealName;

        valid_players.push_back((uintptr_t*)Entities.EntityPtr);

        if (GameVars.inventory) {
            valid_players4inventory.push_back((uintptr_t*)Entities.EntityPtr);
        }

        // Use cached player name from update_list (0 DMA reads)
        const std::string& playerName = Entities.playerName;
        std::string playerNameList = playerName;

        if (GameVars.playerlistm) {
            GameVars.playerList.push_back(playerNameList);
            GameVars.playerDistances.push_back(distance);
        }

        ImU32 color = ImColor(GameVars.EspCarColorArray[0], GameVars.EspCarColorArray[1], GameVars.EspCarColorArray[2], GameVars.EspCarColorArray[3]);
        //DrawStringWithBackGround(globals.Width / 2, globals.Height / 2 - 500, &Col.glassblack, color, Game::GetNetworkClientServerName().c_str());
        //DrawStringWithBackGround(globals.Width / 2, globals.Height / 2 - 500, &Col.glassblack, color, Game::GetPlayerCountString().c_str());

        bool entityIsDead = (Entities.isDead == 1);

        // Dead entities: draw corpse skeleton/box using cached data (0 DMA), then skip normal ESP
        if (entityIsDead) {
            if (GameVars.deadESP && entity == xorstr_("dayzplayer") && Entities.bonesCached) {
                if (GameVars.skeletonESP) {
                    ImVec4 corpseColor = ImVec4(0, 0, 0, 255);
                    DrawSkeleton(Entities, corpseColor, 1);
                }
                if (GameVars.boxESP) {
                    ImVec4 corpseBoxColor = ImVec4(0, 0, 0, 255);
                    DrawDynamicBox(Entities, corpseBoxColor);
                }
                if (GameVars.CorpseTeleport) {
                    ImGuiIO& tpIo = ImGui::GetIO();
                    float dx = screenPosition.x - tpIo.DisplaySize.x / 2;
                    float dy = screenPosition.y - tpIo.DisplaySize.y / 2;
                    float distToCenter = std::sqrt(dx * dx + dy * dy);
                    if (distToCenter <= GameVars.tpfov && GetAsyncKeyState(GameVars.lootTPHotkey)) {
                        PushLootTP(Entities.EntityPtr, localPlayer);
                    }
                }
            }
            continue;
        }

        if ((entity == xorstr_("dayzplayer")) && distance <= GameVars.espDistance && GameVars.playerESP) {
            // Use already-cached playerName from above (0 DMA reads)
            bool isFriend = GameVars.friendsSet.find(playerName) != GameVars.friendsSet.end();

            if (!isFriend) {
                if (GameVars.skeletonESP) {
                    ImVec4 skeletonColorVec4 = ImVec4(GameVars.skeletonColorArray[0], GameVars.skeletonColorArray[1], GameVars.skeletonColorArray[2], GameVars.skeletonColorArray[3]);
                    DrawSkeleton(Entities, skeletonColorVec4, 1);
                }
                if (GameVars.boxESP) {
                    ImVec4 boxColorVec4 = ImVec4(GameVars.boxColorArray[0], GameVars.boxColorArray[1], GameVars.boxColorArray[2], GameVars.boxColorArray[3]);
                    if (GameVars.selectedBoxType == 0) {
                        DrawDynamicBox(Entities, boxColorVec4);
                    }
                    else if (GameVars.selectedBoxType == 1) {
                        DrawCornerBox(Entities, boxColorVec4);
                    }
                }
            }
            else {
                if (GameVars.skeletonESP) {
                    ImVec4 skeletonFriendColorVec4 = ImVec4(GameVars.skeletonFrinColorArray[0], GameVars.skeletonFrinColorArray[1], GameVars.skeletonFrinColorArray[2], GameVars.skeletonFrinColorArray[3]);
                    DrawSkeleton(Entities, skeletonFriendColorVec4, 1);
                }
                if (GameVars.boxESP) {
                    ImVec4 boxFriendColorVec4 = ImVec4(GameVars.boxFrinColorArray[0], GameVars.boxFrinColorArray[1], GameVars.boxFrinColorArray[2], GameVars.boxFrinColorArray[3]);
                    if (GameVars.selectedBoxType == 0) {
                        DrawDynamicBox(Entities, boxFriendColorVec4);
                    }
                    else if (GameVars.selectedBoxType == 1) {
                        DrawCornerBox(Entities, boxFriendColorVec4);
                    }
                }
            }

            std::string espText = "";

            ImVec2 textSize = ImGui::CalcTextSize(espText.c_str());
            float centeredXPosition = screenPosition.x - (textSize.x / 2.0f);

            if (GameVars.inhands) {
                ImU32 color = ImColor(GameVars.EspNameColorArray[0], GameVars.EspNameColorArray[1], GameVars.EspNameColorArray[2], GameVars.EspNameColorArray[3]);
                ImU32 outlineColor = ImColor(0, 0, 0, 255); // Preto para o contorno.
                auto* window = ImGui::GetOverlayDrawList();

                ImFont* font = ImGui::GetFont();
                float fontSize = ImGui::GetFontSize();

                const std::string& itemInHands = Entities.itemInHands;
                ImVec2 textPos = ImVec2(centeredXPosition - 5, screenPosition.y + 25);

                float outlineThickness = 1.0f;
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - outlineThickness), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + outlineThickness), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y - outlineThickness), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y + outlineThickness), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y + outlineThickness), outlineColor, itemInHands.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y - outlineThickness), outlineColor, itemInHands.c_str());

                window->AddText(font, fontSize, textPos, color, itemInHands.c_str());
            }

            if (GameVars.healESP) {
                DrawHealth(Entities);
            }

            if (GameVars.distanceESP) {
                espText += "[" + std::to_string(distance) + "m]";
            }
            if (GameVars.nameESP) {
                if (!espText.empty()) {
                    espText += " - ";
                }
                espText += playerName;
            }

            ImU32 color = ImColor(GameVars.EspNameColorArray[0], GameVars.EspNameColorArray[1], GameVars.EspNameColorArray[2], GameVars.EspNameColorArray[3]);
            ImU32 outlineColor = ImColor(0, 0, 0, 255);
            auto* window = ImGui::GetOverlayDrawList();

            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize();

            ImVec2 textPos = ImVec2(centeredXPosition - 5, screenPosition.y + 5);

            float outlineThickness = 1.0f;
            window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - outlineThickness), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + outlineThickness), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y - outlineThickness), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y + outlineThickness), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y + outlineThickness), outlineColor, espText.c_str());
            window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y - outlineThickness), outlineColor, espText.c_str());

            window->AddText(font, fontSize, textPos, color, espText.c_str());

            if (GameVars.lineESP) {
                ImVec4 linesColorVec4 = ImVec4(GameVars.linesColorArray[0], GameVars.linesColorArray[1], GameVars.linesColorArray[2], GameVars.linesColorArray[3]);
                // Use cached bone position (0 DMA)
                if (Entities.bonesCached && Entities.boneValid[8])
                    DrawLineESPFromCachedPos(linesColorVec4, Entities.bonePositions[8], GameVars.linep);
            }

            if (GameVars.idbones) {
                DrawBoneIDs(Entities.EntityPtr, ImColor(126, 255, 176, 255), 1);
            }
        }

        else if (entity == xorstr_("dayzinfected") && distance <= GameVars.espzDistance) {
            if (GameVars.infectedESP) {
                int textOffsetY = screenPosition.y + 5;
                std::string espText = "";

                if (GameVars.distancezESP) {
                    espText += "[" + std::to_string(distance) + "m]";
                }

                if (GameVars.namezESP) {
                    if (!espText.empty()) {
                        espText += " - ";
                    }
                    espText += "Zombie";
                }

                ImVec2 textSize = ImGui::CalcTextSize(espText.c_str());
                float centeredXPosition = screenPosition.x - (textSize.x / 2.0f);

                ImU32 color = ImColor(GameVars.EspNamezColorArray[0], GameVars.EspNamezColorArray[1], GameVars.EspNamezColorArray[2], GameVars.EspNamezColorArray[3]);
                ImU32 outlineColor = ImColor(0, 0, 0, 255);
                auto* window = ImGui::GetOverlayDrawList();

                ImFont* font = ImGui::GetFont();
                float fontSize = ImGui::GetFontSize();

                ImVec2 textPos = ImVec2(centeredXPosition, textOffsetY);

                float outlineThickness = 1.0f;
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - outlineThickness), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + outlineThickness), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y - outlineThickness), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y + outlineThickness), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x - outlineThickness, textPos.y + outlineThickness), outlineColor, espText.c_str());
                window->AddText(font, fontSize, ImVec2(textPos.x + outlineThickness, textPos.y - outlineThickness), outlineColor, espText.c_str());

                window->AddText(font, fontSize, textPos, color, espText.c_str());

                if (GameVars.lineZESP) {
                    ImVec4 linesColorZVec4 = ImVec4(GameVars.linesColorZArray[0], GameVars.linesColorZArray[1], GameVars.linesColorZArray[2], GameVars.linesColorZArray[3]);
                    // Use cached bone position (0 DMA)
                    if (Entities.bonesCached && Entities.boneValid[8])
                        DrawLineESPFromCachedPos(linesColorZVec4, Entities.bonePositions[8], GameVars.linez);
                }

                if (GameVars.skeletonzESP) {
                    ImVec4 skeletonzColorVec4 = ImVec4(GameVars.skeletonzColorArray[0], GameVars.skeletonzColorArray[1], GameVars.skeletonzColorArray[2], GameVars.skeletonzColorArray[3]);
                    DrawSkeletonZ(Entities, skeletonzColorVec4, 1);
                }

                if (GameVars.idbonesZ) {
                    DrawBoneIDsZ(Entities.EntityPtr, ImColor(126, 255, 176, 255), 1);
                }
            }
        }

        else if (entity == xorstr_("car") && distance <= GameVars.espDistancecars) {
            if (GameVars.carsESP) {
                ImU32 color = ImColor(GameVars.EspCarColorArray[0], GameVars.EspCarColorArray[1], GameVars.EspCarColorArray[2], GameVars.EspCarColorArray[3]);
                DrawPlayerText(screenPosition.x, screenPosition.y + 25, color, ("[" + std::to_string(distance) + "m] - " + entityRealName).c_str());
                //DrawStringWithBackGround(globals.Width / 2, globals.Height / 2 - 500, &Col.glassblack, &Col.gray, "Cars nearby!");
            }
        }

        else if (entity == xorstr_("dayzanimal") && distance <= GameVars.espDistanceanimals) {
            if (GameVars.animalsESP) {
                ImU32 color = ImColor(GameVars.EspAnimalColorArray[0], GameVars.EspAnimalColorArray[1], GameVars.EspAnimalColorArray[2], GameVars.EspAnimalColorArray[3]);
                DrawPlayerText(screenPosition.x, screenPosition.y + 25, color, ("[" + std::to_string(distance) + "m] - " + entityRealName).c_str());
            }
        }
    }
    checkSection("EntityLoop");

    // Log total frame time if it exceeds 50ms
    {
        double totalMs = (now_hpc() - frameStart) * toMs;
        if (totalMs > 16.0) {
            Log("[ PERF ] TOTAL draw_esp frame: " + std::to_string((int)totalMs) + "ms");
        }
    }



    if (GameVars.citiesESP) {

        Vector3 chernogorsk_world_position = { 6700, 20, 2700 };
        Vector3 chernogorsk_screen_position;

        Vector3 electrozavodsk_world_position = { 10300, 10, 2300 };
        Vector3 electrozavodsk_screen_position;

        Vector3 balota_world_position = { 4450, 15, 2300 };
        Vector3 balota_screen_position;

        Vector3 berezino_down_world_position = { 12972, 6, 10058 }; // Parte baixa de Berezino
        Vector3 berezino_screen_position;

        Vector3 solnechniy_world_position = { 13453, 7, 6239 };
        Vector3 solnechniy_screen_position;

        Vector3 kamyshovo_world_position = { 12010, 7, 3486 };
        Vector3 kamyshovo_screen_position;

        Vector3 prigorodki_world_position = { 7473, 7, 2828 };
        Vector3 prigorodki_screen_position;

        Vector3 svetloyarsk_world_position = { 13972, 7, 13263 };
        Vector3 svetloyarsk_screen_position;

        Vector3 krasnostav_world_position = { 11209, 20, 12332 };
        Vector3 krasnostav_screen_position;

        Vector3 novodmitrovsk_world_position = { 11363, 20, 14311 };
        Vector3 novodmitrovsk_screen_position;

        Vector3 airfield_vybor_world_position = { 4617, 340, 10438 };
        Vector3 airfield_vybor_screen_position;

        Vector3 novaya_petrovka_world_position = { 3405, 138, 13078 };
        Vector3 novaya_petrovka_screen_position;

        Vector3 severograd_world_position = { 8099, 137, 12539 };
        Vector3 severograd_screen_position;

        Vector3 gorka_world_position = { 9814, 9, 8947 };
        Vector3 gorka_screen_position;

        Vector3 tulga_world_position = { 12358, 7, 8759 };
        Vector3 tulga_screen_position;

        Vector3 zelenogorsk_world_position = { 2775, 120, 5185 };
        Vector3 zelenogorsk_screen_position;

        Vector3 stary_sobor_world_position = { 6018, 309, 7779 };
        Vector3 stary_sobor_screen_position;

        Vector3 novy_sobor_world_position = { 7126, 299, 7799 };
        Vector3 novy_sobor_screen_position;

        Vector3 vybor_world_position = { 3790, 220, 8912 };
        Vector3 vybor_screen_position;

        Vector3 tisy_world_position = { 3037, 320, 14091 };
        Vector3 tisy_screen_position;

        Vector3 polana_world_position = { 11132, 200, 7681 };
        Vector3 polana_screen_position;

        Vector3 mishkino_world_position = { 3773, 282, 8392 };
        Vector3 mishkino_screen_position;

        Vector3 grishino_world_position = { 6063, 216, 10324 };
        Vector3 grishino_screen_position;

        Vector3 pogorevka_world_position = { 4494, 270, 5998 };
        Vector3 pogorevka_screen_position;

        Vector3 rogovo_world_position = { 4836, 266, 6434 };
        Vector3 rogovo_screen_position;

        Vector3 dubrovka_world_position = { 10073, 162, 9297 };
        Vector3 dubrovka_screen_position;

        Vector3 kabanino_world_position = { 5294, 285, 8632 };
        Vector3 kabanino_screen_position;

        Vector3 petrovka_world_position = { 3223, 146, 11573 };
        Vector3 petrovka_screen_position;

        if (GameVars.Chernogorsk)
            if (Game::WorldToScreen(chernogorsk_world_position, chernogorsk_screen_position)) {
                int distance = Game::GetDistanceToMe(chernogorsk_world_position);
                DrawPlayerText(chernogorsk_screen_position.x, chernogorsk_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Chernogorsk").c_str());
            }

        if (GameVars.Electrozavodsk)
            if (Game::WorldToScreen(electrozavodsk_world_position, electrozavodsk_screen_position)) {
                int distance = Game::GetDistanceToMe(electrozavodsk_world_position);
                DrawPlayerText(electrozavodsk_screen_position.x, electrozavodsk_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Electrozavodsk").c_str());
            }

        if (GameVars.Balota)
            if (Game::WorldToScreen(balota_world_position, balota_screen_position)) {
                int distance = Game::GetDistanceToMe(balota_world_position);
                DrawPlayerText(balota_screen_position.x, balota_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Balota").c_str());
            }

        if (GameVars.Berezino)
            if (Game::WorldToScreen(berezino_down_world_position, berezino_screen_position)) {
                int distance = Game::GetDistanceToMe(berezino_down_world_position);
                DrawPlayerText(berezino_screen_position.x, berezino_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Berezino").c_str());
            }

        if (GameVars.Solnechniy)
            if (Game::WorldToScreen(solnechniy_world_position, solnechniy_screen_position)) {
                int distance = Game::GetDistanceToMe(solnechniy_world_position);
                DrawPlayerText(solnechniy_screen_position.x, solnechniy_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Solnechniy").c_str());
            }

        if (GameVars.Kamyshovo)
            if (Game::WorldToScreen(kamyshovo_world_position, kamyshovo_screen_position)) {
                int distance = Game::GetDistanceToMe(kamyshovo_world_position);
                DrawPlayerText(kamyshovo_screen_position.x, kamyshovo_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Kamyshovo").c_str());
            }

        if (GameVars.Prigorodki)
            if (Game::WorldToScreen(prigorodki_world_position, prigorodki_screen_position)) {
                int distance = Game::GetDistanceToMe(prigorodki_world_position);
                DrawPlayerText(prigorodki_screen_position.x, prigorodki_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Prigorodki").c_str());
            }

        if (GameVars.Svetloyarsk)
            if (Game::WorldToScreen(svetloyarsk_world_position, svetloyarsk_screen_position)) {
                int distance = Game::GetDistanceToMe(svetloyarsk_world_position);
                DrawPlayerText(svetloyarsk_screen_position.x, svetloyarsk_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Svetloyarsk").c_str());
            }

        if (GameVars.Krasnostav)
            if (Game::WorldToScreen(krasnostav_world_position, krasnostav_screen_position)) {
                int distance = Game::GetDistanceToMe(krasnostav_world_position);
                DrawPlayerText(krasnostav_screen_position.x, krasnostav_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Krasnostav").c_str());
            }

        if (GameVars.Novodmitrovsk)
            if (Game::WorldToScreen(novodmitrovsk_world_position, novodmitrovsk_screen_position)) {
                int distance = Game::GetDistanceToMe(novodmitrovsk_world_position);
                DrawPlayerText(novodmitrovsk_screen_position.x, novodmitrovsk_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Novodmitrovsk").c_str());
            }

        if (GameVars.Airfield)
            if (Game::WorldToScreen(airfield_vybor_world_position, airfield_vybor_screen_position)) {
                int distance = Game::GetDistanceToMe(airfield_vybor_world_position);
                DrawPlayerText(airfield_vybor_screen_position.x, airfield_vybor_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Airfield").c_str());
            }

        if (GameVars.Vybor)
            if (Game::WorldToScreen(vybor_world_position, vybor_world_position)) {
                int distance = Game::GetDistanceToMe(vybor_world_position);
                DrawPlayerText(vybor_screen_position.x, vybor_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Vybor").c_str());
            }

        if (GameVars.Tisy)
            if (Game::WorldToScreen(tisy_world_position, tisy_world_position)) {
                int distance = Game::GetDistanceToMe(tisy_world_position);
                DrawPlayerText(tisy_screen_position.x, tisy_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Tisy").c_str());
            }

        if (GameVars.Novaya)
            if (Game::WorldToScreen(novaya_petrovka_world_position, novaya_petrovka_screen_position)) {
                int distance = Game::GetDistanceToMe(novaya_petrovka_world_position);
                DrawPlayerText(novaya_petrovka_screen_position.x, novaya_petrovka_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Novaya Petrovka").c_str());
            }

        if (GameVars.Severograd)
            if (Game::WorldToScreen(severograd_world_position, severograd_screen_position)) {
                int distance = Game::GetDistanceToMe(severograd_world_position);
                DrawPlayerText(severograd_screen_position.x, severograd_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Severograd").c_str());
            }

        if (GameVars.Gorka)
            if (Game::WorldToScreen(gorka_world_position, gorka_screen_position)) {
                int distance = Game::GetDistanceToMe(gorka_world_position);
                DrawPlayerText(gorka_screen_position.x, gorka_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Gorka").c_str());
            }

        if (GameVars.Tulga)
            if (Game::WorldToScreen(tulga_world_position, tulga_screen_position)) {
                int distance = Game::GetDistanceToMe(tulga_world_position);
                DrawPlayerText(tulga_screen_position.x, tulga_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Tulga").c_str());
            }

        if (GameVars.Zelenogorsk)
            if (Game::WorldToScreen(zelenogorsk_world_position, zelenogorsk_screen_position)) {
                int distance = Game::GetDistanceToMe(zelenogorsk_world_position);
                DrawPlayerText(zelenogorsk_screen_position.x, zelenogorsk_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Zelenogorsk").c_str());
            }

        if (GameVars.StarySobor)
            if (Game::WorldToScreen(stary_sobor_world_position, stary_sobor_screen_position)) {
                int distance = Game::GetDistanceToMe(stary_sobor_world_position);
                DrawPlayerText(stary_sobor_screen_position.x, stary_sobor_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Stary Sobor").c_str());
            }

        if (GameVars.Polana)
            if (Game::WorldToScreen(polana_world_position, polana_screen_position)) {
                int distance = Game::GetDistanceToMe(polana_world_position);
                DrawPlayerText(polana_screen_position.x, polana_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Polana").c_str());
            }

        if (GameVars.Mishkino)
            if (Game::WorldToScreen(mishkino_world_position, mishkino_world_position)) {
                int distance = Game::GetDistanceToMe(mishkino_world_position);
                DrawPlayerText(mishkino_screen_position.x, mishkino_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Mishkino").c_str());
            }

        if (GameVars.Polana)
            if (Game::WorldToScreen(polana_world_position, polana_screen_position)) {
                int distance = Game::GetDistanceToMe(polana_world_position);
                DrawPlayerText(polana_screen_position.x, polana_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Polana").c_str());
            }

        if (GameVars.Grishino)
            if (Game::WorldToScreen(grishino_world_position, grishino_world_position)) {
                int distance = Game::GetDistanceToMe(grishino_world_position);
                DrawPlayerText(grishino_screen_position.x, grishino_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Grishino").c_str());
            }

        if (GameVars.Pogorevka)
            if (Game::WorldToScreen(pogorevka_world_position, pogorevka_world_position)) {
                int distance = Game::GetDistanceToMe(pogorevka_world_position);
                DrawPlayerText(pogorevka_screen_position.x, pogorevka_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Pogorevka").c_str());
            }

        if (GameVars.Rogovo)
            if (Game::WorldToScreen(rogovo_world_position, rogovo_world_position)) {
                int distance = Game::GetDistanceToMe(rogovo_world_position);
                DrawPlayerText(rogovo_screen_position.x, rogovo_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Rogovo").c_str());
            }

        if (GameVars.Dubrovka)
            if (Game::WorldToScreen(dubrovka_world_position, dubrovka_world_position)) {
                int distance = Game::GetDistanceToMe(dubrovka_world_position);
                DrawPlayerText(dubrovka_screen_position.x, dubrovka_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Dubrovka").c_str());
            }

        if (GameVars.Kabanino)
            if (Game::WorldToScreen(kabanino_world_position, kabanino_world_position)) {
                int distance = Game::GetDistanceToMe(kabanino_world_position);
                DrawPlayerText(kabanino_screen_position.x, kabanino_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Kabanino").c_str());
            }

        if (GameVars.Petrovka)
            if (Game::WorldToScreen(petrovka_world_position, petrovka_world_position)) {
                int distance = Game::GetDistanceToMe(petrovka_world_position);
                DrawPlayerText(petrovka_screen_position.x, petrovka_screen_position.y, ImColor(255, 255, 255, 255), ("[" + std::to_string(distance) + "m] - Petrovka").c_str());
            }

    }

    if (GameVars.tperson) {
        SetThirdPerson(1);
    }

    if (GameVars.sethour) {
        SetWorldTime(GameVars.dayf);
    }
    else {
        SetWorldTime(1.0f);
    }

    if (GameVars.seteye) {
        SetEyeAcom(GameVars.eye);
    }
    else {
        SetEyeAcom(1.0f);
    }

    if (GameVars.speedhh) {
        SetTick(GameVars.tickness);
    }
    else {
        SetTick(10000000);
    }

    if (GameVars.inventory)
        inventoryPlayers();

    if (GameVars.grass) {
        SetTerrainGrid(0.0f);
    }
    else {
        SetTerrainGrid(10.0f);
    }

    if (GameVars.SilentAim)
        main_aimboo2();

    if (GameVars.LootTeleport)
        main_LootTp(GameVars.isnpc);


    valid_players4inventory.clear();

    valid_players.clear();
    valid_LootTP.clear();

    if (GameVars.ToggleFov) {
        auto* window = ImGui::GetOverlayDrawList();
        ImGuiIO& io = ImGui::GetIO();
        ImU32 color = ImColor(GameVars.colorfovaim[0], GameVars.colorfovaim[1], GameVars.colorfovaim[2], GameVars.colorfovaim[3]);
        window->AddCircle({ io.DisplaySize.x / 2, io.DisplaySize.y / 2 }, GameVars.aimfov, color, 100);
    };

    if (GameVars.ToggleTpFov) {
        auto* window = ImGui::GetOverlayDrawList();
        ImGuiIO& io = ImGui::GetIO();
        ImU32 color = ImColor(GameVars.colorfovtp[0], GameVars.colorfovtp[1], GameVars.colorfovtp[2], GameVars.colorfovtp[3]);
        window->AddCircle({ io.DisplaySize.x / 2, io.DisplaySize.y / 2 }, GameVars.tpfov, color, 100);
    };

    if (GameVars.ToggleInvFov) {
        auto* window = ImGui::GetOverlayDrawList();
        ImGuiIO& io = ImGui::GetIO();
        ImU32 color = ImColor(GameVars.colorfovinv[0], GameVars.colorfovinv[1], GameVars.colorfovinv[2], GameVars.colorfovinv[3]);
        window->AddCircle({ io.DisplaySize.x / 2, io.DisplaySize.y / 2 }, GameVars.invfov, color, 100);
    };
}

void keyboard_listener()
{
    while (true)
    {
        if (GetAsyncKeyState(VK_F1) & 1)
        {
            GameVars.menuShow = not GameVars.menuShow;
        }
        if (GetAsyncKeyState(VK_F3) & 1)
        {
            // Full DMA cache refresh. Guaranteed to catch paged-out entities
            // that appeared in newly-allocated VA regions (new players entering
            // the network bubble, streamed-in world content, etc.).
            //
            // VMMDLL_OPT_REFRESH_ALL drops ALL caches: full TLB (page table
            // translations), full memory content cache, process list, and
            // refreshes every tier. Nothing is left stale — any new virtual
            // page the game has committed since init will be walked fresh on
            // the next read.
            //
            // Cost: ~8 seconds of stalled DMA reads while the FPGA re-walks
            // DayZ's page tables. This blocks the main read loop. Use only
            // when stationary/safe — you will feel the stutter.
            VMM_HANDLE vmm = globals.vmmManager->getVmm();
            if (vmm)
            {
                Log("[F3] Manual full DMA refresh triggered...");
                VMMDLL_ConfigSet(vmm, VMMDLL_OPT_REFRESH_ALL, 1);
                Log("[F3] Refresh complete.");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
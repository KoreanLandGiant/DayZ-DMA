#pragma once
#include "vector.h"
#include "memory.h"
#include "xorstr.h"
#include <string>
#include <map>

// Defini��o da macro ISVALID
#define ISVALID(x) ((x) != 0 && (x) != 0xFFFFFFFFFFFFFFFF)

#define BONE_HEAD 48
#define BONE_NECK 22
#define BONE_CHEST_UPPER 21
#define BONE_CHEST_LOWER 20
#define BONE_SPINE1 19
#define BONE_SPINE2 18
#define BONE_PELVIS 17
#define BONE_LEFT_SHOULDER 61
#define BONE_LEFT_FOREARM1 92
#define BONE_LEFT_FOREARM2 62
#define BONE_LEFT_ELBOW 63
#define BONE_LEFT_ARM 64
#define BONE_RIGHT_SHOULDER 94
#define BONE_RIGHT_SHOULDER2 95
#define BONE_RIGHT_ELBOW 96
#define BONE_RIGHT_FOREARM2 97
#define BONE_RIGHT_FOREARM1 98
#define BONE_LEFT_UPPER_ARM1 0
#define BONE_LEFT_UPPER_ARM2 1
#define BONE_LEFT_UPPER_ARM3 2
#define BONE_LEFT_UPPER_ARM4 3
#define BONE_LEFT_UPPER_ARM5 4
#define BONE_LEFT_UPPER_ARM6 5
#define BONE_LEFT_UPPER_ARM7 6
#define BONE_LEFT_UPPER_ARM8 7
#define BONE_RIGHT_UPPER_ARM1 9
#define BONE_RIGHT_UPPER_ARM2 10
#define BONE_RIGHT_UPPER_ARM3 11
#define BONE_RIGHT_UPPER_ARM4 12
#define BONE_RIGHT_UPPER_ARM5 13
#define BONE_RIGHT_UPPER_ARM6 14
#define BONE_RIGHT_UPPER_ARM7 15
#define BONE_RIGHT_UPPER_ARM8 16

// Estrutura BonePair, se ainda n�o definida
struct BonePair {
	int bone1;
	int bone2;
};

struct BonePosition {
	Vector3 position;
	int index;

	BonePosition(const Vector3& pos, int idx) : position(pos), index(idx) {}
};

BonePair bonePairsHumans[] = {
	{48, 22}, // cabe�a - pesco�o
	{22, 21}, // pesco�o - um pouco em baixo do peito
	{21, 20},
	{20, 19},
	{19, 18},
	{18, 17},
	{17, 0},

	// parte esquerda
	{21, 61}, // um pouco em baixo do peito - ombro esquerdo
	{61, 92}, // ombro esquerdo - antebra�o
	{92, 62}, // antebra�o1 - antebra�o2
	{62, 63}, // antebra�o2 - cotovelo1
	{63, 64}, // cotovelo1 - bra�o
	{0, 1},
	{1, 2},
	{2, 3},
	{3, 4},
	{4, 5},
	{5, 6},
	{6, 7},
	{7, 8},

	// parte direita
	{21, 94}, // um pouco em baixo do peito - ombro direito
	{94, 95}, // ombro direito1 - ombro direito2
	{95, 96}, // ombro direito2 - cotovelo
	{96, 97},  // cotovelo1 - cotovelo2
	{97, 98},  // cotovelo2 - braco
	{0, 9},
	{9, 10},
	{10, 11},
	{11, 12},
	{12, 13},
	{13, 14},
	{14, 15},
	{15, 16}
};

BonePair bonePairsZombies[] = {
	{22, 20},
	{20, 19},
	{19, 18},
	{18, 17},
	{17, 16},
	{16, 15},
	{15, 0},

	{19, 56},
	{56, 57},
	{57, 58},
	{58, 59},
	{59, 60},
	{56, 57},
	{56, 57},
	{0, 9},
	{9, 10},
	{10, 11},
	{11, 12},
	{12, 13},
	{13, 14},

	{19, 24},
	{24, 25},
	{25, 26},
	{26, 27},
	{0, 1},
	{1, 2},
	{2, 3},
	{3, 4},
	{4, 5},
	{5, 6},
	{6, 7}
};

void DrawBox(float x, float y, float width, float height, ImColor color, float thickness) {
	ImGui::GetWindowDrawList()->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), color, 0.0f, 0, thickness);
}

namespace Game
{

	// Example function to get entity dimensions
	bool GetEntityDimensions(uint64_t EntityPtr, int& width, int& height) {
		// Dummy implementation; replace with actual logic to get entity dimensions
		width = 100;  // Example width
		height = 200; // Example height
		return true;
	}
	struct matrix4x4
	{
		float m[12];
	};

	// Example declaration of settings structure
	struct Settings {
		bool DrawFov;
		bool SilentAim;
		// Other settings members
	};

	// Example global settings instance
	Settings settings;

	// =============================================
	// Per-frame camera cache (avoids ~15 DMA reads per WorldToScreen call)
	// =============================================
	struct CameraData {
		uint64_t cameraPtr;
		Vector3 invertedViewTranslation;
		Vector3 invertedViewRight;
		Vector3 invertedViewUp;
		Vector3 invertedViewForward;
		Vector3 viewportSize;
		Vector3 projectionD1;
		Vector3 projectionD2;
		bool valid;
	};

	static CameraData cachedCamera = {};

	void UpdateCameraCache()
	{
		cachedCamera.valid = false;
		cachedCamera.cameraPtr = ReadData<uint64_t>(globals.World + OFF_Camera);
		if (!cachedCamera.cameraPtr)
			return;

		// Use scatter to read all camera vectors in ONE DMA transaction
		auto scatter = globals.vmmManager->initializeScatter(globals.process_id);
		if (!scatter) return;

		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + OFF_GetCoordinate, sizeof(Vector3), &cachedCamera.invertedViewTranslation);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0x8, sizeof(Vector3), &cachedCamera.invertedViewRight);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0x14, sizeof(Vector3), &cachedCamera.invertedViewUp);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0x20, sizeof(Vector3), &cachedCamera.invertedViewForward);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0x58, sizeof(Vector3), &cachedCamera.viewportSize);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0xD0, sizeof(Vector3), &cachedCamera.projectionD1);
		globals.vmmManager->addScatterRead(scatter, cachedCamera.cameraPtr + 0xDC, sizeof(Vector3), &cachedCamera.projectionD2);

		globals.vmmManager->executeScatter(scatter);
		cachedCamera.valid = true;
	}


	BOOLEAN GetBonePosition(DWORD64 pSkeleton, DWORD64 pVisual, DWORD index, Vector3& out)
	{
		DWORD64 animClass = ReadData<DWORD64>(pSkeleton + OFF_AnimClass);
		if (!ISVALID(animClass)) return FALSE;

		DWORD64 matrixClass = ReadData<DWORD64>(animClass + OFF_AnimClass);
		if (!ISVALID(matrixClass)) return FALSE;

		matrix4x4 matrix_a = ReadData<matrix4x4>(pVisual + OFF_VisualState);
		Vector3 matrix_b = ReadData<Vector3>(matrixClass + OFF_Matrixb + index * sizeof(Vector3));

		out.x = (matrix_a.m[0] * matrix_b.x) + (matrix_a.m[3] * matrix_b.y) + (matrix_a.m[6] * matrix_b.z) + matrix_a.m[9];
		out.y = (matrix_a.m[1] * matrix_b.x) + (matrix_a.m[4] * matrix_b.y) + (matrix_a.m[7] * matrix_b.z) + matrix_a.m[10];
		out.z = (matrix_a.m[2] * matrix_b.x) + (matrix_a.m[5] * matrix_b.y) + (matrix_a.m[8] * matrix_b.z) + matrix_a.m[11];

		return TRUE;
	}

	bool IsValidPtr2(void* pointer)
	{
		if (!pointer)
			return false;

		if (pointer < (void*)0xFFF)
			return false;

		if (pointer > (void*)0x7FFFFFFFFFFF)
			return false;

		return true;
	}

	uint64_t GetNetworkManager() {
		return globals.Base + OFF_Network_Manager;
	}

	uint64_t GetNetworkClient()
	{
		return ReadData<uint64_t>(GetNetworkManager() + OFF_Network_Client);
	}

	uint64_t  GetNetworkClientScoreBoard() {
		return ReadData<uint64_t>(GetNetworkClient() + OFF_Network_Table);
	}

	std::string GetNetworkClientServerName() {
		return ReadArmaString(ReadData<uint64_t>(GetNetworkClient() + OFF_Network_ServerName)).c_str();
	}

	struct NetworkTable {
		uint64_t playerArray; // ponteiro
		int playerCount;
	};

	int GetPlayerCount() {
		uint64_t table = ReadData<uint64_t>(GetNetworkClient() + OFF_Network_Table);
		if (!table)
			return 0;

		return ReadData<int>(table + OFF_PlayerCount);
	}

	std::string GetPlayerCountString() {
		int count = GetPlayerCount();
		return "Players: " + std::to_string(count);
	}


	std::string get_server_name()
	{
		static uintptr_t network_client = 0;
		static std::string serverName;

		if (!network_client)
		{
			//GetNetworkClient()
			network_client = ReadData<uintptr_t>(
				globals.Base + OFF_NETWORK + OFF_Network_Client
			);
			if (!network_client)
				return "";
		}



		uintptr_t serverNameStr = ReadData<uintptr_t>(network_client + 0x340);
		if (!serverNameStr)
			return "";

		serverName = ReadArmaString(serverNameStr);
		return serverName;
	}

	int get_player_count()
	{
		//GetNetworkClient()
		auto network_client = ReadData<uintptr_t>(globals.Base + OFF_NETWORK + OFF_Network_Client);

		return ReadData<int>(network_client + OFF_Player_Count);
	}

	float FGetHealth(uint64_t Entity)
	{
		return (ReadData<float>(Entity + 0x194));
	}

	/*
	int realHealth = 100;
	if (hp == 0) {
		realHealth = 100;
	}
	else if (hp == 1e-45f) {
		realHealth = 70;
	}
	else if (hp == 3e-45f) {
		realHealth = 50;
	}
	else if (hp == 4e-45f) {
		realHealth = 30;
	}
	else if (hp == 6e-45f) {
		realHealth = 0;
	}
	*/

	uint64_t get_health(uint64_t Entity) {
		uintptr_t the_class = ReadData<uintptr_t>(Entity + OFF_Quality);
		if (!the_class)
			return 100.0f;

		return static_cast<uint64_t>(the_class);
	}

	std::string DrawHealthText(uint64_t Entity) {
		uint64_t health = get_health(Entity);
		return std::to_string(health);
	}

	uint64_t GetInventory(uint64_t Entity)
	{
		if (!IsValidPtr2((void*)Entity))
			return 0;

		return ReadData<uint64_t>(Entity + OFF_Inventory);
	}

	std::string GetItemInHands(uint64_t Entity)
	{
		return ReadArmaString(ReadData<uint64_t>(
			ReadData<uint64_t>(ReadData<uint64_t>(
				GetInventory(Entity) + OFF_Inhands)
				+ OFF_EntityTypePtr) + OFF_CleanName)).c_str();
	}

	bool is_dead(uint64_t entityPtr) {
		return ReadData<uint8_t>(entityPtr + OFF_playerIsDead) == 1;
	}


	std::string GetQuality(uint64_t Entity) {

		auto quality = ReadData<uint64_t>(Entity + OFF_Quality);

		if (quality == 1) return "Quality(Worn)";
		if (quality == 2) return "Quality(Damaged)";
		if (quality == 3) return "Quality(Badly damaged)";
		if (quality == 4) return "Quality(Ruined)";

		else return "Quality (Pristine)";
	}

	uint64_t NearEntityTable()
	{
		return ReadData<uint64_t>(globals.World + OFF_EntityTable);
	}

	uint32_t NearEntityTableSize()
	{
		return ReadData<uint32_t>(globals.World + OFF_NearTableSize);
	}

	uint64_t GetEntity(uint64_t PlayerList, uint64_t SelectedPlayer)
	{
		// Sorted Object
		return ReadData<uint64_t>(PlayerList + SelectedPlayer * 0x8);
	}

	std::string GetEntityTypeName(uint64_t Entity)
	{
		// Render Entity Type + Config Name
		return ReadArmaString(ReadData<uint64_t>(ReadData<uint64_t>(Entity + OFF_EntityTypePtr) + OFF_EntityTypeName)).c_str();
	}

	uint64_t FarEntityTable()
	{
		return ReadData<uint64_t>(globals.World + OFF_FarEntityTable);
	}

	uint32_t FarEntityTableSize()
	{
		return ReadData<uint32_t>(globals.World + OFF_FarTableSize);
	}

	uint64_t GetCameraOn()
	{
		// Camera On 
		return ReadData<uint64_t>(globals.World + OFF_LocalPlayer1);
	}

	uint64_t GetLocalPlayer()
	{
		uint64_t localPlayer = ReadData<uint64_t>(globals.World + OFF_LocalPlayer1);
		if (!localPlayer) return 0;
		uint64_t localPlayer2 = ReadData<uint64_t>(localPlayer + 0x8);
		if (!localPlayer2 || localPlayer2 <= 0xA8) return 0;
		return localPlayer2 - 0xA8;
	}

	uint64_t GetItemInHands()
	{
		uint64_t localPlayer = GetLocalPlayer();
		return ReadData<uint64_t>(localPlayer + OFF_Inhands);
	}


	uint64_t GetLocalPlayerVisualState()
	{
		return ReadData<uint64_t>(GetLocalPlayer() + OFF_VisualState);
	}

	Vector3 GetLocalPlayerVisualState1()
	{
		return ReadData<Vector3>(GetLocalPlayerVisualState() + OFF_GetCoordinate);
	}


	Vector3 GetCoordinate(uint64_t Entity)
	{
		while (true)
		{
			if (Entity == GetLocalPlayer())
			{
				return Vector3(ReadData<Vector3>(GetLocalPlayerVisualState() + OFF_GetCoordinate));
			}
			else
			{
				return  Vector3(ReadData<Vector3>(ReadData<uint64_t>(Entity + OFF_VisualState) + OFF_GetCoordinate));
			}
		}
	}
	uint64_t GetCamera()
	{
		while (true)
		{
			return ReadData<uint64_t>(globals.World + OFF_Camera);
		}
	}

	Vector3 GetInvertedViewTranslation()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + OFF_GetCoordinate));
	}
	Vector3 GetInvertedViewRight()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0x8));
	}
	Vector3 GetInvertedViewUp()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0x14));
	}
	Vector3 GetInvertedViewForward()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0x20));
	}

	Vector3 GetViewportSize()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0x58));
	}

	Vector3 GetProjectionD1()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0xD0));
	}

	Vector3 GetProjectionD2()
	{
		return Vector3(ReadData<Vector3>(GetCamera() + 0xDC));
	}

	uint32_t GetSlowEntityTableSize()
	{
		return ReadData<uint32_t>(globals.World + OFF_SlowTableSize);
	}

	// Per-frame local player cache
	static uint64_t cachedLocalPlayer = 0;
	static Vector3 cachedLocalPlayerPos = {};

	void UpdateLocalPlayerCache()
	{
		cachedLocalPlayer = GetLocalPlayer();
		if (cachedLocalPlayer)
			cachedLocalPlayerPos = GetCoordinate(cachedLocalPlayer);
	}

	float GetDistanceToMe(Vector3 Entity)
	{
		return Entity.Distance(cachedLocalPlayerPos);
	}

	uint64_t ItemTable()
	{
		return ReadData<uint64_t>(globals.World + OFF_ItemTable);
	}

	uint32_t ItemTableSize()
	{
		return ReadData<uint32_t>(globals.World + OFF_ItemTableSize);
	}


	Vector3 GetItemCoordinate(uint64_t Item)
	{
		return Vector3(ReadData<Vector3>(ReadData<uint64_t>(Item + OFF_VisualState) + OFF_GetCoordinate));
	}

	struct BonePair {
		int boneId1;
		int boneId2;

		BonePair(int id1, int id2) : boneId1(id1), boneId2(id2) {}
	};

	struct BonePosition {
		Vector3 position;
		int index;

		BonePosition(const Vector3& pos, int idx) : position(pos), index(idx) {}
	};

	std::string ReadArmaString(uint64_t address)
	{
		char buffer[256] = {};
		ZwCopyMemory(address + OFF_TEXT, buffer, sizeof(buffer), false);
		std::string str(buffer);

		// Remove caracteres especiais usando uma express�o regular
		str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) { return !std::isalnum(c) && !std::isspace(c); }), str.end());

		return str;
	}
	struct nameid
	{
		UINT64 pt1;
		UINT64 pt2;
	};
	std::map<UINT64, std::string> nameCache;
	std::string getNameFromId(uintptr_t namePointer)
	{
		nameid ID = ReadData<nameid>(namePointer + 0x10);

		std::map<UINT64, std::string>::iterator it = nameCache.find(ID.pt1 + ID.pt2);

		if (it == nameCache.end())
		{

			int size = ReadData<int>(namePointer + 0x8);
			if (size < 1)
				return "";
			char* name = new char[size];
			ZwCopyMemory(namePointer + 0x10, name, size, false);

			if (strstr(name, xorstr_("Animal")) != NULL || strstr(name, xorstr_("Zmb")) != NULL || strstr(name, xorstr_("Firewood")) != NULL || strstr(name, xorstr_("Barrel")) || strstr(name, xorstr_("Watchtower")) || strstr(name, xorstr_("Wood Pillar")) || strstr(name, xorstr_("Roof")) || strstr(name, xorstr_("Wall")) != NULL || strstr(name, xorstr_("Floor")) || strstr(name, xorstr_("Fireplace")) != NULL || strstr(name, xorstr_("Wire Mesh Barrier")) != NULL || strstr(name, xorstr_("Fence")) != NULL)
			{
				std::string text = "";
				nameCache.insert(std::pair<UINT64, std::string>(ID.pt1 + ID.pt2, text));
				delete name;
				return text;
			}

			std::string text = std::string(name);
			delete name;
			nameCache.insert(std::pair<UINT64, std::string>(ID.pt1 + ID.pt2, text));
			return text;
		}
		else
		{
			return it->second;
		}
	}


	std::string GetItemTypeName(uint64_t Item)
	{
		uint64_t name_ptr = ReadData<uint64_t>(Item + OFF_EntityTypePtr);
		uint64_t real_name_ptr = ReadData<uint64_t>(name_ptr + OFF_RealName);
		return ReadArmaString(real_name_ptr);
	}

	std::string GetItemRealName(uint64_t Item)
	{
		uint64_t name_ptr = ReadData<uint64_t>(Item + OFF_EntityTypePtr);
		uint64_t real_name_ptr = ReadData<uint64_t>(name_ptr + OFF_RealName);
		return ReadArmaString(real_name_ptr);
	}

	std::string GetEntityRealName(uint64_t Entity)
	{
		uint64_t name_ptr = ReadData<uint64_t>(Entity + OFF_EntityTypePtr);
		uint64_t real_name_ptr = ReadData<uint64_t>(name_ptr + OFF_RealName);
		return ReadArmaString(real_name_ptr);
	}

	std::string GetPlayerName(uint64_t Player)
	{
		uint64_t name_ptr = ReadData<uint64_t>(Player + OFF_PlayerName);
		return ReadArmaString(name_ptr);
	}

	std::string BulletTable(uint64_t bullet)
	{
		uint64_t name_ptr = ReadData<uint64_t>(globals.World + OFF_Bullets);
		// Add your logic to return a string based on name_ptr or other data
		return ""; // Example return; replace with your actual logic
	}

	uint64_t BulletTableSize() {
		return sizeof(BulletTable(0)); // Pass a dummy argument or a valid argument
	}

	bool WorldToScreen(Vector3 Position, Vector3& output)
	{
		if (!cachedCamera.valid) return false;

		Vector3 temp = Position - cachedCamera.invertedViewTranslation;

		float x = temp.Dot(cachedCamera.invertedViewRight);
		float y = temp.Dot(cachedCamera.invertedViewUp);
		float z = temp.Dot(cachedCamera.invertedViewForward);

		if (z < 0.1f)
			return false;

		output.x = cachedCamera.viewportSize.x * (1 + (x / cachedCamera.projectionD1.x / z));
		output.y = cachedCamera.viewportSize.y * (1 - (y / cachedCamera.projectionD2.y / z));
		output.z = z;
		return true;
	}
	Vector3 GetObjectVisualState(uintptr_t entity)
	{
		if (entity)
		{
			uintptr_t renderVisualState = ReadData<uintptr_t>(entity + OFF_VisualState);

			if (!IsValidPtr2((void*)renderVisualState))
				return Vector3(-1, -1, -1);

			if (renderVisualState)
			{
				Vector3 pos = ReadData<Vector3>(renderVisualState + OFF_GetCoordinate);
				return pos;
			}
		}

		return Vector3(-1, -1, -1);
	}

	bool SetPosition(uint64_t Entity, Vector3 TargetPosition)
	{
		if (Entity == Game::GetLocalPlayer()) {


			WriteData<Vector3>(ReadData<uint64_t>(Entity + 0xF8) + OFF_GetCoordinate, TargetPosition);
		}
		else {
			WriteData<Vector3>(ReadData<uint64_t>(Entity + OFF_VisualState) + OFF_GetCoordinate, TargetPosition);

		}
		return true;
	}

	// Nova fun��o GetEntityBounds
	bool GetEntityBounds(uint64_t Entity, Vector3& minBounds, Vector3& maxBounds) {
		// Implementa��o para obter os limites da entidade
		// Este � apenas um exemplo e pode precisar ser ajustado para o seu jogo espec�fico
		minBounds = Vector3(-0.5f, -0.5f, -0.5f); // Coordenadas m�nimas
		maxBounds = Vector3(0.5f, 0.5f, 0.5f); // Coordenadas m�ximas
		return true;
	}

	uint64_t GeVisualState(uint64_t Enity)
	{
		if (!IsValidPtr2((void*)Enity))
			return 0;

		return ReadData<uint64_t>(Enity + OFF_VisualState);
	}

	BOOL GetBonePosition(DWORD64 skeleton, DWORD64 visual, DWORD index, Vector3* out)
	{
		uint64_t animClass = ReadData<uint64_t>(skeleton + OFF_AnimClass);
		if (!IsValidPtr2((void*)animClass))
			return 0;


		uint64_t matrixClass = ReadData<uint64_t>(animClass + OFF_MatrixClass);
		if (!IsValidPtr2((void*)matrixClass))
			return 0;

		matrix4x4 matrix_a = ReadData< matrix4x4>(visual + 0x8);


		Vector3 matrix_b = ReadData<Vector3>(matrixClass + OFF_Matrixb + index * sizeof(matrix4x4));

		out->x = (matrix_a.m[0] * matrix_b.x) + (matrix_a.m[3] * matrix_b.y) + (matrix_a.m[6] * matrix_b.z) + matrix_a.m[9];
		out->y = (matrix_a.m[1] * matrix_b.x) + (matrix_a.m[4] * matrix_b.y) + (matrix_a.m[7] * matrix_b.z) + matrix_a.m[10];
		out->z = (matrix_a.m[2] * matrix_b.x) + (matrix_a.m[5] * matrix_b.y) + (matrix_a.m[8] * matrix_b.z) + matrix_a.m[11];
		return TRUE;
	}

	uint64_t GetSkeleton(uint64_t Entity, int s1) {

		uint64_t Skeleton = 0;

		if (s1 == 1)
			Skeleton = ReadData<uint64_t>(Entity + OFF_PlayerSkeleton);
		if (s1 == 2)
			Skeleton = ReadData<uint64_t>(Entity + OFF_ZmSkeleton);

		return Skeleton;
	}
}
#pragma once
#include <Windows.h>

#define OFF_TEXT				0x10
#define OFF_LENGTH				0x8
#define OFF_World				0x4263FE8
#define OFF_NEAR_ENTITY_TABLE	0xF48
#define OFF_FAR_ENTITY_TABLE	0x1090
#define OFF_SLOW_ENTITY_TABLE	0x2010
#define OFF_NETWORK				0x100FBD0

#define OFF_CameraOn            0x2968
#define OFF_NetworkManager      0xEF9148
#define OFF_NetworkClient       0x50
#define OFF_scoreboardTable     0x18
#define OFF_PlayerCount         0x24
#define OFF_CurrentId           0x30
#define OFF_EntityTable         0xF48
#define OFF_NearTableSize       0xF50
#define OFF_FarEntityTable      0x1090
#define OFF_FarTableSize        0x1098
#define OFF_SlowTable           0x2010
#define OFF_SlowTableSize       0x2018
#define OFF_ItemTable           0x2060
#define OFF_ItemTableSize       0x2068
#define OFF_playerIsDead        0xE2
#define OFF_playerNetworkID     0x6DC
#define OFF_Inventory           0x650
#define OFF_Inhands             0x1B0
#define OFF_InventoryP          0x150
#define OFF_InventoryPsize      0x15C
#define OFF_EntityTypeName      0xD0
#define OFF_EntityTypePtr       0x180
#define OFF_ModelName           0xB0
#define OFF_RealName            0x518
#define OFF_VisualState         0x1C8
#define OFF_GetCoordinate       0x2C
#define OFF_InventoryI          0x148
#define OFF_InventoryI2         0x38
#define OFF_InventoryIsize      0x44
#define OFF_PlayerSkeleton      0x7E0
#define OFF_ZmSkeleton          0x670
#define OFF_MatrixClass         0xBE8
#define OFF_Matrixb             0x54
#define OFF_AnimClass           0x118
#define OFF_Day                 0x2974
#define OFF_Hour				0x2978
#define OFF_grass               0xC00
#define OFF_Bullets             0xE00
#define OFF_Bulletsize          0xE08
#define OFF_LocalPlayer1        0x2960
#define OFF_LocalPlayer2        0x8
#define OFF_LocalPlayer3        0x5D0	
#define OFF_LocalPlayer4        0x28
#define OFF_Camera              0x1B8
#define OFF_Tickness            0xFF4958
#define OFF_ThirdPerson			0x9C

#define OFF_ObjectBase			0x180 //0x70
#define OFF_CleanName           0x518
#define OFF_CleanName2			0xD0
#define OFF_CleanName5			0xB0
#define OFF_Quality				0x194

// freecam
#define OFF_CameraActive = 0xF59A18;
#define OFF_CameraMode = 0xF59988; // 3 = Debug/Freecam     
// Depurar c�mera
#define OFF_FreeDebugCamera_GetInstance = 0x483390;
#define OFF_Global_FreeDebugCameraInstance = 0xF29EF8;
// Globais de posi��o e rota��o
#define OFF_DebugCameraTargetPos = 0xF599F0;
#define OFF_DebugCameraStartPos = 0xF599B4;
#define OFF_RotationSource1 = 0xF599D0; // Linha 0 (Direita), Linha 1 em +0x10 (Cima)     
#define OFF_RotationSource2 = 0xF59994; // Fonte alternativa     
// Remendos
#define CameraUpdatePatch = 0x87C034;
#define LocalPlayerF = 0xF70840;

#define OFF_PlayerName          0xF8
#define OFF_Network_Manager     0x100FBD0
#define OFF_Network_Client      0x50
#define OFF_Network_Table       0x18
#define OFF_Network_Table_Size  0x1C
#define OFF_Network_Table_ID    0x30
#define OFF_Network_ID          0x6DC
#define OFF_Network_ServerName  0x360
#define OFF_Player_Count        0x24

#define OFF_AMMO_TYPE1          0x6B0
#define OFF_AMMO_TYPE2          0x20
#define OFF_AMMOTYPE_INITSPEED  0x38C
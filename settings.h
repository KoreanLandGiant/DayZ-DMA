#pragma once

#ifndef Settings_H

#include "Singleton.h"
#include "vector"
#include <unordered_set>

inline namespace DayZ
{
	class Variables : public Singleton<Variables>
	{
	public:
		bool menuShow = false;
		bool streamproof = true;

		bool inventory = false;
		bool ToggleInvFov = false;
		float invfov = 60;
		bool inhands = false;

		bool speedhh = false;
		int sliderSpeed = 1;
		int tickness = 10000000 - (sliderSpeed - 1) * 1000000;

		const char* boxTypes[2] = { "Normal Box", "Corner Box" };
		int selectedBoxType = 0;

		const char* linePos[3] = { "Top", "Center", "Bottom" };
		int selectedLinePos = 0;

		bool deadESP = false;
		bool playerESP = false;
		bool playerlistm = false;
		bool healESP = false;
		bool freecam_enabled = false;

		bool skeletonESP = false;
		ImColor skeletonColor = ImColor(255, 255, 255, 200);
		float skeletonColorArray[4] = { skeletonColor.Value.x, skeletonColor.Value.y, skeletonColor.Value.z, skeletonColor.Value.w };
		ImColor skeletonFrinColor = ImColor(0, 33, 255, 160);
		float skeletonFrinColorArray[4] = { skeletonFrinColor.Value.x, skeletonFrinColor.Value.y, skeletonFrinColor.Value.z, skeletonFrinColor.Value.w };

		bool boxESP = false;
		ImColor boxColor = ImColor(0, 113, 1, 255);
		float boxColorArray[4] = { boxColor.Value.x, boxColor.Value.y, boxColor.Value.z, boxColor.Value.w };
		ImColor boxFrinColor = ImColor(85, 255, 0, 255);
		float boxFrinColorArray[4] = { boxFrinColor.Value.x, boxFrinColor.Value.y, boxFrinColor.Value.z, boxFrinColor.Value.w };

		bool lineESP = false;
		ImColor linesColor = ImColor(129, 184, 255, 160);
		float linesColorArray[4] = { linesColor.Value.x, linesColor.Value.y, linesColor.Value.z, linesColor.Value.w };
		ImColor linesFrinColor = ImColor(129, 184, 255, 160);
		float linesFrinColorArray[4] = { linesFrinColor.Value.x, linesFrinColor.Value.y, linesFrinColor.Value.z, linesFrinColor.Value.w };

		bool nameESP = false;
		bool distanceESP = false;
		int espDistance = 1200;

		bool grass = false;

		bool lineZESP = false;
		bool infectedESP = false;
		bool skeletonzESP = false;
		ImColor skeletonzColor = ImColor(255, 34, 34, 160);
		float skeletonzColorArray[4] = { skeletonzColor.Value.x, skeletonzColor.Value.y, skeletonzColor.Value.z, skeletonzColor.Value.w };
		ImColor linesColorZ = ImColor(255, 34, 34, 160);
		float linesColorZArray[4] = { linesColorZ.Value.x, linesColorZ.Value.y, linesColorZ.Value.z, linesColorZ.Value.w };

		bool namezESP = false;
		bool distancezESP = false;
		int espzDistance = 150;

		bool citiesESP = false;
		bool Berezino = false;
		bool Solnechniy = false;
		bool Kamyshovo = false;
		bool Electrozavodsk = false;
		bool Prigorodki = false;
		bool Chernogorsk = false;
		bool Svetloyarsk = false;
		bool Krasnostav = false;
		bool Novodmitrovsk = false;
		bool Airfield = false;
		bool Novaya = false;
		bool Severograd = false;
		bool Gorka = false;
		bool Tulga = false;
		bool Balota = false;
		bool Zelenogorsk = false;
		bool StarySobor = false;
		bool Polana = false;
		bool Mishkino = false;
		bool Grishino = false;
		bool Pogorevka = false;
		bool Rogovo = false;
		bool Dubrovka = false;
		bool Kabanino = false;
		bool Petrovka = false;
		bool Vybor = false;
		bool Tisy = false;

		bool itemsESP = false;
		bool allitemsESP = false;
		int espDistanceitems = 300;

		bool sPristine = true;
		bool sWorn = true;
		bool sDamaged = true;
		bool sBadly = true;
		bool sRuined = true;

		bool carsESP = false;
		int espDistancecars = 300;
		bool animalsESP = false;
		int espDistanceanimals = 300;
		bool tperson = false;
		bool sethour = false;
		int dayf = 10;
		bool seteye = false;
		int eye = 10;

		float linep = 0.01;
		float linez = 0.01;

		int silentAimHotkey = 0x45;
		bool SilentAim = false;
		bool SilentAimZ = false;
		bool ToggleFov = false;
		int LimiteDoSilent = 1500;
		float aimfov = 100;
		int isnpc = 0;
		int bonechoose = 1;

		bool ToggleTpFov = false;
		float tpfov = 100;
		bool LootTeleport = false;
		bool CorpseTeleport = false;
		int lootTPHotkey = 0x49;

		int SizeFont = 16;

		ImColor EspNameColor = ImColor(255, 255, 255, 255);
		float EspNameColorArray[4] = { EspNameColor.Value.x, EspNameColor.Value.y, EspNameColor.Value.z, EspNameColor.Value.w };

		ImColor EspNamezColor = ImColor(255, 34, 34, 160);
		float EspNamezColorArray[4] = { EspNamezColor.Value.x, EspNamezColor.Value.y, EspNamezColor.Value.z, EspNamezColor.Value.w };

		ImColor EspCarColor = ImColor(0, 136, 255, 255);
		float EspCarColorArray[4] = { EspCarColor.Value.x, EspCarColor.Value.y, EspCarColor.Value.z, EspCarColor.Value.w };

		ImColor EspAnimalColor = ImColor(255, 255, 0, 160);
		float EspAnimalColorArray[4] = { EspAnimalColor.Value.x, EspAnimalColor.Value.y, EspAnimalColor.Value.z, EspAnimalColor.Value.w };

		ImColor corespname = ImColor(255, 255, 255, 255);

		float colorfood2[4] = { 255.0f / 255.0f, 234.0f / 255.0f, 144.0f / 255.0f, 255.0f / 255.0f };
		float colordrnks2[4] = { 151.0f / 255.0f, 255.0f / 255.0f, 222.0f / 255.0f, 255.0f / 255.0f };
		float colormedcine2[4] = { 255.0f / 255.0f, 152.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f };

		float colorweapon2[4] = { 255.0f / 255.0f, 104.0f / 255.0f, 104.0f / 255.0f, 255.0f / 255.0f };
		float coloriMagazines2[4] = { 0.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 200.0f / 255.0f };
		float coloriAmmu2[4] = { 0.0f / 255.0f, 157.0f / 255.0f, 255.0f / 255.0f, 200.0f / 255.0f };
		float coloriScoop2[4] = { 85.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f };

		float corcomida2[4] = { 153.0f / 255.0f, 0.0f / 255.0f, 153.0f / 255.0f, 255.0f / 255.0f };

		float colorcloting2[4] = { 255.0f / 255.0f, 118.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f };
		float colorinventor2[4] = { 110.0f / 255.0f, 141.0f / 255.0f, 145.0f / 255.0f, 20.0f / 255.0f };
		float corrodas2[4] = { 0.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f, 200.0f / 255.0f };
		float colorothers2[4] = { 140.0f / 255.0f, 122.0f / 255.0f, 224.0f / 255.0f, 255.0f / 255.0f };
		float colorcont2[4] = { 84.0f / 255.0f, 52.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
		float colorbackpack2[4] = { 184.0f / 255.0f, 248.0f / 255.0f, 149.0f / 255.0f, 255.0f / 255.0f };

		float colorfovaim[4] = { 255, 255, 255, 255 };
		float colorfovtp[4] = { 255, 255, 255, 255 };
		float colorfovinv[4] = { 255, 255, 255, 255 };

		bool qality = false;
		bool itemlistm = false;

		bool weapon = false;
		bool ProxyMagazines = false;
		bool Ammunation = false;
		bool Miras = false;
		bool rodas = false;
		bool clothing = false;
		bool inventoryItem = false;
		bool comida = false;
		bool drinks = false;
		bool medicine = false;
		bool Containers = false;
		bool berrel = false;
		bool BackPack = false;
		bool inventoryItem12 = false;
		bool qualidade = false;
		bool Filtrarlloot = false;
		bool Filtrarlloot1 = false;
		int MaxTextLength = 255;

		char NamesFilter[255 + 1] = "";  // +1 for the null terminator
		std::vector<std::string> lootFilters = { "" };


		uintptr_t* resultant_target_entity = nullptr;
		uintptr_t* resultant_target_entity1 = nullptr;

		std::vector<std::string> playerList;
		std::vector<float> playerDistances;

		std::vector<std::string> itemList;
		std::vector<float> itemDistances;

		std::unordered_set<std::string> friendsSet;
		bool showBots = false;

		bool idbones = false;
		bool idbonesZ = false;

		bool loadConfigRequested = true;
		bool inlocal = false;

		ImFont* smallFont = nullptr;
	};
#define GameVars DayZ::Variables::Get()
}
#endif  !Settings_H
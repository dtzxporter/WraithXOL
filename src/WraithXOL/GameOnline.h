#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <array>

// We need the following classes
#include "InjectionReader.h"
#include "DBGameGenerics.h"
#include "CoDXAssets.h"
#include "CoDIWITranslator.h"
#include "IFSLib.h"

// A structure that represents game offset information
struct DBGameInfo
{
	uint64_t DBAssetPools;
	uint64_t DBPoolSizes;
	uint64_t StringTable;
	uint64_t ImagePackageTable;

	DBGameInfo(uint64_t Pools, uint64_t Sizes, uint64_t Strings, uint64_t Package)
	{
		// Set values
		DBAssetPools = Pools;
		DBPoolSizes = Sizes;
		StringTable = Strings;
		ImagePackageTable = Package;
	}
};

// The current export config
struct GameExportConfig
{
	bool SEAnims;
	bool XAnimsWAW;
	bool XAnimsBO;

	bool Maya;
	bool OBJ;
	bool XNA;
	bool SMD;
	bool XME;

	bool PNG;
	bool DDS;

	GameExportConfig()
	{
		SEAnims = true;
		Maya = true;
		PNG = true;

		XAnimsWAW = false;
		XAnimsBO = false;
		OBJ = false;
		XNA = false;
		SMD = false;
		XME = false;

		DDS = false;
	}
};

// Handles reading from Online (CODOL)
class GameOnline
{
public:
	// -- Game functions

	// Attempt to load the game, waiting forever until the game attaches, or program closes
	static bool LoadGame();

	// Extracts the assets present in the game...
	static void ExtractAssets(bool Anims, bool Models, bool Images, bool Sounds);

	// Loads a string entry
	static std::string LoadStringHandler(uint64_t Index);

	// Reads an xmodel entry
	static std::unique_ptr<XModel_t> ReadXModel(MW2XModel& ModelData, const std::string& Name);
	// Reads a material entry
	static const XMaterial_t ReadXMaterial(uint64_t MaterialPointer);

	// Exports material images, if they don't exist...
	static void ExportMaterialImages(const XMaterial_t& Material, const std::string& ModelBasePath);

	// -- Game data

	// The game instance
	static std::unique_ptr<InjectionReader> GameInstance;

	// The export config
	static GameExportConfig ExportConfiguration;

private:
	// -- Game offsets databases

	// A list of offsets for Online single player
	static std::array<DBGameInfo, 4> SinglePlayerOffsets;

	// A list of game offsets, varies per-game
	static std::vector<uint64_t> GameOffsetInfos;
	// A list of game pool sizes, varies per-game
	static std::vector<uint32_t> GamePoolSizes;

	// A static, IFSFile controller
	static std::unique_ptr<IFSLib> IFSLibrary;
};
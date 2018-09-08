#include "stdafx.h"

// The class we are implementing
#include "GameOnline.h"

// We need the following WraithX classes
#include "Strings.h"
#include "FileSystems.h"
#include "MemoryReader.h"
#include "InjectionReader.h"
#include "Image.h"
#include "Sound.h"
#include "Console.h"

// We need the cod helper classes
#include "CoDXAnimTranslator.h"
#include "CoDXModelTranslator.h"
#include "CoDIWITranslator.h"
#include "DBGameGenerics.h"

// We need the format exporters
#include "SEAnimExport.h"
#include "XAnimRawExport.h"
#include "MayaExport.h"
#include "OBJExport.h"
#include "XNALaraExport.h"
#include "XMEExport.h"
#include "ValveSMDExport.h"

// -- Initialize built-in game offsets databases

// Online
std::array<DBGameInfo, 4> GameOnline::SinglePlayerOffsets =
{{
	{ 0xE3A4B0, 0xE3A1D0, 0x74E4500, 0x0 },	// LATEST
	{ 0xE24958, 0xE24678, 0x74AF000, 0x0 },
	{ 0xE136F8, 0xE13418, 0x73D1400, 0x0 },
	{ 0xE126F8, 0xE12418, 0x7360180, 0x0 },
}};

// Setup the game instance
std::unique_ptr<InjectionReader> GameOnline::GameInstance = nullptr;

// Set game offsets
std::vector<uint64_t> GameOnline::GameOffsetInfos = std::vector<uint64_t>();
// Set game sizes
std::vector<uint32_t> GameOnline::GamePoolSizes = std::vector<uint32_t>();

// Setup IFS
std::unique_ptr<IFSLib> GameOnline::IFSLibrary = nullptr;

// Config
GameExportConfig GameOnline::ExportConfiguration = GameExportConfig();

bool GameOnline::LoadGame()
{
	// Log that we're waiting for the game
	Console::WriteLineHeader("Game", "Waiting for Call of Duty: Online (Launch game...)");

	// Set it up
	GameInstance = std::make_unique<InjectionReader>();

	// Prepare a loop, once we actually attach, we must then validate read perms
	while (!GameInstance->Attach("codoMP_client_shipRetail.exe"));

	// Resolve the module's base address
	auto BaseAddr = GameInstance->GetMainModuleAddress();

	// Attempt to check the process out
	if (GameInstance->Read<uint32_t>(BaseAddr) == 0x905A4D)
	{
		// Log
		Console::SetBackgroundColor(ConsoleColor::Green);
		Console::WriteLineHeader("Game", "Game process found! Permissions granted.");
		Console::SetBackgroundColor(ConsoleColor::Black);

		// Fetch game path, mount IFS directory
		auto GamePath = FileSystems::GetDirectoryName(GameInstance->GetProcessPath());
		auto GameIIPSPath = FileSystems::CombinePath(GamePath, "IIPS\\IIPSDownload");

		// Mount the IFSLibrary
		GameOnline::IFSLibrary = std::make_unique<IFSLib>();
		// Mount it
		GameOnline::IFSLibrary->MountIFSPath(GameIIPSPath);

		// Load image converter
		Image::SetupConversionThread();

		// Log results
		Console::WriteLineHeader("IFS", "Mounted IFS directory, loaded %d files", GameOnline::IFSLibrary->GetLoadedEntries());

		// Success
		return true;
	}

	// Reset
	GameInstance.reset();

	// Prepare to load the game
	return false;
}

void GameOnline::ExtractAssets(bool Anims, bool Models, bool Images, bool Sounds)
{
	// Clean up
	GameOffsetInfos.clear();
	GamePoolSizes.clear();

	// Prepare to export the assets, in order
	auto CurrentDirectory = FileSystems::GetApplicationPath();
	auto ExportPath = FileSystems::CombinePath(CurrentDirectory, "exported_files\\codol");
	auto AnimPath = FileSystems::CombinePath(ExportPath, "xanims");
	auto ModelPath = FileSystems::CombinePath(ExportPath, "xmodels");
	auto ImagePath = FileSystems::CombinePath(ExportPath, "ximages");
	auto SoundsPath = FileSystems::CombinePath(ExportPath, "sounds");

	// Create it
	FileSystems::CreateDirectory(ExportPath);
	FileSystems::CreateDirectory(AnimPath);
	FileSystems::CreateDirectory(ModelPath);
	FileSystems::CreateDirectory(ImagePath);
	FileSystems::CreateDirectory(SoundsPath);

	// Prepare to export, attempt to verify the pools first
	auto PoolOffsets = SinglePlayerOffsets[0];

	// Assign offsets
	GameOffsetInfos.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBAssetPools + (2 * 4)));
	GameOffsetInfos.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBAssetPools + (4 * 4)));
	GameOffsetInfos.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBAssetPools + (0xA * 4)));
	GameOffsetInfos.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBAssetPools + (0xB * 4)));
	GameOffsetInfos.emplace_back(PoolOffsets.StringTable);

	// Assign sizes
	GamePoolSizes.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBPoolSizes + (2 * 4)));
	GamePoolSizes.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBPoolSizes + (4 * 4)));
	GamePoolSizes.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBPoolSizes + (0xA * 4)));
	GamePoolSizes.emplace_back(GameInstance->Read<uint32_t>(PoolOffsets.DBPoolSizes + (0xB * 4)));

	// TODO: Quick game heuristics??

	// Debug
#if _DEBUG
	Console::WriteLineHeader("Debug", "Info: 0x%X 0x%X 0x%X 0x%X %d %d %d", GameOffsetInfos[0], GameOffsetInfos[1], GameOffsetInfos[2], GameOffsetInfos[3], GamePoolSizes[0], GamePoolSizes[1], GamePoolSizes[2]);
#endif

	// Prepare to load and export assets
	if (Anims)
	{
		// Animations are the first offset and first pool, skip 4 byte pointer to free head
		auto AnimationOffset = GameOffsetInfos[0] + 4;
		auto AnimationCount = GamePoolSizes[0];

		// Calculate maximum pool size
		auto MaximumPoolOffset = (AnimationCount * sizeof(MW3XAnim)) + AnimationOffset;
		// Store original offset
		auto MinimumPoolOffset = GameOffsetInfos[0];

		// Store the placeholder anim
		MW3XAnim PlaceholderAnim;
		// Clear it out
		std::memset(&PlaceholderAnim, 0, sizeof(PlaceholderAnim));

		// Loop and read
		for (uint32_t i = 0; i < AnimationCount; i++)
		{
			// Read
			auto AnimResult = GameInstance->Read<MW3XAnim>(AnimationOffset);

			// Check whether or not to skip, if the handle is 0, or, if the handle is a pointer within the current pool
			if ((AnimResult.NamePtr > MinimumPoolOffset && AnimResult.NamePtr < MaximumPoolOffset) || AnimResult.NamePtr == 0)
			{
				// Advance
				AnimationOffset += sizeof(MW3XAnim);
				// Skip this asset
				continue;
			}

			// Validate and load if need be
			auto AnimName = GameInstance->ReadNullTerminatedString(AnimResult.NamePtr);

			// Check placeholder configuration, "void" is the base xanim
			if (AnimName == "void")
			{
				// Set as placeholder animation
				PlaceholderAnim = AnimResult;
			}
			else if (AnimResult.BoneIDsPtr == PlaceholderAnim.BoneIDsPtr && AnimResult.DataBytePtr == PlaceholderAnim.DataBytePtr && AnimResult.DataShortPtr == PlaceholderAnim.DataShortPtr && AnimResult.DataIntPtr == PlaceholderAnim.DataIntPtr && AnimResult.RandomDataBytePtr == PlaceholderAnim.RandomDataBytePtr && AnimResult.RandomDataIntPtr == PlaceholderAnim.RandomDataIntPtr && AnimResult.RandomDataShortPtr == PlaceholderAnim.RandomDataShortPtr && AnimResult.NotificationsPtr == PlaceholderAnim.NotificationsPtr && AnimResult.DeltaPartsPtr == PlaceholderAnim.DeltaPartsPtr)
			{
				// Advance
				AnimationOffset += sizeof(MW3XAnim);
				// Skip this asset
				continue;
			}

			// Prepare to export this asset, by directly creating an XAnim_t from here!
			{
				auto Anim = std::make_unique<XAnim_t>();

				// Copy over default properties
				Anim->AnimationName = AnimName;
				// Frames and Rate
				Anim->FrameCount = AnimResult.NumFrames;
				Anim->FrameRate = AnimResult.Framerate;
				// Check for viewmodel animations
				if ((_strnicmp(AnimName.c_str(), "viewmodel_", 10) == 0))
				{
					// This is a viewmodel animation
					Anim->ViewModelAnimation = true;
				}
				// Check for looping
				Anim->LoopingAnimation = (AnimResult.Looped > 0);

				// Read the delta data
				auto AnimDeltaData = GameInstance->Read<MW3XAnimDeltaParts>(AnimResult.DeltaPartsPtr);

				// Copy over pointers
				Anim->BoneIDsPtr = AnimResult.BoneIDsPtr;
				Anim->DataBytesPtr = AnimResult.DataBytePtr;
				Anim->DataShortsPtr = AnimResult.DataShortPtr;
				Anim->DataIntsPtr = AnimResult.DataIntPtr;
				Anim->RandomDataBytesPtr = AnimResult.RandomDataBytePtr;
				Anim->RandomDataShortsPtr = AnimResult.RandomDataShortPtr;
				Anim->RandomDataIntsPtr = AnimResult.RandomDataIntPtr;
				Anim->LongIndiciesPtr = AnimResult.LongIndiciesPtr;
				Anim->NotificationsPtr = AnimResult.NotificationsPtr;

				// Bone ID index size
				Anim->BoneIndexSize = 2;

				// Copy over counts
				Anim->NoneRotatedBoneCount = AnimResult.NoneRotatedBoneCount;
				Anim->TwoDRotatedBoneCount = AnimResult.TwoDRotatedBoneCount;
				Anim->NormalRotatedBoneCount = AnimResult.NormalRotatedBoneCount;
				Anim->TwoDStaticRotatedBoneCount = AnimResult.TwoDStaticRotatedBoneCount;
				Anim->NormalStaticRotatedBoneCount = AnimResult.NormalStaticRotatedBoneCount;
				Anim->NormalTranslatedBoneCount = AnimResult.NormalTranslatedBoneCount;
				Anim->PreciseTranslatedBoneCount = AnimResult.PreciseTranslatedBoneCount;
				Anim->StaticTranslatedBoneCount = AnimResult.StaticTranslatedBoneCount;
				Anim->NoneTranslatedBoneCount = AnimResult.NoneTranslatedBoneCount;
				Anim->TotalBoneCount = AnimResult.TotalBoneCount;
				Anim->NotificationCount = AnimResult.NotificationCount;

				// Copy delta
				Anim->DeltaTranslationPtr = AnimDeltaData.DeltaTranslationsPtr;
				Anim->Delta2DRotationsPtr = AnimDeltaData.Delta2DRotationsPtr;
				Anim->Delta3DRotationsPtr = AnimDeltaData.Delta3DRotationsPtr;

				// Set types, we use dividebysize for MW3
				Anim->RotationType = AnimationKeyTypes::DivideBySize;
				Anim->TranslationType = AnimationKeyTypes::MinSizeTable;

				// Modern Warfare 3 supports inline indicies
				Anim->SupportsInlineIndicies = true;

				// Translate it!
				auto Translated = CoDXAnimTranslator::TranslateXAnim(Anim);
				// Export if we translated
				if (Translated != nullptr)
				{
					// Save to XAnim
					if (GameOnline::ExportConfiguration.XAnimsWAW)
					{
						auto XAnPath = FileSystems::CombinePath(AnimPath, AnimName);

						if (!FileSystems::FileExists(XAnPath))
							XAnimRaw::ExportXAnimRaw(*Translated.get(), FileSystems::CombinePath(AnimPath, AnimName), XAnimRawVersion::WorldAtWar);
					}
					else if (GameOnline::ExportConfiguration.XAnimsBO)
					{
						auto XAnPath = FileSystems::CombinePath(AnimPath, AnimName);

						if (!FileSystems::FileExists(XAnPath))
							XAnimRaw::ExportXAnimRaw(*Translated.get(), FileSystems::CombinePath(AnimPath, AnimName), XAnimRawVersion::BlackOps);
					}

					// Scale it
					Translated->ScaleAnimation(2.54f);

					// Save to SEAnim
					if (GameOnline::ExportConfiguration.SEAnims)
					{
						auto SEPath = FileSystems::CombinePath(AnimPath, AnimName + ".seanim");
						
						if (!FileSystems::FileExists(SEPath))
							SEAnim::ExportSEAnim(*Translated.get(), SEPath);
					}

					// Log
					Console::WriteLineHeader("Exporter", "Exported \"%s\"", AnimName.c_str());
				}
				else
				{
					// Failed to translate
					Console::SetBackgroundColor(ConsoleColor::Red);
					Console::WriteLineHeader("Exporter", "Failed \"%s\"", AnimName.c_str());
					Console::SetBackgroundColor(ConsoleColor::Black);
				}
			}

			// Advance
			AnimationOffset += sizeof(MW3XAnim);
		}
	}

	if (Models)
	{
		// Models are the second offset and second pool, skip 4 byte pointer to free head
		auto ModelOffset = GameOnline::GameOffsetInfos[1] + 4;
		auto ModelCount = GameOnline::GamePoolSizes[1];

		// Calculate maximum pool size
		auto MaximumPoolOffset = (ModelCount * sizeof(MW2XModel)) + ModelOffset;
		// Store original offset
		auto MinimumPoolOffset = GameOnline::GameOffsetInfos[1];

		// Store the placeholder model
		MW2XModel PlaceholderModel;
		// Clear it out
		std::memset(&PlaceholderModel, 0, sizeof(PlaceholderModel));

		// Loop and read
		for (uint32_t i = 0; i < ModelCount; i++)
		{
			// Read
			auto ModelResult = GameOnline::GameInstance->Read<MW2XModel>(ModelOffset);

			// Check whether or not to skip, if the handle is 0, or, if the handle is a pointer within the current pool
			if ((ModelResult.NamePtr > MinimumPoolOffset && ModelResult.NamePtr < MaximumPoolOffset) || ModelResult.NamePtr == 0)
			{
				// Advance
				ModelOffset += sizeof(MW2XModel);
				// Skip this asset
				continue;
			}

			// Validate and load if need be
			auto ModelName = FileSystems::GetFileName(GameOnline::GameInstance->ReadNullTerminatedString(ModelResult.NamePtr));

			// Check placeholder configuration, "void" is the base xmodel
			if (ModelName == "void")
			{
				// Set as placeholder model
				PlaceholderModel = ModelResult;
			}
			else if (ModelResult.BoneIDsPtr == PlaceholderModel.BoneIDsPtr && ModelResult.ParentListPtr == PlaceholderModel.ParentListPtr && ModelResult.RotationsPtr == PlaceholderModel.RotationsPtr && ModelResult.TranslationsPtr == PlaceholderModel.TranslationsPtr && ModelResult.PartClassificationPtr == PlaceholderModel.PartClassificationPtr && ModelResult.BaseMatriciesPtr == PlaceholderModel.BaseMatriciesPtr && ModelResult.NumLods == PlaceholderModel.NumLods && ModelResult.MaterialHandlesPtr == PlaceholderModel.MaterialHandlesPtr && ModelResult.NumBones == PlaceholderModel.NumBones)
			{
				// Advance
				ModelOffset += sizeof(MW2XModel);
				// Skip this asset
				continue;
			}

			// Prepare to export this asset, by loading the XModel_t and exporting!
			{
				auto LoadResult = ReadXModel(ModelResult, ModelName);
				// If we loaded, prepare to translate
				if (LoadResult != nullptr)
				{
					// Create our usual model specific directory
					auto ModelSpecific = FileSystems::CombinePath(ModelPath, ModelName);
					FileSystems::CreateDirectory(ModelSpecific);

					// Ensure we have lods
					if (LoadResult->ModelLods.size() > 0)
					{
						// Prepare material images
						for (auto& LOD : LoadResult->ModelLods)
						{
							// Iterate over all materials for the lod
							for (auto& Material : LOD.Materials)
							{
								// Process the material
								ExportMaterialImages(Material, ModelSpecific);

								// Build extension
								auto ImageExtension = (GameOnline::ExportConfiguration.PNG) ? ".png" : ".dds";

								// Apply image paths
								for (auto& Image : Material.Images)
								{
									// Append the relative path and image extension here, since we are done with these images
									Image.ImageName = "_images\\\\" + Image.ImageName + ImageExtension;
								}
							}
						}

						// Calculate the biggest lod
						auto BiggestLod = CoDXModelTranslator::CalculateBiggestLodIndex(LoadResult);
						
						// Only translate if we have a biggest lod
						if (BiggestLod >= 0)
						{
							// Translate the xmodel
							auto Translated = CoDXModelTranslator::TranslateXModel(LoadResult, BiggestLod);

							// Ensure that we translated the model
							if (Translated != nullptr)
							{
								// Save to SMD
								if (GameOnline::ExportConfiguration.SMD)
								{
									auto SmdPath = FileSystems::CombinePath(ModelSpecific, Translated->AssetName + ".smd");

									if (!FileSystems::FileExists(SmdPath))
										ValveSMD::ExportSMD(*Translated.get(), SmdPath);
								}
									

								// Save to XME
								if (GameOnline::ExportConfiguration.XME)
								{
									auto XmePath = FileSystems::CombinePath(ModelSpecific, Translated->AssetName + ".XMODEL_EXPORT");

									if (!FileSystems::FileExists(XmePath))
										CodXME::ExportXME(*Translated.get(), XmePath);
								}

								// Scale it
								Translated->ScaleModel(2.54f);

								// Save to MA
								if (GameOnline::ExportConfiguration.Maya)
								{
									auto MaPath = FileSystems::CombinePath(ModelSpecific, Translated->AssetName + ".ma");
									
									if (!FileSystems::FileExists(MaPath))
										Maya::ExportMaya(*Translated.get(), MaPath);
								}

								// Save to obj
								if (GameOnline::ExportConfiguration.OBJ)
								{
									auto OBJPath = FileSystems::CombinePath(ModelSpecific, Translated->AssetName + ".obj");

									if (!FileSystems::FileExists(OBJPath))
										WavefrontOBJ::ExportOBJ(*Translated.get(), OBJPath);
								}

								// Save to XNA
								if (GameOnline::ExportConfiguration.XNA)
								{
									auto XnaPath = FileSystems::CombinePath(ModelSpecific, Translated->AssetName + ".mesh.ascii");
									
									if (!FileSystems::FileExists(XnaPath))
										XNALara::ExportXNA(*Translated.get(), XnaPath);
								}

								// Log
								Console::WriteLineHeader("Exporter", "Exported \"%s\"", ModelName.c_str());
							}
							else
							{
								// Failed to translate
								Console::SetBackgroundColor(ConsoleColor::Red);
								Console::WriteLineHeader("Exporter", "Failed \"%s\"", ModelName.c_str());
								Console::SetBackgroundColor(ConsoleColor::Black);
							}
						}
					}
				}
			}

			// Advance
			ModelOffset += sizeof(MW2XModel);
		}
	}

	if (Images)
	{
		// Images are the third offset and third pool, skip 4 byte pointer to free head
		auto ImageOffset = GameOnline::GameOffsetInfos[2] + 4;
		auto ImageCount = GameOnline::GamePoolSizes[2];

		// Calculate maximum pool size
		auto MaximumPoolOffset = (ImageCount * sizeof(MW3GfxImage)) + ImageOffset;
		// Store original offset
		auto MinimumPoolOffset = GameOnline::GameOffsetInfos[2];

		// Loop and read
		for (uint32_t i = 0; i < ImageCount; i++)
		{
			// Read
			auto ImageResult = GameOnline::GameInstance->Read<MW3GfxImage>(ImageOffset);

			// Check whether or not to skip, if the handle is 0, or, if the handle is a pointer within the current pool
			if ((ImageResult.FreeHeadPtr > MinimumPoolOffset && ImageResult.FreeHeadPtr < MaximumPoolOffset) || ImageResult.NamePtr == 0)
			{
				// Advance
				ImageOffset += sizeof(MW3GfxImage);
				// Skip this asset
				continue;
			}

			// Validate and load if need be
			auto ImageName = FileSystems::GetFileName(GameOnline::GameInstance->ReadNullTerminatedString(ImageResult.NamePtr));

			// If we're gonna extract, prepare to extract it...
			uint32_t ResultSize = 0;
			// Load the image, if possible
			auto LoadResult = GameOnline::IFSLibrary->ReadFileEntry(ImageName + ".iwi", ResultSize);

			// If we loaded, convert to a DDS
			if (LoadResult != nullptr)
			{
				// Convert it
				auto IWIConv = CoDIWITranslator::TranslateIWI(LoadResult, ResultSize);

				// On success, write to format
				if (IWIConv != nullptr)
				{
					// Save to PNG
					if (GameOnline::ExportConfiguration.PNG)
						Image::ConvertImageMemory(IWIConv->DataBuffer, IWIConv->DataSize, ImageFormat::DDS_WithHeader, FileSystems::CombinePath(ImagePath, ImageName + ".png"), ImageFormat::Standard_PNG);

					// Save to DDS
					if (GameOnline::ExportConfiguration.DDS)
					{
						// Prepare writer
						auto Writer = BinaryWriter();
						// Create new image
						Writer.Create(FileSystems::CombinePath(ImagePath, ImageName + ".dds"));

						// Write it
						Writer.Write(IWIConv->DataBuffer, IWIConv->DataSize);
					}

					// Log
					Console::WriteLineHeader("Exporter", "Exported \"%s\"", ImageName.c_str());
				}
			}

			// Advance
			ImageOffset += sizeof(MW3GfxImage);
		}
	}

	if (Sounds)
	{
		// Sounds are the fourth offset and fourth pool, skip 4 byte pointer to free head
		auto SoundOffset = GameOnline::GameOffsetInfos[3] + 4;
		auto SoundCount = GameOnline::GamePoolSizes[3];

		// Calculate maximum pool size
		auto MaximumPoolOffset = (SoundCount * sizeof(MW2SoundList)) + SoundOffset;
		// Store original offset
		auto MinimumPoolOffset = GameOnline::GameOffsetInfos[3];

		// Loop and read
		for (uint32_t i = 0; i < SoundCount; i++)
		{
			// Read
			auto SoundResult = GameOnline::GameInstance->Read<MW2SoundList>(SoundOffset);

			// Check whether or not to skip, if the handle is 0, or, if the handle is a pointer within the current pool
			if ((SoundResult.NamePtr > MinimumPoolOffset && SoundResult.NamePtr < MaximumPoolOffset) || SoundResult.NamePtr == 0)
			{
				// Advance
				SoundOffset += sizeof(MW2SoundList);
				// Skip this asset
				continue;
			}

			// Validate and load if need be
			auto SoundName = FileSystems::GetFileName(GameOnline::GameInstance->ReadNullTerminatedString(SoundResult.NamePtr));

			// Make sure we have results
			if (SoundResult.AliasPtr != 0 && SoundResult.Count > 0)
			{
				// Load the sound
				auto SoundHeader = GameOnline::GameInstance->Read<MW2Sound>(SoundResult.AliasPtr);

				// If the sound is loaded, read it
				if (SoundHeader.SoundFile > 0)
				{
					auto SoundFile = GameOnline::GameInstance->Read<MW2SoundFile>(SoundHeader.SoundFile);

					// We can rip it if it's type 1, and exists (Loaded)
					if (SoundFile.Exists > 0 && SoundFile.Type == 1 && SoundFile.SoundPtr > 0)
					{
						// We must read the info, build header, then write the data
						auto LoadedSound = GameOnline::GameInstance->Read<MW2LoadedSound>(SoundFile.SoundPtr);

						// Write it if not exists
						auto FilePath = FileSystems::CombinePath(SoundsPath, SoundName + ".wav");

						if (!FileSystems::FileExists(FilePath))
						{
							// Write it
							auto Writer = BinaryWriter();
							Writer.Create(FilePath);

							// Check for ADPCM spec
							if (LoadedSound.Format == 0x11)
							{
								// Build header
								Sound::WriteIMAHeaderToFile(Writer, LoadedSound.FrameRate, LoadedSound.ChannelCount, LoadedSound.BitsPerSample, LoadedSound.BlockAlign, LoadedSound.DataSize);

								// Write audio
								uintptr_t ReadResult = 0;
								auto AudioData = GameOnline::GameInstance->Read(LoadedSound.DataPtr, LoadedSound.DataSize, ReadResult);

								// Write it
								if (AudioData != nullptr)
								{
									Writer.Write(AudioData, (uint32_t)ReadResult);
									// Clean up
									delete[] AudioData;
								}
							}
							else
							{
								// Build header
								Sound::WriteWAVHeaderToFile(Writer, LoadedSound.FrameRate, LoadedSound.ChannelCount, LoadedSound.DataSize);

								// Write audio
								uintptr_t ReadResult = 0;
								auto AudioData = GameOnline::GameInstance->Read(LoadedSound.DataPtr, LoadedSound.DataSize, ReadResult);

								// Write it
								if (AudioData != nullptr)
								{
									Writer.Write(AudioData, (uint32_t)ReadResult);
									// Clean up
									delete[] AudioData;
								}
							}
						}

						// Log
						Console::WriteLineHeader("Exporter", "Exported \"%s\"", SoundName.c_str());
					}
				}
			}

			// Advance
			SoundOffset += sizeof(MW2SoundList);
		}
	}
}

std::unique_ptr<XModel_t> GameOnline::ReadXModel(MW2XModel& ModelData, const std::string& Name)
{
	// Prepare to read the xmodel (Reserving space for lods)
	auto ModelAsset = std::make_unique<XModel_t>(ModelData.NumLods);

	// Copy over default properties
	ModelAsset->ModelName = Name;
	// Bone counts
	ModelAsset->BoneCount = ModelData.NumBones;
	ModelAsset->RootBoneCount = ModelData.NumRootBones;

	// Bone data type
	ModelAsset->BoneRotationData = BoneDataTypes::DivideBySize;

	// Bone id info
	ModelAsset->BoneIDsPtr = ModelData.BoneIDsPtr;
	ModelAsset->BoneIndexSize = 2;

	// Bone parent info
	ModelAsset->BoneParentsPtr = ModelData.ParentListPtr;
	ModelAsset->BoneParentSize = 1;

	// Local bone pointers
	ModelAsset->RotationsPtr = ModelData.RotationsPtr;
	ModelAsset->TranslationsPtr = ModelData.TranslationsPtr;

	// Global matricies
	ModelAsset->BaseMatriciesPtr = ModelData.BaseMatriciesPtr;

	// Whether or not a mesh was loaded
	bool HadLoadedMesh = false;

	// Prepare to parse lods
	for (uint32_t i = 0; i < ModelData.NumLods; i++)
	{
		// Grab the stream lod data
		auto XStreamLodPtr = ModelData.ModelLods[i].StreamLodPtr;
		// Read the header
		auto XStreamLod = GameOnline::GameInstance->Read<MW2XModelStreamLod>(XStreamLodPtr);

		// Assign loaded information
		if (XStreamLod.SurfsPtr == 0)
			continue;

		// We had a loaded mesh
		HadLoadedMesh = true;

		// Create the lod and grab reference
		ModelAsset->ModelLods.emplace_back(ModelData.ModelLods[i].NumSurfs);
		// Grab reference
		auto& LodReference = ModelAsset->ModelLods.back();

		// Set distance
		LodReference.LodDistance = ModelData.ModelLods[i].LodDistance;

		// Grab pointer from the lod itself
		auto XSurfacePtr = XStreamLod.SurfsPtr;

		// Load surfaces
		for (uint32_t s = 0; s < ModelData.ModelLods[i].NumSurfs; s++)
		{
			// Create the surface and grab reference
			LodReference.Submeshes.emplace_back();
			// Grab reference
			auto& SubmeshReference = LodReference.Submeshes[s];

			// Read the surface data
			auto SurfaceInfo = GameOnline::GameInstance->Read<MW2XModelSurface>(XSurfacePtr);

			// Apply surface info
			SubmeshReference.VertListcount = SurfaceInfo.VertListCount;
			SubmeshReference.RigidWeightsPtr = SurfaceInfo.RigidWeightsPtr;
			SubmeshReference.VertexCount = SurfaceInfo.VertexCount;
			SubmeshReference.FaceCount = SurfaceInfo.FacesCount;
			SubmeshReference.VertexPtr = SurfaceInfo.VerticiesPtr;
			SubmeshReference.FacesPtr = SurfaceInfo.FacesPtr;

			// Assign weights
			SubmeshReference.WeightCounts[0] = SurfaceInfo.WeightCounts[0];
			SubmeshReference.WeightCounts[1] = SurfaceInfo.WeightCounts[1];
			SubmeshReference.WeightCounts[2] = SurfaceInfo.WeightCounts[2];
			SubmeshReference.WeightCounts[3] = SurfaceInfo.WeightCounts[3];
			// Weight pointer
			SubmeshReference.WeightsPtr = SurfaceInfo.WeightsPtr;

			// Read this submesh's material handle
			auto MaterialHandle = GameOnline::GameInstance->Read<uint32_t>(ModelData.MaterialHandlesPtr);
			// Create the material and add it
			LodReference.Materials.emplace_back(ReadXMaterial(MaterialHandle));

			// Advance
			XSurfacePtr += sizeof(MW2XModelSurface);
			ModelData.MaterialHandlesPtr += sizeof(uint32_t);
		}
	}

	// Return on success
	if (HadLoadedMesh)
		return ModelAsset;

	// Failed
	return nullptr;
}

const XMaterial_t GameOnline::ReadXMaterial(uint64_t MaterialPointer)
{
	// Prepare to parse the material
	auto MaterialData = GameOnline::GameInstance->Read<MW2XMaterial>(MaterialPointer);

	// Allocate a new material with the given image count
	XMaterial_t Result(MaterialData.ImageCount);
	// Clean the name, then apply it
	Result.MaterialName = FileSystems::GetFileNameWithoutExtension(GameOnline::GameInstance->ReadNullTerminatedString(MaterialData.NamePtr));

	// Iterate over material images, assign proper references if available
	for (uint32_t m = 0; m < MaterialData.ImageCount; m++)
	{
		// Read the image info
		auto ImageInfo = GameOnline::GameInstance->Read<MW2XMaterialImage>(MaterialData.ImageTablePtr);
		// Read the image name (End of image - 4)
		auto ImageName = GameOnline::GameInstance->ReadNullTerminatedString(GameOnline::GameInstance->Read<uint32_t>(ImageInfo.ImagePtr + (sizeof(MW3GfxImage) - 4)));

		// Default type
		auto DefaultUsage = ImageUsageType::Unknown;
		// Check
		switch (ImageInfo.Usage)
		{
		case 2: DefaultUsage = ImageUsageType::DiffuseMap; break;
		case 5: DefaultUsage = ImageUsageType::NormalMap; break;
		case 8: DefaultUsage = ImageUsageType::SpecularMap; break;
		}

		// Assign the new image
		Result.Images.emplace_back(DefaultUsage, ImageInfo.ImagePtr, ImageName);

		// Advance
		MaterialData.ImageTablePtr += sizeof(MW2XMaterialImage);
	}

	// Return it
	return Result;
}

void GameOnline::ExportMaterialImages(const XMaterial_t& Material, const std::string& ModelBasePath)
{
	// Images path
	auto ImageRoot = FileSystems::CombinePath(ModelBasePath, "_images");
	// Create
	FileSystems::CreateDirectory(ImageRoot);

	// Extension
	auto Extension = (GameOnline::ExportConfiguration.PNG) ? ".png" : ".dds";

	// Iterate and export if not exists
	for (auto& Image : Material.Images)
	{
		// Grab the full image path, if it doesn't exist convert it!
		auto FullImagePath = FileSystems::CombinePath(ImageRoot, Image.ImageName + Extension);
		// Check if it exists
		if (!FileSystems::FileExists(FullImagePath))
		{
			// If we're gonna extract, prepare to extract it...
			uint32_t ResultSize = 0;
			// Load the image, if possible
			auto LoadResult = GameOnline::IFSLibrary->ReadFileEntry(Image.ImageName + ".iwi", ResultSize);

			// If we loaded, convert to a DDS
			if (LoadResult != nullptr)
			{
				// Convert it
				auto IWIConv = CoDIWITranslator::TranslateIWI(LoadResult, ResultSize);

				// On success, write to format
				if (IWIConv != nullptr)
				{
					// Patch
					auto ImagePatch = (Image.ImageUsage == ImageUsageType::NormalMap) ? ImagePatch::Normal_Bumpmap : ImagePatch::NoPatch;

					// Save to PNG
					if (GameOnline::ExportConfiguration.PNG)
						Image::ConvertImageMemory(IWIConv->DataBuffer, IWIConv->DataSize, ImageFormat::DDS_WithHeader, FullImagePath, ImageFormat::Standard_PNG, ImagePatch);

					// Save to DDS
					if (GameOnline::ExportConfiguration.DDS)
					{
						try
						{
							// Prepare writer
							auto Writer = BinaryWriter();
							// Create new image
							Writer.Create(FullImagePath);

							// Write it
							Writer.Write(IWIConv->DataBuffer, IWIConv->DataSize);
						}
						catch (...)
						{
							// Nothing, already in access
						}
					}
				}
			}
		}
	}
}

std::string GameOnline::LoadStringHandler(uint64_t Index)
{
	// Load the tag names
	return GameOnline::GameInstance->ReadNullTerminatedString((20 * Index) + GameOnline::GameOffsetInfos[4] + 4);
}
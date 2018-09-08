#include "stdafx.h"

#include "CoDXAssets.h"

XAnim_t::XAnim_t()
{
	// Defaults
	AnimationName = "";
	FrameRate = 30.0f;
	FrameCount = 0;
	ViewModelAnimation = false;
	LoopingAnimation = false;
	AdditiveAnimation = false;
	SupportsInlineIndicies = true;
	BoneIDsPtr = 0;
	BoneIndexSize = 2;
	BoneTypeSize = 0;
	DataBytesPtr = 0;
	DataShortsPtr = 0;
	DataIntsPtr = 0;
	RandomDataBytesPtr = 0;
	RandomDataShortsPtr = 0;
	RandomDataIntsPtr = 0;
	LongIndiciesPtr = 0;
	NotificationsPtr = 0;
	DeltaTranslationPtr = 0;
	Delta2DRotationsPtr = 0;
	Delta3DRotationsPtr = 0;
	RotationType = AnimationKeyTypes::DivideBySize;
	TranslationType = AnimationKeyTypes::MinSizeTable;
}

XMaterial_t::XMaterial_t(uint32_t ImageCount)
{
	// Defaults
	MaterialName = "wraith_material";
	// Preallocate images
	Images.reserve(ImageCount);
}

XImage_t::XImage_t(ImageUsageType Usage, uint64_t Pointer, const std::string& Name)
{
	// Defaults
	ImageUsage = Usage;
	ImagePtr = Pointer;
	ImageName = Name;
}

XModelSubmesh_t::XModelSubmesh_t()
{
	// Defaults
	VertListcount = 0;
	RigidWeightsPtr = 0;
	VertexCount = 0;
	FaceCount = 0;
	FacesPtr = 0;
	VertexPtr = 0;
	// Clear
	std::memset(&WeightCounts, 0, sizeof(WeightCounts));
	// Set
	WeightsPtr = 0;
	MaterialIndex = -1;
}

XModelLod_t::XModelLod_t(uint16_t SubmeshCount)
{
	// Reserve memory for the count we have
	Submeshes.reserve(SubmeshCount);
	Materials.reserve(SubmeshCount);
	// Defaults
	LodDistance = 100.0f;
	LodMaxDistance = 200.0f;
	LODStreamKey = 0;
	LODStreamInfoPtr = 0;
}

XModel_t::XModel_t(uint16_t LodCount)
{
	// Reserve memory for the count we have
	ModelLods.reserve(LodCount);
	// Defaults
	ModelName = "";
	BoneRotationData = BoneDataTypes::DivideBySize;
	BoneCount = 0;
	RootBoneCount = 0;
	CosmeticBoneCount = 0;
	BoneIDsPtr = 0;
	BoneIndexSize = 2;
	BoneParentsPtr = 0;
	BoneParentSize = 1;
	RotationsPtr = 0;
	TranslationsPtr = 0;
	BaseMatriciesPtr = 0;
	BoneInfoPtr = 0;
	IsModelStreamed = false;
}

XImageDDS::XImageDDS()
{
	// Defaults
	DataBuffer = nullptr;
	DataSize = 0;
	ImagePatchType = ImagePatch::NoPatch;
}

XImageDDS::~XImageDDS()
{
	// Clean up if need be
	if (DataBuffer != nullptr)
	{
		// Delete it
		delete[] DataBuffer;
	}
}
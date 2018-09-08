#pragma once

#include <cstdint>
#include <memory>

// We need the image class
#include "Image.h"

// Types of key data
enum class AnimationKeyTypes
{
	DivideBySize,
	MinSizeTable,
	HalfFloat
};

// A structure that represents an xanim from Call of Duty
struct XAnim_t
{
	// Constructor
	XAnim_t();

	// The name of the asset
	std::string AnimationName;

	// The framerate of this animation
	float FrameRate;
	// The framecount of this animation
	uint32_t FrameCount;

	// Whether or not this is a viewmodel animation
	bool ViewModelAnimation;
	// Whether or not this animation loops
	bool LoopingAnimation;
	// Whether or not this animation is additive
	bool AdditiveAnimation;
	// Whether or not we support inline-indicies
	bool SupportsInlineIndicies;

	// A pointer to bone name string indicies
	uint64_t BoneIDsPtr;
	// The size of the bone name index
	uint8_t BoneIndexSize;
	// The size of the inline bone indicies (0) when not in use
	uint8_t BoneTypeSize;

	// What type of rotation data we have
	AnimationKeyTypes RotationType;
	// What type of translation data we have
	AnimationKeyTypes TranslationType;

	// A pointer to animation byte size data
	uint64_t DataBytesPtr;
	// A pointer to animation short size data
	uint64_t DataShortsPtr;
	// A pointer to animation integer size data
	uint64_t DataIntsPtr;

	// A pointer to animation (rand) byte size data
	uint64_t RandomDataBytesPtr;
	// A pointer to animation (rand) short size data
	uint64_t RandomDataShortsPtr;
	// A pointer to animation (rand) integer size data
	uint64_t RandomDataIntsPtr;

	// A pointer to animation indicies (When we have more than 255)
	uint64_t LongIndiciesPtr;
	// A pointer to animation notetracks
	uint64_t NotificationsPtr;

	// A pointer to animation delta translations
	uint64_t DeltaTranslationPtr;
	// A pointer to 2D animation delta rotations
	uint64_t Delta2DRotationsPtr;
	// A pointer to 3D animation delta rotations
	uint64_t Delta3DRotationsPtr;

	// The count of non-rotated bones
	uint32_t NoneRotatedBoneCount;
	// The count of 2D rotated bones
	uint32_t TwoDRotatedBoneCount;
	// The count of 3D rotated bones
	uint32_t NormalRotatedBoneCount;
	// The count of 2D static rotated bones
	uint32_t TwoDStaticRotatedBoneCount;
	// The count of 3D static rotated bones
	uint32_t NormalStaticRotatedBoneCount;
	// The count of normal translated bones
	uint32_t NormalTranslatedBoneCount;
	// The count of precise translated bones
	uint32_t PreciseTranslatedBoneCount;
	// The count of static translated bones
	uint32_t StaticTranslatedBoneCount;
	// The count of non-translated bones
	uint32_t NoneTranslatedBoneCount;
	// The total bone count
	uint32_t TotalBoneCount;
	// The count of notetracks
	uint32_t NotificationCount;
};

// Types of bone data
enum class BoneDataTypes
{
	DivideBySize,
	HalfFloat
};

struct XModelSubmesh_t
{
	// Constructor
	XModelSubmesh_t();

	// Count of rigid vertex pairs
	uint32_t VertListcount;
	// A pointer to rigid weights
	uint64_t RigidWeightsPtr;

	// Count of verticies
	uint32_t VertexCount;
	// Count of faces
	uint32_t FaceCount;

	// Pointer to faces
	uint64_t FacesPtr;
	// Pointer to verticies
	uint64_t VertexPtr;

	// A list of weights
	uint16_t WeightCounts[8];
	// A pointer to weight data
	uint64_t WeightsPtr;

	// The index of the material
	int32_t MaterialIndex;
};

// Types of images
enum class ImageUsageType : uint8_t
{
	Unknown,
	DiffuseMap,
	NormalMap,
	SpecularMap,
	GlossMap
};

struct XImage_t
{
	// Constructor
	XImage_t(ImageUsageType Usage, uint64_t Pointer, const std::string& Name);

	// The usage of this image asset
	ImageUsageType ImageUsage;
	// The pointer to the image asset
	uint64_t ImagePtr;

	// The name of this image asset
	std::string ImageName;
};

struct XMaterial_t
{
	// Constructor
	XMaterial_t(uint32_t ImageCount);

	// The material name
	std::string MaterialName;

	// A list of images
	std::vector<XImage_t> Images;
};

struct XModelLod_t
{
	// Constructors
	XModelLod_t(uint16_t SubmeshCount);

	// A list of submeshes for this specific lod
	std::vector<XModelSubmesh_t> Submeshes;
	// A list of material info per-submesh
	std::vector<XMaterial_t> Materials;

	// A key used for the lod data if it's streamed
	uint64_t LODStreamKey;
	// A pointer used for the stream mesh info
	uint64_t LODStreamInfoPtr;

	// The distance this lod displays at
	float LodDistance;
	// The max distance this lod displays at
	float LodMaxDistance;
};

struct XModel_t
{
	// Constructor
	XModel_t(uint16_t LodCount);

	// The name of the asset
	std::string ModelName;

	// The type of data used for bone rotations
	BoneDataTypes BoneRotationData;

	// Whether or not we use the stream mesh loader
	bool IsModelStreamed;

	// The model bone count
	uint32_t BoneCount;
	// The model root bone count
	uint32_t RootBoneCount;
	// The model cosmetic bone count
	uint32_t CosmeticBoneCount;

	// A pointer to bone name string indicies
	uint64_t BoneIDsPtr;
	// The size of the bone name index
	uint8_t BoneIndexSize;

	// A pointer to bone parent indicies
	uint64_t BoneParentsPtr;
	// The size of the bone parent index
	uint8_t BoneParentSize;

	// A pointer to local rotations
	uint64_t RotationsPtr;
	// A pointer to local positions
	uint64_t TranslationsPtr;

	// A pointer to global matricies
	uint64_t BaseMatriciesPtr;

	// A pointer to the bone collision data, hitbox offsets
	uint64_t BoneInfoPtr;

	// A list of lods per this model
	std::vector<XModelLod_t> ModelLods;
};

struct XImageDDS
{
	// Constructors
	XImageDDS();
	~XImageDDS();

	// The DDS data buffer
	int8_t* DataBuffer;
	// The size of the DDS buffer
	uint32_t DataSize;

	// The requested image patch type
	ImagePatch ImagePatchType;
};
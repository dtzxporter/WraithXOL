#pragma once

#include <cstdint>

// We need the following classes
#include <VectorMath.h>

// -- Contains structures for various shared game structures

struct DObjAnimMat
{
	Quaternion Rotation;
	Vector3 Translation;
	float TranslationWeight;
};

union GfxPackedUnitVec
{
	uint32_t PackedInteger;
	int8_t PackedBytes[4];
};

struct GfxVertexBuffer
{
	Vector3 Position;

	uint32_t BiNormal;
	uint32_t ColorRGBA;

	uint16_t UVUPos;
	uint16_t UVVPos;

	uint32_t Normal;
	uint32_t Tangent;
};

struct GfxFaceBuffer
{
	uint16_t Index1;
	uint16_t Index2;
	uint16_t Index3;
};

struct GfxRigidVerts
{
	uint16_t BoneIndex;
	uint16_t VertexCount;

	uint16_t FacesCount;
	uint16_t FacesIndex;

	uint32_t SurfaceCollisionPtr;
};

struct GfxRigidVerts64
{
	uint16_t BoneIndex;
	uint16_t VertexCount;

	uint16_t FacesCount;
	uint16_t FacesIndex;

	uint64_t SurfaceCollisionPtr;
};

struct QuatData
{
	int16_t RotationX;
	int16_t RotationY;
	int16_t RotationZ;
	int16_t RotationW;
};

struct Quat2Data
{
	int16_t RotationZ;
	int16_t RotationW;
};

struct __declspec(align(4)) GfxBoneInfo
{
	float Bounds[2][3];
	float Offset[3];
	float RadiusSquared;
	uint8_t Collmap;
};

#pragma pack(push, 1)
struct MW3XAnim
{
	uint32_t NamePtr;

	uint8_t Padding[10];

	uint16_t NumFrames;
	uint8_t Looped;

	uint8_t NoneRotatedBoneCount;
	uint8_t TwoDRotatedBoneCount;
	uint8_t NormalRotatedBoneCount;
	uint8_t TwoDStaticRotatedBoneCount;
	uint8_t NormalStaticRotatedBoneCount;
	uint8_t NormalTranslatedBoneCount;
	uint8_t PreciseTranslatedBoneCount;
	uint8_t StaticTranslatedBoneCount;
	uint8_t NoneTranslatedBoneCount;
	uint8_t TotalBoneCount;
	uint8_t NotificationCount;

	uint8_t AssetType;
	uint8_t isDefault;

	uint8_t Padding3[10];

	float Framerate;
	float Frequency;

	uint32_t BoneIDsPtr;
	uint32_t DataBytePtr;
	uint32_t DataShortPtr;
	uint32_t DataIntPtr;
	uint32_t RandomDataShortPtr;
	uint32_t RandomDataBytePtr;
	uint32_t RandomDataIntPtr;
	uint32_t LongIndiciesPtr;
	uint32_t NotificationsPtr;
	uint32_t DeltaPartsPtr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW3GfxImage
{
	uint32_t FreeHeadPtr;

	uint8_t UnknownPadding[40];

	uint32_t UnknownPtr;
	uint32_t NamePtr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW2XMaterial
{
	uint8_t Padding[0x10];

	uint32_t NamePtr;

	uint8_t Padding2[0x3D];

	uint8_t ImageCount;
	
	uint8_t Padding3[10];

	uint32_t ImageTablePtr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW2XMaterialImage
{
	uint8_t Padding[7];

	uint8_t Usage;

	uint32_t ImagePtr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW3XAnimDeltaParts
{
	uint32_t DeltaTranslationsPtr;
	uint32_t Delta2DRotationsPtr;
	uint32_t Delta3DRotationsPtr;
};
#pragma pack(pop)

#pragma pack(push,1)
struct MW2XModelSurface
{
	uint32_t Flags;

	uint16_t VertexCount;
	uint16_t FacesCount;

	uint8_t Padding[8];

	uint32_t FacesPtr;

	uint16_t WeightCounts[4];
	uint32_t WeightsPtr;
	uint32_t VerticiesPtr;

	uint32_t Unknown1;

	uint32_t VertListCount;
	uint32_t RigidWeightsPtr;

	uint8_t Padding2[0x24];
};
#pragma pack(pop)

#pragma pack(push,1)
struct MW2XModelStreamLod
{
	uint32_t LodNamePtr;
	uint32_t SurfsPtr;

	uint32_t NumSurfs;
	uint32_t Flags;

	uint8_t Padding[0x1C];

	uint32_t UnknownPtr1;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW2XModelLod
{
	float LodDistance;

	uint16_t NumSurfs;
	uint16_t SurfacesIndex;

	uint32_t StreamLodPtr;
	
	uint32_t LodNamePtr;

	uint8_t Padding[32];
	uint32_t SurfsDataPtr;	

	uint8_t Padding2[4];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW2SoundList
{
	uint32_t NamePtr;
	uint32_t AliasPtr;
	uint32_t Count;
};

struct MW2Sound
{
	uint32_t AliasName;
	uint32_t Subtitle;
	uint32_t SecAliasName;
	uint32_t ChainAlias;
	uint32_t MixerGroup;
	uint32_t SoundFile;
	uint32_t Sequence;
};

struct MW2SoundFile
{
	uint8_t Type;
	uint8_t Exists;
	uint16_t Padding;
	uint32_t SoundPtr;
};

struct MW2LoadedSound
{
	uint32_t NamePtr;
	uint32_t Format;
	uint32_t SeekSize;
	uint32_t DataSize;
	uint32_t FrameRate;
	uint32_t BitsPerSample;
	uint32_t ChannelCount;
	uint32_t Unknown3;	// -1
	uint32_t Unknown2;	// ???
	uint32_t BlockAlign;
	uint32_t SeekTablePtr;
	uint32_t DataPtr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MW2XModel
{
	uint32_t NamePtr;
	uint8_t NumBones;
	uint8_t NumRootBones;
	uint8_t NumSurfaces;
	uint8_t LodRampType;

	uint8_t Padding[0x24];

	uint32_t BoneIDsPtr;
	uint32_t ParentListPtr;
	uint32_t RotationsPtr;
	uint32_t TranslationsPtr;
	uint32_t PartClassificationPtr;
	uint32_t BaseMatriciesPtr;
	uint32_t MaterialHandlesPtr;

	MW2XModelLod ModelLods[4];

	uint8_t Padding2;
	uint8_t NumLods;
	uint8_t MaxLods;

	uint8_t Padding3[0x3D];
};
#pragma pack(pop)

// -- Verify structures

static_assert(sizeof(DObjAnimMat) == 0x20, "Invalid DObjAnimMat Size (Expected 0x20)");
static_assert(sizeof(QuatData) == 0x8, "Invalid QuatData Size (Expected 0x8)");
static_assert(sizeof(Quat2Data) == 0x4, "Invalid Quat2Data Size (Expected 0x4)");
static_assert(sizeof(GfxVertexBuffer) == 0x20, "Invalid GfxVertexBuffer Size (Expected 0x20)");
static_assert(sizeof(GfxPackedUnitVec) == 0x4, "Invalid GfxPackedUnitVec Size (Expected 0x4)");
static_assert(sizeof(GfxRigidVerts) == 0xC, "Invalid GfxRigidVerts Size (Expected 0xC)");
static_assert(sizeof(GfxRigidVerts64) == 0x10, "Invalid GfxRigidVerts64 Size (Expected 0x10)");
static_assert(sizeof(GfxBoneInfo) == 0x2C, "Invalid GfxBoneInfo Size (Expected 0x2C)");

// -- End reading structures
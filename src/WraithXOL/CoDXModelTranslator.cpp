#include "stdafx.h"

// The class we are implementing
#include "CoDXModelTranslator.h"
#include "GameOnline.h"

// We need the following WraithX classes
#include "InjectionReader.h"
#include "MemoryReader.h"
#include "HalfFloats.h"
#include "Strings.h"

// Include generic structures
#include "DBGameGenerics.h"

std::unique_ptr<WraithModel> CoDXModelTranslator::TranslateXModel(const std::unique_ptr<XModel_t>& Model, uint32_t LodIndex, bool JustBones)
{
	// Prepare to generate a WraithModel
	auto ModelResult = std::make_unique<WraithModel>();

	// Apply name
	ModelResult->AssetName = Model->ModelName;

	// Apply distances
	ModelResult->LodDistance = Model->ModelLods[LodIndex].LodDistance;
	ModelResult->LodMaxDistance = Model->ModelLods[LodIndex].LodMaxDistance;

	// Bone matrix size
	uintptr_t ReadDataSize = 0;
	// Calculated matricies size
	auto GlobalMatrixLength = (sizeof(DObjAnimMat) * (Model->BoneCount + Model->CosmeticBoneCount));
	auto LocalTranslationLength = (sizeof(Vector3) * ((Model->BoneCount + Model->CosmeticBoneCount) - Model->RootBoneCount));
	auto LocalRotationLength = (sizeof(QuatData) * ((Model->BoneCount + Model->CosmeticBoneCount) - Model->RootBoneCount));
	
	// Read all bone matricies
	auto GlobalMatrixPtr = GameOnline::GameInstance->Read(Model->BaseMatriciesPtr, GlobalMatrixLength, ReadDataSize);
	auto GlobalMatrixData = MemoryReader(GlobalMatrixPtr, ReadDataSize);

	auto LocalTranslationPtr = GameOnline::GameInstance->Read(Model->TranslationsPtr, LocalTranslationLength, ReadDataSize);
	auto LocalTranslationData = MemoryReader(LocalTranslationPtr, ReadDataSize);

	auto LocalRotationPtr = GameOnline::GameInstance->Read(Model->RotationsPtr, LocalRotationLength, ReadDataSize);
	auto LocalRotationData = MemoryReader(LocalRotationPtr, ReadDataSize);

	// Whether or not use bone was ticked
	bool NeedsLocalPositions = true;

	// Grab the bone ids pointer
	auto BoneIDs = Model->BoneIDsPtr;
	// Grab the bone parent pointer
	auto BoneParents = Model->BoneParentsPtr;

	// Prepare the model for bones
	ModelResult->PrepareBones(Model->BoneCount + Model->CosmeticBoneCount);

	// Iterate and build bones
	for (uint32_t i = 0; i < (Model->BoneCount + Model->CosmeticBoneCount); i++)
	{
		// Read the ID
		uint64_t BoneID = 0;

		// Check size
		switch (Model->BoneIndexSize)
		{
		case 2: BoneID = GameOnline::GameInstance->Read<uint16_t>(BoneIDs); break;
		case 4: BoneID = GameOnline::GameInstance->Read<uint32_t>(BoneIDs); break;
		}

		// Add the new bone
		auto& NewBone = ModelResult->AddBone();

		// Set the new bones name (Determine if we need something else)
		auto BoneName = GameOnline::LoadStringHandler(BoneID);
		// Check for an invalid tag name
		if (BoneName == "")
		{
			// Make a new bone name
			if (i == 0)
			{
				NewBone.TagName = "tag_origin";
			}
			else
			{
				NewBone.TagName = Strings::Format("no_tag_%d", i);
			}
		}
		else
		{
			// Set as read
			NewBone.TagName = BoneName;
		}

		// Read the parent ID (Default as a root bone)
		int32_t BoneParent = -1;

		// Check if we have parent ids yet
		if (i >= Model->RootBoneCount)
		{
			// We have a parent id to read
			switch (Model->BoneParentSize)
			{
			case 1: BoneParent = GameOnline::GameInstance->Read<uint8_t>(BoneParents); break;
			case 2: BoneParent = GameOnline::GameInstance->Read<uint16_t>(BoneParents); break;
			case 4: BoneParent = GameOnline::GameInstance->Read<uint32_t>(BoneParents); break;
			}

			// Check if we're cosmetic bones
			if (i < Model->BoneCount)
			{
				// Subtract position
				BoneParent = (i - BoneParent);
			}
			else
			{
				// Absolute position
				BoneParent = BoneParent;
			}

			// Advance
			BoneParents += Model->BoneParentSize;
		}
		else
		{
			// Take i and subtract 1
			BoneParent = (i - 1);
		}

		// Set bone parent
		NewBone.BoneParent = BoneParent;

		// Read global data
		auto GlobalData = GlobalMatrixData.Read<DObjAnimMat>();
		// Assign global position
		NewBone.GlobalPosition = GlobalData.Translation;
		NewBone.GlobalRotation = GlobalData.Rotation;

		// Check if we aren't a root bone
		if (i > (Model->RootBoneCount - 1))
		{
			// We have local translations and rotations
			NewBone.LocalPosition = LocalTranslationData.Read<Vector3>();

			// Check if it was not zero
			if (NewBone.LocalPosition != Vector3(0, 0, 0))
			{
				// We don't need them
				NeedsLocalPositions = false;
			}

			// Read rotation data
			auto RotationData = LocalRotationData.Read<QuatData>();
			// Build rotation value
			if (Model->BoneRotationData == BoneDataTypes::DivideBySize)
			{
				// Set
				NewBone.LocalRotation = Quaternion((float)RotationData.RotationX / 32768.0f, (float)RotationData.RotationY / 32768.0f, (float)RotationData.RotationZ / 32768.0f, (float)RotationData.RotationW / 32768.0f);
			}
			else if (Model->BoneRotationData == BoneDataTypes::HalfFloat)
			{
				// Set
				NewBone.LocalRotation = Quaternion(HalfFloats::ToFloat((uint16_t)RotationData.RotationX), HalfFloats::ToFloat((uint16_t)RotationData.RotationY), HalfFloats::ToFloat((uint16_t)RotationData.RotationZ), HalfFloats::ToFloat((uint16_t)RotationData.RotationW));
			}
		}

		// Advance
		BoneIDs += Model->BoneIndexSize;
	}

	// Chean up bone data
	GlobalMatrixData.Close();
	LocalTranslationData.Close();
	LocalRotationData.Close();

	// If we're a viewmodel (!UseBones) && we have at least > 1 bone, we need to generate local positions from the globals
	if (NeedsLocalPositions && Model->BoneCount > 1)
	{
		// Generate
		ModelResult->GenerateLocalPositions(true, false);
	}

	// Grab reference to the lod we want to translate
	auto& LodReference = Model->ModelLods[LodIndex];

	// A list of unique material names
	std::map<std::string, uint32_t> UniqueMaterials;

	// Build unique material references (Right now, it's one material per submesh)
	for (uint32_t i = 0; i < (uint32_t)LodReference.Materials.size(); i++)
	{
		// Check if it exists
		auto FindResult = UniqueMaterials.find(LodReference.Materials[i].MaterialName);
		// Check if we have one
		if (FindResult != UniqueMaterials.end())
		{
			// We have it, calculate index and set
			LodReference.Submeshes[i].MaterialIndex = UniqueMaterials[LodReference.Materials[i].MaterialName];
		}
		else
		{
			// Add it
			auto& NewMat = ModelResult->AddMaterial();
			// Assign values
			NewMat.MaterialName = LodReference.Materials[i].MaterialName;

			// Assign image names
			for (auto& Image : LodReference.Materials[i].Images)
			{
				// Check
				switch (Image.ImageUsage)
				{
				case ImageUsageType::DiffuseMap:
					NewMat.DiffuseMapName = Image.ImageName;
					break;
				case ImageUsageType::NormalMap:
					NewMat.NormalMapName = Image.ImageName;
					break;
				case ImageUsageType::SpecularMap:
					NewMat.SpecularMapName = Image.ImageName;
					break;
				}
			}

			// Set
			LodReference.Submeshes[i].MaterialIndex = (uint32_t)(UniqueMaterials.size());

			// Add
			UniqueMaterials.insert(std::make_pair(LodReference.Materials[i].MaterialName, LodReference.Submeshes[i].MaterialIndex));
		}
	}

	// We have a true generic model, no streamed, process data normally
	ModelResult->PrepareSubmeshes((uint32_t)LodReference.Submeshes.size());

	// Iterate over submeshes
	for (auto& Submesh : LodReference.Submeshes)
	{
		// Create and grab a new submesh
		auto& Mesh = ModelResult->AddSubmesh();

		// Set the material (COD has 1 per submesh)
		Mesh.AddMaterial(Submesh.MaterialIndex);

		// Prepare the mesh for the data
		Mesh.PrepareMesh(Submesh.VertexCount, Submesh.FaceCount);

		// Pre-allocate vertex weights (Data defaults to weight 1.0 on bone 0)
		auto VertexWeights = std::vector<WeightsData>(Submesh.VertexCount);
		// Setup weights
		PrepareVertexWeightsA(VertexWeights, Submesh);

		// Mesh buffers size
		uintptr_t ReadDataSize = 0;
		// Calculated buffer size
		auto VerticiesLength = (sizeof(GfxVertexBuffer) * Submesh.VertexCount);
		auto FacesLength = (sizeof(GfxFaceBuffer) * Submesh.FaceCount);

		// Read mesh data
		auto VertexDataPtr = GameOnline::GameInstance->Read(Submesh.VertexPtr, VerticiesLength, ReadDataSize);
		auto VertexData = MemoryReader(VertexDataPtr, ReadDataSize);

		auto FaceDataPtr = GameOnline::GameInstance->Read(Submesh.FacesPtr, FacesLength, ReadDataSize);
		auto FaceData = MemoryReader(FaceDataPtr, ReadDataSize);

		// Iterate over verticies
		for (uint32_t i = 0; i < Submesh.VertexCount; i++)
		{
			// Make a new vertex
			auto& Vertex = Mesh.AddVertex();

			// Read data
			auto VertexInfo = VertexData.Read<GfxVertexBuffer>();
			// Assign data
			Vertex.Position = VertexInfo.Position;

			// Apply UV layer (These games seems to have UVV before UVU) this works on all models
			Vertex.AddUVLayer(HalfFloats::ToFloat(VertexInfo.UVVPos), HalfFloats::ToFloat(VertexInfo.UVUPos));
			// Unpack vertex normal
			Vertex.Normal = UnpackNormalA(VertexInfo.Normal);

			// Assign weights
			auto& WeightValue = VertexWeights[i];

			// Iterate
			for (uint32_t w = 0; w < WeightValue.WeightCount; w++)
			{
				// Add new weight
				Vertex.AddVertexWeight(WeightValue.BoneValues[w], WeightValue.WeightValues[w]);
			}
		}

		// Iterate over faces
		for (uint32_t i = 0; i < Submesh.FaceCount; i++)
		{
			// Read data
			auto FaceIndicies = FaceData.Read<GfxFaceBuffer>();
			// Add the face
			Mesh.AddFace(FaceIndicies.Index1, FaceIndicies.Index2, FaceIndicies.Index3);
		}
	}

	// Return it
	return ModelResult;
}

int32_t CoDXModelTranslator::CalculateBiggestLodIndex(const std::unique_ptr<XModel_t>& Model)
{
	// Fetch lod count
	auto LodCount = (int32_t)Model->ModelLods.size();

	// This function is only valid if we have > 1 lod
	if (LodCount > 1)
	{
		// Perform calculation based on lod properties
		int32_t ResultIndex = 0;

		// Loop over total lods
		for (int32_t i = 0; i < LodCount; i++)
		{
			// Compare surface count and distance
			if ((Model->ModelLods[i].LodDistance < Model->ModelLods[ResultIndex].LodDistance) && (Model->ModelLods[i].Submeshes.size() >= Model->ModelLods[ResultIndex].Submeshes.size()))
			{
				// Set it as the newest result
				ResultIndex = i;
			}
		}

		// Return the biggest index
		return ResultIndex;
	}
	else if (LodCount == 0)
	{
		// There are no lods to export
		return -1;
	}

	// Default to the first lod
	return 0;
}

Vector3 CoDXModelTranslator::UnpackNormalA(uint32_t Normal)
{
	// Unpack a normal, used in [WAW, BO, MW, MW2, MW3]

	// Convert to packed structure
	auto PackedNormal = (GfxPackedUnitVec*)&Normal;

	// Decode the scale of the vector
	float DecodeScale = (float)((float)(uint8_t)PackedNormal->PackedBytes[3] - -192.0) / 32385.0f;

	// Make the vector
	return Vector3(
		(float)((float)(uint8_t)PackedNormal->PackedBytes[0] - 127.0) * DecodeScale,
		(float)((float)(uint8_t)PackedNormal->PackedBytes[1] - 127.0) * DecodeScale,
		(float)((float)(uint8_t)PackedNormal->PackedBytes[2] - 127.0) * DecodeScale);
}

void CoDXModelTranslator::PrepareVertexWeightsA(std::vector<WeightsData>& Weights, const XModelSubmesh_t& Submesh)
{
	// Prepare weights, used in [WAW, BO, BO2, MW, MW2, MW3]

	// The index of read weight data
	uint32_t WeightDataIndex = 0;

	// Prepare the simple, rigid weights
	for (uint32_t i = 0; i < Submesh.VertListcount; i++)
	{
		// Simple weights build, rigid, just apply the proper bone id
		auto RigidInfo = GameOnline::GameInstance->Read<GfxRigidVerts>(Submesh.RigidWeightsPtr + (i * sizeof(GfxRigidVerts)));
		// Apply bone ids properly
		for (uint32_t w = 0; w < RigidInfo.VertexCount; w++)
		{
			// Apply
			Weights[WeightDataIndex].BoneValues[0] = (RigidInfo.BoneIndex / 64);
			// Advance
			WeightDataIndex++;
		}
	}

	// Total weight data read
	uintptr_t ReadDataSize = 0;
	// Calculate the size of weights buffer
	auto WeightsDataLength = ((2 * Submesh.WeightCounts[0]) + (6 * Submesh.WeightCounts[1]) + (10 * Submesh.WeightCounts[2]) + (14 * Submesh.WeightCounts[3]));
	// Read the weight data
	auto WeightsPtr = GameOnline::GameInstance->Read(Submesh.WeightsPtr, WeightsDataLength, ReadDataSize);
	auto WeightsData = MemoryReader(WeightsPtr, ReadDataSize);

	// Prepare single bone weights
	for (uint32_t w = 0; w < Submesh.WeightCounts[0]; w++)
	{
		// Apply
		Weights[WeightDataIndex].BoneValues[0] = (WeightsData.Read<uint16_t>() / 64);
		// Advance
		WeightDataIndex++;
	}

	// Prepare two bone weights
	for (uint32_t w = 0; w < Submesh.WeightCounts[1]; w++)
	{
		// Set size
		Weights[WeightDataIndex].WeightCount = 2;

		// Read IDs (1, 2)
		Weights[WeightDataIndex].BoneValues[0] = (WeightsData.Read<uint16_t>() / 64);
		Weights[WeightDataIndex].BoneValues[1] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 2 and calculate 1
		Weights[WeightDataIndex].WeightValues[1] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		Weights[WeightDataIndex].WeightValues[0] = (1.0f - Weights[WeightDataIndex].WeightValues[1]);

		// Advance
		WeightDataIndex++;
	}

	// Prepare three bone weights
	for (uint32_t w = 0; w < Submesh.WeightCounts[2]; w++)
	{
		// Set size
		Weights[WeightDataIndex].WeightCount = 3;

		// Read 2 IDs (1, 2)
		Weights[WeightDataIndex].BoneValues[0] = (WeightsData.Read<uint16_t>() / 64);
		Weights[WeightDataIndex].BoneValues[1] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 2
		Weights[WeightDataIndex].WeightValues[1] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		// Read 1 ID (3)
		Weights[WeightDataIndex].BoneValues[2] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 3
		Weights[WeightDataIndex].WeightValues[2] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		// Calculate first value
		Weights[WeightDataIndex].WeightValues[0] = (1.0f - (Weights[WeightDataIndex].WeightValues[1] + Weights[WeightDataIndex].WeightValues[2]));

		// Advance
		WeightDataIndex++;
	}

	// Prepare four bone weights
	for (uint32_t w = 0; w < Submesh.WeightCounts[3]; w++)
	{
		// Set size
		Weights[WeightDataIndex].WeightCount = 4;

		// Read 2 IDs (1, 2)
		Weights[WeightDataIndex].BoneValues[0] = (WeightsData.Read<uint16_t>() / 64);
		Weights[WeightDataIndex].BoneValues[1] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 2
		Weights[WeightDataIndex].WeightValues[1] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		// Read 1 ID (3)
		Weights[WeightDataIndex].BoneValues[2] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 3
		Weights[WeightDataIndex].WeightValues[2] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		// Read 1 ID (4)
		Weights[WeightDataIndex].BoneValues[3] = (WeightsData.Read<uint16_t>() / 64);
		// Read value for 4
		Weights[WeightDataIndex].WeightValues[3] = ((float)WeightsData.Read<uint16_t>() / 65536.0f);
		// Calculate first value
		Weights[WeightDataIndex].WeightValues[0] = (1.0f - (Weights[WeightDataIndex].WeightValues[1] + Weights[WeightDataIndex].WeightValues[2] + Weights[WeightDataIndex].WeightValues[3]));

		// Advance
		WeightDataIndex++;
	}
}
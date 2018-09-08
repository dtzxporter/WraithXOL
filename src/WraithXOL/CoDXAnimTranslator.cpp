#include "stdafx.h"

// The class we are implementing
#include "CoDXAnimTranslator.h"
#include "GameOnline.h"

// We need the following WraithX classes
#include "InjectionReader.h"
#include "HalfFloats.h"
#include "Strings.h"

// Generic types
#include "DBGameGenerics.h"

std::unique_ptr<WraithAnim> CoDXAnimTranslator::TranslateXAnim(const std::unique_ptr<XAnim_t>& Animation)
{
	// Prepare to generate a WraithAnim
	auto Anim = std::make_unique<WraithAnim>();

	// Apply name
	Anim->AssetName = Animation->AnimationName;
	// Apply framerate
	Anim->FrameRate = Animation->FrameRate;
	// Apply looping
	Anim->Looping = Animation->LoopingAnimation;

	// Determine animation type
	Anim->AnimType = WraithAnimationType::Relative;
	// Check for viewmodel animations
	if (Animation->ViewModelAnimation)
	{
		// Apply relative tags
		Anim->AddBoneModifier("j_gun", WraithAnimationType::Relative);
		Anim->AddBoneModifier("j_gun1", WraithAnimationType::Relative);

		// Set type to absolute
		Anim->AnimType = WraithAnimationType::Absolute;
	}
	// Check for delta animations
	if (Animation->DeltaTranslationPtr != 0 || Animation->Delta2DRotationsPtr != 0 || Animation->Delta3DRotationsPtr != 0)
	{
		// We have a delta animation
		Anim->AnimType = WraithAnimationType::Delta;
		// Set the delta tag name
		Anim->DeltaTagName = "tag_origin";
	}
	// Check for additive animations
	if (Animation->AdditiveAnimation)
	{
		// Set type to additive (Overrides absolute)
		Anim->AnimType = WraithAnimationType::Additive;
	}

	// Calculate the size of frames and inline bone indicies
	uint32_t FrameSize = (Animation->FrameCount > 255) ? 2 : 1;
	uint32_t BoneTypeSize = (Animation->TotalBoneCount > 255) ? 2 : 1;

	// Override the bone_t size if need be for various games
	if (Animation->BoneTypeSize > 0) { BoneTypeSize = Animation->BoneTypeSize; }

	// A list of bone tag names
	std::vector<std::string> TagNames;
	// Loop and read bone tag names
	for (uint32_t i = 0; i < Animation->TotalBoneCount; i++)
	{
		// Read the ID
		uint64_t BoneID = 0;

		// Check size
		switch (Animation->BoneIndexSize)
		{
		case 2: BoneID = GameOnline::GameInstance->Read<uint16_t>(Animation->BoneIDsPtr); break;
		case 4: BoneID = GameOnline::GameInstance->Read<uint32_t>(Animation->BoneIDsPtr); break;
		}

		// Read the string
		TagNames.emplace_back(GameOnline::LoadStringHandler(BoneID));
		// Advance
		Animation->BoneIDsPtr += Animation->BoneIndexSize;
	}

	// DEBUG CODE:
#ifdef _DEBUG
	bool HasTagTorso = false;
	for (auto& tag : TagNames) { if (tag == "tag_torso") { HasTagTorso = true; break; } }
	if (HasTagTorso && Animation->ViewModelAnimation == false) { printf("Animation has tag_torso but ISN'T a viewmodel anim: %s\n", Animation->AnimationName.c_str()); }
#endif

	// Stage 0: Zero-Rotated bones
	// Zero-rotated bones must be reset to identity, they are the first set of bones
	// Note: NoneTranslated bones aren't reset, they remain scene position
	for (uint32_t i = 0; i < Animation->NoneRotatedBoneCount; i++)
	{
		// Add the keyframe
		Anim->AddRotationKey(TagNames[i], 0, 0, 0, 0, 1.0f);
	}

	// Stage 1: 2D Rotations
	// 2D Rotations appear directly after the "None-Rotated bones"
	for (uint32_t i = Animation->NoneRotatedBoneCount; i < (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount); i++)
	{
		// Read index count
		uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
		// Advance 2 bytes
		Animation->DataShortsPtr += 2;

		// Skip inline indicies if need be, FrameSize == 2 && Animation->SupportInlineIndicies
		if (FrameSize == 2 && Animation->SupportsInlineIndicies && FrameCount >= 0x40) { SkipInlineAnimationIndicies(Animation); }

		// Result size
		uintptr_t ResultSize = 0;
		// Calculated size
		auto DataSize = ((FrameCount + 1) * 4);
		// Read the animation data to a buffer
		auto KeyData = (int16_t*)GameOnline::GameInstance->Read(Animation->RandomDataShortsPtr, DataSize, ResultSize);
		// Advance the size
		Animation->RandomDataShortsPtr += DataSize;

		// Continue if we read it
		if (ResultSize == DataSize && KeyData != nullptr)
		{
			// Loop for count + 1
			for (uint32_t f = 0; f < (FrameCount + 1); f++)
			{
				// Buffer for the frame index
				uint32_t FrameIndex = 0;

				// Read the index depending on frame size
				if (FrameSize == 1)
				{
					// Just read it
					FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
					// Advance 1 byte
					Animation->DataBytesPtr += 1;
				}
				else if (FrameSize == 2)
				{
					// Check for inline
					if (FrameCount < 0x40 || Animation->LongIndiciesPtr == 0)
					{
						// Read from data shorts
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
						// Advance 2 bytes
						Animation->DataShortsPtr += 2;
					}
					else
					{
						// Read from long indicies
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->LongIndiciesPtr);
						// Advance 2 bytes
						Animation->LongIndiciesPtr += 2;
					}
				}

				// Build the rotation key
				if (Animation->RotationType == AnimationKeyTypes::DivideBySize)
				{
					// Add
					Anim->AddRotationKey(TagNames[i], FrameIndex, 0, 0, ((float)KeyData[f * 2] / 32768.f), ((float)KeyData[(f * 2) + 1] / 32768.f));
				}
				else if (Animation->RotationType == AnimationKeyTypes::HalfFloat)
				{
					// Add
					Anim->AddRotationKey(TagNames[i], FrameIndex, 0, 0, HalfFloats::ToFloat((uint16_t)KeyData[f * 2]), HalfFloats::ToFloat((uint16_t)KeyData[(f * 2) + 1]));
				}
			}
		}

		// Clean up
		if (KeyData != nullptr)
		{
			// Delete
			delete[] KeyData;
		}
	}

	// Stage 2: 3D Rotations
	// 3D Rotations appear directly after the "2D Rotations"
	for (uint32_t i = (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount); i < (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount + Animation->NormalRotatedBoneCount); i++)
	{
		// Read index count
		uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
		// Advance 2 bytes
		Animation->DataShortsPtr += 2;

		// Skip inline indicies if need be, FrameSize == 2 && Animation->SupportInlineIndicies
		if (FrameSize == 2 && Animation->SupportsInlineIndicies && FrameCount >= 0x40) { SkipInlineAnimationIndicies(Animation); }

		// Result size
		uintptr_t ResultSize = 0;
		// Calculated size
		auto DataSize = ((FrameCount + 1) * 8);
		// Read the animation data to a buffer
		auto KeyData = (int16_t*)GameOnline::GameInstance->Read(Animation->RandomDataShortsPtr, DataSize, ResultSize);
		// Advance the size
		Animation->RandomDataShortsPtr += DataSize;

		// Continue if we read it
		if (ResultSize == DataSize && KeyData != nullptr)
		{
			// Loop for count + 1
			for (uint32_t f = 0; f < (FrameCount + 1); f++)
			{
				// Buffer for the frame index
				uint32_t FrameIndex = 0;

				// Read the index depending on frame size
				if (FrameSize == 1)
				{
					// Just read it
					FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
					// Advance 1 byte
					Animation->DataBytesPtr += 1;
				}
				else if (FrameSize == 2)
				{
					// Check for inline
					if (FrameCount < 0x40 || Animation->LongIndiciesPtr == 0)
					{
						// Read from data shorts
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
						// Advance 2 bytes
						Animation->DataShortsPtr += 2;
					}
					else
					{
						// Read from long indicies
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->LongIndiciesPtr);
						// Advance 2 bytes
						Animation->LongIndiciesPtr += 2;
					}
				}

				// Build the rotation key
				if (Animation->RotationType == AnimationKeyTypes::DivideBySize)
				{
					// Add
					Anim->AddRotationKey(TagNames[i], FrameIndex, ((float)KeyData[f * 4] / 32768.f), ((float)KeyData[(f * 4) + 1] / 32768.f), ((float)KeyData[(f * 4) + 2] / 32768.f), ((float)KeyData[(f * 4) + 3] / 32768.f));
				}
				else if (Animation->RotationType == AnimationKeyTypes::HalfFloat)
				{
					// Add
					Anim->AddRotationKey(TagNames[i], FrameIndex, HalfFloats::ToFloat((uint16_t)KeyData[f * 4]), HalfFloats::ToFloat((uint16_t)KeyData[(f * 4) + 1]), HalfFloats::ToFloat((uint16_t)KeyData[(f * 4) + 2]), HalfFloats::ToFloat((uint16_t)KeyData[(f * 4) + 3]));
				}
			}
		}

		// Clean up
		if (KeyData != nullptr)
		{
			// Delete
			delete[] KeyData;
		}
	}

	// Stage 3: 2D Static Rotations
	// 2D Static Rotations appear directly after the "3D Rotations"
	for (uint32_t i = (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount + Animation->NormalRotatedBoneCount); i < (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount + Animation->NormalRotatedBoneCount + Animation->TwoDStaticRotatedBoneCount); i++)
	{
		// Read rotation data
		auto RotationData = GameOnline::GameInstance->Read<Quat2Data>(Animation->DataShortsPtr);
		// Advance 4 bytes
		Animation->DataShortsPtr += 4;

		// Build the rotation key
		if (Animation->RotationType == AnimationKeyTypes::DivideBySize)
		{
			// Add
			Anim->AddRotationKey(TagNames[i], 0, 0, 0, ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		}
		else if (Animation->RotationType == AnimationKeyTypes::HalfFloat)
		{
			// Add
			Anim->AddRotationKey(TagNames[i], 0, 0, 0, HalfFloats::ToFloat((uint16_t)RotationData.RotationZ), HalfFloats::ToFloat((uint16_t)RotationData.RotationW));
		}
	}

	// Stage 4: 3D Static Rotations
	// 3D Static Rotations appear directly after the "2D Static Rotations"
	for (uint32_t i = (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount + Animation->NormalRotatedBoneCount + Animation->TwoDStaticRotatedBoneCount); i < (Animation->NoneRotatedBoneCount + Animation->TwoDRotatedBoneCount + Animation->NormalRotatedBoneCount + Animation->TwoDStaticRotatedBoneCount + Animation->NormalStaticRotatedBoneCount); i++)
	{
		// Read rotation data
		auto RotationData = GameOnline::GameInstance->Read<QuatData>(Animation->DataShortsPtr);
		// Advance 8 bytes
		Animation->DataShortsPtr += 8;

		// Build the rotation key
		if (Animation->RotationType == AnimationKeyTypes::DivideBySize)
		{
			// Add
			Anim->AddRotationKey(TagNames[i], 0, ((float)RotationData.RotationX / 32768.f), ((float)RotationData.RotationY / 32768.f), ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		}
		else if (Animation->RotationType == AnimationKeyTypes::HalfFloat)
		{
			// Add
			Anim->AddRotationKey(TagNames[i], 0, HalfFloats::ToFloat((uint16_t)RotationData.RotationX), HalfFloats::ToFloat((uint16_t)RotationData.RotationY), HalfFloats::ToFloat((uint16_t)RotationData.RotationZ), HalfFloats::ToFloat((uint16_t)RotationData.RotationW));
		}
	}

	// Stage 5: Normal Translations (Byte size)
	// Normal Translations appear directly after the "3D Static Rotations"
	for (uint32_t i = 0; i < Animation->NormalTranslatedBoneCount; i++)
	{
		// BoneID Buffer
		uint32_t BoneID = 0;

		// Check size
		switch (BoneTypeSize)
		{
		case 1:
			// Consume the index from data bytes
			BoneID = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
			// Advance 1 byte
			Animation->DataBytesPtr += 1;
			break;
		case 2:
			// Consume the index from data shorts
			BoneID = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
			// Advance 2 bytes
			Animation->DataShortsPtr += 2;
			break;
		}

		// Read index count
		uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
		// Advance 2 bytes
		Animation->DataShortsPtr += 2;

		// Skip inline indicies if need be, FrameSize == 2 && Animation->SupportInlineIndicies
		if (FrameSize == 2 && Animation->SupportsInlineIndicies && FrameCount >= 0x40) { SkipInlineAnimationIndicies(Animation); }

		// Read the min and size table for this translation set
		auto MinVec = GameOnline::GameInstance->Read<Vector3>(Animation->DataIntsPtr);
		// Advance 12 bytes
		Animation->DataIntsPtr += 12;
		// Read size table
		auto SizeVec = GameOnline::GameInstance->Read<Vector3>(Animation->DataIntsPtr);
		// Advance 12 bytes
		Animation->DataIntsPtr += 12;

		// Result size
		uintptr_t ResultSize = 0;
		// Calculated size
		auto DataSize = ((FrameCount + 1) * 3);
		// Read the animation data to a buffer
		auto KeyData = (uint8_t*)GameOnline::GameInstance->Read(Animation->RandomDataBytesPtr, DataSize, ResultSize);
		// Advance the size
		Animation->RandomDataBytesPtr += DataSize;

		// Continue if we read it
		if (ResultSize == DataSize && KeyData != nullptr)
		{
			// Loop for count + 1
			for (uint32_t f = 0; f < (FrameCount + 1); f++)
			{
				// Buffer for the frame index
				uint32_t FrameIndex = 0;

				// Read the index depending on frame size
				if (FrameSize == 1)
				{
					// Just read it
					FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
					// Advance 1 byte
					Animation->DataBytesPtr += 1;
				}
				else if (FrameSize == 2)
				{
					// Check for inline
					if (FrameCount < 0x40 || Animation->LongIndiciesPtr == 0)
					{
						// Read from data shorts
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
						// Advance 2 bytes
						Animation->DataShortsPtr += 2;
					}
					else
					{
						// Read from long indicies
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->LongIndiciesPtr);
						// Advance 2 bytes
						Animation->LongIndiciesPtr += 2;
					}
				}

				// Build the translation key
				if (Animation->TranslationType == AnimationKeyTypes::MinSizeTable)
				{
					// Calculate translation
					float TranslationX = (SizeVec.X * (float)KeyData[(f * 3)]) + MinVec.X;
					float TranslationY = (SizeVec.Y * (float)KeyData[(f * 3) + 1]) + MinVec.Y;
					float TranslationZ = (SizeVec.Z * (float)KeyData[(f * 3) + 2]) + MinVec.Z;
					// Add
					Anim->AddTranslationKey(TagNames[BoneID], FrameIndex, TranslationX, TranslationY, TranslationZ);
				}
			}
		}

		// Clean up
		if (KeyData != nullptr)
		{
			// Delete
			delete[] KeyData;
		}
	}

	// Stage 6: Precise Translations (Short size)
	// Precise Translations appear directly after the "Normal Translations"
	for (uint32_t i = 0; i < Animation->PreciseTranslatedBoneCount; i++)
	{
		// BoneID Buffer
		uint32_t BoneID = 0;

		// Check size
		switch (BoneTypeSize)
		{
		case 1:
			// Consume the index from data bytes
			BoneID = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
			// Advance 1 byte
			Animation->DataBytesPtr += 1;
			break;
		case 2:
			// Consume the index from data shorts
			BoneID = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
			// Advance 2 bytes
			Animation->DataShortsPtr += 2;
			break;
		}

		// Read index count
		uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
		// Advance 2 bytes
		Animation->DataShortsPtr += 2;

		// Skip inline indicies if need be, FrameSize == 2 && Animation->SupportInlineIndicies
		if (FrameSize == 2 && Animation->SupportsInlineIndicies && FrameCount >= 0x40) { SkipInlineAnimationIndicies(Animation); }

		// Read the min and size table for this translation set
		auto MinVec = GameOnline::GameInstance->Read<Vector3>(Animation->DataIntsPtr);
		// Advance 12 bytes
		Animation->DataIntsPtr += 12;
		// Read size table
		auto SizeVec = GameOnline::GameInstance->Read<Vector3>(Animation->DataIntsPtr);
		// Advance 12 bytes
		Animation->DataIntsPtr += 12;

		// Result size
		uintptr_t ResultSize = 0;
		// Calculated size
		auto DataSize = ((FrameCount + 1) * 6);
		// Read the animation data to a buffer
		auto KeyData = (uint16_t*)GameOnline::GameInstance->Read(Animation->RandomDataShortsPtr, DataSize, ResultSize);
		// Advance the size
		Animation->RandomDataShortsPtr += DataSize;

		// Continue if we read it
		if (ResultSize == DataSize && KeyData != nullptr)
		{
			// Loop for count + 1
			for (uint32_t f = 0; f < (FrameCount + 1); f++)
			{
				// Buffer for the frame index
				uint32_t FrameIndex = 0;

				// Read the index depending on frame size
				if (FrameSize == 1)
				{
					// Just read it
					FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
					// Advance 1 byte
					Animation->DataBytesPtr += 1;
				}
				else if (FrameSize == 2)
				{
					// Check for inline
					if (FrameCount < 0x40 || Animation->LongIndiciesPtr == 0)
					{
						// Read from data shorts
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
						// Advance 2 bytes
						Animation->DataShortsPtr += 2;
					}
					else
					{
						// Read from long indicies
						FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->LongIndiciesPtr);
						// Advance 2 bytes
						Animation->LongIndiciesPtr += 2;
					}
				}

				// Build the translation key
				if (Animation->TranslationType == AnimationKeyTypes::MinSizeTable)
				{
					// Calculate translation
					float TranslationX = (SizeVec.X * (float)KeyData[(f * 3)]) + MinVec.X;
					float TranslationY = (SizeVec.Y * (float)KeyData[(f * 3) + 1]) + MinVec.Y;
					float TranslationZ = (SizeVec.Z * (float)KeyData[(f * 3) + 2]) + MinVec.Z;
					// Add
					Anim->AddTranslationKey(TagNames[BoneID], FrameIndex, TranslationX, TranslationY, TranslationZ);
				}
			}
		}

		// Clean up
		if (KeyData != nullptr)
		{
			// Delete
			delete[] KeyData;
		}
	}

	// Stage 7: Static Translations
	// Static Translations appear directly after the "Precise Translations"
	for (uint32_t i = 0; i < Animation->StaticTranslatedBoneCount; i++)
	{
		// Read translation data
		auto Coords = GameOnline::GameInstance->Read<Vector3>(Animation->DataIntsPtr);
		// Advance 12 bytes
		Animation->DataIntsPtr += 12;

		// BoneID Buffer
		uint32_t BoneID = 0;

		// Check size
		switch (BoneTypeSize)
		{
		case 1:
			// Consume the index from data bytes
			BoneID = GameOnline::GameInstance->Read<uint8_t>(Animation->DataBytesPtr);
			// Advance 1 byte
			Animation->DataBytesPtr += 1;
			break;
		case 2:
			// Consume the index from data shorts
			BoneID = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
			// Advance 2 bytes
			Animation->DataShortsPtr += 2;
			break;
		}

		// Build the translation key
		Anim->AddTranslationKey(TagNames[BoneID], 0, Coords.X, Coords.Y, Coords.Z);
	}

	// Stage 8: Delta translation data, handled on a per-game basis
	// Delta translations appear in their own pointer, but have some game specific structures
	if (Animation->DeltaTranslationPtr != 0)
	{
		// Build translations for 32bit games
		DeltaTranslations32(Anim, FrameSize, Animation);
	}

	// Stage 9: Delta 2D rotation data, handled on a per-game basis
	// Delta 2D rotations appear in their own pointer, but have some game specific structures
	if (Animation->Delta2DRotationsPtr != 0)
	{
		// Build 2d rotations for 32bit games
		Delta2DRotations32(Anim, FrameSize, Animation);
	}

	// Stage 10: Delta 3D rotation data, handled on a per-game basis
	// Delta 3D rotations appear in their own pointer, but have some game specific structures
	if (Animation->Delta3DRotationsPtr != 0)
	{
		// Build 3d rotations for 32bit games
		Delta3DRotations32(Anim, FrameSize, Animation);
	}

	// Stage 11: Notifications
	// Build standard notetracks
	NotetracksStandard(Anim, Animation);

	// Return it
	return Anim;
}

void CoDXAnimTranslator::SkipInlineAnimationIndicies(const std::unique_ptr<XAnim_t>& Animation)
{
	// Read until we hit the end
	uint16_t InlineIndex = 0;
	// Skip until we hit the count of indicies
	do
	{
		// Read
		InlineIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DataShortsPtr);
		// Advance 2 bytes
		Animation->DataShortsPtr += 2;
		// Loop until the end
	} while (InlineIndex != Animation->FrameCount);
}

void CoDXAnimTranslator::NotetracksStandard(const std::unique_ptr<WraithAnim>& Anim, const std::unique_ptr<XAnim_t>& Animation)
{
	// Loop over notetracks
	for (uint32_t i = 0; i < Animation->NotificationCount; i++)
	{
		// Read the tag
		auto NotificationTag = GameOnline::LoadStringHandler(GameOnline::GameInstance->Read<uint32_t>(Animation->NotificationsPtr));
		// Read the frame
		uint32_t NotificationFrame = (uint32_t)((float)Animation->FrameCount * GameOnline::GameInstance->Read<float>(Animation->NotificationsPtr + 4));

		// Add the notetrack, if the tag is not blank
		if (!Strings::IsNullOrWhiteSpace(NotificationTag))
		{
			// Add
			Anim->AddNoteTrack(NotificationTag, NotificationFrame);
		}

		// Advance 8 bytes
		Animation->NotificationsPtr += 8;
	}
}

void CoDXAnimTranslator::DeltaTranslations32(const std::unique_ptr<WraithAnim>& Anim, uint32_t FrameSize, const std::unique_ptr<XAnim_t>& Animation)
{
	// Read index count
	uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->DeltaTranslationPtr);
	// Advance 2 bytes
	Animation->DeltaTranslationPtr += 2;
	// Read data size (Determines whether or not to use char's or short's for data)
	uint8_t DataSize = GameOnline::GameInstance->Read<uint8_t>(Animation->DeltaTranslationPtr);
	// Advance 1 byte for frame size, 1 byte for padding
	Animation->DeltaTranslationPtr += 2;

	// Read the min and size table for the delta translations set
	auto MinVec = GameOnline::GameInstance->Read<Vector3>(Animation->DeltaTranslationPtr);
	// Advance 12 bytes
	Animation->DeltaTranslationPtr += 12;
	// Read size table
	auto SizeVec = GameOnline::GameInstance->Read<Vector3>(Animation->DeltaTranslationPtr);
	// Advance 12 bytes
	Animation->DeltaTranslationPtr += 12;

	// Check if we have a frame count of 0, this means that MinXYZ is the key for frame 0, and we should exit
	if (FrameCount == 0)
	{
		// Set a key
		Anim->AddTranslationKey("tag_origin", 0, MinVec.X, MinVec.Y, MinVec.Z);
		// Exit
		return;
	}

	// Read the pointer to delta translation data
	uint32_t DeltaDataPtr = GameOnline::GameInstance->Read<uint32_t>(Animation->DeltaTranslationPtr);
	// Advance 4 bytes
	Animation->DeltaTranslationPtr += 4;

	// Continue if we have translation frames
	if (FrameCount > 0)
	{
		// Loop over frame count
		for (uint32_t i = 0; i < (FrameCount + 1); i++)
		{
			// Buffer for the frame index
			uint32_t FrameIndex = 0;

			// Read the index depending on frame size
			if (FrameSize == 1)
			{
				// Just read it
				FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->DeltaTranslationPtr);
				// Advance 1 byte
				Animation->DeltaTranslationPtr += 1;
			}
			else if (FrameSize == 2)
			{
				// Read from long indicies
				FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->DeltaTranslationPtr);
				// Advance 2 bytes
				Animation->DeltaTranslationPtr += 2;
			}

			// Read the translation
			float XCoord = 0, YCoord = 0, ZCoord = 0;
			// Check size
			if (DataSize == 1)
			{
				// Read small sizes (char)
				XCoord = GameOnline::GameInstance->Read<uint8_t>(DeltaDataPtr);
				YCoord = GameOnline::GameInstance->Read<uint8_t>(DeltaDataPtr + 1);
				ZCoord = GameOnline::GameInstance->Read<uint8_t>(DeltaDataPtr + 2);
				// Advance 3 bytes
				DeltaDataPtr += 3;
			}
			else
			{
				// Read big sizes (short)
				XCoord = GameOnline::GameInstance->Read<uint16_t>(DeltaDataPtr);
				YCoord = GameOnline::GameInstance->Read<uint16_t>(DeltaDataPtr + 2);
				ZCoord = GameOnline::GameInstance->Read<uint16_t>(DeltaDataPtr + 4);
				// Advance 6 bytes
				DeltaDataPtr += 6;
			}

			// Calculate the offsets
			float TranslationX = (SizeVec.X * XCoord) + MinVec.X;
			float TranslationY = (SizeVec.Y * YCoord) + MinVec.Y;
			float TranslationZ = (SizeVec.Z * ZCoord) + MinVec.Z;
			// Add the translation key
			Anim->AddTranslationKey("tag_origin", FrameIndex, TranslationX, TranslationY, TranslationZ);
		}
	}
}

void CoDXAnimTranslator::Delta2DRotations32(const std::unique_ptr<WraithAnim>& Anim, uint32_t FrameSize, const std::unique_ptr<XAnim_t>& Animation)
{
	// Read index count
	uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->Delta2DRotationsPtr);
	// Advance 2 bytes, and 2 padding bytes
	Animation->Delta2DRotationsPtr += 4;

	// Check if we have a frame count of 0, this means that the next two shorts is the key for frame 0, and we should exit
	if (FrameCount == 0)
	{
		// Read rotation data
		auto RotationData = GameOnline::GameInstance->Read<Quat2Data>(Animation->Delta2DRotationsPtr);
		// Advance 4 bytes
		Animation->Delta2DRotationsPtr += 4;

		// Build the rotation key
		Anim->AddRotationKey("tag_origin", 0, 0, 0, ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		// Exit
		return;
	}

	// Read the pointer to delta translation data
	uint32_t DeltaDataPtr = GameOnline::GameInstance->Read<uint32_t>(Animation->Delta2DRotationsPtr);
	// Advance 4 bytes
	Animation->Delta2DRotationsPtr += 4;

	// Continue if we have rotation frames
	if (FrameCount > 0)
	{
		// Loop over frame count
		for (uint32_t i = 0; i < (FrameCount + 1); i++)
		{
			// Buffer for the frame index
			uint32_t FrameIndex = 0;

			// Read the index depending on frame size
			if (FrameSize == 1)
			{
				// Just read it
				FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->Delta2DRotationsPtr);
				// Advance 1 byte
				Animation->Delta2DRotationsPtr += 1;
			}
			else if (FrameSize == 2)
			{
				// Read from long indicies
				FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->Delta2DRotationsPtr);
				// Advance 2 bytes
				Animation->Delta2DRotationsPtr += 2;
			}

			// Read rotation data
			auto RotationData = GameOnline::GameInstance->Read<Quat2Data>(DeltaDataPtr);
			// Advance 4 bytes
			DeltaDataPtr += 4;

			// Build the rotation key
			Anim->AddRotationKey("tag_origin", FrameIndex, 0, 0, ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		}
	}
}

void CoDXAnimTranslator::Delta3DRotations32(const std::unique_ptr<WraithAnim>& Anim, uint32_t FrameSize, const std::unique_ptr<XAnim_t>& Animation)
{
	// Read index count
	uint32_t FrameCount = GameOnline::GameInstance->Read<uint16_t>(Animation->Delta3DRotationsPtr);
	// Advance 2 bytes, and 2 padding bytes
	Animation->Delta3DRotationsPtr += 4;

	// Check if we have a frame count of 0, this means that the next four shorts is the key for frame 0, and we should exit
	if (FrameCount == 0)
	{
		// Read rotation data
		auto RotationData = GameOnline::GameInstance->Read<QuatData>(Animation->Delta3DRotationsPtr);
		// Advance 8 bytes
		Animation->Delta3DRotationsPtr += 8;

		// Build the rotation key
		Anim->AddRotationKey("tag_origin", 0, ((float)RotationData.RotationX / 32768.f), ((float)RotationData.RotationY / 32768.f), ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		// Exit
		return;
	}

	// Read the pointer to delta translation data
	uint32_t DeltaDataPtr = GameOnline::GameInstance->Read<uint32_t>(Animation->Delta3DRotationsPtr);
	// Advance 4 bytes
	Animation->Delta3DRotationsPtr += 4;

	// Continue if we have rotation frames
	if (FrameCount > 0)
	{
		// Loop over frame count
		for (uint32_t i = 0; i < (FrameCount + 1); i++)
		{
			// Buffer for the frame index
			uint32_t FrameIndex = 0;

			// Read the index depending on frame size
			if (FrameSize == 1)
			{
				// Just read it
				FrameIndex = GameOnline::GameInstance->Read<uint8_t>(Animation->Delta3DRotationsPtr);
				// Advance 1 byte
				Animation->Delta3DRotationsPtr += 1;
			}
			else if (FrameSize == 2)
			{
				// Read from long indicies
				FrameIndex = GameOnline::GameInstance->Read<uint16_t>(Animation->Delta3DRotationsPtr);
				// Advance 2 bytes
				Animation->Delta3DRotationsPtr += 2;
			}

			// Read rotation data
			auto RotationData = GameOnline::GameInstance->Read<QuatData>(DeltaDataPtr);
			// Advance 8 bytes
			DeltaDataPtr += 8;

			// Build the rotation key
			Anim->AddRotationKey("tag_origin", FrameIndex, ((float)RotationData.RotationX / 32768.f), ((float)RotationData.RotationY / 32768.f), ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f));
		}
	}
}
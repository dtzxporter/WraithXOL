#include "stdafx.h"

// The class we are implementing
#include "IFSLib.h"

// We need the following classes
#include "BinaryReader.h"
#include "MemoryReader.h"
#include "Compression.h"
#include "FileSystems.h"
#include "Hashing.h"

// Jenkens hash
#include "JenkinsHash.h"

// Whether or not the IFSEncrypt table was built
bool IFSEncryptionBuilt = false;
// The IFSEncrypt table, used internally
uint32_t IFSEncryptionTable[0x500];

// Build the encryption table
void BuildIFSEncryptionTable()
{
	// Seed and constant
	int32_t q, r = 0x100001;
	uint32_t seed = 0;

	// Generate 5 256 byte long tables
	for (int i = 0; i < 0x100; i++)
	{
		for (int j = 0; j < 5; j++)
		{
			// Pass 1
			auto qt = std::div(r * 125 + 3, 0x2AAAAB);
			q = qt.quot;
			r = qt.rem;
			seed = (uint32_t)(r & 0xFFFF) << 16;

			// Pass 2
			qt = std::div(r * 125 + 3, 0x2AAAAB);
			q = qt.quot;
			r = qt.rem;
			seed |= (uint32_t)(r & 0xFFFF);

			// Assign it
			IFSEncryptionTable[0x100 * j + i] = seed;
		}
	}
}

// Hash a string input with the given hash table
const uint32_t HashString(const std::string& Value, uint32_t HashOffset)
{
	// Seed and constant
	uint32_t hash = 0x7FED7FED, seed = 0xEEEEEEEE;
	
	// Buffers
	int8_t c;
	uint8_t b;

	for (size_t i = 0; i < Value.size(); i++)
	{
		// Strip invalid chars
		c = Value[i];
		if (c >= 128)
			c = '?';
		b = (uint8_t)c;

		// Char to upper
		if (b > 0x60 && b < 0x7B)
			b -= 0x20;

		// Hash the round and shift the seed
		hash = IFSEncryptionTable[HashOffset + b] ^ (hash + seed);
		seed += hash + (seed << 5) + b + 3;
	}

	// Return result
	return hash;
}

// Decrypts an IFS data block
void DecryptIFSBlock(uint32_t* Data, uint32_t Length, uint32_t Hash)
{
	// Buffer and constant
	uint32_t Buffer, Temp = 0xEEEEEEEE;

	// Iterate backwards
	for (uint32_t i = Length; i-- != 0;)
	{
		// Read from the data stream into temp
		Temp += IFSEncryptionTable[0x400 + (Hash & 0xFF)];
		Buffer = *Data ^ (Temp + Hash);

		// Shift the seed
		Temp += Buffer + (Temp << 5) + 3;

		// Assign decrypted value
		*Data++ = Buffer;

		// Shift the hash
		Hash = (Hash >> 11) | (0x11111111 + ((Hash ^ 0x7FF) << 21));
	}
}

// Calculates the rounded buffer size
const uint32_t IntegralBufferSize(uint32_t Buffer)
{
	// Calculate
	if ((Buffer % 4) == 0)
		return Buffer / 4;

	// Add one for safety
	return (Buffer / 4) + 1;
}

// Calculate the JenkensHashLittle2 of a value
const uint64_t HashLookupString(std::string Value)
{
	// Constants
	uint32_t Pb = 1;
	uint32_t Pc = 2;

	// Make it lowercase, replace '/' - '\\'
	auto HashLower = Strings::Replace(Strings::ToLower(Value), "/", "\\");
	// Hash it
	hashlittle2(HashLower.c_str(), Value.size(), &Pc, &Pb);

	// Return actual hash
	return Pc + ((uint64_t)Pb << 32);
}

// Reads a bit-len integer from the stream
const int64_t ReadBitLenInteger(const uint8_t* Buffer, uint32_t BitIndex, uint32_t NumBits)
{
	int64_t Data = 0, Wei = 1;

	for (uint32_t j = 0; j < NumBits; j++)
	{
		if ((((Buffer[BitIndex / 8]) >> (BitIndex % 8)) & 1) != 0) Data += Wei;
		BitIndex++; Wei *= 2;
	}

	return Data;
}

// Reads a bit-len integer from the stream
const uint64_t ReadBitLenUInteger(const uint8_t* Buffer, uint32_t BitIndex, uint32_t NumBits)
{
	uint64_t Data = 0, Wei = 1;

	for (uint32_t j = 0; j < NumBits; j++)
	{
		if ((((Buffer[BitIndex / 8]) >> (BitIndex % 8)) & 1) != 0) Data += Wei;
		BitIndex++; Wei *= 2;
	}

	return Data;
}

// Structures for reading

#pragma pack(push, 1)
struct IFSHeader
{
	uint32_t Magic;
	uint32_t HeaderSize;
	uint16_t Version;
	uint16_t SectorSize;

	uint64_t ArchiveSize;
	uint64_t BetTablePos;
	uint64_t HetTablePos;
	uint64_t MD5TablePos;
	uint64_t BitmapPos;

	uint64_t HetTableSize;
	uint64_t BetTableSize;
	uint64_t MD5TableSize;
	uint64_t BitmapSize;

	uint32_t MD5PieceSize;
	uint32_t RawChunkSize;
};

struct IFSHetHeader
{
	uint32_t Magic;
	uint32_t Version;
	uint32_t DataSize;
};

struct IFSBetHeader
{
	uint32_t Magic;
	uint32_t Version;
	uint32_t DataSize;
};

struct IFSHetTable
{
	uint32_t TableSize;
	uint32_t EntryCount;
	uint32_t HashTableSize;
	uint32_t HashEntrySize;
	uint32_t IndexSizeTotal;
	uint32_t IndexSizeExtra;
	uint32_t IndexSize;
	uint32_t BlockTableSize;
};

struct IFSBetTable
{
	uint32_t TableSize;
	uint32_t EntryCount;
	uint32_t TableEntrySize;

	uint32_t BitIndexFilePos;
	uint32_t BitIndexFileSize;
	uint32_t BitIndexCmpSize;
	uint32_t BitIndexFlagPos;
	uint32_t BitIndexHashPos;

	uint32_t UnknownRepeatPos;

	uint32_t BitCountFilePos;
	uint32_t BitCountFileSize;
	uint32_t BitCountCmpSize;
	uint32_t BitCountFlagSize;
	uint32_t BitCountHashSize;

	uint32_t UnknownZero;

	uint32_t HashSizeTotal;
	uint32_t HashSizeExtra;
	uint32_t HashSize;

	uint32_t HashPart1;
	uint32_t HashPart2;

	uint32_t HashArraySize;
};

union IVPartLength
{
	uint64_t FullIV;

	struct
	{
		uint32_t IVIndex;
		uint32_t IVBlockSize;
	};

	IVPartLength() : FullIV(0) { }
};
#pragma pack(pop)

IFSLib::IFSLib()
{
	// Initialize the library
	this->Initialize();
}

IFSLib::~IFSLib()
{
	// Clean up the library
	this->IFSPackages.clear();
	this->IFSPackages.shrink_to_fit();

	// Clean up encryption stuff
	ctr_done(&this->EncryptionKey);
}

void IFSLib::Initialize()
{
	// Ensure the IFSEncryptionTable was built
	if (!IFSEncryptionBuilt)
		BuildIFSEncryptionTable();
	
	// We must setup the AES key with the default IV (AES192)
	unsigned char AesKey[24] = { 0x15, 0x9a, 0x03, 0x25, 0xe0, 0x75, 0x2e, 0x80, 0xc6, 0xc0, 0x94, 0x2a, 0x50, 0x5c, 0x1c, 0x68, 0x8c, 0x17, 0xef, 0x53, 0x99, 0xf8, 0x68, 0x3c };
	// Setup the default IV
	unsigned int AesIV[4] = { 0x1010101, 0x1010101, 0x1010101, 0x1010101 };

	// Initialize LibTomCrypt + LibTomMath
	ltc_mp = ltm_desc;

	register_cipher(&aes_desc);
	register_hash(&sha256_desc);

	// Initialize
	ctr_start(find_cipher("aes"), (unsigned char*)&AesIV[0], &AesKey[0], 24, 0, CTR_COUNTER_BIG_ENDIAN, &this->EncryptionKey);

	// All set
	IFSEncryptionBuilt = true;
}

void IFSLib::AddPackage(const std::string& PackagePath)
{
	// Load the package, we don't want the list file
	this->LoadPackageInternal(PackagePath, std::vector<std::string>());
}

std::vector<std::string> IFSLib::ParsePackage(const std::string& PackagePath)
{
	// Load the package, we want the list
	auto Result = std::vector<std::string>();

	// Read it
	this->LoadPackageInternal(PackagePath, Result, true);

	// Return it
	return Result;
}

void IFSLib::LoadPackageInternal(const std::string& PackagePath, std::vector<std::string>& LoadedListFile, bool Audio)
{
	// Prepare to open the package
	auto Reader = BinaryReader();
	// Open it (Sharing mode!)
	auto open = Reader.Open(PackagePath, true);

	// Read the header
	auto Header = Reader.Read<IFSHeader>();
	// Verify magic
	if (Header.Magic != 0x7366696e) return;

	// Prepare to load the het table first
	Reader.SetPosition(Header.HetTablePos);
	// Calculate table hashes
	auto HetKey = HashString("(hash table)", 0x300);
	auto BetKey = HashString("(block table)", 0x300);

	// Calculate the list file hashs
	uint64_t ListFileHash = 0;

	// A list of file entries
	std::unordered_map<uint64_t, IFSFileEntry> FileEntries;

	// Add the package to the cache
	this->IFSPackages.emplace_back(PackagePath);
	// Get index
	auto PackageIndex = (uint32_t)(this->IFSPackages.size() - 1);

	// Read result
	uint64_t ReadResult = 0;

	IFSHetTable HetTable;
	IFSBetTable BetTable;

	uint64_t AndMask = 0, OrMask = 0;

	// Begin HetTable parse
	{
		// Read the het header
		auto HetHeader = Reader.Read<IFSHetHeader>();
		// Allocate a working buffer
		auto HetBuffer = std::make_unique<uint32_t[]>(IntegralBufferSize(HetHeader.DataSize));
		// Clear it
		std::memset(HetBuffer.get(), 0, IntegralBufferSize(HetHeader.DataSize) * 4);

		// Read the data
		Reader.Read((uint8_t*)HetBuffer.get(), HetHeader.DataSize, ReadResult);
		// Decrypt the data
		DecryptIFSBlock(HetBuffer.get(), IntegralBufferSize(HetHeader.DataSize), HetKey);

		// Read the required hettable information
		auto MemReader = MemoryReader((int8_t*)HetBuffer.get(), IntegralBufferSize(HetHeader.DataSize) * 4, true);

		// Read het table
		HetTable = MemReader.Read<IFSHetTable>();

		// Calculate masks
		if (HetTable.HashEntrySize != 0x40)
			AndMask = (uint64_t)1 << HetTable.HashEntrySize;
		AndMask--;
		OrMask = (uint64_t)1 << (HetTable.HashEntrySize - 1);
	}

	// Jump to BetTable
	Reader.SetPosition(Header.BetTablePos);

	// Begin BetTable parse
	{
		// Read the bet header
		auto BetHeader = Reader.Read<IFSBetHeader>();
		// Allocate a working buffer
		auto BetBuffer = std::make_unique<uint32_t[]>(IntegralBufferSize(BetHeader.DataSize));
		// Clear it
		std::memset(BetBuffer.get(), 0, IntegralBufferSize(BetHeader.DataSize) * 4);

		// Read the data
		Reader.Read((uint8_t*)BetBuffer.get(), BetHeader.DataSize, ReadResult);
		// Decrypt the data
		DecryptIFSBlock(BetBuffer.get(), IntegralBufferSize(BetHeader.DataSize), BetKey);

		// Read the required bettable information
		auto MemReader = MemoryReader((int8_t*)BetBuffer.get(), IntegralBufferSize(BetHeader.DataSize) * 4, true);

		// Read bet table
		BetTable = MemReader.Read<IFSBetTable>();

		// Allocate memory for table entries, and hash table
		auto TableEntries = std::make_unique<uint8_t[]>((BetTable.TableEntrySize * BetTable.EntryCount + 7) / 8);
		auto TableHashes = std::make_unique<uint8_t[]>((BetTable.HashSizeTotal*BetTable.EntryCount + 7) / 8);

		// Read the tables
		MemReader.Read((BetTable.TableEntrySize * BetTable.EntryCount + 7) / 8, (int8_t*)TableEntries.get());
		MemReader.Read((BetTable.HashSizeTotal * BetTable.EntryCount + 7) / 8, (int8_t*)TableHashes.get());

		// Offsets
		uint32_t BitOffset = 0, HashOffset = 0;

		// Prepare to parse and read each entry
		for (uint32_t i = 0; i < BetTable.EntryCount; i++)
		{
			// New entry, set index
			IFSFileEntry Entry; Entry.FilePackageIndex = PackageIndex;

			// Read data
			Entry.FilePosition = ReadBitLenInteger(TableEntries.get(), BitOffset, BetTable.BitCountFilePos); BitOffset += BetTable.BitCountFilePos;
			Entry.FileSize = ReadBitLenInteger(TableEntries.get(), BitOffset, BetTable.BitCountFileSize); BitOffset += BetTable.BitCountFileSize;
			Entry.CompressedSize = ReadBitLenInteger(TableEntries.get(), BitOffset, BetTable.BitCountCmpSize); BitOffset += BetTable.BitCountCmpSize;
			Entry.Flags = ReadBitLenInteger(TableEntries.get(), BitOffset, BetTable.BitCountFlagSize); BitOffset += BetTable.BitCountFlagSize;

			// Skip over unknown data
			BitOffset += BetTable.BitCountHashSize;
			BitOffset += BetTable.HashArraySize;

			// Grab the hash and use as the key
			auto NameHash = ReadBitLenUInteger(TableHashes.get(), HashOffset, BetTable.HashSizeTotal); HashOffset += BetTable.HashSizeTotal;

			// Check for list file, starts at header size
			if (Entry.FilePosition == Header.HeaderSize && Entry.Flags == 0x80000000)
				ListFileHash = NameHash;

			// Add it
			FileEntries[NameHash] = Entry;
		}
	}

	// Calculates the hash
	auto GetBetHash = [AndMask, OrMask](uint64_t Hash) -> uint64_t
	{
		// Calculate
		uint64_t Buffer = (Hash & AndMask) | OrMask;

		// Shift it
		return Buffer & (AndMask >> 0x8);
	};

	// Find this packages '(listfile)', it provides the names of all file entries
	if (FileEntries.find(ListFileHash) == FileEntries.end())
		return;

	auto& ListFile = FileEntries[ListFileHash];

	// Jump to the list file offset, and read the buffer, it's a string...
	Reader.SetPosition(ListFile.FilePosition);
	// Allocate a string
	std::string ListFileBuffer;
	// Resize
	ListFileBuffer.resize(ListFile.FileSize);

	// Read into buffer
	Reader.Read((uint8_t*)&ListFileBuffer[0], ListFile.FileSize, ReadResult);

	// Find list
	if (ListFileBuffer.find(".lst\r\n") == std::string::npos)
		return;

	// Split by delim
	for (auto& Line : Strings::SplitString(ListFileBuffer, '\n', true))
	{
		// Trim the line
		Strings::Trim(Line);

		// Only IWIs matter, unless we want audio
		if (Strings::EndsWith(Line, ".iwi") || (Audio && Strings::EndsWith(Line, ".mp3")))
		{
			// Calculate XXHash
			auto EntryHash = Hashing::HashXXHashString(FileSystems::GetFileName(Line));
			auto BetHash = GetBetHash(HashLookupString(Line));

			// Add it
			LoadedListFile.emplace_back(Line);

			// Check for entry in file...
			if (FileEntries.find(BetHash) != FileEntries.end())
			{
				// If exists, switch if hires!
				if (this->IFSFiles.find(EntryHash) == this->IFSFiles.end())
					this->IFSFiles[EntryHash] = FileEntries[BetHash];
				else if (Strings::StartsWith(Line, "hires/"))
					this->IFSFiles[EntryHash] = FileEntries[BetHash];
			}
		}
	}
}

void IFSLib::MountIFSPath(const std::string& IFSPath)
{
	// Load all ifs files from the given path
	auto IFSFiles = FileSystems::GetFiles(IFSPath, "*.ifs");

	// Iterate and load
	for (auto& File : IFSFiles)
		this->AddPackage(File);
}

size_t IFSLib::GetLoadedEntries()
{
	return this->IFSFiles.size();
}

std::unique_ptr<uint8_t[]> IFSLib::ReadFileEntry(const std::string& Name, uint32_t& ResultSize)
{
	// Multi-stage read, we must decrypt, then decompress the zlib buffer
	auto NameString = FileSystems::GetFileName(Name);
	auto NameHash = Hashing::HashXXHashString(NameString);
	// Setup
	ResultSize = 0;

	// Ensure existance first
	if (this->IFSFiles.find(NameHash) == this->IFSFiles.end())
		return nullptr;

	auto& FileEntry = this->IFSFiles[NameHash];

	// Open the package for reading
	auto Reader = BinaryReader();
	// Open it (Sharing mode!)
	Reader.Open(this->IFSPackages[FileEntry.FilePackageIndex], true);

	// Hop to the offset
	Reader.SetPosition(FileEntry.FilePosition);

	uint64_t ReadResult = 0;

	// Read the data, it's encrypted right now though (Compressed size MUST = the full size here...)
	auto MemReader = MemoryReader(Reader.Read(FileEntry.CompressedSize, ReadResult), ReadResult);
	// The unpacked size is appended to the end
	MemReader.SetPosition(MemReader.GetLength() - 4);

	// Read this, it's used for the IV
	auto UnpackedSize = MemReader.Read<uint32_t>();
	auto PackedSize = (uint32_t)(MemReader.GetLength() - 4);
	auto Nounce = Hashing::HashCRC32StringInt(NameString, (uint32_t)NameString.size());

	// Build the IV
	auto FileIV = std::make_unique<uint8_t[]>(0x10);

	// Copy data
	std::memset(FileIV.get(), 0, 0x10);
	std::memcpy(FileIV.get(), &Nounce, 4);
	std::memcpy(FileIV.get() + 4, &UnpackedSize, 4);

	// Store the counter
	IVPartLength IVCounter = IVPartLength();

	// Go back to start
	MemReader.SetPosition(0);

	// Allocate the result buffer
	auto ResultBuffer = std::make_unique<uint8_t[]>(UnpackedSize);

	// Read packed size
	uint32_t ReadDataSize = 0;
	uint32_t ReadUnpackedData = 0;

	// Working buffers
	auto EncryptedBuffer = std::make_unique<uint8_t[]>(0x8000);
	auto DecryptedBuffer = std::make_unique<uint8_t[]>(PackedSize);

	// We must decrypt
	while (ReadDataSize < PackedSize)
	{
		// Ask for some data
		auto Left = (PackedSize - ReadDataSize);
		auto BlockSize = (Left > 0x8000) ? 0x8000 : Left;

		// Shift the index
		IVCounter.IVIndex = ReadDataSize;
		IVCounter.IVBlockSize = BlockSize;

		// Set the IV Counter
		std::memcpy(FileIV.get() + 8, &IVCounter, 8);
		// Set the current IV
		ctr_setiv(&FileIV.get()[0], 0x10, &this->EncryptionKey);

		// Read the data, decrypt in-place, then unzip
		MemReader.Read(BlockSize, (int8_t*)EncryptedBuffer.get());

		// Decrypt the buffer
		ctr_decrypt((uint8_t*)EncryptedBuffer.get(), (uint8_t*)DecryptedBuffer.get() + ReadDataSize, BlockSize, &this->EncryptionKey);

		// Advance
		ReadDataSize += BlockSize;
	}

	// Prepare to decompress
	if (ReadDataSize > 0)
	{
		Compression::DecompressZLibBlock((int8_t*)DecryptedBuffer.get(), (int8_t*)ResultBuffer.get(), PackedSize, UnpackedSize);
	}

	// Set it up
	ResultSize = UnpackedSize;

	// Worked, return buffer
	return ResultBuffer;
}
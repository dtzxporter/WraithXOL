#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

// Configure LibTom
#define LTM_DESC

// Encryption
#include "tomcrypt.h"

// An entry in the IFS package
struct IFSFileEntry
{
	uint32_t FilePackageIndex;
	uint64_t FilePosition;
	uint64_t FileSize;
	uint64_t CompressedSize;
	uint64_t Flags;
};

// A class that handles reading from IFS packages
class IFSLib
{
public:
	// Constructors
	IFSLib();
	virtual ~IFSLib();
	
	// Parse and load an IFS file to the package cache
	void AddPackage(const std::string& PackagePath);
	// Parse and load an IFS file with the list file
	std::vector<std::string> ParsePackage(const std::string& PackagePath);
	// Parse and load all available IFS packages in the path
	void MountIFSPath(const std::string& IFSPath);

	// Gets the count of entries
	size_t GetLoadedEntries();

	// Attemps to read an entry (Name is the file name, with extension)
	std::unique_ptr<uint8_t[]> ReadFileEntry(const std::string& Name, uint32_t& ResultSize);

private:

	// A list of loaded IFS files
	std::unordered_map<uint64_t, IFSFileEntry> IFSFiles;
	// A list of loaded IFSPackage paths
	std::vector<std::string> IFSPackages;

	// Loads a package, inserting into the given listfile
	void LoadPackageInternal(const std::string& PackagePath, std::vector<std::string>& LoadedListFile, bool Audio = false);

	// The encryption key base
	symmetric_CTR EncryptionKey;

	// Initialize the IFS code, and setup the encryption
	void Initialize();
};
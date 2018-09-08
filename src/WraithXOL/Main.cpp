#include <conio.h>
#include <stdio.h>
#include <memory>
#include <chrono>

// Wraith application and api (Must be included before additional includes)
#include "WraithApp.h"
#include "WraithX.h"
#include "Console.h"
#include "Hashing.h"
#include "Sound.h"
#include "Instance.h"
#include "FileSystems.h"
#include "IFSLib.h"
#include "Systems.h"

// We need the online game module
#include "GameOnline.h"

// Allows straight unpacking of IFS
void UnpackIFSFile(const std::string& IFS, bool DDS = false)
{
	// Make it
	auto ExportFolder = FileSystems::CombinePath(FileSystems::CombinePath(FileSystems::GetApplicationPath(), "exported_files\\codol"), FileSystems::GetFileNameWithoutExtension(IFS));
	// Create it
	FileSystems::CreateDirectory(ExportFolder);

	// Mount the IFS file
	auto IFSHandler = IFSLib();
	// Load it
	auto ListFile = IFSHandler.ParsePackage(IFS);

	// Log info
	Console::WriteLineHeader("IFS", "Loaded \"%s\"", FileSystems::GetFileName(IFS).c_str());
	Console::WriteLineHeader("IFS", "Loaded %d files", ListFile.size());

	// Iterate and export them
	for (auto& File : ListFile)
	{
		// Fetch result file
		auto FileName = FileSystems::GetFileName(File);

		// Extract it
		uint32_t ResultSize = 0;
		// Read the entry
		auto ResultFile = IFSHandler.ReadFileEntry(FileName, ResultSize);

		// If not blank, export
		if (ResultFile != nullptr)
		{
			// If MP3, write it, else, convert it
			if (Strings::EndsWith(FileName, ".mp3"))
			{
				// Write to a file
				auto Writer = BinaryWriter();
				Writer.Create(FileSystems::CombinePath(ExportFolder, FileName));
				Writer.Write(ResultFile.get(), ResultSize);
			}
			else
			{
				// Convert IWI
				auto IWIConv = CoDIWITranslator::TranslateIWI(ResultFile, ResultSize);
				// Check
				if (IWIConv != nullptr)
				{
					// Save to a file
					if (DDS)
					{
						// Write to a file, with a dds
						auto Writer = BinaryWriter();
						Writer.Create(FileSystems::CombinePath(ExportFolder, FileSystems::GetFileNameWithoutExtension(FileName) + ".dds"));
						Writer.Write(IWIConv->DataBuffer, IWIConv->DataSize);
					}
					else
					{
						// Transcode to PNG
						Image::ConvertImageMemory(IWIConv->DataBuffer, IWIConv->DataSize, ImageFormat::DDS_WithHeader, FileSystems::CombinePath(ExportFolder, FileSystems::GetFileNameWithoutExtension(FileName) + ".png"), ImageFormat::Standard_PNG);
					}
				}
			}

			// Log it
			Console::WriteLineHeader("IFS", "Exported \"%s\"", FileSystems::GetFileNameWithoutExtension(FileName).c_str());
		}
	}

	// Log complete
	Console::WriteLineHeader("IFS", "Exported all existing IFS assets");
}

// Main entry point of app
int main(int argc, char** argv)
{
	// Start the instance (We must provide the main window title, never include versions from now on)
	auto CanContinue = Instance::BeginSingleInstance("Wraith Cyborg");
	// Only resume if we can
	if (CanContinue)
	{
		// Initialize the API (This must be done BEFORE running a WraithApp)
		if (!WraithX::InitializeAPI(true))
		{
			// Failed to initialize
			MessageBoxA(NULL, "A fatal error occured while initializing Wraith", "Wraith", MB_OK | MB_ICONEXCLAMATION);
			// Failed
			return -1;
		}

		// Enter debug mode
		Systems::EnterDebugMode();

		// Setup the custom color scheme
		Console::InitializeWraithColorPallete();
		Console::SetTitle("Wraith Cyborg");
		// Output heading
		Console::WriteLineHeader("Initialize", "-----------------------------------------------------------------");
		Console::WriteLineHeader("Initialize", "Wraith Cyborg (CODOL Asset Extractor)");
		Console::WriteLineHeader("Initialize", "Author: DTZxPorter");
		Console::WriteLineHeader("Initialize", "Desc: Allows extraction of models, animations, images and sounds");
		Console::WriteLineHeader("Initialize", "-----------------------------------------------------------------");

		// If we have an argument, and the argument is an IFS file, jump to the generic unpacker
		if (argc > 1 && Strings::EndsWith(argv[1], ".ifs"))
		{
			// DDS flags
			if (argc > 2 && Strings::StartsWith(argv[2], "dds"))
			{
				// Ask to unpack this
				UnpackIFSFile(std::string(argv[1]), true);
			}
			else
			{
				// Ask to unpack this
				UnpackIFSFile(std::string(argv[1]));
			}

			// End the routine
			return 0;
		}
		
		// Prepare the game manager, we must attach early in the spawn!
		if (GameOnline::LoadGame())
		{
			// From here, pass specific arguments to the exporter, we can do everything now... (All we needed was the handle...)
			while (true)
			{
				// Ask what we want to rip
				Console::WriteHeader("User Input", "Command: ");
				// Get the user input, split into command
				auto SplitCommand = Strings::SplitString(Strings::ToLower(Console::ReadLine()), ' ', true);

				// Reset export config
				GameOnline::ExportConfiguration = GameExportConfig();

				// Ignore blank
				if (SplitCommand.size() <= 0)
					continue;
				
				// Check
				if (SplitCommand[0] == "exit")
					break;
				else if (SplitCommand[0] == "ripanims")
				{
					// Rip XAnims to a file, if we have param 2, it's format (default seanim, alt xanim)
					if (SplitCommand.size() > 1 && SplitCommand[1] == "xanimwaw")
					{
						GameOnline::ExportConfiguration.SEAnims = false;
						GameOnline::ExportConfiguration.XAnimsWAW = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] == "xanimbo")
					{
						GameOnline::ExportConfiguration.SEAnims = false;
						GameOnline::ExportConfiguration.XAnimsBO = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] != "seanim")
					{
						// Error
						Console::WriteLineHeader("Command", "Unknown format, valid: \"xanimwaw, xanimbo\" (Default: seanim)");

						// Next
						continue;
					}

					// Rip, then log
					GameOnline::ExtractAssets(true, false, false, false);
					Console::WriteLineHeader("Exporter", "Exported all loaded XAnims...");
				}
				else if (SplitCommand[0] == "ripmodels")
				{
					// Rip XModels to a file, if we have param 2, it's format (default ma)
					if (SplitCommand.size() > 1 && SplitCommand[1] == "obj")
					{
						GameOnline::ExportConfiguration.Maya = false;
						GameOnline::ExportConfiguration.OBJ = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] == "xna")
					{
						GameOnline::ExportConfiguration.Maya = false;
						GameOnline::ExportConfiguration.XNA = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] == "smd")
					{
						GameOnline::ExportConfiguration.Maya = false;
						GameOnline::ExportConfiguration.SMD = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] == "xme")
					{
						GameOnline::ExportConfiguration.Maya = false;
						GameOnline::ExportConfiguration.XME = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] != "ma")
					{
						// Error
						Console::WriteLineHeader("Command", "Unknown format, valid: \"obj, xna, smd, xme\" (Default: ma)");

						// Next
						continue;
					}

					// Validate the format next
					if (SplitCommand.size() > 2 && SplitCommand[2] == "dds")
					{
						GameOnline::ExportConfiguration.PNG = false;
						GameOnline::ExportConfiguration.DDS = true;
					}
					else if (SplitCommand.size() > 2 && SplitCommand[2] != "png")
					{
						// Error
						Console::WriteLineHeader("Command", "Unknown format, valid: \"dds\" (Default: png)");

						// Next
						continue;
					}

					// Rip, then log
					GameOnline::ExtractAssets(false, true, false, false);
					Console::WriteLineHeader("Exporter", "Exported all loaded XModels");
				}
				else if (SplitCommand[0] == "ripimages")
				{
					// Rip XImages to a file, if we have param 2, it's format (default png, alt dds)
					if (SplitCommand.size() > 1 && SplitCommand[1] == "dds")
					{
						GameOnline::ExportConfiguration.PNG = false;
						GameOnline::ExportConfiguration.DDS = true;
					}
					else if (SplitCommand.size() > 1 && SplitCommand[1] != "png")
					{
						// Error
						Console::WriteLineHeader("Command", "Unknown format, valid: \"dds\" (Default: png)");

						// Next
						continue;
					}

					// Rip, then log
					GameOnline::ExtractAssets(false, false, true, false);
					Console::WriteLineHeader("Exporter", "Exported all loaded XImages");
				}
				else if (SplitCommand[0] == "ripsounds")
				{
					// Rip loaded Sounds to a file
					GameOnline::ExtractAssets(false, false, false, true);
					Console::WriteLineHeader("Exporter", "Exported all loaded Sounds");
				}
				else
				{
					// Unknown command
					Console::WriteLineHeader("Command", "Unknown command, try \"ripanims, ripmodels, ripsounds, or ripimages\"");
				}
			}
		}
		else
		{
			// Log error
			Console::SetBackgroundColor(ConsoleColor::Red);
			Console::WriteLineHeader("Game", "Failed to attach to the game, you may have launched too late!");
			Console::SetBackgroundColor(ConsoleColor::Black);
			Console::ReadKey();
		}

		// Leave debug
		Systems::LeaveDebugMode();

		// Shutdown the API, we're done
		WraithX::ShutdownAPI(false);

		// Stop the instance
		Instance::EndSingleInstance();
	}

	// We're done here
	return 0;
}
// AutoFileSynchronedline.cpp
// An automatic synchronization system
// 
// Version 0.0.1.1 by DOF Studio
// Opensourced with Apache 2.0 License
//

#include <iostream>

#include "Libs/ThreadPool.hpp"
#include "Libs/AdminAccess.hpp"

#include "AutoFileSynchronedline.hpp"

// Namespace AutoFileSync starts
namespace AutoFileSync
{
	// Afsync Command line system (requires admin prev)
	//
	// Automatic File Synchronizor (afsync)
	// Copy Right: DOF Studio 2024
	// 
	// Syntax: programname.exe src dest [optional args]
	// Optional Args Syntax: -arg_name=arg_value
	// Optional Args: 
	//   -subf  whether to monitor subfolders or not, non-0 or 0, defualt 0
	//   -intv  synchronization interval, in msecond, default 300000
	//   -verb  verbosity setting, 0 or 1 or 2, default 2
	//   -core  multi-thread threads used, any Z+, default 20
	int AutoFileSyncCommandline(int argc, char* argv[]) noexcept
	{
		// Standard Admin-Fetching Template
		//
		//
		// if it is not in admin mode,
		if (!AdminAccess::process_isadmin())
		{
			// Do something...
			std::cout << "Automatic File Copier needs admin privileges!" << std::endl;
			std::cout << "Process ID [" << AdminAccess::processid() << "]." << std::endl;
			return -1;
		}

		// Too few args, print help then
		if (argc < 3)
		{
			std::cout << "Automatic File Synchronizor (afsync)" << std::endl;
			std::cout << "Copy Right : DOF Studio 2024" << std::endl;
			std::cout << "" << std::endl;
			std::cout << "Syntax: program_name.exe src dest [optional args]" << std::endl;
			std::cout << "Optional Args Syntax: -arg_name=arg_value" << std::endl;
			std::cout << "Optional Args: " << std::endl;
			std::cout << "  -subf  whether to monitor subfolders or not, non-0 or 0, defualt 0" << std::endl;
			std::cout << "  -intv  synchronization interval, in msecond, default 300000" << std::endl;
			std::cout << "  -verb  verbosity setting, 0 or 1 or 2, default 2" << std::endl;
			std::cout << "  -core  multi-thread threads used, any Z+, default 20" << std::endl;
			std::cout << "" << std::endl;
			std::cout << "! Error, too few arguments!" << std::endl;

			return -1;
		}

		// Default args
		const std::string src = std::string(argv[1]);
		const std::string dest = std::string(argv[2]);
		bool has_subfolder = false;
		long long interval = 300000;
		long long verbosity = 2;
		long long cores = 20;

		// Eval args
		for (int i = 3; i < argc; ++i)
		{
			std::string arg = argv[i];

			// Valid arg
			if (arg.starts_with("-subf="))
			{
				std::string arg_content = arg.substr(strlen("-subf="));
				has_subfolder = atoll(arg_content.c_str()) != 0;
			}
			else if (arg.starts_with("-intv="))
			{
				std::string arg_content = arg.substr(strlen("-intv="));
				interval = atoll(arg_content.c_str());
				if (interval == 0)
				{
					interval = 300000;
				}
			}
			else if (arg.starts_with("-verb="))
			{
				std::string arg_content = arg.substr(strlen("-verb="));
				verbosity = atoll(arg_content.c_str());
				if (verbosity > 2 || verbosity < 0)
				{
					verbosity = 2;
				}
			}
			else if (arg.starts_with("-core="))
			{
				std::string arg_content = arg.substr(strlen("-core="));
				cores = atoll(arg_content.c_str());
				if (cores > tpool::ThreadNum() || cores <= 0)
				{
					cores = tpool::ThreadNum();
				}
			}

			// Invalid arg
			else
			{
				std::cout << "Omitted invalid arg: " << arg << std::endl;
			}
		}

		// Start the service
		AutoFileSynchonizor afsync(src, dest, has_subfolder, {}, interval, verbosity, cores);
		if (afsync.api_start_working() == false)
		{
			std::cout << "! Error, failed to start the synchronizor." << std::endl;
			return -2;
		}

		// Stop the service
		std::cout << "To stop that process, please type in \"stop\" in lower cases." << std::endl;
		std::cout << std::endl;
		std::string readline;
		do
		{
			std::cin >> readline;
		} while (readline != "stop");
		if (afsync.api_stop_working() == false)
		{
			std::cout << "! Error, failed to stop the synchronizor." << std::endl;
			return -3;
		}
		else
		{
			std::cout << "The synchronization service has stopped." << std::endl;
			return 0;
		}
	}

}
// Namespace AutoFileSync ends

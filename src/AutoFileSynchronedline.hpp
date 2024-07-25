// AutoFileSynchronedline.hpp
// An automatic synchronization system
// 
// Version 0.0.1.1 by DOF Studio
// Opensourced with Apache 2.0 License
//

#include "AutoFileSynchronizor.hpp"

#pragma once

#pragma warning (disable: 4018)
#pragma warning (disable: 4244)
#pragma warning (disable: 4251)
#pragma warning (disable: 4267)
#pragma warning (disable: 4661)
#pragma warning (disable: 4715)
#pragma warning (disable: 4804)
#pragma warning (disable: 4819)
#pragma warning (disable: 4919)
#pragma warning (disable: 4996)
#pragma warning (disable: 6031)

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
	int AutoFileSyncCommandline(int argc, char* argv[]) noexcept;

}
// Namespace AutoFileSync ends

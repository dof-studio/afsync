// AutoFileSync_Entrypoint.cpp
// An automatic synchronization system
// 
// Version 0.0.1.1 by DOF Studio
// Opensourced with Apache 2.0 License
//

#include <iostream>

#include "AutoFileSynchronizor.hpp"
#include "AutoFileSynchronedline.hpp"

// Debug or Release Mode
#define AFSYNC_DEBUGON        0

// Entrypoint
int main(int argc, char* argv[])
{
	#if AFSYNC_DEBUGON == 1
	// Debug Interactive line Mode
	std::string src;
	std::string dest = "$";
	long long interval = 300000;
	long long verbosity = 2;
	long long cores = 20;
	std::cout << "Src: "; std::cin >> src;
	std::cout << "Interval: "; std::cin >> interval;
	std::cout << "Verbosity: "; std::cin >> verbosity;
	std::cout << "Cores: "; std::cin >> cores;
	AutoFileSync::AutoFileSynchonizor afsync(src, dest, false, {}, interval, verbosity, cores);
	if (afsync.api_start_working() == false)
	{
		std::cout << "Failed to start!\r\n";
		system("pause");
		return -1;
	}
	std::cout << "Started! Press enter to stop!\r\n";
	std::string enterToStop;
	std::cin >> enterToStop;
	afsync.api_stop_working();

	#else
	// Release Command line Mode
	return AutoFileSync::AutoFileSyncCommandline(argc, argv);

	#endif

	return 0;
}

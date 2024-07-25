// AutoFileSynchronizor.hpp
// An automatic synchronization system
// 
// Version 0.0.1.1 by DOF Studio
// Opensourced with Apache 2.0 License
//

#include <string>
#include <vector>
#include <thread>
#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>

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

// DEFINE AUTOFILECOPIER VERSION
#define __AUTOFILECOPIER_VERSION__             0x00000011

// DEFINE AUTOFILECOPIER DLL EXPORT
#define __AUTOFILECOPIER_DLL_EXPORT__         _declspec(dllexport)

// DEFINE AUTOFILECOPIER INLINE FUNCTION
#define __AUTOFILECOPIER_INLINE_FUNCTION__     inline

// DEFINE AUTOFILECOPIER EXTERN FUNCTION
#define __AUTOFILECOPIER_EXTERN_FUNCTION__     extern

// DEFINE AUTOFILECOPIER TEMPLATE FUNCTION
#define __AUTOFILECOPIER_TEMPLATE__            template

// DEFINE AUTOFILECOPIER CLASS NOTATION
#define __AUTOFILECOPIER_CLASS__               class

// DEFINE AUTOFILECOPIER CONST NOTATION
#define __AUTOFILECOPIER_CONST__               const

// DEFINE AUTOFILECOPIER FUNCTION NOTATION
#define __AUTOFILECOPIER_FUNCTION__            

// Namespace AutoFileSync starts
namespace AutoFileSync
{
	// class AutoFileSynchonizor
	// 自动进行对指定文件夹进行备份
	// 备份，每隔一段时间，若：
	// 1) 监控的文件夹内文件数量有所变化
	// 2) 数量不变但是crc变化
	// 那么进行备份
	__AUTOFILECOPIER_CLASS__
	__AUTOFILECOPIER_DLL_EXPORT__
	AutoFileSynchonizor
	{
	private:
		// Srouce to be mointored
		std::string _src = "";
		// Including subfolders
		bool _src_has_subfolders = false;
		// Excluding subfolder names
		std::vector<std::string> _src_except_subfolders;
		std::unordered_set<std::string> _src_set_except_subfolders;

	private:
		// Destination to copy
		std::string _dest = "";

		// Files to check crc
		std::vector<std::string> _file_tochk;

		// File and subfolder names to copy
		std::vector<std::string> _file_sub_tocopy;

	private:
		// Different crc count
		long long different_count = 0;
		// Accessory shared_mutex (shared_lock for readers and unique_lock for writers)
		std::shared_mutex map_mutex;
		// Last - monitored files (fullpath) and hashes
		std::unordered_map<std::string, unsigned long long> last_monitored;
		// Current - mointored files (fullpath) and hashes
		std::unordered_map<std::string, unsigned long long> current_monitored;
		// Note: unordered_map is NOT thread-safe, so use a mutex to avoid concurrency errors

	private:
		// Crc-checking threadpool ptr
		void* chck = nullptr;
		// Synchonizor threadpool ptr
		void* sync = nullptr;

	private:
		// Configurations
		long long _confg_verbosity = 2;             // 打印系统工作日志0,1,2
		long long _confg_interval = 5 * 60 * 1000;  // 同步的检测时间间隔

		// Default settings
		long long _settings_sleepinterval = 200;    // 线程休眠检测的时间

		// Validity
		bool _valid = false;

		// Working Thread ptr
		std::thread* _worker = nullptr;

		// Working Clock ptr
		void* clock = nullptr;

		// Working Stop signal
		bool _worker_control_tostop = false;   // send stop signal
		bool _worker_feedback_stopped = false; // the thread has stopped

	public:
		// Constructor
		AutoFileSynchonizor(const std::string& src, const std::string& dest = "$",
			bool has_subfolders = false, const std::vector<std::string>& excluded_subfolders = {},
			long long interval = 5 * 60 * 1000, long long verbosity = 2, int cores = 20) noexcept;

		// Destructor
		~AutoFileSynchonizor() noexcept;

		// Copy constructor = delete
		AutoFileSynchonizor(const AutoFileSynchonizor& y) noexcept = delete;
		AutoFileSynchonizor& operator=(const AutoFileSynchonizor& y) noexcept = delete;

		// Move constructor = delete
		AutoFileSynchonizor(AutoFileSynchonizor&& y) noexcept = delete;
		AutoFileSynchonizor& operator=(AutoFileSynchonizor&& y) noexcept = delete;

		// Validity
		bool valid() const noexcept;

	private:
		// Kernel - Once, update file info
		bool _kernel_once_updfileinfo() noexcept;

		// Kernel - Thread, compute crc of a given file (write to map)
		void _kernel_thread_computecrc(const std::string& filepath, bool compare = true);

		// Kernel - Once, checking synchronizable (called by gotosync)
		bool _kernel_once_chksync() noexcept;

		// Kernel - Once, go to synchronize (calling check and maybe copy files)
		bool _kernel_once_gotosync() noexcept;

		// Kernel - Thread, Loop, continuously monitoring (working thread)
		void _kernel_thread_loop_workingthread() noexcept;

		// Kernel - Once, start monitoring (on the working thread)
		bool _kernel_once_startworking() noexcept;

		// Kernel - Once, stop monitoring (on the working thread)
		bool _kernel_once_stopworking() noexcept;

	public:
		// API - Once, start monitoring (on the working thread)
		bool api_start_working() noexcept;

		// API - Once, stop monitoring (on the working thread)
		bool api_stop_working() noexcept;
	};

}
// Namespace AutoFileSync ends

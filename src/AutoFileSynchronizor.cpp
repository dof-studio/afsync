// AutoFileSynchronizor.cpp
// An automatic synchronization system
// 
// Version 0.0.1.1 by DOF Studio
// Opensourced with Apache 2.0 License
//

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

#include "Libs/CRC.hpp"
#include "Libs/FILE.hpp"
#include "Libs/Clock.hpp"
#include "Libs/ThreadPool.hpp"

#include "AutoFileSynchronizor.hpp"

// Namespace AutoFileSync starts
namespace AutoFileSync
{
	// Utils (not headerable)
	// Kernel - tpool::ThreadPool pointer fetcher
	__AUTOFILECOPIER_FUNCTION__
	__AUTOFILECOPIER_INLINE_FUNCTION__
	tpool::ThreadPool* _afsync_util_threadpool_ptr(void* anyptr) noexcept
	{
		return (tpool::ThreadPool*)anyptr;
	}

	// Utils (not headerable)
	// Kernel - Clocks::Clock pointer fetcher
	__AUTOFILECOPIER_FUNCTION__
	__AUTOFILECOPIER_INLINE_FUNCTION__
	Clocks::Clock* _afsync_util_clock_ptr(void* anyptr) noexcept
	{
		return (Clocks::Clock*)anyptr;
	}

	// class AutoFileSynchonizor
	// 自动进行对指定文件夹进行备份
	// 备份，每隔一段时间，若：
	// 1) 监控的文件夹内文件数量有所变化
	// 2) 数量不变但是crc变化
	// 那么进行备份

	// Constructor
	AutoFileSynchonizor::AutoFileSynchonizor(const std::string& src, const std::string& dest,
		bool has_subfolders, const std::vector<std::string>& excluded_subfolders,
		long long interval, long long verbosity, int cores) noexcept
	{
		// Create threadpools
		tpool::ThreadPool* chck_nptr = _afsync_util_threadpool_ptr(chck);
		chck_nptr = new tpool::ThreadPool(cores);
		tpool::ThreadPool* sync_nptr = _afsync_util_threadpool_ptr(sync);
		sync_nptr = new tpool::ThreadPool(cores);

		// Create timer clocks
		Clocks::Clock* clock_nptr = _afsync_util_clock_ptr(clock);
		clock_nptr = new Clocks::Clock();

		// Eval Elements
		this->_src = abspath(src);
		this->_dest = abspath(dest);
		this->_src_has_subfolders = has_subfolders;
		this->_src_except_subfolders = excluded_subfolders;
		this->_confg_interval = interval;
		this->_confg_verbosity = verbosity;

		// Reset elements - See if a default dest is set
		if (dest == "" || dest == "$" || direxist(this->_dest) == false)
		{
			this->_dest = this->_src.substr(0, min(_src.find_last_of('/'), _src.find_last_of('\\'))) + "/AutoFileCopier/Synchronization/";
		}

		// Create a set
		for (const std::string& subs : _src_except_subfolders)
		{
			_src_set_except_subfolders.insert(subs);
		}

		// Check src and dest validity
		if (direxist(this->_src) == false)
		{
			this->_valid = false;
			return;
		}
		if (direxist(this->_dest) == false)
		{
			if (makedirs(this->_dest) == false)
			{
				this->_valid = false;
				return;
			}
		}

		this->_valid = true;
		return;
	}

	// Destructor
	AutoFileSynchonizor::~AutoFileSynchonizor() noexcept
	{
		// Release pointers
		if (this->chck != nullptr)
		{
			tpool::ThreadPool* chck_nptr = _afsync_util_threadpool_ptr(chck);
			delete chck_nptr;
			chck_nptr = nullptr;
			chck = nullptr;
		}
		if (this->sync != nullptr)
		{
			tpool::ThreadPool* sync_nptr = _afsync_util_threadpool_ptr(sync);
			delete sync_nptr;
			sync_nptr = nullptr;
			sync = nullptr;
		}
		if (this->clock != nullptr)
		{
			Clocks::Clock* clock_nptr = _afsync_util_clock_ptr(clock);
			delete clock_nptr;
			clock_nptr = nullptr;
			clock = nullptr;
		}
		if (this->_worker != nullptr)
		{
			delete _worker;
			_worker = nullptr;
		}

		return;
	}
	
	// Validity
	bool AutoFileSynchonizor::valid() const noexcept
	{
		return this->_valid;
	}

	// Kernel - Once, update file info
	bool AutoFileSynchonizor::_kernel_once_updfileinfo() noexcept
	{
		if (this->_valid == false)
		{
			return false;
		}

		// If the src is not existing
		if (direxist(this->_src) == false)
		{
			this->_valid = false;
			return false;
		}

		// Get the files within the mother folder
		std::vector<std::string> mother_list = listdir(this->_src, false);
		for (std::string& it : mother_list)
		{
			it = this->_src + "/" + it;
			it = abspath(it);
		}

		// Try to split files and folders
		std::vector<std::string> mother_files;
		std::vector<std::string> mother_folders;
		for (const std::string& it : mother_list)
		{
			if (fileexist(it) == false)
			{
				mother_folders.push_back(it);
			}
			else
			{
				mother_files.push_back(it);
			}
		}

		// If applicable, try to get the allowed subfolders and subfiles
		std::vector<std::string> allowed_subfolders;
		std::vector<std::string> allowed_subfiles;
		if (this->_src_has_subfolders)
		{
			// Try to deliminate the "\\" or "/" in the end
			for (std::string& it : mother_folders)
			{
				if (it.ends_with("\\") || it.ends_with("/"))
				{
					it = it.substr(0, it.size() - 1);
				}
			}

			// Get the pure folder name and register allowed ones
			for (const std::string& it : mother_folders)
			{
				size_t nmstart = min(it.find_last_of('/', 0), it.find_last_of('\\', 0));
				std::string purename = it.substr(nmstart + 1);
				// Not in the exception list
				if (_src_set_except_subfolders.find(purename) == _src_set_except_subfolders.end())
				{
					allowed_subfolders.push_back(it);
				}
			}

			// For each subfolders, get subfiles and concat them
			for (const std::string& sub : allowed_subfolders)
			{
				std::vector<std::string> subfiles = dir(sub, false);
				for (const std::string& file : subfiles)
				{
					allowed_subfiles.push_back(abspath(file));
				}
			}
		}

		// Register: files to check crc
		this->_file_tochk = mother_files;
		for (std::string& it : allowed_subfiles)
		{
			this->_file_tochk.emplace_back(std::move(it));
		}

		// Register: files and folders to copy
		this->_file_sub_tocopy = mother_files;
		for (std::string& it : allowed_subfolders)
		{
			this->_file_sub_tocopy.emplace_back(std::move(it));
		}

		return true;
	}

	// Kernel - Thread, compute crc of a given file (write to map)
	void AutoFileSynchonizor::_kernel_thread_computecrc(const std::string& filepath, bool compare)
	{
		if (this->_valid == false)
		{
			return;
		}

		// File non-existed
		if (fileexist(filepath) == false)
		{
			return;
		}

		// Compute crc of a file
		// Lambda Functions
		auto __crccal__ = [](const char* filepath) -> unsigned long long
		{
			// Whether a file exists, and its hash is the same as the specified
			if (fileexist(filepath) == true)
			{
				// Create mem and tmp
				constexpr size_t tmpmem = 4 * 1024 * 1024;
				unsigned char* tmp = new unsigned char[tmpmem + 1];
				memset(tmp, 0, tmpmem + 1);
				WinReadWrite<unsigned char> io((unsigned char*)filepath, 0, tmpmem);
				if (io.Ready_() == false)
				{
					delete[] tmp;
					tmp = nullptr;
					return 0ULL;
				}

				// Create state to compute crc64
				crc64_table state = crc64_init();
				size_t readbytes = 0;
				while (io.filePosition_() < io.fileLength_())
				{
					// Read
					if (io.fileLength_() - io.filePosition_() >= tmpmem)
					{
						memset(tmp, 0, tmpmem + 1);
						readbytes = tmpmem;
						io.WinReadAuto(tmp, readbytes, 0, FILE_CUR);
					}
					else
					{
						memset(tmp, 0, tmpmem + 1);
						readbytes = io.fileLength_() - io.filePosition_();
						io.WinReadAuto(tmp, readbytes, 0, FILE_CUR);
					}

					// Update Hash
					crc64_update(tmp, readbytes, &state);
				}
				io.Close_();
				delete[] tmp;
				tmp = nullptr;

				uint64_t hash = crc64_final(&state);
				return hash;
			}

			return 0ULL;
		};
		unsigned long long crc = __crccal__(filepath.c_str());

		// Register the crc to the current_map
		this->map_mutex.lock();
		this->current_monitored[filepath] = crc;
		this->map_mutex.unlock();

		// If we need to compare, let's compare
		if (compare)
		{
			this->map_mutex.lock();
			auto it = this->last_monitored.find(filepath);

			// A new file
			if (it == this->last_monitored.end())
			{
				different_count++;
				this->map_mutex.unlock();
				return;
			}

			// Get the crc
			unsigned long long last_crc = this->last_monitored[filepath];

			// Pop back the last_mointored
			this->last_monitored.erase(filepath);
			this->map_mutex.unlock();

			// The same
			if (crc == last_crc)
			{
				return;
			}

			// Modified
			else
			{
				different_count++;
				return;
			}
		}

		return;
	}

	// Kernel - Once, checking synchronizable (called by gotosync)
	bool AutoFileSynchonizor::_kernel_once_chksync() noexcept
	{
		if (this->_valid == false)
		{
			return false;
		}

		// Update fileinfo
		if (_kernel_once_updfileinfo() == false)
		{
			return false;
		}

		// Ptr transformation
		tpool::ThreadPool* this_chck_nptr = _afsync_util_threadpool_ptr(chck);
		tpool::ThreadPool* this_sync_nptr = _afsync_util_threadpool_ptr(sync);

		// 如果是第一次检查(总是需要备份)
		if (_file_tochk.size() > 0 && last_monitored.size() == 0)
		{
			// Initials
			this->different_count = 0;
			this->map_mutex.lock();
			this->current_monitored.clear();
			this->map_mutex.unlock();

			// Lambda
			auto __ = [this](const std::string& filepath, bool compare) -> void
			{
				this->_kernel_thread_computecrc(filepath, compare);
				return;
			};

			// 循环计算所有的crc
			for (const std::string& it : _file_tochk)
			{
				this_chck_nptr->Invoke(__, it, true);
			}
			this_chck_nptr->WaitTillAll();

			// 如果Pop过后的last_monitor还有文件，那么different_count+=.size()
			this->map_mutex.lock();
			if (this->last_monitored.size() > 0)
			{
				this->different_count += this->last_monitored.size();
			}
			
			// 将current挪到last
			this->last_monitored = this->current_monitored;
			this->map_mutex.unlock();

			// 总是需要备份
			return true;
		}

		// 如果不是第一次检查
		else if (_file_tochk.size() > 0 && last_monitored.size() > 0)
		{
			// Initials
			this->different_count = 0;
			this->map_mutex.lock();
			this->current_monitored.clear();
			this->map_mutex.unlock();

			// Lambda
			auto __ = [this](const std::string& filepath, bool compare) -> void
			{
				this->_kernel_thread_computecrc(filepath, compare);
				return;
			};

			// 文件数量有变，直接需要备份
			this->map_mutex.lock_shared();
			if (_file_tochk.size() != last_monitored.size())
			{
				this->map_mutex.unlock_shared();

				// 循环计算所有的crc
				for (const std::string& it : _file_tochk)
				{
					this_chck_nptr->Invoke(__, it, true);
				}
				this_chck_nptr->WaitTillAll();

				// 如果Pop过后的last_monitor还有文件，那么different_count+=.size()
				this->map_mutex.lock();
				if (this->last_monitored.size() > 0)
				{
					this->different_count += this->last_monitored.size();
				}

				// 将current挪到last
				this->last_monitored = this->current_monitored;
				this->map_mutex.unlock();

				// 直接需要备份
				return true;
			}

			// 否则，crc有变，需要备份
			else
			{
				this->map_mutex.unlock_shared();

				// 循环计算所有的crc
				for (const std::string& it : _file_tochk)
				{
					this_chck_nptr->Invoke(__, it, true);
				}
				this_chck_nptr->WaitTillAll();

				// 如果Pop过后的last_monitor还有文件，那么different_count+=.size()
				this->map_mutex.lock();
				if (this->last_monitored.size() > 0)
				{
					this->different_count += this->last_monitored.size();
				}

				// 将current挪到last
				this->last_monitored = this->current_monitored;
				this->map_mutex.unlock();

				// 检查是否有crc不一致
				if (this->different_count > 0)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}

		// 可能，没有文件需要检查
		return false;
	}

	// Kernel - Once, go to synchronize (calling check and maybe copy files)
	bool AutoFileSynchonizor::_kernel_once_gotosync() noexcept
	{
		if (this->_valid == false)
		{
			return false;
		}

		// Go to synchronize
		if (this->_kernel_once_chksync() == true)
		{
			// variable: _file_tochk has stored the files that were lastly checked
			// variable: _file_sub_tocopy has stored the files and folders that need to be copied and synchronized

			// Lambda to get the current time in string
			// Note 这里必须用%H.%M.%S，因为windows文件名不支持:
			auto curtime = [](const std::string & format = "%Y-%m-%d %H.%M.%S") ->std::string
			{
				// Get current time as time_t object
				std::time_t t = std::time(nullptr);

				// Convert time_t to tm struct for local time
				std::tm* localTime = std::localtime(&t);

				// Use stringstream for formatted output
				std::stringstream ss;

				// Use std::put_time to format the time as per given format string
				ss << std::put_time(localTime, format.c_str());

				// Return the formatted string
				return ss.str();
			};
			
			// Lambda to replace backslashes with forward slashes
			auto replblack = [](std::string path) -> std::string
			{
				std::replace(path.begin(), path.end(), '\\', '/');
				return path;
			};

			// Lambda to trim trailing slashes
			auto trimtails = [](std::string path) -> std::string
			{
				while (!path.empty() && path.back() == '/') {
					path.pop_back();
				}
				return path;
			};

			// Lambda to get the file or folder name from the path
			auto filenamer = [&](const std::string& path) -> std::string
			{
				std::string modifiedPath = replblack(path);
				modifiedPath = trimtails(modifiedPath);
				size_t lastSlashPos = modifiedPath.find_last_of('/');

				if (lastSlashPos == std::string::npos)
				{
					// No slash found, return the entire string
					return modifiedPath;
				}
				else 
				{
					// Return the substring after the last slash
					return modifiedPath.substr(lastSlashPos + 1);
				}
			};

			// Create new sync folder name and folder
			std::string folder_name = filenamer(this->_src) + " " + curtime();
			std::string folder_path = abspath(this->_dest) + "/" + folder_name;
			if (makedirs(folder_path) == false)
			{
				return false;
			}

			// Copy files into the new folder
			for (const std::string& it : this->_file_sub_tocopy)
			{
				// file
				if (fileexist(it) == true)
				{
					copy(it, folder_path + "/" + filenamer(it), true);
				}

				// folder
				else if(direxist(it) == true)
				{
					copyAll(it, folder_path + "/" + filenamer(it), true);
				}

				// Invalid, maybe deleted, ignore it
			}

			return true;
		}

		// No need~
		else
		{
			return true;
		}
	}

	// Kernel - Thread, Loop, continuously monitoring (working thread)
	void AutoFileSynchonizor::_kernel_thread_loop_workingthread() noexcept
	{
		if (this->_valid == false)
		{
			return;
		}

		// Lambda to get the current time in string
		auto curtime = [](const std::string& format = "%Y-%m-%d %H:%M:%S") ->std::string
		{
			// Get current time as time_t object
			std::time_t t = std::time(nullptr);

			// Convert time_t to tm struct for local time
			std::tm* localTime = std::localtime(&t);

			// Use stringstream for formatted output
			std::stringstream ss;

			// Use std::put_time to format the time as per given format string
			ss << std::put_time(localTime, format.c_str());

			// Return the formatted string
			return ss.str();
		};

		// 计时开始
		Clocks::Clock* clock_nptr = _afsync_util_clock_ptr(clock);
		clock_nptr->start();

		// Verbosity - start print
		if (this->_confg_verbosity >= 1)
		{
			std::cout << curtime() << " : " << "Synchonization starts!" << std::endl;
			std::cout << curtime() << " : " << "Mointering at directory " + this->_src << std::endl;
			std::cout << std::endl;
		}

		// While stop signal is not sent
		while (this->_worker_control_tostop == false)
		{
			// 如果计时器经过时间大于预设，备份
			if (clock_nptr->elapse() * 1000 > this->_confg_interval)
			{
				// 计时重新启动
				clock_nptr->end();
				clock_nptr->start();

				// Call 备份 _kernel_once_gotosync()
				bool syncresl = this->_kernel_once_gotosync();

				// Verbosity - sync result print
				if (this->_confg_verbosity >= 1)
				{
					size_t diffcnt = this->different_count;
					if (syncresl == true && diffcnt > 0)
					{
						std::cout << curtime() << " : " << "A new synchonization successfully created!";
						std::cout << ", " << this->different_count << " files are updated!" << std::endl;
					}
					else if (syncresl == true && diffcnt == 0)
					{
						std::cout << curtime() << " : " << "No change detected... Pending to synchronize..." << std::endl;
					}
					else
					{
						std::cout << curtime() << " : " << "An error happened in the new synchonization." << std::endl;
					}
				}

				// Verbosity - files and crc print
				if (this->_confg_verbosity >= 2)
				{
					const std::string spaces = "      ";
					
					this->map_mutex.lock_shared();

					// Copy of Last - monitored files (fullpath) and hashes
					std::unordered_map<std::string, unsigned long long> lm = this->last_monitored;
					// Copy of Current - mointored files (fullpath) and hashes
					std::unordered_map<std::string, unsigned long long> cm = this->current_monitored;

					this->map_mutex.unlock_shared();

					// Print file existance and crcs
					for (const auto& it : cm)
					{
						// Variable preps
						const std::string& filepath = it.first;
						const unsigned long long cur_hash = it.second;
						const bool fileexistance = lm.find(filepath) != lm.end();
						const unsigned long long las_hash = (fileexistance ? lm[filepath] : 0);

						// Print
						if (fileexistance == true)
						{
							std::cout << spaces << "An existing file " << filepath << std::endl;
						}
						else
						{
							std::cout << spaces << "An updated file  " << filepath << " : current hash " << cur_hash << std::endl;
						}
					}
				}

				// Verbosity - endl print
				if (this->_confg_verbosity >= 1)
				{
					std::cout << std::endl;
				}
			}

			// 休眠
			Sleep(this->_settings_sleepinterval);
		}

		// 计时结束
		clock_nptr->end();

		// Verbosity - stop print
		if (this->_confg_verbosity >= 1)
		{
			std::cout << curtime() << " : " << "Synchonization was stopped by the user!" << std::endl;
			std::cout << std::endl;
		}

		// Send stop working feedback
		this->_worker_feedback_stopped = true;

		return;
	}

	// Kernel - Once, start monitoring (on the working thread)
	bool AutoFileSynchonizor::_kernel_once_startworking() noexcept
	{
		if (this->_valid == false)
		{
			return false;
		}

		if (this->_worker_control_tostop == true)
		{
			return false;
		}

		// Check if there's thread not aborted, abort this function
		if (this->_worker != nullptr)
		{
			return false;
		}

		// Refresh variables
		this->_worker_control_tostop = false;
		this->_worker_feedback_stopped = false;

		// Start the new thread
		auto __ = [this]() -> void
		{
			this->_kernel_thread_loop_workingthread();
			return;
		};
		this->_worker = new std::thread(__);
		this->_worker->detach();

		return true;
	}

	// Kernel - Once, stop monitoring (on the working thread)
	bool AutoFileSynchonizor::_kernel_once_stopworking() noexcept
	{
		if (this->_valid == false)
		{
			return false;
		}

		if (this->_worker_feedback_stopped == true)
		{
			return false;
		}

		// Check if there's thread not started, abort this function
		if (this->_worker == nullptr)
		{
			return false;
		}

		// Sending the stop signal
		this->_worker_control_tostop = true;

		// Wait until stop
		while (this->_worker_feedback_stopped == false)
		{
			// 休眠
			Sleep(this->_settings_sleepinterval);
		}

		// Release the resources of the worker thread
		delete this->_worker;
		this->_worker = nullptr;

		return true;
	}

	// API - Once, start monitoring (on the working thread)
	bool AutoFileSynchonizor::api_start_working() noexcept
	{
		return this->_kernel_once_startworking();
	}

	// API - Once, stop monitoring (on the working thread)
	bool AutoFileSynchonizor::api_stop_working() noexcept
	{
		return this->_kernel_once_stopworking();
	}

}
// Namespace AutoFileSync ends

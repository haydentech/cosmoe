/*
 * Copyright 2013, Paweł Dziepak, pdziepak@quarnos.org.
 * Copyright 2002-2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include <algorithm>


#include <system_info.h>


#if __HAIKU_BEOS_COMPATIBLE


#define LEGACY_B_CPU_X86			15

#define LEGACY_B_AT_CLONE_PLATFORM	2

typedef struct {
	bigtime_t	active_time;	/* usec of doing useful work since boot */
} legacy_cpu_info;

typedef struct {
	int32			id[2];				/* unique machine ID */
	bigtime_t		boot_time;			/* time of boot (usecs since 1/1/1970) */

	int32			cpu_count;			/* number of cpus */
	int32			cpu_type;			/* type of cpu */
	int32			cpu_revision;		/* revision # of cpu */
	legacy_cpu_info	cpu_infos[8];		/* info about individual cpus */
	int64			cpu_clock_speed;	/* processor clock speed (Hz) */
	int64			bus_clock_speed;	/* bus clock speed (Hz) */
	int32			platform_type;	/* type of machine we're on */

	int32			max_pages;			/* total # of accessible pages */
	int32			used_pages;			/* # of accessible pages in use */
	int32			page_faults;		/* # of page faults */
	int32			max_sems;
	int32			used_sems;
	int32			max_ports;
	int32			used_ports;
	int32			max_threads;
	int32			used_threads;
	int32			max_teams;
	int32			used_teams;

	char			kernel_name[256];
	char			kernel_build_date[32];
	char			kernel_build_time[32];
	int64			kernel_version;

	bigtime_t		_busy_wait_time;	/* reserved for whatever */

	int32			cached_pages;
	uint32			abi;				/* the system API */
	int32			ignored_pages;		/* # of ignored/inaccessible pages */
	int32			pad;
} legacy_system_info;


extern "C" status_t
_get_system_info(legacy_system_info* info, size_t size)
{
	if (info == NULL || size != sizeof(legacy_system_info))
		return B_BAD_VALUE;
	memset(info, 0, sizeof(legacy_system_info));

	system_info systemInfo;
	status_t error = _kern_get_system_info(&systemInfo);
	if (error != B_OK)
		return error;

	cpu_info cpuInfos[8];
	error = _kern_get_cpu_info(0, std::min(systemInfo.cpu_count, uint32(8)),
			cpuInfos);
	if (error != B_OK)
		return error;

	info->boot_time = systemInfo.boot_time;
	info->cpu_count = std::min(systemInfo.cpu_count, uint32(8));
	for (int32 i = 0; i < info->cpu_count; i++)
		info->cpu_infos[i].active_time = cpuInfos[i].active_time;

	info->platform_type = LEGACY_B_AT_CLONE_PLATFORM;
	info->cpu_type = LEGACY_B_CPU_X86;

	uint32 topologyNodeCount = 0;
	cpu_topology_node_info* topology = NULL;
	error = get_cpu_topology_info(NULL, &topologyNodeCount);
	if (error != B_OK)
		return B_OK;
	if (topologyNodeCount != 0) {
		topology = new(std::nothrow) cpu_topology_node_info[topologyNodeCount];
		if (topology == NULL)
			return B_NO_MEMORY;
	}
	error = get_cpu_topology_info(topology, &topologyNodeCount);
	if (error != B_OK) {
		delete[] topology;
		return error;
	}

	for (uint32 i = 0; i < topologyNodeCount; i++) {
		if (topology[i].type == B_TOPOLOGY_CORE) {
			info->cpu_clock_speed = topology[i].data.core.default_frequency;
			break;
		}
	}
	info->bus_clock_speed = info->cpu_clock_speed;
	delete[] topology;

	info->max_pages = std::min(systemInfo.max_pages, uint64(INT32_MAX));
	info->used_pages = std::min(systemInfo.used_pages, uint64(INT32_MAX));
	info->cached_pages = std::min(systemInfo.cached_pages, uint64(INT32_MAX));
	info->ignored_pages = std::min(systemInfo.ignored_pages, uint64(INT32_MAX));
	info->page_faults = std::min(systemInfo.page_faults, uint32(INT32_MAX));
	info->max_sems = std::min(systemInfo.max_sems, uint32(INT32_MAX));
	info->used_sems = std::min(systemInfo.used_sems, uint32(INT32_MAX));
	info->max_ports = std::min(systemInfo.max_ports, uint32(INT32_MAX));
	info->used_ports = std::min(systemInfo.used_ports, uint32(INT32_MAX));
	info->max_threads = std::min(systemInfo.max_threads, uint32(INT32_MAX));
	info->used_threads = std::min(systemInfo.used_threads, uint32(INT32_MAX));
	info->max_teams = std::min(systemInfo.max_teams, uint32(INT32_MAX));
	info->used_teams = std::min(systemInfo.used_teams, uint32(INT32_MAX));

	strlcpy(info->kernel_name, systemInfo.kernel_name,
		sizeof(info->kernel_name));
	strlcpy(info->kernel_build_date, systemInfo.kernel_build_date,
		sizeof(info->kernel_build_date));
	strlcpy(info->kernel_build_time, systemInfo.kernel_build_time,
		sizeof(info->kernel_build_time));
	info->kernel_version = systemInfo.kernel_version;

	info->abi = systemInfo.abi;

	return B_OK;
}


#endif	// __HAIKU_BEOS_COMPATIBLE


status_t
__get_system_info(system_info* info)
{
	if (info == NULL)
		return B_BAD_VALUE;

	memset(info, 0, sizeof(system_info));

	// Get Linux system info
	struct sysinfo si;
	if (sysinfo(&si) != 0)
		return B_ERROR;

	// Boot time - convert to microseconds since epoch
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		bigtime_t now = (bigtime_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
		bigtime_t uptime = (bigtime_t)si.uptime * 1000000LL;
		info->boot_time = now - uptime;
	}

	// CPU count
	long cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpuCount > 0)
		info->cpu_count = (uint32)cpuCount;
	else
		info->cpu_count = 1;

	// Memory information - convert from bytes to pages
	long pageSize = sysconf(_SC_PAGESIZE);
	if (pageSize <= 0)
		pageSize = 4096;

	info->max_pages = (uint64)si.totalram / pageSize;
	info->used_pages = (uint64)(si.totalram - si.freeram) / pageSize;
	info->cached_pages = (uint64)si.bufferram / pageSize;
	info->block_cache_pages = 0; // Not directly available on Linux
	info->ignored_pages = 0;

	info->needed_memory = 0;
	info->free_memory = (uint64)si.freeram;

	// Swap information
	info->max_swap_pages = (uint64)si.totalswap / pageSize;
	info->free_swap_pages = (uint64)si.freeswap / pageSize;

	// Page faults - read from /proc/self/stat if available
	FILE* fp = fopen("/proc/self/stat", "r");
	if (fp != NULL) {
		unsigned long minflt = 0, majflt = 0;
		// Skip to fields 10 and 12 (minor and major page faults)
		if (fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu",
				&minflt, &majflt) == 2) {
			info->page_faults = (uint32)(minflt + majflt);
		}
		fclose(fp);
	}

	// Not 1-to-1 with Linux semaphores, so set to something benign
	info->used_sems = 0;
	info->max_sems = 256;

	// Threads - approximate from /proc/sys/kernel/threads-max
	fp = fopen("/proc/sys/kernel/threads-max", "r");
	if (fp != NULL) {
		unsigned long threads_max;
		if (fscanf(fp, "%lu", &threads_max) == 1)
			info->max_threads = (uint32)threads_max;
		fclose(fp);
	}
	if (info->max_threads == 0)
		info->max_threads = 32768;

	// Count running threads from /proc
	info->used_threads = 0;
	DIR* procDir = opendir("/proc");
	if (procDir != NULL) {
		struct dirent* entry;
		while ((entry = readdir(procDir)) != NULL) {
			// Check if directory name is numeric (PID)
			if (entry->d_type == DT_DIR && entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
				char taskPath[512];
				snprintf(taskPath, sizeof(taskPath), "/proc/%s/task", entry->d_name);
				DIR* taskDir = opendir(taskPath);
				if (taskDir != NULL) {
					struct dirent* taskEntry;
					while ((taskEntry = readdir(taskDir)) != NULL) {
						if (taskEntry->d_name[0] >= '0' && taskEntry->d_name[0] <= '9')
							info->used_threads++;
					}
					closedir(taskDir);
				}
			}
		}
		closedir(procDir);
	}

	// Process limits - read from /proc/sys/kernel/pid_max
	fp = fopen("/proc/sys/kernel/pid_max", "r");
	if (fp != NULL) {
		unsigned long pid_max;
		if (fscanf(fp, "%lu", &pid_max) == 1) {
			info->max_teams = (uint32)pid_max;
			info->max_ports = (uint32)pid_max; // use same limit
		}
		fclose(fp);
	}
	if (info->max_teams == 0) {
		info->max_teams = 32768;
		info->max_ports = 32768;
	}

	// Count running processes
	info->used_teams = 0;
	procDir = opendir("/proc");
	if (procDir != NULL) {
		struct dirent* entry;
		while ((entry = readdir(procDir)) != NULL) {
			if (entry->d_type == DT_DIR && entry->d_name[0] >= '0' && entry->d_name[0] <= '9')
				info->used_teams++;
		}
		closedir(procDir);
	}

	// Ports - not 1-to-1 with Linux IPC, so set to something benign
	info->used_ports = 1;

	// Kernel information
	strlcpy(info->kernel_name, "Linux", sizeof(info->kernel_name));
	
	// Read kernel version from /proc/version
	fp = fopen("/proc/version", "r");
	if (fp != NULL) {
		char version[256];
		if (fgets(version, sizeof(version), fp) != NULL) {
			// Extract just the version number
			char* versionStart = strstr(version, "version ");
			if (versionStart != NULL) {
				versionStart += 8; // skip "version "
				char* versionEnd = strchr(versionStart, ' ');
				if (versionEnd != NULL) {
					size_t len = versionEnd - versionStart;
					if (len >= sizeof(info->kernel_build_date))
						len = sizeof(info->kernel_build_date) - 1;
					memcpy(info->kernel_build_date, versionStart, len);
					info->kernel_build_date[len] = '\0';
				}
			}
		}
		fclose(fp);
	}

	strlcpy(info->kernel_build_time, __TIME__, sizeof(info->kernel_build_time));
	info->kernel_version = 1; // simplified version number

	// ABI - use GCC 4 ABI for modern compilers
	info->abi = B_HAIKU_ABI_GCC_4;

	return B_OK;
}


status_t
__get_cpu_info(uint32 firstCPU, uint32 cpuCount, cpu_info* info)
{
	if (info == NULL || cpuCount == 0)
		return B_BAD_VALUE;

	long totalCPUs = sysconf(_SC_NPROCESSORS_ONLN);
	if (totalCPUs <= 0)
		return B_ERROR;

	if (firstCPU >= (uint32)totalCPUs)
		return B_BAD_VALUE;

	// Limit to available CPUs
	uint32 availCount = (uint32)totalCPUs - firstCPU;
	if (cpuCount > availCount)
		cpuCount = availCount;

	// Read CPU frequency and stats from /proc/cpuinfo and /proc/stat
	for (uint32 i = 0; i < cpuCount; i++) {
		info[i].active_time = 0;
		info[i].enabled = true;
		info[i].current_frequency = 0;

		// Try to read frequency from /sys/devices/system/cpu/cpuX/cpufreq/scaling_cur_freq
		char freqPath[256];
		snprintf(freqPath, sizeof(freqPath),
			"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", firstCPU + i);
		
		FILE* fp = fopen(freqPath, "r");
		if (fp != NULL) {
			unsigned long freq_khz;
			if (fscanf(fp, "%lu", &freq_khz) == 1) {
				info[i].current_frequency = freq_khz * 1000ULL; // Convert kHz to Hz
			}
			fclose(fp);
		}

		// If frequency not available, try cpuinfo_max_freq or read from /proc/cpuinfo
		if (info[i].current_frequency == 0) {
			snprintf(freqPath, sizeof(freqPath),
				"/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", firstCPU + i);
			fp = fopen(freqPath, "r");
			if (fp != NULL) {
				unsigned long freq_khz;
				if (fscanf(fp, "%lu", &freq_khz) == 1) {
					info[i].current_frequency = freq_khz * 1000ULL;
				}
				fclose(fp);
			}
		}
	}

	// Read active time from /proc/stat
	FILE* fp = fopen("/proc/stat", "r");
	if (fp != NULL) {
		char line[512];
		while (fgets(line, sizeof(line), fp) != NULL) {
			unsigned int cpu_num;
			unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
			
			if (sscanf(line, "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu",
					&cpu_num, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
				
				if (cpu_num >= firstCPU && cpu_num < firstCPU + cpuCount) {
					// Calculate active time (user + nice + system) in microseconds
					// Values from /proc/stat are in USER_HZ units (typically 100 Hz)
					long clk_tck = sysconf(_SC_CLK_TCK);
					if (clk_tck <= 0)
						clk_tck = 100;
					
					unsigned long long active_ticks = user + nice + system;
					info[cpu_num - firstCPU].active_time = 
						(bigtime_t)((active_ticks * 1000000ULL) / clk_tck);
				}
			}
		}
		fclose(fp);
	}

	return B_OK;
}


status_t
__get_cpu_topology_info(cpu_topology_node_info* topologyInfos,
	uint32* topologyInfoCount)
{
	return B_ERROR;
}


status_t
__start_watching_system(int32 object, uint32 flags, port_id port, int32 token)
{
	return B_ERROR;
}


status_t
__stop_watching_system(int32 object, uint32 flags, port_id port, int32 token)
{
	return B_ERROR;
}


int32
is_computer_on(void)
{
	return true;
}


double
is_computer_on_fire(void)
{
	return 0.63739;
}


B_DEFINE_WEAK_ALIAS(__get_system_info, get_system_info);
B_DEFINE_WEAK_ALIAS(__get_cpu_info, get_cpu_info);
B_DEFINE_WEAK_ALIAS(__get_cpu_topology_info, get_cpu_topology_info);


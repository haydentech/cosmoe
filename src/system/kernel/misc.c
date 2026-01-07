//------------------------------------------------------------------------------
//	Copyright (c) 2003 Tom Marshall, 2003-2025 Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		misc.c
//	Authors:		Tom Marshall (tommy@tig-grr.com)
//	Authors:		Bill Hayden (hayden@haydentech.com)
//------------------------------------------------------------------------------


#include <sys/types.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

// On macOS, avoid thread_info collision with mach headers
#ifdef __APPLE__
#define COSMOE_NO_THREAD_INFO
#endif

#include <Debug.h>
#include <SupportDefs.h>
#include <OS.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(__APPLE__)
// macOS-specific headers for system information
#include <sys/sysctl.h>        // For sysctlbyname() and sysctl()
#include <mach/mach.h>          // For Mach kernel interface
#include <mach/mach_host.h>     // For host_processor_info(), host_statistics64()
#include <mach/mach_host.h>     // For processor_cpu_load_info_t and related types
#else
#warning System information not available on this platform
#warning system_time() will always return 0 on this platform
#endif

size_t	cosmoe_strlcpy(char *dst, const char *src, size_t dstsize)
{
	if (!dst || !src)
		return (0);

	size_t srcsize = strlen(src);
	size_t i = 0;

	if (dstsize != 0)
	{
		while (src[i] != '\0' && i < (dstsize - 1))
		{
			dst[i] = src[i];
			i++;
		}
		dst[i] = '\0';
	}
	return (srcsize);
}


size_t	cosmoe_strlcat(char *dst, const char *src, size_t dstsize)
{
	if (dstsize <= strlen(dst))
		return (dstsize + strlen(src));

	size_t c = strlen(dst);
	size_t d = 0;
	
	while (src[d] != '\0' && c + 1 < dstsize)
	{
		dst[c] = src[d];
		c++;
		d++;
	}
	dst[c] = '\0';
	return (strlen(dst) + strlen(&src[d]));
}


#ifdef _WIN32
#include <windows.h>

ssize_t cosmoe_readlink(const char *path, char *buf, size_t bufsiz)
{
	HANDLE hFile;
	DWORD attr;
	DWORD len;
	
	// Check if path exists and is a symlink
	attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		errno = ENOENT;
		return -1;
	}
	
	if (!(attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
		errno = EINVAL;
		return -1;
	}
	
	// Open the symlink without following it
	hFile = CreateFileA(path, 
	                    GENERIC_READ,
	                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                    NULL,
	                    OPEN_EXISTING,
	                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
	                    NULL);
	
	if (hFile == INVALID_HANDLE_VALUE) {
		errno = EACCES;
		return -1;
	}
	
	// Use GetFinalPathNameByHandle to get the target
	char tempBuf[MAX_PATH * 2];
	len = GetFinalPathNameByHandleA(hFile, tempBuf, sizeof(tempBuf), FILE_NAME_NORMALIZED);
	CloseHandle(hFile);
	
	if (len == 0 || len >= sizeof(tempBuf)) {
		errno = EIO;
		return -1;
	}
	
	// Remove the "\\?\" or "\\?\UNC\" prefix if present
	const char *result = tempBuf;
	if (len > 4 && strncmp(tempBuf, "\\\\?\\", 4) == 0) {
		if (len > 8 && strncmp(tempBuf + 4, "UNC\\", 4) == 0) {
			// \\?\UNC\server\share -> \\server\share
			result = tempBuf + 6;  // Skip "\\?\UN", keep "C\"
			*(char*)result = '\\';  // Make it "\\"
			len -= 6;
		} else {
			// \\?\C:\path -> C:\path
			result = tempBuf + 4;
			len -= 4;
		}
	}
	
	// Copy to output buffer
	size_t copyLen = (len < bufsiz) ? len : bufsiz;
	memcpy(buf, result, copyLen);
	
	// POSIX readlink does NOT null-terminate
	return copyLen;
}
#endif


/* helper for get_system_info */

status_t get_cpu_topology_info(cpu_topology_node_info* topologyInfos,
						uint32* topologyInfoCount)
{
	*topologyInfoCount = 3;

	if (topologyInfos == NULL)
		return B_ERROR;

	topologyInfos[0].type = B_TOPOLOGY_ROOT;

	#if defined(__x86_64__) || defined(_M_X64)
	topologyInfos[0].data.root.platform = B_CPU_x86_64;
	#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
	topologyInfos[0].data.root.platform = B_CPU_x86;
	#elif defined(__aarch64__) || defined(_M_ARM64)
	topologyInfos[0].data.root.platform = B_CPU_ARM_64;
	#elif defined(mips) || defined(__mips__) || defined(__mips)
	topologyInfos[0].data.root.platform = B_CPU_MIPS;
	#elif defined(__sh__)
	topologyInfos[0].data.root.platform = B_CPU_SH;
	#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
	topologyInfos[0].data.root.platform = B_CPU_PPC;
	#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
	topologyInfos[0].data.root.platform = B_CPU_PPC_64;
	#elif defined(__sparc__) || defined(__sparc)
	topologyInfos[0].data.root.platform = B_CPU_SPARC;
	#elif defined(__m68k__)
	topologyInfos[0].data.root.platform = B_CPU_M68K,
	#else
	topologyInfos[0].data.root.platform = B_CPU_UNKNOWN;
	#endif

	topologyInfos[1].type = B_TOPOLOGY_PACKAGE;
	
	#if defined(__x86_64__) || defined(_M_X64)
	topologyInfos[1].data.package.vendor = B_CPU_VENDOR_INTEL;
	#elif defined(__aarch64__) || defined(_M_ARM64)
	topologyInfos[1].data.package.vendor = B_CPU_VENDOR_ARM;
	#endif

	topologyInfos[2].type = B_TOPOLOGY_CORE;

#ifdef __linux__
	// Linux-specific: Read CPU information from /proc/cpuinfo
	FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo != NULL)
	{
		char line[256];
		float speed;
		int model;

		while (fgets(line, sizeof(line), cpuinfo))
		{
			if (sscanf(line, "cpu MHz		: %f", &speed) == 1)
			{
				topologyInfos[2].data.core.default_frequency = (uint64)(speed * 1000000.0);
			}

			if (sscanf(line, "model		: %d", &model) == 1)
			{
				topologyInfos[2].data.core.model = model;
			}
		}

		fclose(cpuinfo);
	}
#elif __APPLE__
	// macOS alternative: Use sysctl to get CPU information
	uint64_t cpu_freq = 0;
	size_t size_freq = sizeof(cpu_freq);
	
	// Get CPU frequency
	if (sysctlbyname("hw.cpufrequency", &cpu_freq, &size_freq, NULL, 0) == 0) {
		topologyInfos[2].data.core.default_frequency = cpu_freq;
	}
	
	// Get CPU model number
	int cpu_model = 0;
	size_t size_model = sizeof(cpu_model);
	if (sysctlbyname("machdep.cpu.model", &cpu_model, &size_model, NULL, 0) == 0) {
		topologyInfos[2].data.core.model = cpu_model;
	}
	
	// Get CPU brand string (for informational purposes, though not stored in topology)
	char brand_string[256];
	size_t size_brand = sizeof(brand_string);
	if (sysctlbyname("machdep.cpu.brand_string", brand_string, &size_brand, NULL, 0) == 0) {
		// Brand string retrieved successfully (could be used for logging/debugging)
		(void)brand_string; // Suppress unused variable warning
	}
#endif

	return B_OK;
}


status_t _get_cpu_info_etc(uint32 firstCPU, uint32 cpuCount, cpu_info* info, size_t size)
{
	if (info == NULL)
		return B_ERROR;

	if (size != sizeof(cpu_info))
		return B_ERROR;

#if defined(__linux__)
	/* Populate per-CPU info: frequency and active_time.
	 * Frequency: try sysfs (/sys/devices/system/cpu/cpuN/cpufreq/scaling_cur_freq),
	 *            fall back to /proc/cpuinfo parsing.
	 * active_time: read /proc/stat cpuN line and compute jiffies not-idle, convert to microseconds.
	 */
	FILE* fp;
	char line[512];
	long clk_tck = sysconf(_SC_CLK_TCK);
	if (clk_tck <= 0) clk_tck = 100;

	/* Initialize results */
	for (uint32 i = 0; i < cpuCount; i++) {
		info[i].active_time = 0;
		info[i].enabled = false;
		info[i].current_frequency = 0;
	}

	/* Try reading frequency per CPU from sysfs */
	for (uint32 i = 0; i < cpuCount; i++) {
		char freqPath[PATH_MAX];
		snprintf(freqPath, sizeof(freqPath), "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", firstCPU + i);
		fp = fopen(freqPath, "r");
		if (fp != NULL) {
			unsigned long freq_khz = 0;
			if (fscanf(fp, "%lu", &freq_khz) == 1) {
				info[i].current_frequency = (uint64)freq_khz * 1000ULL; /* kHz -> Hz */
				info[i].enabled = true;
			}
			fclose(fp);
		}
	}

	/* If any frequency is still zero, parse /proc/cpuinfo and map cpu MHz to each processor */
	uint32 cpuid = 0;
	if ((fp = fopen("/proc/cpuinfo", "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			/* processor line */
			if (strncmp(line, "processor", 9) == 0) {
				char *p = strchr(line, ':');
				if (p) {
					unsigned int cpu_index = (unsigned int)atoi(p+1);
					cpuid = cpu_index;
				}
			} else if (strncmp(line, "cpu MHz", 7) == 0) {
				char *p = strchr(line, ':');
				if (p) {
					double mhz = atof(p+1);
					if (cpuid >= firstCPU && cpuid < firstCPU + cpuCount) {
						uint32 idx = cpuid - firstCPU;
						if (info[idx].current_frequency == 0) {
							info[idx].current_frequency = (uint64)(mhz * 1000000.0); /* MHz to Hz */
							info[idx].enabled = true;
						}
					}
				}
			}
		}
		fclose(fp);
	}

	/* Parse /proc/stat for per-CPU active_time (non-idle) */
	if ((fp = fopen("/proc/stat", "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			unsigned cpu_index;
			unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;

			/* Parse lines beginning with 'cpu' followed by a number */
			if (sscanf(line, "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
					&cpu_index, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice) >= 5) {
				if (cpu_index >= firstCPU && cpu_index < firstCPU + cpuCount) {
					uint32 idx = cpu_index - firstCPU;
					/* Compute active jiffies as all ticks minus idle and iowait */
					unsigned long long all = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
					unsigned long long idle_all = idle + iowait;
					unsigned long long active_ticks = (all > idle_all) ? (all - idle_all) : 0ULL;
					/* Convert ticks to microseconds */
					info[idx].active_time = (bigtime_t)((active_ticks * 1000000ULL) / (unsigned long long)clk_tck);
					info[idx].enabled = true;
				}
			}
		}
		fclose(fp);
	}

#elif __APPLE__
	/* macOS alternative: Use host_processor_info() to get CPU information */
	
	/* Initialize results with defaults */
	for (uint32 i = 0; i < cpuCount; i++) {
		info[i].active_time = 0;
		info[i].enabled = false;
		info[i].current_frequency = 0;
	}
	
	// Get CPU frequency using sysctl
	uint64_t cpu_freq = 0;
	size_t freq_size = sizeof(cpu_freq);
	if (sysctlbyname("hw.cpufrequency", &cpu_freq, &freq_size, NULL, 0) == 0) {
		// Apply the same frequency to all requested CPUs
		for (uint32 i = 0; i < cpuCount; i++) {
			info[i].current_frequency = cpu_freq;
		}
	}
	
	// Get CPU usage information using host_processor_info
	mach_port_t host_port = mach_host_self();
	processor_cpu_load_info_t cpu_load_info;
	mach_msg_type_number_t cpu_load_info_count;
	natural_t processor_count;
	
	kern_return_t kr = host_processor_info(host_port, 
	                                       PROCESSOR_CPU_LOAD_INFO,
	                                       &processor_count,
	                                       (processor_info_array_t *)&cpu_load_info,
	                                       &cpu_load_info_count);
	
	if (kr == KERN_SUCCESS) {
		// Calculate active time for each CPU
		for (uint32 i = 0; i < cpuCount && (firstCPU + i) < processor_count; i++) {
			uint32 cpu_idx = firstCPU + i;
			
			// Sum up all tick types
			unsigned long long user_ticks = cpu_load_info[cpu_idx].cpu_ticks[CPU_STATE_USER];
			unsigned long long system_ticks = cpu_load_info[cpu_idx].cpu_ticks[CPU_STATE_SYSTEM];
			unsigned long long idle_ticks = cpu_load_info[cpu_idx].cpu_ticks[CPU_STATE_IDLE];
			unsigned long long nice_ticks = cpu_load_info[cpu_idx].cpu_ticks[CPU_STATE_NICE];
			
			// Active time is everything except idle
			unsigned long long active_ticks = user_ticks + system_ticks + nice_ticks;
			
			// Convert ticks to microseconds (Mach uses 100 ticks per second)
			info[i].active_time = (bigtime_t)((active_ticks * 1000000ULL) / 100ULL);
			info[i].enabled = true;
		}
		
		// Deallocate the memory allocated by host_processor_info
		vm_deallocate(mach_task_self(),
		              (vm_address_t)cpu_load_info,
		              (vm_size_t)(cpu_load_info_count * sizeof(*cpu_load_info)));
	}
#endif

	info->enabled = true;
	return B_OK;
}


#if defined(__i386__) || defined(__x86_64__)
status_t
get_cpuid(cpuid_info *info, uint32 eaxRegister, uint32 cpuNum)
{
	return B_ERROR;
}
#endif

status_t get_system_info(system_info* psInfo)
{
	FILE* fp;

	psInfo->boot_time = real_time_clock_usecs() - system_time();

	// Number of processors
	uint32 ncpu = 0;
	char buffer[80];

#ifdef __linux__
	// Linux-specific: Count processors from /proc/cpuinfo
	if ((fp = fopen( "/proc/cpuinfo", "r" )) != NULL)
	{
		while(fgets( buffer, sizeof(buffer), fp) != NULL)
		{
			if (strncmp(buffer, "processor\t", 10) == 0)
				ncpu++;
		}
		fclose( fp );
	} else {
		ncpu = 1;
	}
#elif defined(__APPLE__)
	/* macOS alternative: Use sysctl to get processor count */
	int mib[2];
	size_t len = sizeof(ncpu);

	/* Try hw.ncpu first (number of available CPUs) */
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &ncpu, &len, NULL, 0) != 0) {
		/* Fallback: try using sysctlbyname */
		if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) != 0) {
			/* Last resort: get physical CPU count */
			if (sysctlbyname("hw.physicalcpu", &ncpu, &len, NULL, 0) != 0) {
				ncpu = 1; /* Ultimate fallback */
			}
		}
	}
#else
	/* Unknown platform: default to 1 CPU */
	ncpu = 1;
#endif

	psInfo->cpu_count = ncpu;

#if defined(__linux__) || defined(__APPLE__)
	struct utsname unamebuffer;

	// Kernel version
	if (uname(&unamebuffer) == 0)
	{
		#if defined(__linux__)
		strcpy(psInfo->kernel_name, "Linux ");
		strcat(psInfo->kernel_name, unamebuffer.sysname);
		#elif defined(__APPLE__)
		/* macOS reports "Darwin" as sysname */
		strcpy(psInfo->kernel_name, "Darwin");
		#endif
		strcpy(psInfo->kernel_build_date, unamebuffer.release);
		strcpy(psInfo->kernel_build_time, "unknown");
        
		#if defined(__APPLE__)
		/* On macOS, version string is like "Darwin Kernel Version 21.6.0: ..." */
		const char* version_str = strstr(unamebuffer.version, "Version ");
		if (version_str != NULL) {
			version_str += 8;  /* Skip "Version " */
			psInfo->kernel_version = atoi(version_str);
		} else {
			psInfo->kernel_version = 0LL;
		}
		#else
		psInfo->kernel_version = atoi(unamebuffer.version);
		#endif
	}
	else
	{
		#if defined(__linux__)
		strcpy(psInfo->kernel_name, "Linux");
		#elif defined(__APPLE__)
		strcpy(psInfo->kernel_name, "Darwin");
		#endif
		strcpy(psInfo->kernel_build_date, "unknown");
		strcpy(psInfo->kernel_build_time, "unknown");
		psInfo->kernel_version = 0LL;
	}
#else
	/* Unknown platform: provide generic information */
	strcpy(psInfo->kernel_name, "Windows");
	strcpy(psInfo->kernel_build_date, "unknown");
	strcpy(psInfo->kernel_build_time, "unknown");
	psInfo->kernel_version = 0LL;
#endif

	// Memory
#ifdef __linux__
	struct sysinfo sinfo;

	if (sysinfo(&sinfo) == 0)
	{
		psInfo->max_pages = sinfo.totalram / B_PAGE_SIZE;
		psInfo->ignored_pages = 100;
		psInfo->used_pages = (sinfo.totalram - sinfo.freeram) / B_PAGE_SIZE;
	}
#elif defined(__APPLE__)
	// macOS alternative: Use sysctl for memory information
	int mib2[2];
	int64_t physical_memory = 0;
	size_t length = sizeof(physical_memory);
	
	// Get total physical memory
	mib2[0] = CTL_HW;
	mib2[1] = HW_MEMSIZE;
	if (sysctl(mib2, 2, &physical_memory, &length, NULL, 0) == 0) {
		psInfo->max_pages = physical_memory / B_PAGE_SIZE;
		psInfo->ignored_pages = 100;
		
		// Get free memory using host_statistics
		mach_port_t host_port = mach_host_self();
		vm_statistics64_data_t vm_stat;
		mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(integer_t);
		
		if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
			// Calculate used pages: total - (free + inactive)
			unsigned long long free_bytes = (unsigned long long)(vm_stat.free_count + vm_stat.inactive_count) * B_PAGE_SIZE;
			psInfo->used_pages = (physical_memory - free_bytes) / B_PAGE_SIZE;
		} else {
			// Fallback: assume 50% memory usage
			psInfo->used_pages = psInfo->max_pages / 2;
		}
	} else {
		// Ultimate fallback values
		psInfo->max_pages = 1024 * 1024;  // Assume 4GB
		psInfo->ignored_pages = 100;
		psInfo->used_pages = 512 * 1024;   // Assume 2GB used
	}
#endif

	// Ports
	psInfo->max_ports = port_max_ports();
	psInfo->used_ports = port_used_ports();

	return 0;
}


void debugger(const char *message)
{
	printf("BUG: %s\n", message);
}


void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}


status_t
set_timezone(const char *timezone)
{
	/* FIXME */
	return B_ERROR;
}


bigtime_t
system_time(void)
{
	struct timespec ts;
	
	// Use CLOCK_MONOTONIC for high-resolution system uptime
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
	{
		return (bigtime_t)ts.tv_sec * 1000000LL + (bigtime_t)(ts.tv_nsec / 1000);
	}
	
	return 0;
}


#include <ByteOrder.h>

#if __GNUC__ < 4

uint16
__swap_int16(uint16 value)
{
	return (value >> 8) | (value << 8);
}

uint32
__swap_int32(uint32 value)
{
	return (value >> 24) | ((value & 0xff0000) >> 8) | ((value & 0xff00) << 8)
		| (value << 24);
}

uint64
__swap_int64(uint64 value)
{
	return (uint64)(__swap_int32((uint32)(value >> 32)))
		| ((uint64)(__swap_int32((uint32)(value))) << 32);
}
#endif


float
__swap_float(float value)
{
   float retVal;
   char *floatToConvert = (char*)&value;
   char *returnFloat = (char*)&retVal;

   // swap the bytes into a temporary buffer
   returnFloat[0] = floatToConvert[3];
   returnFloat[1] = floatToConvert[2];
   returnFloat[2] = floatToConvert[1];
   returnFloat[3] = floatToConvert[0];

   return retVal;
}

double
__swap_double(double value)
{
   double retVal;
   char *doubleToConvert = (char*)&value;
   char *returnDouble = (char*)&retVal;

   // swap the bytes into a temporary buffer
   returnDouble[0] = doubleToConvert[7];
   returnDouble[1] = doubleToConvert[6];
   returnDouble[2] = doubleToConvert[5];
   returnDouble[3] = doubleToConvert[4];
   returnDouble[4] = doubleToConvert[3];
   returnDouble[5] = doubleToConvert[2];
   returnDouble[6] = doubleToConvert[1];
   returnDouble[7] = doubleToConvert[0];

   return retVal;
}

int
fs_stat_index(dev_t device, const char *name, struct index_info *indexInfo)
{
	//FIXME
	return B_ERROR;
}


int __libc_argc;
char** __libc_argv;

extern char **environ;
void save_arg(int argc, char **argv, char **env)
{
	__libc_argc = argc;
	__libc_argv = argv;
}
#ifdef __linux__
__attribute__((section(".init_array"))) static void *foo_constructor = &save_arg;
#endif

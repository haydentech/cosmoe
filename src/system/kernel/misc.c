//------------------------------------------------------------------------------
//	Copyright (c) 2003, Tom Marshall
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
//	File Name:		misc.cpp
//	Authors:		Tom Marshall (tommy@tig-grr.com)
//------------------------------------------------------------------------------


#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <errno.h>
#include <unistd.h>

#include <Debug.h>
#include <SupportDefs.h>
#include <OS.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(linux)
#include <sys/sysinfo.h>
#else
#warning System information not available on this platform
#warning system_time() will always return 0 on this platform
#endif


/* helper for get_system_info */
status_t get_cpu_info(uint32 firstCPU, uint32 cpuCount, cpu_info* psInfo)
{
#if defined(linux)  && false
	FILE*         fp;
	int           ncpu;
	char          buf[80];
	char*         p;
	bigtime_t     systime;
	bigtime_t     idletime;
	unsigned long n1, n2, n3, nidle;

	systime = system_time();
	psInfo->boot_time = real_time_clock_usecs() - systime;
	ncpu = 0;
	if( (fp = fopen( "/proc/cpuinfo", "r" )) != NULL )
	{
		while( fgets( buf, sizeof(buf), fp ) != NULL )
		{
			if ( strncmp( buf, "processor\t", 10 ) == 0 )
			{
				ncpu++;
			}

			if (strncmp( buf, "cpu MHz\t", 8 ) == 0)
			{
				p = strchr( buf, ':' );
				if( p != NULL )
				{
					psInfo->cpu_clock_speed = atoi( p+2 );
				}
			}
		}
		fclose( fp );
	}
	psInfo->cpu_count = ncpu;

	if( (fp = fopen( "/proc/stat", "r" )) != NULL )
	{
		while( fgets( buf, sizeof(buf), fp ) != NULL )
		{
			if( ncpu == 1 && strncmp( buf, "cpu ", 4 ) == 0 )
			{
				/* there are no cpuN lines, use the overall stat */
				sscanf( buf+4, "%lu %lu %lu %lu", &n1, &n2, &n3, &nidle );
				idletime = (bigtime_t)nidle * 10000LL;
				psInfo->cpu_infos[0].active_time = systime - idletime;
				break;
			}

			if( strncmp( buf, "cpu", 3 ) == 0 )
			{
				sscanf( buf+3, "%d %lu %lu %lu %lu", &ncpu, &n1, &n2, &n3, &nidle );
				if( ncpu < psInfo->cpu_count )
				{
					idletime = (bigtime_t)nidle * 10000LL;
					psInfo->cpu_infos[ncpu].active_time = systime - idletime;
				}
			}
		}
		fclose( fp );
	}
#endif
}

status_t		get_cpu_topology_info(cpu_topology_node_info* topologyInfos,
						uint32* topologyInfoCount)
{
	return B_ERROR;
}

#if defined(__i386__) || defined(__x86_64__)
get_cpuid(cpuid_info *info, uint32 eaxRegister,
						uint32 cpuNum)
{
	return B_ERROR;
}
#endif

/* helper for get_system_info */
static void get_mem_info( system_info* psInfo )
{
}


/* helper for get_system_info */
static void get_fs_info( system_info* psInfo )
{
}


status_t get_system_info(system_info* psInfo)
{
	struct utsname unamebuffer;

	if (uname(&unamebuffer) == 0)
	{
		strcpy( psInfo->kernel_name, unamebuffer.sysname );
		strcpy( psInfo->kernel_build_date, unamebuffer.release );
		strcpy( psInfo->kernel_build_time, "unknown" );
	}
	else
	{
		strcpy( psInfo->kernel_name, "unknown" );
		strcpy( psInfo->kernel_build_date, "unknown" );
		strcpy( psInfo->kernel_build_time, "unknown" );
	}
	psInfo->kernel_version = 2LL;
	cpu_info cpuInfo;
	get_cpu_info(1, 1, &cpuInfo); /* set boot time and cpu info */
	get_mem_info( psInfo ); /* set various mem info */
	get_fs_info( psInfo );  /* set various fs info */

	return 0;
}


void	debugger(const char *message)
{
	printf("BUG: %s\n", message);
}


void	debug_printf(const char *format, ...)
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
#if defined(linux)
	struct sysinfo sinfo;

	if (sysinfo(&sinfo) == 0)
	{
		return (bigtime_t) sinfo.uptime * 1000000;
	}
#endif
	return 0;
}



#include <ByteOrder.h>

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

float
__swap_float(float value)
{
	//FIXME
	return value;
}

int
fs_stat_index(dev_t device, const char *name, struct index_info *indexInfo)
{
	//FIXME
	return B_ERROR;
}

status_t		snooze_until(bigtime_t time, int timeBase)
{
	//FIXME
	return B_OK;
}


int __libc_argc;
char** __libc_argv;

extern char **environ;
void save_arg(int argc, char **argv, char **env)
{
	__libc_argc = argc;
	__libc_argv = argv;
}
__attribute__((section(".init_array"))) static void *foo_constructor = &save_arg;

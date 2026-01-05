/*
 * Copyright 2002-2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


/*!
	\file ResourceFile.cpp
	ResourceFile implementation.
*/


#include <ResourceFile.h>

#include <algorithm>
#include <new>
#include <stdio.h>
#include <string.h>

#include <AutoDeleter.h>
#include <BufferIO.h>
#include <Elf.h>
#include <Entry.h>
#include <Exception.h>
#include <Path.h>
#include <Pef.h>
#include <ResourceItem.h>
#include <ResourcesContainer.h>
#include <ResourcesDefs.h>
//#include <Warnings.h>

// Platform-specific headers for extended attributes
#if defined(__APPLE__) || defined(__linux__)
	#include <sys/xattr.h>
	#include <errno.h>
	#include <fcntl.h>
	#include <limits.h>  // For PATH_MAX
	#include <unistd.h>  // For close, readlink
#elif defined(_WIN32)
	#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#endif


namespace BPrivate {
namespace Storage {


// Extended attribute name for resources
static const char* kResourceXAttrName = "user.cosmoe.resources";

// sanity bounds
static const uint32	kMaxResourceCount			= 10000;


// debugging
//#define DBG(x) x
#define DBG(x)
#define OUT	printf

#define B_VERSION_INFO_TYPE 'APPV'

static const uint32 kVersionInfoIntCount = 5;

// #pragma mark - helper functions/classes


static void
read_exactly(BPositionIO& file, off_t position, void* buffer, size_t size,
	const char* errorMessage = NULL)
{
	ssize_t read = file.ReadAt(position, buffer, size);
	if (read < 0)
		throw Exception(read, errorMessage);
	else if ((size_t)read != size) {
		if (errorMessage) {
			throw Exception("%s Read too few bytes (%ld/%lu).", errorMessage,
							read, size);
		} else
			throw Exception("Read too few bytes (%ld/%lu).", read, size);
	}
}


static void
read_from_buffer(const char* buffer, size_t bufferSize, off_t position, 
	void* dest, size_t size, const char* errorMessage = NULL)
{
	if (position < 0 || (size_t)position + size > bufferSize) {
		if (errorMessage)
			throw Exception(B_IO_ERROR, "%s Read out of bounds.", errorMessage);
		else
			throw Exception(B_IO_ERROR, "Read out of bounds.");
	}
	memcpy(dest, buffer + position, size);
}


template<typename TV, typename TA>
static inline TV
align_value(const TV& value, const TA& alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
}


static uint32
calculate_checksum(const void* data, uint32 size)
{
	uint32 checkSum = 0;
	const uint8* csData = (const uint8*)data;
	const uint8* dataEnd = csData + size;
	const uint8* current = csData;
	for (; current < dataEnd; current += 4) {
		uint32 word = 0;
		int32 bytes = std::min((int32)4, (int32)(dataEnd - current));
		for (int32 i = 0; i < bytes; i++)
			word = (word << 8) + current[i];
		checkSum += word;
	}
	return checkSum;
}


static inline const void*
skip_bytes(const void* buffer, int32 offset)
{
	return (const char*)buffer + offset;
}


static inline void*
skip_bytes(void* buffer, int32 offset)
{
	return (char*)buffer + offset;
}


static void
fill_pattern(uint32 byteOffset, void* _buffer, uint32 count)
{
	uint32* buffer = (uint32*)_buffer;
	for (uint32 i = 0; i < count; i++)
		buffer[i] = kUnusedResourceDataPattern[(byteOffset / 4 + i) % 3];
}


static void
fill_pattern(const void* dataBegin, void* buffer, uint32 count)
{
	fill_pattern((char*)buffer - (const char*)dataBegin, buffer, count);
}


static void
fill_pattern(const void* dataBegin, void* buffer, const void* bufferEnd)
{
	fill_pattern(dataBegin, buffer,
				 ((const char*)bufferEnd - (char*)buffer) / 4);
}


static bool
check_pattern(uint32 byteOffset, void* _buffer, uint32 count,
	bool hostEndianess)
{
	bool result = true;
	uint32* buffer = (uint32*)_buffer;
	for (uint32 i = 0; result && i < count; i++) {
		uint32 value = buffer[i];
		if (!hostEndianess)
			value = B_SWAP_INT32(value);
		result
			= (value == kUnusedResourceDataPattern[(byteOffset / 4 + i) % 3]);
	}
	return result;
}


// #pragma mark -


struct MemArea {
	MemArea(const void* data, uint32 size) : data(data), size(size) {}

	inline bool check(const void* _current, uint32 skip = 0) const
	{
		const char* start = (const char*)data;
		const char* current = (const char*)_current;
		return (start <= current && start + size >= current + skip);
	}

	const void*	data;
	uint32		size;
};


struct resource_parse_info {
	off_t				file_size;
	int32				resource_count;
	ResourcesContainer*	container;
	char*				info_table;
	uint32				info_table_offset;
	uint32				info_table_size;
};


// #pragma mark -


ResourceFile::ResourceFile()
	:
	fFile(),
	fFilePath(NULL),
	fHostEndianess(true),
	fEmptyResources(true),
	fXAttrResourceData(NULL),
	fXAttrResourceSize(0)
{
}


ResourceFile::~ResourceFile()
{
	Unset();
}


status_t
ResourceFile::SetTo(BFile* file, bool clobber)
{
	return SetTo(file, NULL, clobber);
}


status_t
ResourceFile::SetTo(BFile* file, const char* path, bool clobber)
{
	status_t error = (file ? B_OK : B_BAD_VALUE);
	Unset();
	if (error == B_OK) {
		// Store the path if provided
		if (path) {
			fFilePath = strdup(path);
			if (!fFilePath)
				return B_NO_MEMORY;
		} else {
			// No path provided - try to get it from the file descriptor
			char pathBuffer[PATH_MAX];
#if defined(__APPLE__)
			int fd = file->Dup();
			if (fcntl(fd, F_GETPATH, pathBuffer) == 0) {
				fFilePath = strdup(pathBuffer);
				if (!fFilePath) {
					close(fd);
					return B_NO_MEMORY;
				}
			}
			close(fd);
#elif defined(__linux__)
			int fd = file->Dup();
			char procPath[64];
			snprintf(procPath, sizeof(procPath), "/proc/self/fd/%d", fd);
			ssize_t len = readlink(procPath, pathBuffer, sizeof(pathBuffer) - 1);
			close(fd);
			if (len != -1) {
				pathBuffer[len] = '\0';
				fFilePath = strdup(pathBuffer);
				if (!fFilePath)
					return B_NO_MEMORY;
			}
#endif
		}
		try {
			_InitFile(*file, clobber);
		} catch (Exception& exception) {
			Unset();
			if (exception.Error() != B_OK)
				error = exception.Error();
			else
				error = B_ERROR;
		}
	}
	return error;
}


void
ResourceFile::Unset()
{
	fFile.Unset();
	fHostEndianess = true;
	fEmptyResources = true;
	free(fFilePath);
	fFilePath = NULL;
	delete[] fXAttrResourceData;
	fXAttrResourceData = NULL;
	fXAttrResourceSize = 0;
}


void
ResourceFile::_ReadExactly(off_t position, void* buffer, size_t size, 
	const char* errorMessage)
{
	if (fXAttrResourceData) {
		// Read from xattr buffer
		read_from_buffer(fXAttrResourceData, fXAttrResourceSize, position, 
			buffer, size, errorMessage);
	} else {
		// Read from file
		read_exactly(fFile, position, buffer, size, errorMessage);
	}
}


status_t
ResourceFile::InitCheck() const
{
	return fFile.InitCheck();
}


status_t
ResourceFile::InitContainer(ResourcesContainer& container)
{
	container.MakeEmpty();
	status_t error = InitCheck();
	if (error == B_OK && !fEmptyResources) {
		resource_parse_info parseInfo;
		parseInfo.file_size = 0;
		parseInfo.resource_count = 0;
		parseInfo.container = &container;
		parseInfo.info_table = NULL;
		parseInfo.info_table_offset = 0;
		parseInfo.info_table_size = 0;
		try {
			// get the file size
			if (fXAttrResourceData) {
				// Reading from xattr, use xattr size not file size
				parseInfo.file_size = fXAttrResourceSize;
			} else {
				error = fFile.GetSize(&parseInfo.file_size);
				if (error != B_OK)
					throw Exception(error, "Failed to get the file size.");
			}
			_ReadHeader(parseInfo);
			_ReadIndex(parseInfo);
			_ReadInfoTable(parseInfo);
			container.SetModified(false);
		} catch (Exception& exception) {
			if (exception.Error() != B_OK)
				error = exception.Error();
			else
				error = B_ERROR;
		}
		delete[] parseInfo.info_table;
	}
	return error;
}


status_t
ResourceFile::ReadResource(ResourceItem& resource, bool force)
{
	status_t error = InitCheck();
	size_t size = resource.DataSize();
	if (error == B_OK && (force || !resource.IsLoaded())) {
		void* data = NULL;
		error = resource.SetSize(size);

		if (error == B_OK) {
			data = resource.Data();
			// Check if resources are stored in extended attributes
			if (fXAttrResourceData) {
				// Reading from xattr buffer
				off_t offset = resource.Offset();
				if (offset + size <= (off_t)fXAttrResourceSize) {
					memcpy(data, fXAttrResourceData + offset, size);
				} else {
					error = B_IO_ERROR;
				}
			} else {
				// Reading from file
				ssize_t bytesRead = fFile.ReadAt(resource.Offset(), data, size);
				if (bytesRead < 0)
					error = bytesRead;
				else if ((size_t)bytesRead != size)
					error = B_IO_ERROR;
			}
		}
		if (error == B_OK) {
			// convert the data, if necessary
			if (!fHostEndianess) {
				if (resource.Type() == B_VERSION_INFO_TYPE) {
					// Version info contains integers that need to be swapped
					swap_data(B_UINT32_TYPE, data,
						kVersionInfoIntCount * sizeof(uint32),
						B_SWAP_ALWAYS);
				} else
					swap_data(resource.Type(), data, size, B_SWAP_ALWAYS);
			}
			resource.SetLoaded(true);
			resource.SetModified(false);
		}
	}
	return error;
}


status_t
ResourceFile::ReadResources(ResourcesContainer& container, bool force)
{
	status_t error = InitCheck();
	int32 count = container.CountResources();
	for (int32 i = 0; error == B_OK && i < count; i++) {
		if (ResourceItem* resource = container.ResourceAt(i))
			error = ReadResource(*resource, force);
		else
			error = B_ERROR;
	}
	return error;
}


status_t
ResourceFile::WriteResources(ResourcesContainer& container)
{
	status_t error = InitCheck();
	if (error == B_OK && !fFile.File()->IsWritable())
		error = B_NOT_ALLOWED;
	if (error == B_OK)
		error = _WriteResources(container);
	if (error == B_OK)
		fEmptyResources = false;
	return error;
}


void
ResourceFile::_InitFile(BFile& file, bool clobber)
{
	status_t error = B_OK;
	fFile.Unset();
	fFile.SetTo(&file, 0);
	
	// Try to load resources from extended attribute
	if (_TryLoadResourcesFromXAttr(file)) {
		// Resources found in xattr - determine endianness from the resource header
		if (fXAttrResourceSize >= 4) {
			uint32 magic;
			memcpy(&magic, fXAttrResourceData, 4);
			if (magic == kResourcesHeaderMagic) {
				fHostEndianess = true;  // Native endianness
			} else if (B_SWAP_INT32(magic) == kResourcesHeaderMagic) {
				fHostEndianess = false;  // Swapped endianness
			} else {
				throw Exception(B_IO_ERROR, "Invalid resources header magic in extended attribute.");
			}
		}
		fEmptyResources = false;
	} else {
		// No resources found - file is empty or will be created
		fHostEndianess = true;
		fEmptyResources = true;
	}
	
	error = fFile.InitCheck();
	if (error != B_OK)
		throw Exception(error, "Failed to initialize resource file.");
	
	// Clobber if desired - just write an empty resources container
	if (clobber) {
		ResourcesContainer container;
		WriteResources(container);
	}
}


void
ResourceFile::_ReadHeader(resource_parse_info& parseInfo)
{
	// read the header
	resources_header header;
	_ReadExactly(0, &header, kResourcesHeaderSize,
		"Failed to read the header.");
	// check the header
	// magic
	uint32 magic = _GetInt(header.rh_resources_magic);
	if (magic == kResourcesHeaderMagic) {
		// everything is fine
	} else if (B_SWAP_INT32(magic) == kResourcesHeaderMagic) {
//		const char* endianessStr[2] = { "little", "big" };
//		int32 endianess
//			= (fHostEndianess == ((bool)B_HOST_IS_LENDIAN ? 0 : 1));
//		Warnings::AddCurrentWarning("Endianess seems to be %s, although %s "
//									"was expected.",
//									endianessStr[1 - endianess],
//									endianessStr[endianess]);
		fHostEndianess = !fHostEndianess;
	} else
		throw Exception(B_IO_ERROR, "Invalid resources header magic.");
	// resource count
	uint32 resourceCount = _GetInt(header.rh_resource_count);
	if (resourceCount > kMaxResourceCount)
		throw Exception(B_IO_ERROR, "Bad number of resources.");
	// index section offset
	uint32 indexSectionOffset = _GetInt(header.rh_index_section_offset);
	if (indexSectionOffset != kResourceIndexSectionOffset) {
		throw Exception(B_IO_ERROR, "Unexpected resource index section "
			"offset. Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".",
			indexSectionOffset, kResourceIndexSectionOffset);
	}
	// admin section size
	uint32 indexSectionSize = kResourceIndexSectionHeaderSize
							  + kResourceIndexEntrySize * resourceCount;
	indexSectionSize = align_value(indexSectionSize,
								   kResourceIndexSectionAlignment);
	uint32 adminSectionSize = _GetInt(header.rh_admin_section_size);
	if (adminSectionSize != indexSectionOffset + indexSectionSize) {
		throw Exception(B_IO_ERROR, "Unexpected resource admin section size. "
			"Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".", adminSectionSize,
			indexSectionOffset + indexSectionSize);
	}
	// set the resource count
	parseInfo.resource_count = resourceCount;
}


void
ResourceFile::_ReadIndex(resource_parse_info& parseInfo)
{
	int32& resourceCount = parseInfo.resource_count;
	off_t& fileSize = parseInfo.file_size;
	// Don't use BBufferIO - use _ReadExactly which handles xattr
	// BBufferIO buffer(&fFile, 2048, false);

	// read the header
	resource_index_section_header header;
	_ReadExactly(kResourceIndexSectionOffset, &header,
		kResourceIndexSectionHeaderSize,
		"Failed to read the resource index section header.");
	// check the header
	// index section offset
	uint32 indexSectionOffset = _GetInt(header.rish_index_section_offset);
	if (indexSectionOffset != kResourceIndexSectionOffset) {
		throw Exception(B_IO_ERROR, "Unexpected resource index section "
			"offset. Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".",
			indexSectionOffset, kResourceIndexSectionOffset);
	}
	// index section size
	uint32 expectedIndexSectionSize = kResourceIndexSectionHeaderSize
		+ kResourceIndexEntrySize * resourceCount;
	expectedIndexSectionSize = align_value(expectedIndexSectionSize,
										   kResourceIndexSectionAlignment);
	uint32 indexSectionSize = _GetInt(header.rish_index_section_size);
	if (indexSectionSize != expectedIndexSectionSize) {
		throw Exception(B_IO_ERROR, "Unexpected resource index section size. "
			"Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".", indexSectionSize,
			expectedIndexSectionSize);
	}
	// unknown section offset
	uint32 unknownSectionOffset
		= _GetInt(header.rish_unknown_section_offset);
	if (unknownSectionOffset != indexSectionOffset + indexSectionSize) {
		throw Exception(B_IO_ERROR, "Unexpected resource index section size. "
			"Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".",
			unknownSectionOffset, indexSectionOffset + indexSectionSize);
	}
	// unknown section size
	uint32 unknownSectionSize = _GetInt(header.rish_unknown_section_size);
	if (unknownSectionSize != kUnknownResourceSectionSize) {
		throw Exception(B_IO_ERROR, "Unexpected resource index section "
			"offset. Is: %" B_PRIu32 ", should be: %" B_PRIu32 ".",
			unknownSectionOffset, kUnknownResourceSectionSize);
	}

	// info table offset and size
	uint32 infoTableOffset = _GetInt(header.rish_info_table_offset);
	uint32 infoTableSize = _GetInt(header.rish_info_table_size);
	if (infoTableOffset + infoTableSize > fileSize)
		throw Exception(B_IO_ERROR, "Invalid info table location.");
	parseInfo.info_table_offset = infoTableOffset;
	parseInfo.info_table_size = infoTableSize;

	// read the index entries
	uint32 indexTableOffset = indexSectionOffset
		+ kResourceIndexSectionHeaderSize;
	int32 maxResourceCount = (unknownSectionOffset - indexTableOffset)
		/ kResourceIndexEntrySize;
	int32 actualResourceCount = 0;
	bool tableEndReached = false;
	for (int32 i = 0; !tableEndReached && i < maxResourceCount; i++) {
		// read one entry
		tableEndReached = !_ReadIndexEntry(parseInfo, i,
			indexTableOffset, (i >= resourceCount));
		if (!tableEndReached)
			actualResourceCount++;
	}
	// check resource count
	if (actualResourceCount != resourceCount) {
		if (actualResourceCount > resourceCount) {
//			Warnings::AddCurrentWarning("Resource index table contains "
//										"%ld entries, although it should be "
//										"%ld only.", actualResourceCount,
//										resourceCount);
		}
		resourceCount = actualResourceCount;
	}
}


bool
ResourceFile::_ReadIndexEntry(resource_parse_info& parseInfo, int32 index, uint32 tableOffset,
	bool peekAhead)
{
	off_t& fileSize = parseInfo.file_size;
	bool result = true;
	resource_index_entry entry;

	// read one entry
	off_t entryOffset = tableOffset + index * kResourceIndexEntrySize;
	_ReadExactly(entryOffset, &entry, kResourceIndexEntrySize,
		"Failed to read a resource index entry.");

	// check, if the end is reached early
	if (result && check_pattern(entryOffset, &entry,
			kResourceIndexEntrySize / 4, fHostEndianess)) {
		result = false;
	}
	uint32 offset = _GetInt(entry.rie_offset);
	uint32 size = _GetInt(entry.rie_size);

	// check the location
	if (result && offset + size > fileSize) {
		if (!peekAhead) {
			throw Exception(B_IO_ERROR, "Invalid resource index entry: index: "
				"%" B_PRId32 ", offset: %" B_PRIu32 " (%" B_PRIx32 "), "
				"size: %" B_PRIu32 " (%" B_PRIx32 ").",
				index + 1, offset, offset, size, size);
		}
		result = false;
	}

	// add the entry
	if (result) {
		ResourceItem* item = new(std::nothrow) ResourceItem;
		if (!item)
			throw Exception(B_NO_MEMORY);
		item->SetLocation(offset, size);
		if (!parseInfo.container->AddResource(item, index, false)) {
			delete item;
			throw Exception(B_NO_MEMORY);
		}
	}

	return result;
}


void
ResourceFile::_ReadInfoTable(resource_parse_info& parseInfo)
{
	int32& resourceCount = parseInfo.resource_count;
	// read the info table
	// alloc memory for the table
	char* tableData = new(std::nothrow) char[parseInfo.info_table_size];
	if (!tableData)
		throw Exception(B_NO_MEMORY);
	int32 dataSize = parseInfo.info_table_size;
	parseInfo.info_table = tableData;	// freed by the info owner
	_ReadExactly(parseInfo.info_table_offset, tableData, dataSize,
		"Failed to read resource info table.");
	//
	bool* readIndices = new(std::nothrow) bool[resourceCount + 1];
		// + 1 => always > 0
	if (!readIndices)
		throw Exception(B_NO_MEMORY);
	ArrayDeleter<bool> readIndicesDeleter(readIndices);
	for (int32 i = 0; i < resourceCount; i++)
		readIndices[i] = false;
	MemArea area(tableData, dataSize);
	const void* data = tableData;
	// check the table end/check sum
	if (_ReadInfoTableEnd(data, dataSize))
		dataSize -= kResourceInfoTableEndSize;
	// read the infos
	int32 resourceIndex = 1;
	uint32 minRemainderSize
		= kMinResourceInfoBlockSize + kResourceInfoSeparatorSize;
	while (area.check(data, minRemainderSize)) {
		// read a resource block
		if (!area.check(data, kMinResourceInfoBlockSize)) {
			throw Exception(B_IO_ERROR, "Unexpected end of resource info "
				"table at index %" B_PRId32 ".", resourceIndex);
		}
		const resource_info_block* infoBlock
			= (const resource_info_block*)data;
		type_code type = _GetInt(infoBlock->rib_type);
		// read the infos of this block
		const resource_info* info = infoBlock->rib_info;
		while (info) {
			data = _ReadResourceInfo(parseInfo, area, info, type, readIndices);
			// prepare for next iteration, if there is another info
			if (!area.check(data, kResourceInfoSeparatorSize)) {
				throw Exception(B_IO_ERROR, "Unexpected end of resource info "
					"table after index %" B_PRId32 ".", resourceIndex);
			}
			const resource_info_separator* separator
				= (const resource_info_separator*)data;
			if (_GetInt(separator->ris_value1) == 0xffffffff
				&& _GetInt(separator->ris_value2) == 0xffffffff) {
				// info block ends
				info = NULL;
				data = skip_bytes(data, kResourceInfoSeparatorSize);
			} else {
				// another info follows
				info = (const resource_info*)data;
			}
			resourceIndex++;
		}
		// end of the info block
	}
	// handle special case: empty resource info table
	if (resourceIndex == 1) {
		if (!area.check(data, kResourceInfoSeparatorSize)) {
			throw Exception(B_IO_ERROR, "Unexpected end of resource info "
				"table.");
		}
		const resource_info_separator* tableTerminator
			= (const resource_info_separator*)data;
		if (_GetInt(tableTerminator->ris_value1) != 0xffffffff
			|| _GetInt(tableTerminator->ris_value2) != 0xffffffff) {
			throw Exception(B_IO_ERROR, "The resource info table ought to be "
				"empty, but is not properly terminated.");
		}
		data = skip_bytes(data, kResourceInfoSeparatorSize);
	}
	// Check, if the correct number of bytes are remaining.
	uint32 bytesLeft = (const char*)tableData + dataSize - (const char*)data;
	if (bytesLeft != 0) {
		throw Exception(B_IO_ERROR, "Error at the end of the resource info "
			"table: %" B_PRIu32 " bytes are remaining.", bytesLeft);
	}
	// check, if all items have been initialized
	for (int32 i = resourceCount - 1; i >= 0; i--) {
		if (!readIndices[i]) {
//			Warnings::AddCurrentWarning("Resource item at index %ld "
//										"has no info. Item removed.", i + 1);
			if (ResourceItem* item = parseInfo.container->RemoveResource(i))
				delete item;
			resourceCount--;
		}
	}
}


bool
ResourceFile::_ReadInfoTableEnd(const void* data, int32 dataSize)
{
	bool hasTableEnd = true;
	if ((uint32)dataSize < kResourceInfoSeparatorSize)
		throw Exception(B_IO_ERROR, "Info table is too short.");
	if ((uint32)dataSize < kResourceInfoTableEndSize)
		hasTableEnd = false;
	if (hasTableEnd) {
		const resource_info_table_end* tableEnd
			= (const resource_info_table_end*)
			  skip_bytes(data, dataSize - kResourceInfoTableEndSize);
		if (_GetInt(tableEnd->rite_terminator) != 0)
			hasTableEnd = false;
		if (hasTableEnd) {
			dataSize -= kResourceInfoTableEndSize;
			// checksum
			uint32 checkSum = calculate_checksum(data, dataSize);
			uint32 fileCheckSum = _GetInt(tableEnd->rite_check_sum);
			if (checkSum != fileCheckSum) {
				throw Exception(B_IO_ERROR, "Invalid resource info table check"
					" sum: In file: %" B_PRIx32 ", calculated: %" B_PRIx32 ".",
					fileCheckSum, checkSum);
			}
		}
	}
//	if (!hasTableEnd)
//		Warnings::AddCurrentWarning("resource info table has no check sum.");
	return hasTableEnd;
}


const void*
ResourceFile::_ReadResourceInfo(resource_parse_info& parseInfo,
	const MemArea& area, const resource_info* info, type_code type,
	bool* readIndices)
{
	int32& resourceCount = parseInfo.resource_count;
	int32 id = _GetInt(info->ri_id);
	int32 index = _GetInt(info->ri_index);
	uint16 nameSize = _GetInt(info->ri_name_size);
	const char* name = info->ri_name;
	// check the values
	bool ignore = false;
	// index
	if (index < 1 || index > resourceCount) {
//		Warnings::AddCurrentWarning("Invalid index field in resource "
//									"info table: %lu.", index);
		ignore = true;
	}
	if (!ignore) {
		if (readIndices[index - 1]) {
			throw Exception(B_IO_ERROR, "Multiple resource infos with the "
				"same index field: %" B_PRId32 ".", index);
		}
		readIndices[index - 1] = true;
	}
	// name size
	if (!area.check(name, nameSize)) {
		throw Exception(B_IO_ERROR, "Invalid name size (%" B_PRIu16 ") "
			"for index %" B_PRId32 " in resource info table.",
			nameSize, index);
	}
	// check, if name is null terminated
	if (name[nameSize - 1] != 0) {
//		Warnings::AddCurrentWarning("Name for index %ld in "
//									"resource info table is not null "
//									"terminated.", index);
	}
	// set the values
	if (!ignore) {
		BString resourceName(name, nameSize);
		if (ResourceItem* item = parseInfo.container->ResourceAt(index - 1))
			item->SetIdentity(type, id, resourceName.String());
		else {
			throw Exception(B_IO_ERROR, "Unexpected error: No resource item "
				"at index %" B_PRId32 ".", index);
		}
	}
	return skip_bytes(name, nameSize);
}


status_t
ResourceFile::_WriteResources(ResourcesContainer& container)
{
	status_t error = B_OK;
	int32 resourceCount = container.CountResources();
	char* buffer = NULL;
	char* resourceData = NULL;
	
	try {
		// calculate sizes and offsets
		// header
		uint32 size = kResourcesHeaderSize;
		size_t bufferSize = size;
		// index section
		uint32 indexSectionOffset = size;
		uint32 indexSectionSize = kResourceIndexSectionHeaderSize
			+ resourceCount * kResourceIndexEntrySize;
		indexSectionSize = align_value(indexSectionSize,
			kResourceIndexSectionAlignment);
		size += indexSectionSize;
		bufferSize = std::max((uint32)bufferSize, indexSectionSize);
		// unknown section
		uint32 unknownSectionOffset = size;
		uint32 unknownSectionSize = kUnknownResourceSectionSize;
		size += unknownSectionSize;
		bufferSize = std::max((uint32)bufferSize, unknownSectionSize);
		// data
		uint32 dataOffset = size;
		uint32 dataSize = 0;
		for (int32 i = 0; i < resourceCount; i++) {
			ResourceItem* item = container.ResourceAt(i);
			if (!item->IsLoaded())
				throw Exception(B_IO_ERROR, "Resource is not loaded.");
			dataSize += item->DataSize();
			bufferSize = std::max(bufferSize, item->DataSize());
		}
		size += dataSize;
		// info table
		uint32 infoTableOffset = size;
		uint32 infoTableSize = 0;
		type_code type = 0;
		for (int32 i = 0; i < resourceCount; i++) {
			ResourceItem* item = container.ResourceAt(i);
			if (i == 0 || type != item->Type()) {
				if (i != 0)
					infoTableSize += kResourceInfoSeparatorSize;
				type = item->Type();
				infoTableSize += kMinResourceInfoBlockSize;
			} else
				infoTableSize += kMinResourceInfoSize;

			const char* name = item->Name();
			if (name && name[0] != '\0')
				infoTableSize += strlen(name) + 1;
		}
		infoTableSize += kResourceInfoSeparatorSize
			+ kResourceInfoTableEndSize;
		size += infoTableSize;
		bufferSize = std::max((uint32)bufferSize, infoTableSize);

		// Allocate buffer for building complete resource data
		resourceData = new(std::nothrow) char[size];
		if (!resourceData)
			throw Exception(B_NO_MEMORY);
		memset(resourceData, 0, size);
		
		// Also allocate working buffer for sections
		buffer = new(std::nothrow) char[bufferSize];
		if (!buffer)
			throw Exception(B_NO_MEMORY);
		
		void* data = buffer;
		// header
		resources_header* resourcesHeader = (resources_header*)data;
		resourcesHeader->rh_resources_magic = kResourcesHeaderMagic;
		resourcesHeader->rh_resource_count = resourceCount;
		resourcesHeader->rh_index_section_offset = indexSectionOffset;
		resourcesHeader->rh_admin_section_size = indexSectionOffset
												 + indexSectionSize;
		for (int32 i = 0; i < 13; i++)
			resourcesHeader->rh_pad[i] = 0;
		// Copy header to resource data
		memcpy(resourceData, buffer, kResourcesHeaderSize);
		
		// index section
		data = buffer;
		// header
		resource_index_section_header* indexHeader
			= (resource_index_section_header*)data;
		indexHeader->rish_index_section_offset = indexSectionOffset;
		indexHeader->rish_index_section_size = indexSectionSize;
		indexHeader->rish_unknown_section_offset = unknownSectionOffset;
		indexHeader->rish_unknown_section_size = unknownSectionSize;
		indexHeader->rish_info_table_offset = infoTableOffset;
		indexHeader->rish_info_table_size = infoTableSize;
		fill_pattern(buffer - indexSectionOffset,
			&indexHeader->rish_unused_data1, 1);
		fill_pattern(buffer - indexSectionOffset,
			indexHeader->rish_unused_data2, 25);
		fill_pattern(buffer - indexSectionOffset,
			&indexHeader->rish_unused_data3, 1);
		// index table
		data = skip_bytes(data, kResourceIndexSectionHeaderSize);
		resource_index_entry* entry = (resource_index_entry*)data;
		uint32 entryOffset = dataOffset;
		for (int32 i = 0; i < resourceCount; i++, entry++) {
			ResourceItem* item = container.ResourceAt(i);
			uint32 entrySize = item->DataSize();
			entry->rie_offset = entryOffset;
			entry->rie_size = entrySize;
			entry->rie_pad = 0;
			entryOffset += entrySize;
		}
		fill_pattern(buffer - indexSectionOffset, entry,
			buffer + indexSectionSize);
		// Copy index section to resource data
		memcpy(resourceData + indexSectionOffset, buffer, indexSectionSize);
		
		// unknown section
		fill_pattern(unknownSectionOffset, buffer, unknownSectionSize / 4);
		// Copy unknown section to resource data
		memcpy(resourceData + unknownSectionOffset, buffer, unknownSectionSize);
		
		// data
		uint32 itemOffset = dataOffset;
		for (int32 i = 0; i < resourceCount; i++) {
			data = buffer;
			ResourceItem* item = container.ResourceAt(i);
			const void* itemData = item->Data();
			uint32 itemSize = item->DataSize();
			if (!itemData && itemSize > 0)
				throw Exception(error, "Invalid resource item data.");
			if (itemData) {
				// swap data, if necessary
				if (!fHostEndianess) {
					memcpy(data, itemData, itemSize);
					if (item->Type() == B_VERSION_INFO_TYPE) {
						// Version info contains integers
						// that need to be swapped
						swap_data(B_UINT32_TYPE, data,
							kVersionInfoIntCount * sizeof(uint32),
							B_SWAP_ALWAYS);
					} else
						swap_data(item->Type(), data, itemSize, B_SWAP_ALWAYS);
					itemData = data;
				}
				// Copy item data to resource data
				memcpy(resourceData + itemOffset, itemData, itemSize);
			}
			item->SetOffset(itemOffset);
			itemOffset += itemSize;
		}
		// info table
		data = buffer;
		type = 0;
		for (int32 i = 0; i < resourceCount; i++) {
			ResourceItem* item = container.ResourceAt(i);
			resource_info* info = NULL;
			if (i == 0 || type != item->Type()) {
				if (i != 0) {
					resource_info_separator* separator
						= (resource_info_separator*)data;
					separator->ris_value1 = 0xffffffff;
					separator->ris_value2 = 0xffffffff;
					data = skip_bytes(data, kResourceInfoSeparatorSize);
				}
				type = item->Type();
				resource_info_block* infoBlock = (resource_info_block*)data;
				infoBlock->rib_type = type;
				info = infoBlock->rib_info;
			} else
				info = (resource_info*)data;
			// info
			info->ri_id = item->ID();
			info->ri_index = i + 1;
			info->ri_name_size = 0;
			data = info->ri_name;

			const char* name = item->Name();
			if (name && name[0] != '\0') {
				uint32 nameLen = strlen(name);
				memcpy(info->ri_name, name, nameLen + 1);
				data = skip_bytes(data, nameLen + 1);
				info->ri_name_size = nameLen + 1;
			}
		}
		// separator
		resource_info_separator* separator = (resource_info_separator*)data;
		separator->ris_value1 = 0xffffffff;
		separator->ris_value2 = 0xffffffff;
		// table end
		data = skip_bytes(data, kResourceInfoSeparatorSize);
		resource_info_table_end* tableEnd = (resource_info_table_end*)data;
		tableEnd->rite_check_sum = calculate_checksum(buffer,
			infoTableSize - kResourceInfoTableEndSize);
		tableEnd->rite_terminator = 0;
		// Copy info table to resource data
		memcpy(resourceData + infoTableOffset, buffer, infoTableSize);
		
		// For pure resource files (not executables/libraries), write to file as before
		// For executables/libraries (ELF, Mach-O), write to extended attributes
	// Always use extended attributes for storing resources
	if (!fFilePath)
		throw Exception(B_ERROR, "No file path available for extended attribute operations.");
	
	
	// Write resources to extended attribute
#if defined(__APPLE__)
	int result = setxattr(fFilePath, kResourceXAttrName, resourceData, size, 0, 0);
	if (result != 0) {
		error = errno;
		throw Exception(error, "Failed to write resources to extended attribute '%s': %s", 
			fFilePath, strerror(errno));
	}
#elif defined(__linux__)
	int result = setxattr(fFilePath, kResourceXAttrName, resourceData, size, 0);
	if (result != 0) {
		error = errno;
		throw Exception(error, "Failed to write resources to extended attribute '%s': %s",
			fFilePath, strerror(errno));
	}
#else
	throw Exception(B_UNSUPPORTED, "Extended attributes not supported on this platform.");
#endif
	
	// Reload xattr so InitContainer can read it
	if (error == B_OK) {
			BFile* file = fFile.File();
			if (file)
				_TryLoadResourcesFromXAttr(*file);
		}
		
	} catch (Exception& exception) {
		if (exception.Error() != B_OK)
			error = exception.Error();
		else
			error = B_ERROR;
	}
	delete[] buffer;
	delete[] resourceData;
	return error;
}


bool
ResourceFile::_TryLoadResourcesFromXAttr(BFile& file)
{
	// Try to use stored file path first
	const char* filePath = fFilePath;
	char pathBuffer[PATH_MAX];
	
	if (!filePath) {
		// No stored path, try to get it from the file descriptor
#if defined(__APPLE__)
		int fd = file.Dup();
		if (fcntl(fd, F_GETPATH, pathBuffer) == -1) {
			close(fd);
			return false;
		}
		close(fd);
		filePath = pathBuffer;
#elif defined(__linux__)
		int fd = file.Dup();
		char procPath[64];
		snprintf(procPath, sizeof(procPath), "/proc/self/fd/%d", fd);
		ssize_t len = readlink(procPath, pathBuffer, sizeof(pathBuffer) - 1);
		close(fd);
		if (len == -1)
			return false;
		pathBuffer[len] = '\0';
		filePath = pathBuffer;
#else
		// Windows - would need different approach
		return false;
#endif
	}
	
	// Try to read resources from extended attribute
#if defined(__APPLE__)
	// First get the size
	ssize_t xattrSize = getxattr(filePath, kResourceXAttrName, NULL, 0, 0, 0);
	if (xattrSize <= 0)
		return false;
	
	// Allocate buffer and read
	delete[] fXAttrResourceData;
	fXAttrResourceData = new(std::nothrow) char[xattrSize];
	if (!fXAttrResourceData)
		return false;
	
	ssize_t bytesRead = getxattr(filePath, kResourceXAttrName, 
		fXAttrResourceData, xattrSize, 0, 0);
	if (bytesRead != xattrSize) {
		delete[] fXAttrResourceData;
		fXAttrResourceData = NULL;
		return false;
	}
	
	fXAttrResourceSize = xattrSize;
	return true;
	
#elif defined(__linux__)
	// First get the size
	ssize_t xattrSize = getxattr(filePath, kResourceXAttrName, NULL, 0);
	if (xattrSize <= 0)
		return false;
	
	// Allocate buffer and read
	fXAttrResourceData = new(std::nothrow) char[xattrSize];
	if (!fXAttrResourceData)
		return false;
	
	ssize_t bytesRead = getxattr(filePath, kResourceXAttrName, 
		fXAttrResourceData, xattrSize);
	if (bytesRead != xattrSize) {
		delete[] fXAttrResourceData;
		fXAttrResourceData = NULL;
		return false;
	}
	
	fXAttrResourceSize = xattrSize;
	return true;
	
#elif defined(_WIN32)
	// Windows: Use Alternate Data Streams
	char adsPath[MAX_PATH + 64];
	snprintf(adsPath, sizeof(adsPath), "%s:%s", filePath, kResourceXAttrName);
	
	HANDLE hFile = CreateFileA(adsPath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
		CloseHandle(hFile);
		return false;
	}
	
	fXAttrResourceData = new(std::nothrow) char[fileSize];
	if (!fXAttrResourceData) {
		CloseHandle(hFile);
		return false;
	}
	
	DWORD bytesRead;
	BOOL readResult = ReadFile(hFile, fXAttrResourceData, fileSize, 
		&bytesRead, NULL);
	CloseHandle(hFile);
	
	if (!readResult || bytesRead != fileSize) {
		delete[] fXAttrResourceData;
		fXAttrResourceData = NULL;
		return false;
	}
	
	fXAttrResourceSize = fileSize;
	return true;
	
#else
	return false;
#endif
}


};	// namespace Storage
};	// namespace BPrivate

/*
 * Copyright 2006, Marcus Overhagen, <marcus@overhagen.de>
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>
#include <stdio.h>
#include <string.h>

static int sFailures = 0;

int
main()
{
	port_id id;
	status_t s;
	ssize_t size;
	int32 code;
	
	char data[100];
	
	
	id = create_port(10, "test port");
	printf("created port %d\n", id);
	
	s = write_port(id, 0x1234, data, 10);
	printf("write port result 0x%08x (%s)\n", s, strerror(s));
	if (s != B_OK)
		++sFailures;

	s = write_port(id, 0x5678, data, 20);
	printf("write port result 0x%08x (%s)\n", s, strerror(s));
	if (s != B_OK)
		++sFailures;
	
	s = close_port(id);
	printf("close port result 0x%08x (%s)\n", s, strerror(s));
	if (s != B_OK)
		++sFailures;

	// BeBook: does return B_BAD_PORT_ID if port was closed
	s = write_port(id, 0x5678, data, 20);
	printf("write port result 0x%08x (%s)\n", s, strerror(s));
	printf("%s\n", s == B_BAD_PORT_ID ? "SUCCESS" : "FAILURE");
	if (s != B_BAD_PORT_ID)
		++sFailures;

	// BeBook: does block when port is empty, and unblocks when port is written to or deleted
	size = port_buffer_size(id); 
	printf("port_buffer_size %lld (0x%08llx) (%s)\n", (long long)size,
		(unsigned long long)size, strerror(size));
	if (size != B_BAD_PORT_ID)
		++sFailures;

	// BeBook: does block when port is empty, and unblocks when port is written to or deleted
	size = read_port(id, &code, data, sizeof(data)); 
	printf("read port code %x, size %lld (0x%08llx) (%s)\n", code,
		(long long)size, (unsigned long long)size, strerror(size));
	if (size != B_BAD_PORT_ID)
		++sFailures;

	// BeBook: does block when port is empty, and unblocks when port is written to or deleted
	size = port_buffer_size(id); 
	printf("port_buffer_size %lld (0x%08llx) (%s)\n", (long long)size,
		(unsigned long long)size, strerror(size));
	if (size != B_BAD_PORT_ID)
		++sFailures;

	// BeBook: does block when port is empty, and unblocks when port is written to or deleted
	size = read_port(id, &code, data, sizeof(data)); 
	printf("read port code %x, size %lld (0x%08llx) (%s)\n", code,
		(long long)size, (unsigned long long)size, strerror(size));
	if (size != B_BAD_PORT_ID)
		++sFailures;
	
	printf("port_buffer_size should fail now:\n");

	// BeBook: does block when port is empty, and unblocks when port is written to or deleted
	size = port_buffer_size(id); 
	printf("port_buffer_size %lld (0x%08llx) (%s)\n", (long long)size,
		(unsigned long long)size, strerror(size));
	if (size != B_BAD_PORT_ID)
		++sFailures;
	
	printf("failures=%d\n", sFailures);
	return sFailures == 0 ? 0 : 1;
}

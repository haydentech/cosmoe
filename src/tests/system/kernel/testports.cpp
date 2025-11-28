// Standard Includes -----------------------------------------------------------
#include <stdio.h>
#include <string.h>

// System Includes -------------------------------------------------------------
#include <OS.h>

// Project Includes ------------------------------------------------------------

// Local Includes --------------------------------------------------------------

// Local Defines ---------------------------------------------------------------
#define dprintf printf

// Globals ---------------------------------------------------------------------

static void port_test();
static void port_info_test();
static void port_buffer_size_test();
static void port_ownership_test();
static void port_error_test();


int main()
{
	dprintf("\n==============================================\n");
	dprintf("  Port API Tests\n");
	dprintf("==============================================\n");
	
	port_test();
	port_info_test();
	port_buffer_size_test();
	port_ownership_test();
	port_error_test();
	
	dprintf("\n==============================================\n");
	dprintf("  All Port Tests Complete\n");
	dprintf("==============================================\n");
	
	return 0;
}


static int32 port_test_thread_func(void *arg);

port_id test_p1, test_p2, test_p3, test_p4, test_p5;

void port_test()
{
	char testdata[5];
	thread_id t;
	int32 dummy;
	int32 dummy2;
	port_id dummy_port;
	status_t status;

	strcpy(testdata, "abcd");

	dprintf("porttest: begin test\n");

	/* Create ports */

	test_p1 = create_port(1,    "test port #1");
	test_p2 = create_port(10,   "test port #2");
	test_p3 = create_port(256,  "test port #3");
	test_p4 = create_port(256,  "test port #4");
	test_p5 = create_port(4097, "test port #5"); /* queue too long */

	dprintf("porttest (%s):'test port #1' has id %d\n",
			(test_p1 >= 0) ? "pass" : "FAIL", test_p1);
	dprintf("porttest (%s):'test port #2' has id %d\n",
			(test_p2 >= 0) ? "pass" : "FAIL", test_p2);
	dprintf("porttest (%s):'test port #3' has id %d\n",
			(test_p3 >= 0) ? "pass" : "FAIL", test_p3);
	dprintf("porttest (%s):'test port #4' has id %d\n",
			(test_p4 >= 0) ? "pass" : "FAIL", test_p4);
	dprintf("porttest (%s):invalid 'test port #5' has id %d\n",
			(test_p5 == B_BAD_VALUE) ? "pass" : "FAIL", test_p5);

	/* Manipulate ports */

	dummy_port = find_port("test port #1");
	dprintf("porttest (%s): find_port(test port #1) returned %d\n",
			(test_p1 == dummy_port) ? "pass" : "FAIL", dummy_port);

	dprintf("porttest (info): write_port() on 1, 2 and 3\n");
	status = write_port(test_p1, 1, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port(test port #1) returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	write_port(test_p2, 666, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port(test port #2) returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	write_port(test_p3, 999, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port(test port #3) returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	dummy = port_count(test_p1);
	dprintf("porttest (%s): port_count(test_p1) = %ld\n",
			(dummy == 1) ? "pass" : "FAIL", (long)dummy);

	status = write_port_etc(test_p1, 1, &testdata, sizeof(testdata), B_TIMEOUT, 1000000);
	dprintf("porttest (%s): write_port() on 1 with timeout of 1 sec (blocks 1 sec) returned %d\n",
			(status == B_TIMED_OUT) ? "pass" : "FAIL", status);

	status = write_port_etc(test_p2, 777, &testdata, sizeof(testdata), B_TIMEOUT, 1000000);
	dprintf("porttest (%s): write_port() on 2 with timeout of 1 sec (won't block) returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	status = read_port_etc(test_p4, &dummy, &dummy2, sizeof(dummy2), B_TIMEOUT, 1000000);
	dprintf("porttest (%s): read_port() on empty port 4 with timeout of 1 sec (blocks 1 sec) returned %d\n",
			(status == B_TIMED_OUT) ? "pass" : "FAIL", status);

	dprintf("porttest (pass): spawning thread for port 1\n");
	t = spawn_thread(port_test_thread_func, "port_test", B_NORMAL_PRIORITY, NULL);
	resume_thread(t);

	status = write_port(test_p1, 1, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port() on 1 returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	// now we can write more (no blocking)
	status = write_port(test_p1, 2, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port() on 2 returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	status = write_port(test_p1, 3, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port() on 3 returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	dprintf("porttest: waiting on spawned thread\n");
	wait_for_thread(t, NULL);

	status = close_port(test_p2);
	dprintf("porttest (%s): close_port() on 2 returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	status = write_port(test_p2, 4, &testdata, sizeof(testdata));
	dprintf("porttest (%s): write_port() on closed port 2 returned %d\n",
			(status < 0) ? "pass" : "FAIL", status);

	status = delete_port(test_p2);
	dprintf("porttest (%s): delete_port() on 2 returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	dprintf("porttest: end test main thread\n");
}


static int32
port_test_thread_func(void *arg)
{
	int32 msg_code;
	char buf[6];
	buf[5] = '\0';
	status_t status;

	dprintf("porttest: enter port_test_thread_func()\n");

	status = read_port(test_p1, &msg_code, &buf, 3);
	dprintf("porttest (%s): read_port() on 1, code %d, buf %s, returned %d\n",
			(status >= 0) ? "pass" : "FAIL", msg_code, buf, status);

	status = read_port(test_p1, &msg_code, &buf, 4);
	dprintf("porttest (%s): read_port() on 1, code %d, buf %s, returned %d\n",
			(status >= 0) ? "pass" : "FAIL", msg_code, buf, status);
	buf[4] = 'X';

	status = read_port(test_p1, &msg_code, &buf, 5);
	dprintf("porttest (%s): read_port() on 1, code %d, buf %s, returned %d\n",
			((status >= 0) && (buf[4] != 'X')) ? "pass" : "FAIL", msg_code, buf, status);

	status = read_port(test_p1, &msg_code, &buf, 3);
	dprintf("porttest (%s): read_port() on 1, code %d, buf %s, returned %d\n",
			(status >= 0) ? "pass" : "FAIL", msg_code, buf, status);
	
	status = delete_port(test_p1);
	dprintf("porttest (%s): delete_port() on 1 (from other thread) returned %d\n",
			(status == 0) ? "pass" : "FAIL", status);

	dprintf("porttest: leave port_test_thread_func()\n");

	return 0;
}


// Test get_port_info and get_next_port_info
void port_info_test()
{
	dprintf("\n=== Port Info Tests ===\n");
	
	port_id p1 = create_port(5, "info_test_port_1");
	port_id p2 = create_port(10, "info_test_port_2");
	
	// Test get_port_info
	port_info info;
	status_t status = get_port_info(p1, &info);
	dprintf("porttest (%s): get_port_info() returned %d\n",
			(status == B_OK) ? "pass" : "FAIL", status);
	
	if (status == B_OK) {
		dprintf("porttest (%s): port name is '%s'\n",
				(strcmp(info.name, "info_test_port_1") == 0) ? "pass" : "FAIL", info.name);
		dprintf("porttest (%s): port capacity is %ld\n",
				(info.capacity == 5) ? "pass" : "FAIL", (long)info.capacity);
		dprintf("porttest (%s): port queue_count is %ld\n",
				(info.queue_count == 0) ? "pass" : "FAIL", (long)info.queue_count);
		dprintf("porttest (%s): port team is %ld\n",
				(info.team > 0) ? "pass" : "FAIL", (long)info.team);
	}
	
	// Write some messages and check queue_count
	char data[] = "test";
	write_port(p1, 100, data, sizeof(data));
	write_port(p1, 200, data, sizeof(data));
	
	status = get_port_info(p1, &info);
	dprintf("porttest (%s): queue_count after 2 writes is %ld\n",
			(info.queue_count == 2) ? "pass" : "FAIL", (long)info.queue_count);
	
	// Test get_next_port_info - iterate through all ports
	dprintf("\nporttest (info): Testing get_next_port_info iteration\n");
	int32 cookie = 0;
	int count = 0;
	bool found_p1 = false, found_p2 = false;
	
	while (get_next_port_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		count++;
		if (info.port == p1) found_p1 = true;
		if (info.port == p2) found_p2 = true;
		if (count > 1000) break; // safety
	}
	
	dprintf("porttest (%s): found %d ports in current team\n",
			(count >= 2) ? "pass" : "FAIL", count);
	dprintf("porttest (%s): found info_test_port_1 in iteration\n",
			found_p1 ? "pass" : "FAIL");
	dprintf("porttest (%s): found info_test_port_2 in iteration\n",
			found_p2 ? "pass" : "FAIL");
	
	// Test get_port_info with invalid port
	status = get_port_info(999999, &info);
	dprintf("porttest (%s): get_port_info() with invalid port returned %d\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	delete_port(p1);
	delete_port(p2);
}


// Test port_buffer_size and port_buffer_size_etc
void port_buffer_size_test()
{
	dprintf("\n=== Port Buffer Size Tests ===\n");
	
	port_id p = create_port(5, "buffer_size_test");
	
	// Write messages of different sizes
	char small_data[] = "hi";
	char medium_data[] = "hello world";
	char large_data[100];
	memset(large_data, 'X', sizeof(large_data));
	
	write_port(p, 1, small_data, sizeof(small_data));
	write_port(p, 2, medium_data, sizeof(medium_data));
	write_port(p, 3, large_data, sizeof(large_data));
	
	// Test port_buffer_size - should return size of first message
	ssize_t size = port_buffer_size(p);
	dprintf("porttest (%s): port_buffer_size() returned %ld (expected %zu)\n",
			(size == sizeof(small_data)) ? "pass" : "FAIL", (long)size, sizeof(small_data));
	
	// Read the first message
	int32 code;
	char buffer[200];
	read_port(p, &code, buffer, sizeof(buffer));
	
	// Now buffer_size should return size of second message
	size = port_buffer_size(p);
	dprintf("porttest (%s): port_buffer_size() after read returned %ld (expected %zu)\n",
			(size == sizeof(medium_data)) ? "pass" : "FAIL", (long)size, sizeof(medium_data));
	
	// Test port_buffer_size_etc with timeout on empty port
	read_port(p, &code, buffer, sizeof(buffer)); // read second message
	read_port(p, &code, buffer, sizeof(buffer)); // read third message
	
	size = port_buffer_size_etc(p, B_TIMEOUT, 100000); // 0.1 second timeout
	dprintf("porttest (%s): port_buffer_size_etc() on empty port with timeout returned %ld\n",
			(size == B_TIMED_OUT) ? "pass" : "FAIL", (long)size);
	
	// Test B_WOULD_BLOCK
	size = port_buffer_size_etc(p, B_TIMEOUT, 0);
	dprintf("porttest (%s): port_buffer_size_etc() with 0 timeout returned %ld (B_WOULD_BLOCK)\n",
			(size == B_WOULD_BLOCK) ? "pass" : "FAIL", (long)size);
	
	delete_port(p);
	
	// Test buffer_size on deleted port
	size = port_buffer_size(p);
	dprintf("porttest (%s): port_buffer_size() on deleted port returned %ld\n",
			(size == B_BAD_PORT_ID) ? "pass" : "FAIL", (long)size);
}


// Test set_port_owner
void port_ownership_test()
{
	dprintf("\n=== Port Ownership Tests ===\n");
	
	port_id p = create_port(5, "ownership_test");
	
	// Get initial owner
	port_info info;
	get_port_info(p, &info);
	team_id original_owner = info.team;
	dprintf("porttest (info): port created with owner team %ld\n", (long)original_owner);
	
	// Try to set owner to current team (should succeed even though it's already the owner)
	status_t status = set_port_owner(p, original_owner);
	dprintf("porttest (%s): set_port_owner() to same team returned %d\n",
			(status == B_OK) ? "pass" : "FAIL", status);
	
	// Verify owner didn't change
	get_port_info(p, &info);
	dprintf("porttest (%s): owner is still %ld\n",
			(info.team == original_owner) ? "pass" : "FAIL", (long)info.team);
	
	// Test with invalid port
	status = set_port_owner(999999, original_owner);
	dprintf("porttest (%s): set_port_owner() with invalid port returned %d\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	// Test with invalid team (negative team_id)
	status = set_port_owner(p, -1);
	dprintf("porttest (%s): set_port_owner() with invalid team returned %d\n",
			(status == B_BAD_TEAM_ID) ? "pass" : "FAIL", status);
	
	delete_port(p);
}


// Test error conditions and edge cases
void port_error_test()
{
	dprintf("\n=== Port Error Handling Tests ===\n");
	
	// Test create_port with invalid queue_length
	port_id p = create_port(-1, "negative_length");
	dprintf("porttest (%s): create_port(-1) returned %d (B_BAD_VALUE)\n",
			(p == B_BAD_VALUE) ? "pass" : "FAIL", p);
	
	p = create_port(0, "zero_length");
	dprintf("porttest (%s): create_port(0) returned %d (B_BAD_VALUE)\n",
			(p == B_BAD_VALUE) ? "pass" : "FAIL", p);
	
	// Test write_port_etc with B_WOULD_BLOCK
	p = create_port(1, "full_port_test");
	char data[] = "test";
	write_port(p, 1, data, sizeof(data)); // fill the queue
	
	status_t status = write_port_etc(p, 2, data, sizeof(data), B_TIMEOUT, 0);
	dprintf("porttest (%s): write_port_etc() on full port with 0 timeout returned %d (B_WOULD_BLOCK)\n",
			(status == B_WOULD_BLOCK) ? "pass" : "FAIL", status);
	
	// Test read_port_etc with B_WOULD_BLOCK
	port_id p2 = create_port(5, "empty_port_test");
	int32 code;
	char buffer[100];
	status = read_port_etc(p2, &code, buffer, sizeof(buffer), B_TIMEOUT, 0);
	dprintf("porttest (%s): read_port_etc() on empty port with 0 timeout returned %d (B_WOULD_BLOCK)\n",
			(status == B_WOULD_BLOCK) ? "pass" : "FAIL", status);
	
	// Test find_port with non-existent name
	port_id found = find_port("port_that_does_not_exist_xyz");
	dprintf("porttest (%s): find_port() with invalid name returned %d (B_NAME_NOT_FOUND)\n",
			(found == B_NAME_NOT_FOUND) ? "pass" : "FAIL", found);
	
	// Test port_count
	write_port(p2, 1, data, sizeof(data));
	write_port(p2, 2, data, sizeof(data));
	write_port(p2, 3, data, sizeof(data));
	
	int32 count = port_count(p2);
	dprintf("porttest (%s): port_count() after 3 writes returned %ld\n",
			(count == 3) ? "pass" : "FAIL", (long)count);
	
	// Test port_count with invalid port
	count = port_count(999999);
	dprintf("porttest (%s): port_count() with invalid port returned %ld\n",
			(count == B_BAD_PORT_ID) ? "pass" : "FAIL", (long)count);
	
	// Test that deleted port returns B_BAD_PORT_ID for operations
	port_id p3 = create_port(5, "to_be_deleted");
	delete_port(p3);
	
	status = write_port(p3, 1, data, sizeof(data));
	dprintf("porttest (%s): write_port() on deleted port returned %d (B_BAD_PORT_ID)\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	status = read_port(p3, &code, buffer, sizeof(buffer));
	dprintf("porttest (%s): read_port() on deleted port returned %d (B_BAD_PORT_ID)\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	count = port_count(p3);
	dprintf("porttest (%s): port_count() on deleted port returned %ld (B_BAD_PORT_ID)\n",
			(count == B_BAD_PORT_ID) ? "pass" : "FAIL", (long)count);
	
	// Test close_port with invalid port
	status = close_port(999999);
	dprintf("porttest (%s): close_port() with invalid port returned %d (B_BAD_PORT_ID)\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	// Test delete_port with invalid port
	status = delete_port(999999);
	dprintf("porttest (%s): delete_port() with invalid port returned %d (B_BAD_PORT_ID)\n",
			(status == B_BAD_PORT_ID) ? "pass" : "FAIL", status);
	
	// Test get_next_port_info with invalid team
	int32 cookie = 0;
	port_info info;
	status = get_next_port_info(-1, &cookie, &info);
	dprintf("porttest (%s): get_next_port_info() with invalid team returned %d (B_BAD_VALUE)\n",
			(status == B_BAD_VALUE) ? "pass" : "FAIL", status);
	
	// Test read_port with buffer too small (should still work, just truncate)
	port_id p4 = create_port(5, "truncate_test");
	char large_msg[100];
	memset(large_msg, 'A', sizeof(large_msg));
	write_port(p4, 999, large_msg, sizeof(large_msg));
	
	char small_buf[10];
	ssize_t bytes = read_port(p4, &code, small_buf, sizeof(small_buf));
	dprintf("porttest (%s): read_port() with small buffer returned %ld bytes (truncated from %zu)\n",
			(bytes == sizeof(small_buf)) ? "pass" : "FAIL", (long)bytes, sizeof(large_msg));
	
	delete_port(p);
	delete_port(p2);
	delete_port(p4);
}

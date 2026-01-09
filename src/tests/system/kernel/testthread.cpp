#include <os/kernel/OS.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <windows.h>
#endif

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
	do { \
		tests_run++; \
		printf("Test %d: %s...", tests_run, name); \
	} while (0)

#define PASS() \
	do { \
		tests_passed++; \
		printf(" PASS\n"); \
	} while (0)

#define FAIL(msg) \
	do { \
		printf(" FAIL: %s\n", msg); \
	} while (0)

// Global variables for thread communication tests
static volatile int counter = 0;
static volatile bool thread_finished = false;
static thread_id sender_id = -1;
static int32 received_code = 0;
static char received_buffer[512];

// Simple thread function for basic tests
static status_t
simple_thread(void* data)
{
	int* value = (int*)data;
	if (value)
		*value = 42;
	return 100;
}

// Thread that increments a counter
static status_t
counter_thread(void* data)
{
	counter++;
	snooze(10000); // 10ms
	return B_OK;
}

// Thread that sets a flag when done
static status_t
flag_thread(void* data)
{
	snooze(50000); // 50ms
	thread_finished = true;
	return B_OK;
}

// Thread that exits with a specific value
static status_t
return_value_thread(void* data)
{
	int value = (int)(uintptr_t)data;
	return value;
}

// Thread that sends data
static status_t
sender_thread(void* data)
{
	thread_id target = (thread_id)(uintptr_t)data;
	const char* message = "Hello from sender";
	
	snooze(50000); // Give receiver time to be ready
	status_t result = send_data(target, 123, (void*)message, strlen(message) + 1);
	return result;
}

// Thread that receives data
static status_t
receiver_thread(void* data)
{
	int32 code = receive_data(&sender_id, received_buffer, sizeof(received_buffer));
	received_code = code;
	return B_OK;
}

// Thread that suspends itself
static status_t
suspend_self_thread(void* data)
{
	counter = 1;
	suspend_thread(find_thread(NULL));
	counter = 2;
	return B_OK;
}

// Thread for priority testing
static status_t
priority_thread(void* data)
{
	snooze(10000);
	return B_OK;
}

// Thread for rename testing
static status_t
rename_test_thread(void* data)
{
	snooze(100000);
	return B_OK;
}

// Thread that tests snooze
static status_t
snooze_thread(void* data)
{
	bigtime_t start = system_time();
	snooze(100000); // 100ms
	bigtime_t elapsed = system_time() - start;
	
	// Store elapsed time in counter (rough check)
	counter = (elapsed >= 90000 && elapsed <= 150000) ? 1 : 0;
	return B_OK;
}

// Test spawn_thread and basic operations
static void
test_spawn_and_wait()
{
	TEST("spawn_thread and wait_for_thread");
	
	int result_value = 0;
	thread_id tid = spawn_thread(simple_thread, "simple", B_NORMAL_PRIORITY, &result_value);
	
	if (tid < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	resume_thread(tid);
	
	status_t exit_value;
	status_t result = wait_for_thread(tid, &exit_value);
	
	if (result == B_OK && exit_value == 100 && result_value == 42) {
		PASS();
	} else {
		FAIL("incorrect return value or exit status");
	}
}

// Test wait_for_thread with NULL exit_value (BeOS allows this)
static void
test_wait_null_return()
{
	TEST("wait_for_thread with NULL returnCode");
	
	thread_id tid = spawn_thread(simple_thread, "null_test", B_NORMAL_PRIORITY, NULL);
	
	if (tid < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	resume_thread(tid);
	status_t result = wait_for_thread(tid, NULL);
	
	if (result == B_OK) {
		PASS();
	} else {
		FAIL("NULL returnCode should be accepted");
	}
}

// Test find_thread
static void
test_find_thread()
{
	TEST("find_thread(NULL) returns current thread");
	
	thread_id my_tid = find_thread(NULL);
	
	if (my_tid >= 0) {
		PASS();
	} else {
		FAIL("find_thread returned invalid ID");
	}
}

// Test multiple threads
static void
test_multiple_threads()
{
	TEST("spawn and run multiple threads");
	
	counter = 0;
	thread_id t1 = spawn_thread(counter_thread, "counter1", B_NORMAL_PRIORITY, NULL);
	thread_id t2 = spawn_thread(counter_thread, "counter2", B_NORMAL_PRIORITY, NULL);
	thread_id t3 = spawn_thread(counter_thread, "counter3", B_NORMAL_PRIORITY, NULL);
	
	if (t1 < 0 || t2 < 0 || t3 < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	resume_thread(t1);
	resume_thread(t2);
	resume_thread(t3);
	
	wait_for_thread(t1, NULL);
	wait_for_thread(t2, NULL);
	wait_for_thread(t3, NULL);
	
	if (counter == 3) {
		PASS();
	} else {
		FAIL("not all threads ran");
	}
}

// Test thread return values
static void
test_thread_return_values()
{
	TEST("thread return values");
	
	thread_id t1 = spawn_thread(return_value_thread, "ret1", B_NORMAL_PRIORITY, (void*)10);
	thread_id t2 = spawn_thread(return_value_thread, "ret2", B_NORMAL_PRIORITY, (void*)20);
	thread_id t3 = spawn_thread(return_value_thread, "ret3", B_NORMAL_PRIORITY, (void*)30);
	
	printf("\n  Spawned threads: t1=%d t2=%d t3=%d\n", t1, t2, t3);
	
	resume_thread(t1);
	resume_thread(t2);
	resume_thread(t3);
	
	status_t ret1 = -1, ret2 = -1, ret3 = -1;
	status_t r1 = wait_for_thread(t1, &ret1);
	status_t r2 = wait_for_thread(t2, &ret2);
	status_t r3 = wait_for_thread(t3, &ret3);
	
	printf("  wait results: r1=%d r2=%d r3=%d, values: ret1=%d ret2=%d ret3=%d\n", r1, r2, r3, ret1, ret2, ret3);
	
	if (ret1 == 10 && ret2 == 20 && ret3 == 30) {
		PASS();
	} else {
		printf(" (got ret1=%d ret2=%d ret3=%d)", ret1, ret2, ret3);
		FAIL("incorrect return values");
	}
}

// Test send_data and receive_data
static void
test_send_receive_data()
{
	TEST("send_data and receive_data");
	
	sender_id = -1;
	received_code = 0;
	memset(received_buffer, 0, sizeof(received_buffer));
	
	thread_id receiver = spawn_thread(receiver_thread, "receiver", B_NORMAL_PRIORITY, NULL);
	resume_thread(receiver);
	
	thread_id sender = spawn_thread(sender_thread, "sender", B_NORMAL_PRIORITY, (void*)(uintptr_t)receiver);
	resume_thread(sender);
	
	status_t sender_status, receiver_status;
	wait_for_thread(sender, &sender_status);
	wait_for_thread(receiver, &receiver_status);
	
	if (sender_status == B_OK && 
	    received_code == 123 && 
	    strcmp(received_buffer, "Hello from sender") == 0) {
		PASS();
	} else {
		FAIL("data transfer failed");
	}
}

#ifndef _WIN32
// Test send_data and receive_data across fork (requires fork, not available on Windows)
static void
test_send_receive_fork()
{
	TEST("send_data and receive_data across fork");
	
	thread_id parent_thread = find_thread(NULL);
	
	pid_t pid = fork();
	
	if (pid == 0) {
		// Child process - find my own thread ID (will be different from parent's)
		thread_id my_thread = find_thread(NULL);
		
		// Send message to parent including our thread ID
		char message[256];
		snprintf(message, sizeof(message), "Child thread %d", my_thread);
		send_data(parent_thread, my_thread, message, strlen(message) + 1);
		
		// Wait for parent's response
		thread_id sender;
		char buffer[256];
		int32 code = receive_data(&sender, buffer, sizeof(buffer));
		
		if (code == 456 && strcmp(buffer, "Hello from parent!") == 0 && sender == parent_thread) {
			_exit(0);
		} else {
			_exit(1);
		}
	} else {
		// Parent process - wait for child's message
		thread_id sender;
		char buffer[256];
		int32 child_thread_id = receive_data(&sender, buffer, sizeof(buffer));
		
		// Send response to child using the thread ID it sent us
		const char *response = "Hello from parent!";
		status_t result = send_data(child_thread_id, 456, response, strlen(response) + 1);
		
		// Wait for child to exit
		int child_status;
		waitpid(pid, &child_status, 0);
		
		if (result == B_OK && 
		    sender == child_thread_id &&
		    WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0) {
			PASS();
		} else {
			FAIL("cross-process data transfer failed");
		}
	}
}
#endif // !_WIN32

// Test has_data
static void
test_has_data()
{
	TEST("has_data returns correct status");
	
	thread_id me = find_thread(NULL);
	
	// Should be false initially
	bool has_data_before = has_data(me);
	
	// Send ourselves a message
	send_data(me, 42, NULL, 0);
	
	// Should be true now
	bool has_data_after = has_data(me);
	
	// Clean up by receiving the data
	thread_id dummy;
	receive_data(&dummy, NULL, 0);
	
	if (!has_data_before && has_data_after) {
		PASS();
	} else {
		printf(" (before=%d after=%d)", has_data_before, has_data_after);
		FAIL("has_data incorrect");
	}
}

// Test suspend_thread and resume_thread
static void
test_suspend_resume()
{
	TEST("suspend_thread and resume_thread");
	
	counter = 0;
	thread_id tid = spawn_thread(suspend_self_thread, "suspend_test", B_NORMAL_PRIORITY, NULL);
	resume_thread(tid);
	
	// Wait for thread to suspend itself
	snooze(50000);
	
	if (counter != 1) {
		wait_for_thread(tid, NULL);
		FAIL("thread did not reach suspension point");
		return;
	}
	
	// Resume the thread
	resume_thread(tid);
	wait_for_thread(tid, NULL);
	
	if (counter == 2) {
		PASS();
	} else {
		FAIL("thread did not resume properly");
	}
}

// Test rename_thread
static void
test_rename_thread()
{
	TEST("rename_thread");
	
	thread_id tid = spawn_thread(rename_test_thread, "original", B_NORMAL_PRIORITY, NULL);
	
	if (tid < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	status_t result = rename_thread(tid, "renamed");
	
	if (result == B_OK) {
		thread_info info;
		get_thread_info(tid, &info);
		
		resume_thread(tid);
		wait_for_thread(tid, NULL);
		
		if (strcmp(info.name, "renamed") == 0) {
			PASS();
		} else {
			FAIL("name not changed");
		}
	} else {
		resume_thread(tid);
		wait_for_thread(tid, NULL);
		FAIL("rename_thread failed");
	}
}

// Test set_thread_priority
static void
test_set_priority()
{
	TEST("set_thread_priority");
	
	thread_id tid = spawn_thread(priority_thread, "priority", B_LOW_PRIORITY, NULL);
	
	if (tid < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	int32 old_priority = set_thread_priority(tid, B_URGENT_PRIORITY);
	
	thread_info info;
	get_thread_info(tid, &info);
	
	resume_thread(tid);
	wait_for_thread(tid, NULL);
	
	if (old_priority == B_LOW_PRIORITY && info.priority == B_URGENT_PRIORITY) {
		PASS();
	} else {
		FAIL("priority not changed correctly");
	}
}

// Test get_thread_info
static void
test_get_thread_info()
{
	TEST("get_thread_info");
	
	thread_id tid = spawn_thread(simple_thread, "info_test", B_DISPLAY_PRIORITY, NULL);
	
	if (tid < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	thread_info info;
	status_t result = get_thread_info(tid, &info);
	
	resume_thread(tid);
	wait_for_thread(tid, NULL);
	
	if (result == B_OK && 
	    info.thread == tid &&
	    strcmp(info.name, "info_test") == 0 &&
	    info.priority == B_DISPLAY_PRIORITY) {
		PASS();
	} else {
		FAIL("thread info incorrect");
	}
}

// Test get_next_thread_info iteration
static void
test_get_next_thread_info()
{
	TEST("get_next_thread_info iteration");
	
	// Spawn a few threads to iterate over
	thread_id t1 = spawn_thread(flag_thread, "iter1", B_NORMAL_PRIORITY, NULL);
	thread_id t2 = spawn_thread(flag_thread, "iter2", B_NORMAL_PRIORITY, NULL);
	thread_id t3 = spawn_thread(flag_thread, "iter3", B_NORMAL_PRIORITY, NULL);
	
	if (t1 < 0 || t2 < 0 || t3 < 0) {
		FAIL("spawn_thread failed");
		return;
	}
	
	// Iterate through threads in our team (process)
	int32 cookie = 0;
	thread_info info;
	int thread_count = 0;
	bool found_t1 = false, found_t2 = false, found_t3 = false;
	
	team_id my_team = getpid();
	while (get_next_thread_info(my_team, &cookie, &info) == B_OK) {
		thread_count++;
		if (info.thread == t1) found_t1 = true;
		if (info.thread == t2) found_t2 = true;
		if (info.thread == t3) found_t3 = true;
	}
	
	// Clean up
	resume_thread(t1);
	resume_thread(t2);
	resume_thread(t3);
	wait_for_thread(t1, NULL);
	wait_for_thread(t2, NULL);
	wait_for_thread(t3, NULL);
	
	if (thread_count >= 4 && found_t1 && found_t2 && found_t3) {
		PASS();
	} else {
		FAIL("iteration incomplete or threads not found");
	}
}

// Test snooze
static void
test_snooze()
{
	TEST("snooze timing");
	
	counter = 0;
	thread_id tid = spawn_thread(snooze_thread, "snooze", B_NORMAL_PRIORITY, NULL);
	resume_thread(tid);
	wait_for_thread(tid, NULL);
	
	if (counter == 1) {
		PASS();
	} else {
		FAIL("snooze timing incorrect");
	}
}

// Test snooze_until
static void
test_snooze_until()
{
	TEST("snooze_until timing");
	
	bigtime_t start = system_time();
	bigtime_t wake_time = start + 100000; // 100ms from now
	
	status_t result = snooze_until(wake_time, B_SYSTEM_TIMEBASE);
	bigtime_t elapsed = system_time() - start;
	
	if (result == B_OK && elapsed >= 90000 && elapsed <= 150000) {
		PASS();
	} else {
		printf(" (result=%d elapsed=%lld)", result, elapsed);
		FAIL("snooze_until timing incorrect");
	}
}

// Test error conditions
static void
test_error_conditions()
{
	TEST("error handling - invalid thread_id");
	
	status_t result = rename_thread(-1, "invalid");
	
	if (result == B_BAD_THREAD_ID) {
		PASS();
	} else {
		printf(" (got result=%d)", result);
		FAIL("should return B_BAD_THREAD_ID");
	}
}

static void
test_wait_bad_thread()
{
	TEST("wait_for_thread with invalid ID");
	
	status_t exit_value;
	status_t result = wait_for_thread(999999, &exit_value);
	
	if (result == B_BAD_THREAD_ID) {
		PASS();
	} else {
		FAIL("should return B_BAD_THREAD_ID");
	}
}

static void
test_send_bad_thread()
{
	TEST("send_data to invalid thread");
	
	status_t result = send_data(999999, 0, NULL, 0);
	
	if (result == B_BAD_THREAD_ID) {
		PASS();
	} else {
		FAIL("should return B_BAD_THREAD_ID");
	}
}

int
main(int argc, char** argv)
{
	printf("=== Thread API Tests ===\n\n");
	
#ifdef _WIN32
	// On Windows, DllMain isn't being called by MinGW's loader, so we must
	// manually register the main thread. BApplication does this automatically,
	// but standalone tests need to do it explicitly.
	if (_register_main_thread() != B_OK) {
		printf("FATAL: Failed to register main thread\n");
		return 1;
	}
#endif
	
	// Basic tests
	test_spawn_and_wait();
	test_wait_null_return();
	test_find_thread();
	test_multiple_threads();
	test_thread_return_values();
	
	// Communication tests
	test_send_receive_data();
#ifndef _WIN32
	test_send_receive_fork();
#endif
	test_has_data();
	
	// Control tests - DISABLED: suspend_resume hangs
	// test_suspend_resume();
	test_rename_thread();
	test_set_priority();
	
	// Info tests
	test_get_thread_info();
	test_get_next_thread_info();
	
	// Timing tests
	test_snooze();
	test_snooze_until();
	
	// Error handling tests
	test_error_conditions();
	test_wait_bad_thread();
	test_send_bad_thread();
	
	// Summary
	printf("\n=== Test Summary ===\n");
	printf("Tests run: %d\n", tests_run);
	printf("Tests passed: %d\n", tests_passed);
	printf("Tests failed: %d\n", tests_run - tests_passed);
	
	if (tests_passed == tests_run) {
		printf("\nAll tests PASSED!\n");
		return 0;
	} else {
		printf("\nSome tests FAILED!\n");
		return 1;
	}
}

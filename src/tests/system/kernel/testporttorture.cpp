// Standard Includes -----------------------------------------------------------
#include <stdio.h>
#include <string.h>

// System Includes -------------------------------------------------------------
#include <OS.h>

// Local Defines ---------------------------------------------------------------
#define dprintf printf
#define test_iterations 5000

static int sFailures = 0;

static const char*
ResultString(bool success)
{
	if (!success)
		++sFailures;

	return success ? "pass" : "FAIL";
}

static void port_test();


int main()
{
	port_test();
	printf("porttest: failures=%d\n", sFailures);
	return sFailures == 0 ? 0 : 1;
}


static int32 port_test_thread_func(void *arg);

port_id test_p3;

void port_test()
{
	char testdata[1515];
	thread_id t;
	status_t status;

	for (int x=0; x < 1514; x++) {
		testdata[x] = 50 + (x % 28);
	}
	testdata[1514] = 0;
	//strcpy(testdata, "abcd");

	dprintf("porttest: begin test\n");

	/* Create ports */

	test_p3 = create_port(140,  "test port");

	dprintf("porttest (%s):'test port' has id %d\n",
			ResultString(test_p3 >= 0), test_p3);

	if (test_p3 < 0)
		return;

	/* Manipulate ports */

	dprintf("porttest (pass): spawning thread\n");
	t = spawn_thread(port_test_thread_func, "port_test", B_NORMAL_PRIORITY, NULL);
	resume_thread(t);

	for (int y = 0; y < test_iterations; y++) {
		status = write_port(test_p3, 1, &testdata, sizeof(testdata));
		dprintf("porttest (%s): write_port() %d returned %d\n",
				ResultString(status == 0), y, status);

		if (status < 0)
			return;
	}

	status_t ret;
	status_t err = wait_for_thread(t, &ret);
	printf("Thread returned a status of %d, err of %d\n", ret, err);

	dprintf("porttest: end test main thread\n");
}


static int32
port_test_thread_func(void *arg)
{
	int32 msg_code;
	char buf[1515];
	buf[1514] = '\0';
	status_t status;
	int err;

	dprintf("porttest: enter port_test_thread_func()\n");

	for (int z = 0; z < test_iterations; z++) {
		status = read_port(test_p3, &msg_code, &buf, 1514);
		err = errno;
		dprintf("porttest (%s): read_port() on %d, code %d, returned %d\n",
			ResultString(status >= 0), z, msg_code, status);
		if (status < 0)
			dprintf("errno = %d\n", err);	
	}

	status = close_port(test_p3);
	dprintf("porttest (%s): close_port() on 2 returned %d\n",
			ResultString(status == 0), status);

	status = delete_port(test_p3);
	dprintf("porttest (%s): delete_port() on 2 returned %d\n",
			ResultString(status == 0), status);

	dprintf("porttest: leave port_test_thread_func()\n");

	return 0;
}

#include "../common.h"

#include <Application.h>
#include <View.h>
#include <Window.h>
#include <WindowStack.h>


class WindowStackUnitTest : public TestCase {
public:
	void TestMembership();
};


void
WindowStackUnitTest::TestMembership()
{
	BApplication app("application/x-vnd.cosmoe-windowstack-test");
	BWindow* first = new BWindow(BRect(0, 0, 100, 100), "first",
		B_TITLED_WINDOW, 0);
	BWindow* second = new BWindow(BRect(0, 0, 100, 100), "second",
		B_TITLED_WINDOW, 0);
	BWindow* third = new BWindow(BRect(0, 0, 100, 100), "third",
		B_TITLED_WINDOW, 0);
	BWindow* fourth = new BWindow(BRect(0, 0, 100, 100), "fourth",
		B_TITLED_WINDOW, 0);

	BWindowStack stack(first);
	BWindowStack secondStack(second);
	BWindowStack thirdStack(third);
	BWindowStack invalid(NULL);
	BMessenger messenger;
	CPPUNIT_ASSERT_EQUAL((int32)0, invalid.CountWindows());
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_VALUE, invalid.AddWindow(second));
	CPPUNIT_ASSERT_EQUAL((int32)1, stack.CountWindows());
	CPPUNIT_ASSERT(stack.HasWindow(first));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.WindowAt(0, messenger));
	CPPUNIT_ASSERT(messenger == BMessenger(first));
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_INDEX, stack.WindowAt(-1, messenger));
	CPPUNIT_ASSERT(messenger == BMessenger(first));
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_INDEX, stack.AddWindowAt(second, 2));
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_VALUE, stack.AddWindow(BMessenger()));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK,
		stack.AddWindow(BMessenger(NULL, second)));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.AddWindowAt(third, 1));
	CPPUNIT_ASSERT_EQUAL((int32)3, secondStack.CountWindows());
	CPPUNIT_ASSERT(secondStack.HasWindow(BMessenger(NULL, first)));
	CPPUNIT_ASSERT_EQUAL((status_t)B_NAME_IN_USE, stack.AddWindow(second));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.WindowAt(1, messenger));
	CPPUNIT_ASSERT(messenger == BMessenger(third));

	BView* view = new BView(BRect(0, 0, 10, 10), "view", B_FOLLOW_NONE, 0);
	first->AddChild(view);
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_VALUE, stack.AddWindow(BMessenger(view)));

	BWindowStack fourthStack(fourth);
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, fourthStack.AddWindow(second));
	CPPUNIT_ASSERT_EQUAL((int32)2, stack.CountWindows());
	CPPUNIT_ASSERT_EQUAL((int32)2, secondStack.CountWindows());
	CPPUNIT_ASSERT(!stack.HasWindow(second));
	CPPUNIT_ASSERT(secondStack.HasWindow(fourth));
	CPPUNIT_ASSERT_EQUAL((status_t)B_ENTRY_NOT_FOUND, stack.RemoveWindow(second));
	CPPUNIT_ASSERT_EQUAL((status_t)B_BAD_INDEX, stack.RemoveWindowAt(2));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.RemoveWindowAt(1, &messenger));
	CPPUNIT_ASSERT(messenger == BMessenger(third));
	CPPUNIT_ASSERT_EQUAL((int32)1, thirdStack.CountWindows());
	CPPUNIT_ASSERT_EQUAL((int32)1, stack.CountWindows());

	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.AddWindow(third));
	CPPUNIT_ASSERT_EQUAL((status_t)B_OK, stack.RemoveWindow(first));
	CPPUNIT_ASSERT_EQUAL((int32)1, stack.CountWindows());
	CPPUNIT_ASSERT_EQUAL((int32)1, thirdStack.CountWindows());
	{
		BWindowStack temporary(first);
		CPPUNIT_ASSERT_EQUAL((status_t)B_OK, temporary.AddWindow(third));
	}
	CPPUNIT_ASSERT_EQUAL((int32)2, stack.CountWindows());
	delete third;
	CPPUNIT_ASSERT_EQUAL((int32)1, stack.CountWindows());
	CPPUNIT_ASSERT_EQUAL((int32)0, thirdStack.CountWindows());
	delete fourth;
	CPPUNIT_ASSERT_EQUAL((int32)1, secondStack.CountWindows());
	delete second;
	delete first;
}


CppUnit::Test*
WindowStackTestSuite()
{
	CppUnit::TestSuite* suite = new CppUnit::TestSuite();
	suite->addTest(new CppUnit::TestCaller<WindowStackUnitTest>(
		"Membership", &WindowStackUnitTest::TestMembership));
	return suite;
}
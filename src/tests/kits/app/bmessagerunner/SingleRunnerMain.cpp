// Standalone runner for BMessageRunner tests.
// This executable runs individual test cases in a separate process to avoid
// cross-test global state interference.

#include <stdio.h>
#include <string>

#include "BMessageRunnerTester.h"
#include "MessageRunnerTestHelpers.h"

int main(int argc, char** argv)
{
    std::string test = argc > 1 ? argv[1] : "A1";
    printf("Standalone MessageRunner test runner: running test %s\n", test.c_str());
    fflush(stdout);

    try {
        TBMessageRunnerTester tester;
        if (test == "A1") {
            tester.BMessageRunnerA1();
        } else if (test == "A2") {
            tester.BMessageRunnerA2();
        } else if (test == "A3") {
            tester.BMessageRunnerA3();
        } else if (test == "A4") {
            tester.BMessageRunnerA4();
        } else if (test == "A5") {
            tester.BMessageRunnerA5();
        } else if (test == "A6") {
            tester.BMessageRunnerA6();
        } else if (test == "A7") {
            tester.BMessageRunnerA7();
        } else if (test == "A8") {
            tester.BMessageRunnerA8();
        } else if (test == "B1") {
            tester.BMessageRunnerB1();
        } else if (test == "B2") {
            tester.BMessageRunnerB2();
        } else if (test == "B3") {
            tester.BMessageRunnerB3();
        } else if (test == "B4") {
            tester.BMessageRunnerB4();
        } else if (test == "B5") {
            tester.BMessageRunnerB5();
        } else if (test == "B6") {
            tester.BMessageRunnerB6();
        } else if (test == "B7") {
            tester.BMessageRunnerB7();
        } else if (test == "B8") {
            tester.BMessageRunnerB8();
        } else if (test == "B9") {
            tester.BMessageRunnerB9();
        } else {
            printf("Unknown test '%s'\n", test.c_str());
            return 1;
        }
    } catch (...) {
        printf("Unhandled exception in test runner\n");
        return 1;
    }

    printf("Test %s completed\n", test.c_str());
    return 0;
}

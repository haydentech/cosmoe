Usage Examples:
```bash
./run_unit_tests.sh                  # Run all tests
./run_unit_tests.sh -l               # List all available tests
./run_unit_tests.sh Storage          # Run all storage suite tests
./run_unit_tests.sh Storage::BPath   # Run only tests from the Storage suite whose names start with 'BPath' (e.g. 'BPath::Init Test')
```

Run standalone units tests that exercise the Cosmoe low-level routines:
```bash
./run_kernel_test.sh
```
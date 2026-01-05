// Include this file immediately before main() to provide Cosmoe support for macOS

#ifdef __APPLE__
extern "C" int mac_main(int argc, char** argv);

// On macOS, main calls NSApplicationMain, which then calls cosmoe_main
int main(int argc, char **argv)
{
	return mac_main(argc, argv);
}

#define main cosmoe_main
#endif

extern "C" 

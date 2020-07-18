#include "netconfapplication.h"
#include "loguru/debug.h"

#include <unistd.h>

int main(int argc, char *argv[])
{
	loguru::g_stderr_verbosity = 3;
	char *var = getenv("DEBUG");
	if (var)
		loguru::g_stderr_verbosity = std::stoi(var);

	if (std::string(argv[0]) == "ulhwcli" || std::string(argv[0]) == "./ulhwcli")
		return NetconfApplication::instance().initClient(argc, argv);

	gWarn("Initing server: %d", NetconfApplication::instance().initServer());
	return NetconfApplication::instance().startEventLoop();
}

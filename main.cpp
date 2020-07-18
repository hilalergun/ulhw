#include "netconfapplication.h"
#include "loguru/debug.h"
#include "utils.h"

#include <unistd.h>

int main(int argc, char *argv[])
{
	loguru::g_stderr_verbosity = 3;
	char *var = getenv("DEBUG");
	if (var)
		loguru::g_stderr_verbosity = std::stoi(var);

	auto v = split(std::string(argv[0]), '/', false);
	if (v.back() == "ulhwcli")
		return NetconfApplication::instance().initClient(argc, argv);

	gWarn("Initing server: %d", NetconfApplication::instance().initServer());
	return NetconfApplication::instance().startEventLoop();
}

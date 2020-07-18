#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <libyang/libyang.h>

static inline void dumpNode(const lys_module *m)
{
	char *strp;
	lys_print_mem(&strp, m, LYS_OUT_JSON, 0, 0, 0);
	if (strp) {
		gWarn("%s", strp);
		delete strp;
	}
}

static inline void dumpNode(lyd_node *rpc)
{
	char *strp;
	lyd_print_mem(&strp, rpc, LYD_XML, 0);
	if (strp) {
		gWarn("%s", strp);
		delete strp;
	}
}

static inline char * nodeToString(lyd_node *rpc)
{
	char *strp;
	lyd_print_mem(&strp, rpc, LYD_XML, 0);
	return strp;
}


static inline std::vector<std::string>
split(const std::string &s, char delimiter, bool keepEmpty = true)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter)) {
		if (keepEmpty || token.size())
			tokens.push_back(token);
	}
	return tokens;
}


#endif // UTILS_H

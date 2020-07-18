#ifndef NETCONFAPPLICATION_H
#define NETCONFAPPLICATION_H

class NetconfApplicationPriv;

struct ly_ctx;
struct lyd_node;
struct nc_session;
struct nc_server_reply;

#include <string>

class NetconfApplication
{
public:
	static NetconfApplication & instance();

	int initServer();
	int initClient(int argc, char *argv[]);

	int startEventLoop();

	/* rpc interface */
	nc_server_reply * get(lyd_node *rpc, nc_session *);
	nc_server_reply * edit(lyd_node *rpc, nc_session *);

protected:
	NetconfApplication();
	~NetconfApplication();
	NetconfApplication(const NetconfApplication &) = delete;
	void operator=(const NetconfApplication &) = delete;

	void installRPCCallback(std::string rpc, void *callback);

	NetconfApplicationPriv *p;
};

#endif // NETCONFAPPLICATION_H

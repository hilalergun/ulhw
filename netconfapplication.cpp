#include "netconfapplication.h"
#include "loguru/debug.h"
#include "utils.h"
#include "ulakdatastore.h"

#include <libyang/libyang.h>
#include <libyang/tree_schema.h>
#include <libnetconf2/netconf.h>
#include <libnetconf2/messages_server.h>
#include <libnetconf2/session_server.h>
#include <libnetconf2/session_client.h>

#include <errno.h>

#include <thread>

static inline int passwd_auth_clb(const struct nc_session *, const char *password, void *)
{
	gLog("Password check");
	if (std::string(password) != "ulak")
		return -1;
	return 0;
}

static inline int load_hostkey(const char *name, void *, char **privkey_path, char **, int *)
{
	gLog("hostkey load request");
	if (std::string(name) == "key_rsa") {
		*privkey_path = strdup("/run/user/1000/id_rsa");
		return 0;
	}
	return -ENOENT;
}

static void serverThread()
{
	nc_session *session = nullptr;
	nc_pollsession *ps = nc_ps_new();
	gLog("Starting");
	while (1) {
		if (!session) {
			NC_MSG_TYPE msgtype;
			msgtype = nc_accept(-1, &session);
			if (msgtype == NC_MSG_WOULDBLOCK) {
				continue;
			}
			if (msgtype == NC_MSG_HELLO) {
				int ret = nc_ps_add_session(ps, session);
				gWarn("Accepted a new session '%d', polling", ret);
			}
		}
		int ret = nc_ps_poll(ps, 100, &session);
		while (ret & NC_PSPOLL_TIMEOUT)
			ret = nc_ps_poll(ps, 100, &session);
		if (ret & NC_PSPOLL_SESSION_TERM) {
			nc_ps_del_session(ps, session);
			nc_session_free(session, nullptr);
			session = nullptr;
		}
		gLog("Poll response 0x%x", ret);
	}
	nc_ps_free(ps);
}

static nc_server_reply * op_get(lyd_node *rpc, nc_session *ncs)
{
	return NetconfApplication::instance().get(rpc, ncs);
}

static nc_server_reply * op_get_config(lyd_node *rpc, nc_session *ncs)
{
	/* there is little difference between get and get-config */
	return NetconfApplication::instance().get(rpc, ncs);
}

static nc_server_reply * op_edit_config(lyd_node *rpc, nc_session *ncs)
{
	return NetconfApplication::instance().edit(rpc, ncs);
}

static nc_server_reply * rpc_unsupported(struct lyd_node *, struct nc_session *)
{
	auto e = nc_err(NC_ERR_OP_FAILED, NC_ERR_TYPE_APP);
	nc_err_set_msg(e, "Unsupported RPC", "en");
	return nc_server_reply_err(e);
}

class NetconfApplicationPriv
{
public:
	ly_ctx *ctx = nullptr;
	const lys_module *m = nullptr;
	std::thread sthread;
	UlakDataStore *store;
};

NetconfApplication &NetconfApplication::instance()
{
	static NetconfApplication a;
	return a;
}

int NetconfApplication::initServer()
{
	p->store = new UlakDataStore();
	p->ctx = ly_ctx_new("/usr/share/yuma/modules/ietf", 0);
	if (!p->ctx)
		return -ENOMEM;
	auto ietfnc = ly_ctx_load_module(p->ctx, "ietf-netconf", nullptr);
	ly_ctx_load_module(p->ctx, "ietf-netconf-monitoring", nullptr);
	ly_ctx_load_module(p->ctx, "ietf-netconf-with-defaults", nullptr);
	p->m = lys_parse_path(p->ctx, "/run/user/1000/netas.yang"
										  , LYS_IN_YANG);
	if (!p->m)
		return -EINVAL;
	lys_features_enable(ietfnc, "*");

	int err = nc_server_init((ly_ctx *)p->ctx);
	if (err)
		return err;

	installRPCCallback("/ietf-netconf:get", (void *)op_get);
	installRPCCallback("/ietf-netconf:get-config", (void *)op_get_config);
	installRPCCallback("/ietf-netconf:edit-config", (void *)op_edit_config);
	nc_set_global_rpc_clb(rpc_unsupported);

	nc_server_set_capab_withdefaults(NC_WD_EXPLICIT, NC_WD_ALL | NC_WD_ALL_TAG | NC_WD_TRIM | NC_WD_EXPLICIT);
	nc_server_add_endpt("ssh", NC_TI_LIBSSH);
	nc_server_endpt_set_address("ssh", "0.0.0.0");
	nc_server_endpt_set_port("ssh", 12345);
	nc_server_ssh_endpt_add_hostkey("ssh", "key_rsa", -1);
	nc_server_ssh_set_hostkey_clb(load_hostkey, nullptr, nullptr);
	nc_server_ssh_endpt_set_auth_methods("ssh", NC_SSH_AUTH_PASSWORD);
	nc_server_ssh_set_passwd_auth_clb(passwd_auth_clb, NULL, NULL);

	return 0;
}

static char *client_password_cb(const char *, const char *, void *)
{
	return strdup("ulak");
}

int NetconfApplication::initClient(int argc, char *argv[])
{
	if (argc < 3)
		return -EINVAL;

	auto *ctx = ly_ctx_new("/usr/share/yuma/modules/ietf", 0);
	if (!ctx)
		return -ENOMEM;
	const lys_module *ietfnc = ly_ctx_load_module(ctx, "ietf-netconf", nullptr);
	ly_ctx_load_module(ctx, "ietf-netconf-monitoring", nullptr);
	ly_ctx_load_module(ctx, "ietf-netconf-with-defaults", nullptr);
	auto *m = lys_parse_path(ctx, "/run/user/1000/netas.yang"
										  , LYS_IN_YANG);
	if (!m)
		return -EINVAL;
	lys_features_enable(ietfnc, "*");

	nc_client_init();
	nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PASSWORD, 100);
	nc_client_ssh_set_auth_pref(NC_SSH_AUTH_INTERACTIVE, -1);
	nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PUBLICKEY, -1);
	nc_client_ssh_set_username("ulak");
	nc_client_ssh_set_auth_password_clb(client_password_cb, nullptr);
	nc_session *ses = nc_connect_ssh(argv[1], 12345, ctx);
	if (!ses) {
		gWarn("Error connecting to server");
		return -EINVAL;
	}

	std::string action(argv[2]);
	nc_rpc *rpc = nullptr;

	std::vector<std::pair<std::string, std::string>> configs;
	for (int i = 3; i < argc; i++) {
		std::string val(argv[i]);
		auto fields = split(val, '=', true);
		if (fields.size() == 1)
			fields.push_back("");
		configs.push_back(std::pair<std::string, std::string>(fields[0], fields[1]));
	}

	if (action == "get") {
		rpc = nc_rpc_get(nullptr, NC_WD_ALL, NC_PARAMTYPE_CONST);
	} else if (action == "get-config") {
		rpc = nc_rpc_getconfig(NC_DATASTORE_RUNNING, nullptr, NC_WD_ALL, NC_PARAMTYPE_CONST);
	} else if (action == "edit-config") {
		std::string user_types;
		auto *node3 = lyd_new_path(nullptr, ctx, "/netas:device",
								   nullptr, LYD_ANYDATA_DATATREE, 0);
		for (auto p: configs) {
			if (!lyd_new_leaf(node3, nullptr, p.first.data(), p.second.data())) {
				user_types += ":" + p.first + "=" + p.second;
			}
		}
		if (user_types.length())
			lyd_new_leaf(node3, nullptr, "user_types", user_types.data());
		rpc = nc_rpc_edit(NC_DATASTORE_RUNNING, NC_RPC_EDIT_DFLTOP_REPLACE,
						  NC_RPC_EDIT_TESTOPT_UNKNOWN, NC_RPC_EDIT_ERROPT_CONTINUE,
						  nodeToString(node3), NC_PARAMTYPE_FREE);
		lyd_free_withsiblings(node3);
	}
	if (!rpc) {
		gWarn("Error creating rpc object, unsupported action provided");
		return -ENOMEM;
	}
	uint64_t rpcid = 0;
	int ret = nc_send_rpc(ses, rpc, -1, &rpcid);
	if (ret != NC_MSG_RPC) {
		gWarn("Error '%d' sending rpc", ret);
		return -EINVAL;
	}
	nc_reply *reply = nullptr;
	ret = nc_recv_reply(ses, rpc, rpcid, -1, 0, &reply);
	if (ret != NC_MSG_REPLY) {
		gWarn("RPC reply recv'ed with err '%d'", ret);
		return -EINVAL;
	}
	if (reply->type == NC_RPL_OK)
		return 0;
	if (reply->type != NC_RPL_DATA) {
		gWarn("Un-expected reply data %d", reply->type);
		return -EINVAL;
	}
	nc_reply_data *ncrd = (nc_reply_data *)reply;
	auto node = ncrd->data->child;
	while (node) {
		lyd_node_anydata *any = (lyd_node_anydata *)node;
		if (std::string(node->schema->name) == "user_types") {
			auto fields = split(any->value.str, ':', false);
			for (auto f: fields) {
				auto vals = split(f, '=', true);
				printf("%s: %s\n", vals[0].data(), vals[1].data());
			}
		} else
			printf("%s: %s\n", node->schema->name, any->value.str);
		node = node->next;
	}
	return 0;
}

int NetconfApplication::startEventLoop()
{
	if (!p->ctx) {
		gWarn("Server is not init'ed, cannot start event loop");
		return -EINVAL;
	}
	p->sthread = std::thread(serverThread);
	p->sthread.join();
	return 0;
}

nc_server_reply *NetconfApplication::get(lyd_node *rpc, nc_session *)
{
	auto *node3 = lyd_new_path(nullptr, p->ctx, "/netas:device",
							   nullptr, LYD_ANYDATA_DATATREE, 0);
	for (auto k: p->store->keys())
		lyd_new_leaf(node3, nullptr, k.data(), p->store->get(k).data());
	//lyd_new_leaf(node3, nullptr, "manuuid", p->store->get("manuuid").data());
	//lyd_new_leaf(node3, nullptr, "model", p->store->get("model").data());
	//lyd_new_leaf(node3, nullptr, "serial", p->store->get("serial").data());
	auto *root = lyd_dup(rpc, 0);
	lyd_new_output_anydata(root, NULL, "data", node3, LYD_ANYDATA_DATATREE);
	return nc_server_reply_data(root, NC_WD_ALL, NC_PARAMTYPE_FREE);
}

nc_server_reply *NetconfApplication::edit(lyd_node *rpc, nc_session *)
{
	ly_set *nodeset = lyd_find_path(rpc, "/ietf-netconf:edit-config/config");
	if (nodeset->number == 0) {
		auto e = nc_err(NC_ERR_OP_FAILED, NC_ERR_TYPE_APP);
		nc_err_set_msg(e, "Expected config is empty", "en");
		ly_set_free(nodeset);
		return nc_server_reply_err(e);
	}
	lyd_node_anydata *any = (lyd_node_anydata *)nodeset->set.d[0];
	auto root = lyd_parse_xml(p->ctx, &any->value.xml, LYD_OPT_CONFIG | LYD_OPT_DESTRUCT | LYD_OPT_STRICT);
	auto node = root->child;
	while (node) {
		lyd_node_anydata *any = (lyd_node_anydata *)node;
		gLog("%s: %s", node->schema->name, any->value.str);
		if (std::string(node->schema->name) != "user_types")
			p->store->set(node->schema->name, any->value.str);
		else
			p->store->merge(node->schema->name, any->value.str);
		node = node->next;
	}
	p->store->sync();
	ly_set_free(nodeset);
	lyd_free_withsiblings(root);
	return nc_server_reply_ok();
}

NetconfApplication::NetconfApplication()
{
	p = new NetconfApplicationPriv;
}

NetconfApplication::~NetconfApplication()
{
	delete p;
}

void NetconfApplication::installRPCCallback(std::string rpc, void *callback)
{
	auto *node = ly_ctx_get_node(p->ctx, NULL, rpc.data(), 0);
	nc_set_rpc_callback(node, callback);
}

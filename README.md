# What is in this repository?

This repository contains software implementation of a NETCONF (RFC 6241) protocol. Both server and client are present in this repository.

Following features are supported:

* NETCONF over SSH (RFC 6242)
* get, get-config and edit-config NETCONF RPCs
* Only one datastore configuration is implemented
* JSON file backend support
* YANG data definition support

Following features of NETCONF are not supported:

* NETCONF over TLS (RFC 7589)
* NETCONF Call Home (RFC 8071)
* NETCONF event notifications (RFC 5277)
* Full datastore implementation as defined in the RFC 6241

# Implementation details

Both client and server are implemented C++11. libnetconf2 [1] and libyang [2] are used for NETCONF protocol parsing. libssh provides the underlying ssh transport. For datastore implementation, I developed my own file-backend instead of using a full RFC compliant datastore implementation. JSON support is provided by an 3rd party opensource library [3], and loguru [4] is used for logging backend implementation.

Application is composed of 2 main classes:

1. NetconfApplication provides both client and server functionality.
2. UlakDataStore provides file backend as datastore.

# Building and running the project

Project is developed on Ubuntu 18.04 machine, and it should build on any decent recent Linux distribution.

For building this project you should have libyang and libnetconf2 installed in your system. If these are not present on your package manager, you find prebuilt binaries at [5]. For Ubuntu 18.04, necessary files are present in the source repo and you can install these with the following command from the main repo directory:

```bash
> make install_debs
```

Project uses cmake as build system which should be present in any of package managers available. For debian/ubuntu variants one can install it with:

```bash
> [sudo] apt-get install cmake
```

and then project can be built with following command:

``` bash
> make all
```

Now everything should be ready for running applications. You can start server with the following command:

```bash
> build/ulhw
```

Then client can be run from another shell:

```bash
> build/ulhwcli 127.0.0.1 get
```

will dump all the settings present at the moment. A new config option can be added with edit-config RPC of NETCONF:

```bash
> build/ulhwcli 127.0.0.1 edit-config manufacturer=ULAK location=ankara
```

A setting can be erased like:

```bash
> build/ulhwcli 127.0.0.1 edit-config -location=
```

Please note that you cannot delete builtin properties.
# References

* [1] https://github.com/CESNET/libnetconf2
* [2] https://github.com/CESNET/libyang
* [3] https://github.com/nlohmann/json
* [4] https://github.com/emilk/loguru
* [5] https://download.opensuse.org/repositories/home:/liberouter/xUbuntu_18.04/amd64/

#include "ulakdatastore.h"
#include "3rdparty/json.hpp"
#include "loguru/debug.h"
#include "utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>

using json = nlohmann::json;

static int readFile(const std::__cxx11::string &filename,
					unsigned char **buffer, size_t *len)
{
	FILE *fp = std::fopen(filename.data(), "rb");
	if (!fp)
		return -errno;
	std::fseek(fp, 0, SEEK_END);
	size_t length = std::ftell(fp);
	*buffer = new uint8_t[length];
	unsigned char *buf = *buffer;
	std::fseek(fp, 0, SEEK_SET);
	size_t off = 0;
	size_t left = length;
	do {
		size_t read = std::fread(buf + off, 1, left, fp);
		if (read <= 0)
			break;
		left -= read;
		off += read;
	} while (left > 0);
	if (off != length) {
		std::fclose(fp);
		return -errno;
	}
	gWarn("File length is %lld bytes", length);
	*len = length;
	std::fclose(fp);
	return 0;
}

UlakDataStore::UlakDataStore()
{
	storeData = loadFromFile("/run/user/1000/store.json");
	if (storeData.size() == 0) {
		gWarn("No settings found, loading defaults");
		storeData = createDefaults();
		saveToFile("/run/user/1000/store.json", storeData);
	}
}

UlakDataStore::~UlakDataStore()
{
	sync();
}

std::vector<std::string> UlakDataStore::keys()
{
	std::vector<std::string> l;
	auto j = json::parse(storeData);
	for (auto e: j.items())
		l.push_back(e.key());
	return l;
}

std::string UlakDataStore::get(std::string key)
{
	auto j = json::parse(storeData);
	auto v = j[key];
	if (v.is_null())
		return "";
	return v;
}

void UlakDataStore::set(std::string key, std::string value)
{
	auto j = json::parse(storeData);
	j[key] = value;
	storeData = j.dump();
}

void UlakDataStore::merge(std::string key, std::string value)
{
	auto fields = split(get(key), ':', false);
	std::unordered_map<std::string, std::string> ht;
	for (auto f: fields) {
		auto vals = split(f, '=', true);
		ht[vals[0]] = vals[1];
	}

	fields = split(value, ':', false);
	for (auto f: fields) {
		auto vals = split(f, '=', true);
		if (vals[0][0] == '-') {
			vals[0].erase(0, 1);
			ht.erase(vals[0]);
			continue;
		}
		ht[vals[0]] = vals[1];
		gWarn("%s %s", vals[0].data(), vals[1].data());
	}
	value = "";
	for (auto p: ht)
		value += ":" + p.first + "=" + p.second;
	set(key, value);
}

void UlakDataStore::sync()
{
	saveToFile("/run/user/1000/store.json", storeData);
}

std::string UlakDataStore::createDefaults()
{
	json j;
	j["manufacturer"] = "Netas";
	j["manuuid"] = "NNTM31";
	j["model"] = "ULAKMacroENB";
	j["serial"] = "0K7558";
	return j.dump();
}

std::string UlakDataStore::loadFromFile(std::string filename)
{
	unsigned char *buf;
	size_t len;
	int err = readFile(filename, &buf, &len);
	if (err) {
		gWarn("Error '%d' importing file '%s'", err, filename.data());
		return "";
	}

	std::string s((const char *)buf, len);
	delete buf;
	return s;
}

int UlakDataStore::saveToFile(std::string filename, std::string content)
{
	int flags = O_WRONLY | O_CREAT;
	int fd = open(filename.data(), flags, S_IWUSR | S_IRUSR);
	if (fd < 0)
		return -errno;
	const char *buffer = content.data();
	size_t left = content.size();
	size_t off = 0;
	while (left > 0) {
		ssize_t written = write(fd, buffer + off, left);
		if (written < 0)
			return -errno;
		left -= written;
		off += written;
	}
	close(fd);
	return 0;
}

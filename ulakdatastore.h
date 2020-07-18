#ifndef ULAKDATASTORE_H
#define ULAKDATASTORE_H

#include <string>
#include <vector>

class UlakDataStore
{
public:
	UlakDataStore();
	~UlakDataStore();

	std::vector<std::string> keys();
	std::string get(std::string key);
	void set(std::string key, std::string value);
	void sync();

protected:
	std::string createDefaults();
	std::string loadFromFile(std::string filename);
	int saveToFile(std::string filename, std::string content);

	std::string storeData;
};

#endif // ULAKDATASTORE_H

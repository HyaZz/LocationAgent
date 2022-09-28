#include <iostream>
#include <fstream>
#include "Config.h"


bool Config::loadConfig(const std::string& file_name)
{
	if (file_name.empty())
		return false;

	std::ifstream ifs(file_name);
	if (!ifs.is_open())
	{
		std::cout << "Open config file " << file_name << " failed." << std::endl;
		return false;
	}

	std::string line;
	while (getline(ifs, line))
	{
		if (!line.empty())
		{
			auto pos = line.find("=");
			if (pos == std::string::npos || line[0] == '#')
				continue;

			map_config.emplace(std::make_pair(line.substr(0, pos), line.substr(pos + 1, line.size())));
		}
	}

	ifs.close();
	return true;
}


std::string Config::getConfigStringItem(const std::string& item_name, std::string default_str)
{
	auto it = map_config.find(item_name);
	if ( it != map_config.end())
	{
		return it->second;
	}
	else
		return default_str;
}

int Config::getConfigIntItem(const std::string& item_name, int default_value)
{
	auto it = map_config.find(item_name);
	if (it != map_config.end())
	{
		return std::stoi(it->second);
	}
	else
		return default_value;
}

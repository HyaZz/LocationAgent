#pragma once
#include <string>
#include <unordered_map>


/** 配置类
  * 在文件中配置参数，形式如下:
  * server_port=8606
  * deephub_url=ws://localhost:8081/deephub/v1/ws/socket
  * ...
  */
class Config
{
	using Congfigs = std::unordered_map<std::string,std::string>;

public:
	bool loadConfig(const std::string& file_name);

	std::string getConfigStringItem(const std::string& item_name, std::string default_str = "");

	int getConfigIntItem(const std::string& item_name, int default_value = 0);

private:
	Congfigs map_config;
};

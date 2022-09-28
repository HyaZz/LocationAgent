#include <iostream>
#include <mutex>
#include "LocationAgent.h"
#include "DataDefine.h"

namespace Situation {
	LocationAgent agent_server;
	DataQueue<PostionData> pos_queue;
	std::mutex mtx;
	std::condition_variable cv;
	bool flag = false;
	std::string receive_message;
}



int main()
{
	auto ret = Situation::agent_server.Initialize();
	if (!ret) 
	{
		std::cout << "Initialize error:get more information via log."<< std::endl;
		return false;
	}
	
	ret = Situation::agent_server.startServer();
	if (!ret)
	{
		std::cout << "startServer error:get more information via log." << std::endl;
		return false;
	}

	Situation::agent_server.finalize();

	return true;
}


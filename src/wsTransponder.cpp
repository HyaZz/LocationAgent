#include <iostream>
#include <thread>
#include <mutex>

#include "wsTransponder.h"
#include "UDPAccepter.h"
#include "DataDefine.h"


IWebSocket::wsHandler wsTransponder::ws;


void wsTransponder::sendToDeephub()
{
	/// 回调函数，通知udpAccepter
	auto handle_message = [&](const std::string& message)
	{
		if (!message.empty()) { Situation::receive_message = message; }
	};

	/// 通过条件变量等待位置信息队列中有数据
	while (true)
	{
		std::unique_lock <std::mutex> lck(Situation::mtx);
		while (!Situation::flag)
		{
			Situation::cv.wait(lck);
		}

		ws->send(makeBody(Situation::pos_queue.front()));
		if (ws->getState() != IWebSocket::States::CLOSED)
		{
			ws->poll();
			ws->dispatch(handle_message);
		}
		Situation::pos_queue.pop();
		Situation::flag = false;
	}
}


bool wsTransponder::startServer(const std::string& url)
{
	ws = m_websocket->connect(url);
	if (NULL == ws)
	{
		std::cout << "establish websocket connection error," << std::endl;
		std::cout << "so the agent will become a UDP server, which can't transpond the postion data." << std::endl;
		std::cout << "please check the deephub service or url." << std::endl;
		return false;
	}

	std::thread th1(&wsTransponder::sendToDeephub);
	th1.detach();

	return true;
}

/*
 *  按deephub接收的报文形式拼接json字符串
 *  后续引入json库来处理
 **/
std::string wsTransponder::makeBody(PostionData& postion)
{
	auto front = R"(
		{
			"event": "message",
				"topic" : "location_updates",
				"subscription_id" : 1252,
				"payload" : [
			{
				"position": {
					"type": "Point",
						"coordinates" : [
							)";

	auto back = R"(
						]
				},
					"source": "dcd1fb23-9c7b-42af-984a-b733e24b1846",
								"provider_type" : "uwb",
								"provider_id" : "ming1368"
			}
				]
		})";

	return (front + std::to_string(postion.pos.x) + "," + std::to_string(postion.pos.y) + back);
}


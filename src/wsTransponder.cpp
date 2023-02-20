#include <iostream>
#include <thread>
#include <mutex>

#include "wsTransponder.h"
#include "UDPAccepter.h"
#include "DataDefine.h"
#include "LocationAgent.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"


namespace Situation {
	extern LocationAgent agent_server;
}


void wsTransponder::loadProvider()
{
	/// to do what you want to load

}

void wsTransponder::sendToDeephub()
{
	/// 通过条件变量等待位置信息队列中有数据
	while (true)
	{
		if (NULL != ws)
		{
			std::unique_lock <std::mutex> lck(Situation::mtx);
			while (!Situation::flag)
			{
				Situation::cv.wait(lck);
			}

			ws->send(makeJsonMessageBody(Situation::pos_queue.front()));
			if (ws->getState() != IWebSocket::States::CLOSED)
			{
				ws->poll();
				ws->dispatch(UDPAccepter::handle_message);

				Situation::pos_queue.pop();
				Situation::flag = false;
			}
			else
			{
				delete ws;
				ws = NULL;
				ws = IWebSocket::connect(Situation::agent_server.cfg.getConfigStringItem("deephub_url"));
				if (NULL == ws)
				{
					break;
				}
			}
		}
		else
		{
			ws = IWebSocket::connect(Situation::agent_server.cfg.getConfigStringItem("deephub_url"));
		}

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

	std::thread th1(&wsTransponder::sendToDeephub,this);
	th1.detach();

	return true;
}

/*
 *  按deephub接收的报文形式组装json字符串
 *  形式如下：
 *  {
 *      "event": "message",
 *      "topic": "location_updates",
 *      "payload": [
 *      {
 *          "position": {
 *              "type": "Point",
 *              "coordinates": [
 *                  5,
 *                  4
 *              ]
 *          },
 *          "source": "fdb6df62-bce8-6c23-e342-80bd5c938774",
 *          "provider_type": "uwb",
 *          "provider_id": "77:4f:34:69:27:40"
 *      }
 *    ]
 *  }
 **/
std::string wsTransponder::makeJsonMessageBody(PostionData& postion)
{
	rapidjson::StringBuffer json_str;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> jRoot(json_str);

	jRoot.StartObject();
	jRoot.Key("event"); jRoot.String("message");
	jRoot.Key("topic"); jRoot.String("location_updates");

	jRoot.Key("payload");
	jRoot.StartArray();
	jRoot.StartObject();

	jRoot.Key("position");
	jRoot.StartObject();
	jRoot.Key("type"); jRoot.String("Point");
	jRoot.Key("coordinates");
	jRoot.StartArray();
	jRoot.Int(postion.pos.x);
	jRoot.Int(postion.pos.y);
	jRoot.EndArray();
	jRoot.EndObject();

	jRoot.Key("source"); jRoot.String("dcd1fb23-9c7b-42af-984a-b733e24b1846");
	jRoot.Key("provider_type"); jRoot.String("uwb");
	jRoot.Key("provider_id"); jRoot.String("ming1368");
	jRoot.EndObject();
	jRoot.EndArray();

	jRoot.EndObject();

	return json_str.GetString();
}


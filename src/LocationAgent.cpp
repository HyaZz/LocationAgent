#include <winsock.h>
#include <iostream>

#include "LocationAgent.h"
#include "UDPAccepter.h"
#include "wsTransponder.h"


int LocationAgent::Initialize()
{
	if (cfg.loadConfig("..\\LocationAgent\\config.cfg") == 0)
	{
		std::cout << "loadConfig error:" << GetLastError() << std::endl;
		return false;
	}

	/// 我们需要使用WSAStartup进行系统初始化；
	/// 为了避免资源泄漏，我们配对使用WSACleanup进行资源的回收
	WSADATA ws;
	if (WSAStartup(MAKEWORD(2, 2), &ws))
	{
		std::cout << "WSAStartup error:" << GetLastError() << std::endl;
		return false;
	}
	return true;
}


int LocationAgent::startServer()
{
	bool result = false;
	/// 启动websocket连接服务
	/// 如果启动失败，LocationAgent自动退化成UDP服务器
	wsTransponder ws_transponder;
	result = ws_transponder.startServer(cfg.getConfigStringItem("deephub_url"));

	/// 启动UDP报文接收服务
	UDPAccepter udp_accepter(cfg.getConfigIntItem("buff_size", 1024));
	result = udp_accepter.startServer(cfg.getConfigIntItem("port", 8088));

	return result;
}


void LocationAgent::finalize()
{
	//资源回收
	WSACleanup();
}

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

	/// ������Ҫʹ��WSAStartup����ϵͳ��ʼ����
	/// Ϊ�˱�����Դй©���������ʹ��WSACleanup������Դ�Ļ���
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
	/// ����websocket���ӷ���
	/// �������ʧ�ܣ�LocationAgent�Զ��˻���UDP������
	wsTransponder ws_transponder;
	result = ws_transponder.startServer(cfg.getConfigStringItem("deephub_url"));

	/// ����UDP���Ľ��շ���
	UDPAccepter udp_accepter(cfg.getConfigIntItem("buff_size", 1024));
	result = udp_accepter.startServer(cfg.getConfigIntItem("port", 8088));

	return result;
}


void LocationAgent::finalize()
{
	//��Դ����
	WSACleanup();
}

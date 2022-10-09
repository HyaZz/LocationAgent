#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>

#include "UDPAccepter.h"
#include "websocket.h"
#include "Config.h"
#include "DataDefine.h"

#pragma comment(lib, "ws2_32.lib")


std::string UDPAccepter::receive_message = {};

bool UDPAccepter::startServer(int port)
{
	/// 1.����UDP socket
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
	{
		std::cout << "socket  error:" << GetLastError() << std::endl;
		return false;
	}

	/// 2.�󶨵�ip�Ͷ˿�
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (bind(s, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		std::cout << "bind error:" << GetLastError() << std::endl;
		return false;
	}

	/// 3.��������
	int ret = 0;
	do
	{
		/// ���տͻ��˵�ַ
		sockaddr_in client_addr;
		memset(&client_addr, 0, sizeof(client_addr));

		int len = sizeof(SOCKADDR);//Ҫ��ֵ�ṹ��ĳ���

		/// ����ʽ����
		std::cout << "waiting for data>";
		ret = recvfrom(s, recv_buff, buff_size, 0, (SOCKADDR*)&client_addr, &len);
		std::cout << "recvfrom " << ret << ":\n" << recv_buff << std::endl;

		/// ��������������Ȼ�������Ϣ������
		PostionData postion;
		dataParser(recv_buff, postion);
		Situation::pos_queue.push(postion);

		/// ���������ȴ��߳�
		std::unique_lock <std::mutex> lck(Situation::mtx);
		Situation::flag = true;
		Situation::cv.notify_all();

		if(!receive_message.empty())
		{
			sendto(s, receive_message.c_str(), receive_message.size(), 0, (SOCKADDR*)&client_addr, len);
			memset(recv_buff, 0, buff_size);
			receive_message.clear();
		}

	} while (ret != SOCKET_ERROR && ret != 0);

	return true;
}


bool UDPAccepter::dataParser(const char* message, PostionData& postion)
{
	std::string msg(message,strlen(message));
	auto pos = msg.find(':');
	if (pos == std::string::npos)
	{
		std::cout << "[UDPAccepter::dataParser]bad message received." << std::endl;
		return false;
	}

	auto msg_type = msg.substr(0, pos);
	auto msg_info = msg.substr(pos + 1);

	/// trimһ����Ϣ
	msg_info.erase(0, msg_info.find_first_not_of(" "));
	msg_info.erase(msg_info.find_last_not_of(" ") + 1);

	///�з�λ����Ϣ
	std::vector<std::string> postions;
	UDPAccepter::splitTokenToList(msg_info, postions, ',');


	if (!msg_type.compare("display"))
	{
		///λ������ display:[LEN],[DEVID],[SEQ],[TIMESTAMP],[LAYID],[POS]
		postion.device_id = postions[1];
		postion.sequence = std::stoul(postions[2]);
		postion.lay_id = std::stoul(postions[4]);
		postion.pos.x = std::stoi(postions[5]);
		postion.pos.y = std::stoi(postions[6]);
		postion.pos.z = std::stoi(postions[7]);
		if(postions.size() == DISPLAY_MSG_FIELD_NUM)
			postion.group_id = std::stoul(postions[8]);
	}
	else if(!msg_type.compare("status2"))
	{
		///λ�òο����� status2:[LEN],[DEVID],[SEQ],[TIMESTAMP],[DEVID],[RANGE]
		postion.device_id = postions[1];
		postion.sequence = std::stoul(postions[2]);
		postion.base_station_id = postions[4];
		postion.range = std::stoul(postions[5]);
	}
	else if (!msg_type.compare("gpsposi"))
	{
		/// GPSλ������(����V3) gpsposi:[LEN],[DEVID],[SEQ],[TIMESTAMP],[LNG],[LAT]
		/// [LNG]Ϊ������ֵ����ֵΪ��������ֵΪ������[LAT]Ϊγ����ֵ����ֵΪ��γ����ֵΪ��γ��
		/// ���Ⱥ�γ����ֵ����ȷ��С�����7λ��
		postion.device_id = postions[1];
		postion.sequence = std::stoul(postions[2]);
		postion.longitude = std::stod(postions[4]);
		postion.latitude = std::stod(postions[5]);
	}
	else if (!msg_type.compare("status1"))
	{
		///�ߵ�����(����V3) status1:[LEN],IMU,[DEVID],[TIMESTAMP],[SEQ],[XACC],[YACC],[ZACC],[TEMP],[HUMI]
		postion.supplementary_type = postions[1];
		postion.device_id = postions[2];
		postion.sequence = std::stoul(postions[4]);
		postion.x_acc = std::stoi(postions[5]);
		postion.y_acc = std::stoi(postions[6]);
		postion.z_acc = std::stoi(postions[7]);
	}
	else
	{
		std::cout << "[UDPAccepter::dataParser]unknown message type." << std::endl;
		return false;
	}

	return true;
}


void UDPAccepter::splitTokenToList(std::string& raw_string, std::vector<std::string>& strings, char separator)
{
	if (raw_string.empty())
		return;

	std::string temp_str{};
	for (auto const& item : raw_string)
	{
		if (item != separator)
			temp_str += item;
		else
		{
			strings.emplace_back(temp_str);
			temp_str.clear();
		}
	}
	strings.emplace_back(temp_str);
}


void UDPAccepter::handle_message(const std::string& message)
{
	if (!message.empty()) { receive_message = message; }
}

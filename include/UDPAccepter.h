#pragma once
#include "DataDefine.h"

#define DISPLAY_MSG_FIELD_NUM 9
#define STATUS1_MSG_FIELD_NUM 6
#define GPSPOSI_MSG_FIELD_NUM 6
#define STATUS2_MSG_FIELD_NUM 10


/** 用于接收UDP协议报文数据的类
  */
class UDPAccepter
{
public:
	UDPAccepter(int buff_size_) :buff_size(buff_size_)
	{
		recv_buff = new char[buff_size_];
		if (NULL == recv_buff)
			return;
		memset(recv_buff, 0, buff_size);
	}

	~UDPAccepter()
	{
		if (NULL != recv_buff)
			delete recv_buff;
	}

public:
	bool startServer(int port);

	bool dataParser(const char* message, PostionData& postion);

	static void splitTokenToList(std::string& raw_string, std::vector<std::string>& strings, char separator);

private:
	char* recv_buff;
	uint32_t buff_size;
};


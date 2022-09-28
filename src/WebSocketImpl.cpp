#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#include "WebSocketImpl.h"
#include <iostream>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment( lib, "ws2_32" )



namespace {

	// https://www.rfc-editor.org/rfc/pdfrfc/rfc6455.txt.pdf  websocket协议文档
	//
	//  0                   1                   2                   3
	//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	// +-+-+-+-+-------+-+-------------+-------------------------------+
	// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
	// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
	// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
	// | |1|2|3|       |K|             |                               |
	// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	// |     Extended payload length continued, if payload len == 127  |
	// + - - - - - - - - - - - - - - - +-------------------------------+
	// |                               |Masking-key, if MASK set to 1  |
	// +-------------------------------+-------------------------------+
	// | Masking-key (continued)       |          Payload Data         |
	// +-------------------------------- - - - - - - - - - - - - - - - +
	// :                     Payload Data continued ...                :
	// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
	// |                     Payload Data continued ...                |
	// +---------------------------------------------------------------+

	class Websocket : public IWebSocket
	{
	public:

		/// websocket报文头
		struct WebSocketHeader
		{
			bool fin;
			enum OpCode
			{
				CONTINUATION_FRAME = 0x0,
				TEXT_FRAME = 0x1,
				BINARY_FRAME = 0x2,
				CLOSE = 0x8,
				PING = 0x9,
				PONG = 0xa,
			} opcode;
			bool mask;
			uint64_t  payload_length = 0;
			uint8_t mask_key[4] = {};
		};


		/// 发送缓存和接收缓存
		std::vector<uint8_t> transmit_buff;
		std::vector<uint8_t> receive_buff;

		Websocket(SOCKET sockhd, bool useMask = true) : sockhd(sockhd), state(States::OPEN), useMask(useMask) {
		}

		States getState() const { return state; }

		void poll(int timeout) override 
		{
			if (state == States::CLOSED)
			{
				if (timeout > 0)
				{
					timeval tv = { timeout / 1000,(timeout % 1000) * 1000 };
					select(0, NULL, NULL, NULL, &tv); //non-block
				}
				return;
			}

			/// 设置超时时间后，设置监听socket可读可写描述符
			if (timeout > 0)
			{
				fd_set rfds;
				fd_set wfds;
				timeval tv = { timeout / 1000,(timeout % 1000) * 1000 };
				FD_ZERO(&rfds);
				FD_ZERO(&wfds);
				FD_SET(sockhd,&rfds);
				if (transmit_buff.size()) { FD_SET(sockhd, &wfds); }
				select(sockhd + 1, &rfds, &wfds, 0, &tv);
			}

			while (true)
			{
				/// 初始时receive_buff可能为0
				int capacity_size = receive_buff.size();
				receive_buff.resize(capacity_size + EXTERNED_BUFF_SIZE);
 
				auto ret = recv(sockhd, (char*)&receive_buff[0] + capacity_size, EXTERNED_BUFF_SIZE, 0);
				auto may_error = WSAGetLastError();
				if (ret < 0 && (may_error == WSAEWOULDBLOCK || // 非阻塞套接字操作无法立即完成
					may_error == WSAEINPROGRESS))  // 某个阻塞操作正在执行
				{
					receive_buff.resize(capacity_size);
					break;
				}
				else if (ret <= 0)
				{
					receive_buff.resize(capacity_size);
					closesocket(sockhd);
					state = States::CLOSED;
					std::cout << (ret < 0 ? "connection error!" : "connection closed!") << std::endl;
					break;
				}
				else
					receive_buff.resize(capacity_size + ret);
			}

			while (transmit_buff.size())
			{
				int ret = ::send(sockhd, (char*)&transmit_buff[0], transmit_buff.size(), 0);
				auto may_error = WSAGetLastError();
				if (ret < 0 && (may_error == WSAEWOULDBLOCK || // 非阻塞套接字操作无法立即完成
					may_error == WSAEINPROGRESS))  // 某个阻塞操作正在执行
				{
					break;
				}
				else if (ret <= 0)
				{
					closesocket(sockhd);
					state = States::CLOSED;
					std::cout << (ret < 0 ? "connection error!" : "connection closed!") << std::endl;
					break;
				}
				else
					transmit_buff.clear();
			}

			if (!transmit_buff.size() && state == States::CLOSING)
			{
				closesocket(sockhd);
				state = States::CLOSED;
			}
		}


		virtual void _dispatch(Callback_Imp& callable) {
			struct CallbackAdapter : public BytesCallback_Imp
				// Adapt void(const std::string<uint8_t>&) to void(const std::string&)
			{
				Callback_Imp& callable;
				CallbackAdapter(Callback_Imp& callable) : callable(callable) { }
				void operator()(const std::vector<uint8_t>& message) {
					std::string stringMessage(message.begin(), message.end());
					callable(stringMessage);
				}
			};
			CallbackAdapter bytesCallback(callable);
			_dispatchBinary(bytesCallback);
		}

		
		virtual void _dispatchBinary(BytesCallback_Imp& callable)
		{
			while (true)
			{
				WebSocketHeader header;

				/// 根据websockt协议，头部至少也要有2个字节
				/// 但也可能有的websocket服务器收到发来的报文后没有给客户端回复任何消息
				if (receive_buff.size() < 2) { return; }

				uint8_t* data = (uint8_t*)receive_buff[0];
				header.fin = (data[0] & 0x80) == 0x80;
				header.opcode = static_cast<WebSocketHeader::OpCode>(data[0] & 0x0f);
				header.mask = (data[1] & 0x80) == 0x80;
				auto payload_len = static_cast<uint32_t>(data[1] & 0x7f);
				auto header_size = 2 + (header.mask ? 4 : 0);

				int mask_key_index = 0;
				if (payload_len < 0x7e)
				{
					header.payload_length = payload_len;
					mask_key_index = 2;
				} 
				else if(payload_len == 0x7e)
				{
					header.payload_length |= ((uint64_t)data[2]) << 8;
					header.payload_length |= ((uint64_t)data[3]) << 0;
					mask_key_index = 4;
					header_size += 2;
				}
				else if (payload_len == 0x7f)
				{
					header.payload_length |= ((uint64_t)data[2]) << 56;
					header.payload_length |= ((uint64_t)data[3]) << 48;
					header.payload_length |= ((uint64_t)data[4]) << 40;
					header.payload_length |= ((uint64_t)data[5]) << 32;
					header.payload_length |= ((uint64_t)data[6]) << 24;
					header.payload_length |= ((uint64_t)data[7]) << 16;
					header.payload_length |= ((uint64_t)data[8]) << 8;
					header.payload_length |= ((uint64_t)data[9]) << 0;
					mask_key_index = 10;
					header_size += 8;
				}

				/// 读取masking key
				if (header.mask)
				{
					header.mask_key[0] = ((uint8_t)data[mask_key_index + 0]) << 0;
					header.mask_key[1] = ((uint8_t)data[mask_key_index + 1]) << 0;
					header.mask_key[2] = ((uint8_t)data[mask_key_index + 2]) << 0;
					header.mask_key[3] = ((uint8_t)data[mask_key_index + 3]) << 0;
				}

				/// 校验下我们拿到的数据，然后开始处理
				if (receive_buff.size() < header_size + header.payload_length) { return; }

				if (header.opcode == WebSocketHeader::OpCode::TEXT_FRAME ||
					header.opcode == WebSocketHeader::OpCode::BINARY_FRAME ||
					header.opcode == WebSocketHeader::OpCode::CONTINUATION_FRAME)
				{
					/// 对payload做反掩码
					if (header.mask)
					{
						for (size_t i = 0; i != header.payload_length; ++i)
						{ 
							receive_buff[header_size + i] ^= header.mask_key[i%4];
						}
					}

					{
						/// 用临时变量做回调函数入参
						std::vector<uint8_t> recive_data;
						recive_data.insert(recive_data.end(), receive_buff.begin() + header_size, receive_buff.begin() + header_size + (size_t)header.payload_length);

						if (header.fin)
						{
							//callable(recive_data);
						}
					}
				}
				else if (header.opcode == WebSocketHeader::OpCode::PING)
				{
					if (header.mask)
					{ 
						for (size_t i = 0; i != header.payload_length; ++i)
						{
							receive_buff[i + header_size] ^= header.mask_key[i%4];
						}
					}

					std::string data(receive_buff.begin() + header_size, receive_buff.begin() + header_size + (size_t)header.payload_length);
					sendData(WebSocketHeader::OpCode::PONG, data.size(), data.begin(), data.end());
				}
				else if (header.opcode == WebSocketHeader::OpCode::PONG) { }
				else if (header.opcode == WebSocketHeader::OpCode::CLOSE) { close(); }
				else 
				{ 
					std::cout << "[dispatch]Got unexpected WebSocket message.\n";
					close();
				}

				receive_buff.clear();
			}
		}


		void send(const std::string& message) override
		{
			sendData(WebSocketHeader::TEXT_FRAME, message.size(), message.begin(), message.end());
		}


		template<typename Iterator>
		void sendData(WebSocketHeader::OpCode opcode, uint64_t message_size, Iterator msg_begin, Iterator msg_end)
		{
			// The masking key is a 32-bit value chosen at random by the client.
			const uint8_t masking_key[4] = { 0x12,0x34,0x56,0x78 };

			if (state == States::CLOSED || state == States::CLOSING) { return; }

			std::vector<uint8_t> header;

			/// 这里是根据websocket协议，分配存放websocket头部大小的计算方式
			/// 1.首先至少包含2个字节，即协议头中的前两个字节
			/// 2.如果消息长度小于等于126，则直接加4字节的masking-key
			/// 3.如果消息长度大于126，则再拓展2个字节，即表示payload长度的位数是7+16
			/// 4.如果消息长度不大于等于65536，则直接加4字节的masking-key
			/// 5.如果消息长度大于65536，则再拓展6个字节，即表示payload长度的位数是7+64，最后加4字节的masking-key
			header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);

			/// 虽然websocket协议允许客户端发多个frame，但是这里fin位总是为1,
			/// 因为基本上每个frame所携带的负载就够我们完成通信了
			header[0] = 0x80 | opcode; 

			//  8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			// +-+-------------+-------------------------------+
			// |M| Payload len |    Extended payload length    |
			// |A|     (7)     |             (16/64)           |
			// |S|             |   (if payload len==126/127)   |
			// |K|             |                               |
			// 处理第2个8位设置
			if (message_size < 126) // payload len 7
			{
				// 最高位直接加上mask位1,后边紧跟32位的Masking-key
				header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
				if (useMask)
				{
					header[2] = masking_key[0];
					header[3] = masking_key[1];
					header[4] = masking_key[2];
					header[5] = masking_key[3];
				}
			}
			else if (message_size < 65536) // payload len 16
			{
				/// 126 [0x0111 1110]
				/// 假设message_size是30000,即0x0111 0101 0011 0000
				header[1] = 126 | (useMask ? 0x80 : 0);
				header[2] = (message_size >> 8) & 0xff; //将数据长度写到扩展的2个字节中
				header[3] = (message_size >> 0) & 0xff;
				if (useMask)
				{
					header[4] = masking_key[0];
					header[5] = masking_key[1];
					header[6] = masking_key[2];
					header[7] = masking_key[3];
				}
			}
			else  // payload len 64 这种情况不常见，需要造数据测试
			{
				/// 127 [0x0111 1111]
				header[1] = 127 | (useMask ? 0x80 : 0);
				header[2] = (message_size >> 56) & 0xff; //将数据长度写到扩展的8个字节中
				header[3] = (message_size >> 48) & 0xff;
				header[4] = (message_size >> 40) & 0xff;
				header[5] = (message_size >> 32) & 0xff;
				header[6] = (message_size >> 24) & 0xff;
				header[7] = (message_size >> 16) & 0xff;
				header[8] = (message_size >> 8) & 0xff;
				header[9] = (message_size >> 0) & 0xff;
				if (useMask) {
					header[10] = masking_key[0];
					header[11] = masking_key[1];
					header[12] = masking_key[2];
					header[13] = masking_key[3];
				}
			}

			///填充header和message到发送端buff
			transmit_buff.insert(transmit_buff.end(), header.begin(), header.end());
			transmit_buff.insert(transmit_buff.end(), msg_begin, msg_end);

			/// 用掩码对数据载荷做反掩码
			if (useMask)
			{
				for (int i = 0; i != message_size; ++i)
				{
					/// 原始数据的第i字节与Masking-key第(i%4)字节后，得到真正发送数据的第i字节
					/// 取余的方式还可以是 i & 0x3
					/// *(transmit_buff.begin() - header.size() + i) ^= masking_key[i & 0x3]; 这样是不是也行
					*(transmit_buff.end() - (long)message_size + i) ^= masking_key[i%4];
				}
			}
		}


		void close()
		{
			if (state == States::CLOSING || state == States::CLOSED) { return; }
			state = States::CLOSING;
			uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
			std::vector<char> header(closeFrame, closeFrame + 6);
			//txbuf.insert(txbuf.end(), header.begin(), header.end());
		}

	private:
		SOCKET		sockhd;
		States		state;
		bool		useMask;
	};

	/** 通过调用getaddrinfo函数返回socket信息
	  * 关于addrinfo结构体，参考 https://blog.csdn.net/u011003120/article/details/78277133
	  */
	SOCKET getSocket(const std::string& host, int port)
	{
		SOCKET sockfd;
		struct addrinfo hints;
		struct addrinfo* result;
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_UNSPEC; //未指定
		hints.ai_socktype = SOCK_STREAM;

		int ret = 0;
		if ((ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result)) != 0)
		{
			std::cout << "[getaddrinfo]: " << gai_strerror(ret) << std::endl;
			return 1;
		}

		/// 由于一个域名可以对应多个IP地址，因此getaddrinfo返回的result是个addrinfo链表
		for (auto p = result; p != nullptr; p = p->ai_next)
		{
			sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sockfd == INVALID_SOCKET) continue;
			if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) break;
			///连接不成功，关闭socket
			closesocket(sockfd);
			sockfd = INVALID_SOCKET;
		}

		/// getaddrinfo成功获取到地址后，必须要对该部分内存进行释放
		freeaddrinfo(result);
		return sockfd;
	}

}

IWebSocket::wsHandler connectImpl(const std::string& url, const std::string& origin)
{
	char host[128];
	int port;
	char path[128];
	memset(host, 0, 128);
	memset(path, 0, 128);

	if (url.size() >= MAX_URL_LENGTH)
	{
		std::cout << "ERROR: url size limit exceeded: " << url << std::endl;
		return NULL;
	}

	if (origin.size() >= MAX_ORIGIN_LENGTH)
	{
		std::cout << "ERROR: url size limit exceeded: " << url << std::endl;
		return NULL;
	}

	/// 校验url合法性
	if (sscanf(url.c_str(), "ws://%[^:/]:%d/%s", host, &port, path) == 3) {
	}
	else if (sscanf(url.c_str(), "ws://%[^:/]/%s", host, path) == 2) {
		port = 8081;
	}
	else if (sscanf(url.c_str(), "ws://%[^:/]:%d", host, &port) == 2) {
		path[0] = '\0';
	}
	else if (sscanf(url.c_str(), "ws://%[^:/]", host) == 1) {
		port = 8081;
		path[0] = '\0';
	}
	else {
		std::cout << "ERROR: WebSocket url would be wrong: " << url << std::endl;
		return NULL;
	}


	SOCKET sock_handler = getSocket(host, port);
	if (sock_handler == INVALID_SOCKET)
	{
		std::cout << "[connectImpl]: Couldn't connect to " << url << std::endl;
		return NULL;
	}

	/** websocket握手
	  * 一般的格式如下：
	  * GET /chat HTTP/1.1
	  * Host: server.example.com
	  * Upgrade: websocket
	  * Connection: Upgrade
	  * Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
	  * Origin: http://somesite.com
	  * Sec-WebSocket-Protocol: chat, superchat
	  * Sec-WebSocket-Version: 13
	  * 
	  * 注意项包括:
	  * 请求行: 请求方法必须是GET, HTTP版本至少是1.1
	  * 请求必须含有Host
	  *	请求如果来自浏览器客户端, 必须包含Origin
	  *	请求必须含有Connection, 其值必须含有"Upgrade"记号
	  *	请求必须含有Upgrade, 其值必须含有"websocket"关键字
	  *	请求必须含有Sec-Websocket-Version, 其值必须是13
	  *	请求必须含有Sec-Websocket-Key, 用于提供基本的防护, 比如无意的连接
	  */
	char line[MAX_LINE_SIZE];
	memset(line, 0, MAX_LINE_SIZE);

	snprintf(line, MAX_LINE_SIZE, "GET /%s HTTP/1.1\r\n", path);
	::send(sock_handler, line, strlen(line), 0);

	snprintf(line, MAX_LINE_SIZE, "Host: %s:%d\r\n", host, port);
	::send(sock_handler, line, strlen(line), 0);

	snprintf(line, MAX_LINE_SIZE, "Upgrade: websocket\r\n");
	::send(sock_handler, line, strlen(line), 0);

	snprintf(line, MAX_LINE_SIZE, "Connection: Upgrade\r\n");
	::send(sock_handler, line, strlen(line), 0);

	if (!origin.empty()) {
		snprintf(line, MAX_LINE_SIZE, "Origin: %s\r\n", origin.c_str());
		::send(sock_handler, line, strlen(line), 0);
	}

	snprintf(line, MAX_LINE_SIZE, "Sec-WebSocket-Key: SSdtIGhvdSB5dQ==\r\n"); ::send(sock_handler, line, strlen(line), 0);
	snprintf(line, MAX_LINE_SIZE, "Sec-WebSocket-Version: 13\r\n"); ::send(sock_handler, line, strlen(line), 0);
	snprintf(line, MAX_LINE_SIZE, "\r\n"); ::send(sock_handler, line, strlen(line), 0);

	int i = 0;
	for (; i < 2 || (i < 255 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i)
	{
		auto recv_value = recv(sock_handler, line + i, 1, 0);
		if (recv_value == 0)
		{ 
			return NULL; 
		}
	}

	line[i] = 0;
	if (i == 255) 
	{
		std::cout << "[connectImpl]: Received an invalid status line connecting to: " << url << std::endl;
		return NULL;
	}


	/** 服务器回复握手一般格式:
	  * HTTP/1.1 101 Switching Protocols
	  *	Upgrade: websocket
	  *	Connection: Upgrade
	  *	Sec-WebSocket-Accept: qiJ7AOaWTgaZjodFT/glN86u9jQ=
	  * Sec-WebSocket-Protocol: chat
	  */
	int status;
	if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101)
	{
		std::cout << "[connectImpl]: Received a bad status connecting to: " << url << ":" << line << std::endl;
		return NULL;
	}

	while (true) 
	{
		for (i = 0; i < 2 || (i < 255 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i)
		{ 
			if (recv(sock_handler, line + i, 1, 0) == 0) { return NULL; }
		}
		if (line[0] == '\r' && line[1] == '\n') { break; } //最后一行
	}


	/// 在一些对时延要求较高的交互式操作环境中，所有的小分组必须尽快发送出去
	/// 有时候必须要关闭Nagle算法。该算法要求一个TCP连接上最多只能有一个未被确认的小分组。
	/// 在该分组的确认到达之前不能发送其他小分组
	int opt = 1;
	setsockopt(sock_handler, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

	u_long flag = 1;
	/// 控制套接字的I/O模式,flag为1时开启非阻塞模式
	if (ioctlsocket(sock_handler, FIONBIO, &flag) != 0)
	{
		closesocket(sock_handler);
		return NULL;
	}

	std::cout << "[connectImpl]: Now has connected to: " << url << std::endl;

	return IWebSocket::wsHandler(new Websocket(sock_handler));
}



IWebSocket::wsHandler IWebSocket::connect(const std::string& url, const std::string& origin)
{
	return connectImpl(url, origin);
}
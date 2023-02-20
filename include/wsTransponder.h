#pragma once
#include "WebSocketImpl.h"
#include "DataDefine.h"
#include <unordered_map>

struct LocationProvider {
	std::string provider_id;

	/// the technology of location postion
	enum class Provider_type
	{
		UWB,
		GPS,
		WIFI,
		RFID,
		IBEACON,
		VIRTUAL,
		UNKNOWN
	};

	std::string source_id;
	int floor;
};

using Providers = std::unordered_map<std::string, LocationProvider>;
using ProviderIDS = std::unordered_map<std::string, std::string>;

class wsTransponder {
public:
	bool startServer(const std::string& url);

	void loadProvider();

	std::string makeJsonMessageBody(PostionData& postion);

	void sendToDeephub();

public:
	IWebSocket* m_websocket;
	IWebSocket::wsHandler ws;
	Providers map_provider;
	ProviderIDS map_provider_ids;
};
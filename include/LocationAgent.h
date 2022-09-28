#pragma once
#include "Config.h"


class LocationAgent
{
public:

	int Initialize();

	int startServer();

	void finalize();

public:
	Config cfg;
	
};
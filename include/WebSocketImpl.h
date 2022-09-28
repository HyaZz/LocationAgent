#pragma once
#include <string>
#include <vector>

#define MAX_URL_LENGTH		128
#define MAX_ORIGIN_LENGTH	200
#define MAX_LINE_SIZE		256
#define EXTERNED_BUFF_SIZE	2048


struct Callback_Imp { virtual void operator()(const std::string& message) = 0; };
struct BytesCallback_Imp { virtual void operator()(const std::vector<uint8_t>& message) = 0; };

/// websockt�ӿ���
class IWebSocket
{
public:
	using wsHandler = IWebSocket*;
	using States = enum class States { OPEN, CONNECTING, CLOSING, CLOSED };

	/// ����websocket������
	static wsHandler connect(const std::string& url, const std::string& origin = std::string());

	virtual ~IWebSocket() { }
	virtual void poll(int timeout = 0) = 0;
	virtual void send(const std::string& message) = 0;
	virtual void close() = 0;
	virtual States getState() const = 0;

	/// ����һ��string���Ͳ����Ļص�������������c++11 lambda,std::function����c����ָ��
	template<class Callable>
	void dispatch(Callable callable)
	{
		struct _Callback : public Callback_Imp {
			Callable& callable;
			_Callback(Callable& callable) : callable(callable) { }
			void operator()(const std::string& message) { callable(message); }
		};
		_Callback callback(callable);
		_dispatch(callback);
	}

	/// ����һ��string���Ͳ����Ļص�����
	template<class Callable>
	void dispatchBinary(Callable callable)
	{
		struct _Callback : public BytesCallback_Imp {
			Callable& callable;
			_Callback(Callable& callable) : callable(callable) { }
			void operator()(const std::vector<uint8_t>& message) { callable(message); }
		};
		_Callback callback(callable);
		_dispatchBinary(callback);
	}

protected:
	virtual void _dispatch(Callback_Imp& callable) = 0;
	virtual void _dispatchBinary(BytesCallback_Imp& callable) = 0;
};



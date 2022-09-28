#pragma once
#include <string>
#include <queue>
#include <list>
#include <mutex>

struct ThreeDimensionalCoordinate
{
	int x	= 0;
	int y	= 0;
	int z	= 0;
};


struct PostionData
{
	/// 设备编号，通常是tag的id
	std::string device_id;

	/// 序列号，由位置解算引擎提供
	uint32_t    sequence;

	/// 层号
	uint32_t    lay_id;

	/// 3维坐标
	ThreeDimensionalCoordinate	pos;

	/// 组id
	uint32_t	group_id;

	/// 基站id，位置参考数据中用到
	std::string base_station_id;

	/// 距离基站的距离，单位cm 
	uint32_t	range	= 0;

	/// 经度数值，正值为东经，负值为西经
	double		longitude;

	/// 纬度数值，正值为北纬，负值为南纬
	double		latitude;

	/// 补充类型，惯导数据中用到
	std::string supplementary_type;

	/// X方向加速度、Y方向加速度、Z方向加速度
	int			x_acc		= 0;
	int			y_acc		= 0;
	int			z_acc		= 0;
};

using DataList = std::list<PostionData>;


template<typename T>
class DataQueue
{
public:
	void push(const T& item) { data_quque.push_back(item); }

	void pop() { data_quque.pop_front(); }

	T& front() { return data_quque.front(); }

	int size() const { return data_quque.size(); }

	bool empty() const { return data_quque.empty(); }

private:
	std::list<T> data_quque;
};

namespace Situation {
	extern DataQueue<PostionData> pos_queue;
	extern std::condition_variable cv;
	extern bool flag;
	extern std::mutex mtx;
	extern std::string receive_message;
}

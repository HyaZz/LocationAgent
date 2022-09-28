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
	/// �豸��ţ�ͨ����tag��id
	std::string device_id;

	/// ���кţ���λ�ý��������ṩ
	uint32_t    sequence;

	/// ���
	uint32_t    lay_id;

	/// 3ά����
	ThreeDimensionalCoordinate	pos;

	/// ��id
	uint32_t	group_id;

	/// ��վid��λ�òο��������õ�
	std::string base_station_id;

	/// �����վ�ľ��룬��λcm 
	uint32_t	range	= 0;

	/// ������ֵ����ֵΪ��������ֵΪ����
	double		longitude;

	/// γ����ֵ����ֵΪ��γ����ֵΪ��γ
	double		latitude;

	/// �������ͣ��ߵ��������õ�
	std::string supplementary_type;

	/// X������ٶȡ�Y������ٶȡ�Z������ٶ�
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

/************************************************
* ���ڲ������Ӿ��ȷֲ�������� 
************************************************/

#ifndef RANDOM_H
#define RANDOM_H

#include <cstdlib>
#include <cmath>

class Random {
public:
    // [0, 1) ֮��ķ��Ӿ��ȷֲ������ֵ
	// ���� n-1 �Ķ�������ȫΪ1 ʱ��0��1���ֵĸ��ʲ��Ǿ��ȵġ�
    static double uniform(double max = 1) {
        return ((double)std::rand() / (RAND_MAX))*max;
    }
};
#endif

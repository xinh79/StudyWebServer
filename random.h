/************************************************
* 用于产生服从均匀分布的随机数 
************************************************/

#ifndef RANDOM_H
#define RANDOM_H

#include <cstdlib>
#include <cmath>

class Random {
public:
    // [0, 1) 之间的服从均匀分布的随机值
	// 仅当 n-1 的二进制数全为1 时，0，1出现的概率才是均等的。
    static double uniform(double max = 1) {
        return ((double)std::rand() / (RAND_MAX))*max;
    }
};
#endif

/************************************************
* ���ڲ���locker�Ļ������� 
************************************************/
#if 0

#include <unistd.h>
#include <pthread.h>
#include <iostream>

#include "locker.h"
#include "random.h"

using namespace std;

//Ϊ�˷���������Ȱѹ�����Դ�ͻ���������һ���ṹ���� 
typedef struct
{
  int sharedSource; 
  locker mutex;
  sem l_sem;
  cond l_cond;
}Data;

void *pthread1(void *arg) {
	Data *data = (Data*)arg;
	while (1) {
		cout << "=========== pthread1 ===========" << endl;
		unsigned int num = (unsigned int)Random::uniform(10);
		cout << "num: " << num << endl; 
		data->mutex.lock();
		sleep(num);
		data->sharedSource++;
		cout << "data->sharedSource: " << data->sharedSource << endl;
		data->mutex.unlock();
		cout << "=========== pthread1 ===========" << endl;
	}
}

void *pthread2(void *arg) {
	sleep(1); 
	Data *data = (Data*)arg;
	while (1) {
		cout << "=========== pthread2 ===========" << endl;
		unsigned int num = (unsigned int)Random::uniform(10);
		cout << "num: " << num << endl; 
		data->mutex.lock();
		sleep(num);
		data->sharedSource--;
		cout << "data->sharedSource: " << data->sharedSource << endl;
		data->mutex.unlock();
//		sleep(5);
		cout << "=========== pthread2 ===========" << endl;
	}
}



int main()
{
	Data data;
	data.sharedSource = 0;
	pthread_t p1;
	pthread_t p2;
	
	pthread_create(&p1, NULL, pthread1, (void*)(&data));
	pthread_create(&p2, NULL, pthread2, (void*)(&data));
	
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
	
}
#endif

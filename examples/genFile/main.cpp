#include <stdlib.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iterator>
#define RUTA "DATA"
#define TYPE double
using namespace std;
int main(int argc, char **argv){
	vector<TYPE> v{};
	struct timespec seed;	
	if(argc!=2){
		cout << "./vfg <size>" << endl;
		exit(-1);
	}
	clock_gettime(CLOCK_REALTIME, &seed);
	int size = atoi(argv[1]);
	v.resize(size);
	srand((TYPE)seed.tv_nsec);	
	auto contador{0};
	for(auto& ii : v){
		//int signo = (rand()%2)?-1:1;
		//ii = rand()*signo;
		ii = contador;
		contador++;
	}
	ofstream archivo{RUTA, ios::out};
	archivo.write((char*)v.data(), v.size()*sizeof(TYPE));
	archivo.close();
	return 0;
}
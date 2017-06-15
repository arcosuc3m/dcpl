#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}
int suma (int a){
	return a+1;
}
class clasechunga
{
public:
	clasechunga(){};
	~clasechunga(){};
	int operator()(int a){
		return a+1;
	}
	
};


int main(int argc, char** argv){
	dcpl::inicializador aux(argc, argv);
	bool ben = argc != 1;
	dcpl::DistributedVector<double> v = ben?dcpl::DistributedVector<double>(dcpl::BENCHMARK):dcpl::DistributedVector<double> (dcpl::OPTIMIZED);

	dcpl::ifstream doubles("double.data");	
	dcpl::ifstream out("double.out");	
	auto start = chrono::system_clock::now();

	doubles.read(v, 1000000000);
	out.write(v,    1000000000);	


	auto end = chrono::system_clock::now();
	if(!ben)dcpl::cout<< std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;
	return EXIT_SUCCESS;
}

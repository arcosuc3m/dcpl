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
	doubles.read(v, 1000000000);
	auto endIt = v.begin();
	std::advance(endIt, 100000000-1);
	auto start = chrono::system_clock::now();

	dcpl::transform(v.begin(), endIt, v.begin(),[](double a){return a+1;});

	auto end = chrono::system_clock::now();
	dcpl::cout<<  std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;
	return EXIT_SUCCESS;
}

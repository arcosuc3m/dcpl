#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}
class clasechunga
{
public:
	clasechunga(){};
	~clasechunga(){};
	int operator()(int a, int b){
		return a+b;
	}
	
};


int main(int argc, char** argv){
	dcpl::inicializador aux{argc, argv};
	dcpl::DistributedVector<int> v{dcpl::OPTIMIZED};
	dcpl::cout << "HOLA" << endl;
	dcpl::ifstream stream{"DATA"};	
	stream.read(v, 10);
	dcpl::transform(v.begin(), v.end(), v.begin(), [](int& a){return 1+a;});
	for(auto ii:v){
		dcpl::cout << ii << endl;
	}
	auto last = v.begin();
	std::advance(last, 3);
	dcpl::cout << "MAIN: " << dcpl::reduce(v.begin(), v.end(), 10, clasechunga{}) << endl;
}
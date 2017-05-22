#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}



int main(int argc, char** argv){
	dcpl::inicializador aux{argc, argv};
	dcpl::DistributedVector<int> v{dcpl::OPTIMIZED};
	dcpl::ifstream stream{"DATA"};
	dcpl::ifstream stream2{"DATA2"};
	stream.read(v, 100000000);
	stream2.write(v, 100000000);	
	dcpl::cout << "HOLI" << endl;
	for(auto ii:v){
		dcpl::cout << ii << endl;
	}		
	

}
#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}



int main(int argc, char** argv){
	dcpl::inicializador aux{argc, argv};
	dcpl::DistributedVector<int> v{dcpl::BLOCK};
	dcpl::ifstream stream{"DATA"};	
	dcpl::cout << "FIN" << endl;
	auto a = 0;
	stream.read(v, 100000000);
	auto start = std::chrono::system_clock::now();
	for(auto ii = 0; ii < v.size(); ++ii){
		if(!(ii%100000)) dcpl::cout << "-" << endl;
		a = v.get(ii, 0);
	}
	auto end = std::chrono::system_clock::now();
	dcpl::cout << "CON GET: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;
	start = std::chrono::system_clock::now();
	a++;
	for(int ii = 0; ii < v.size(); ii++){
		a = v[ii];
	}
	end = std::chrono::system_clock::now();
	dcpl::cout << "CORCHETE: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;

	cout << "FIN: "<<dcpl::my_context.rank << endl;
	return 0;
}
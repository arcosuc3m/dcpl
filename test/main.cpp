#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}

using namespace std;

int main(int argc, char** argv){
	dcpl::inicializador aux{argc, argv};
	dcpl::DistributedVector<int> v{dcpl::ROBIN, 1};	
	dcpl::DistributedVector<int> v2{dcpl::BLOCK};	
	v.llenar("DATA");
	v2.llenar("DATA");
	dcpl::cout << "inicio transform" << endl;
	dcpl::transform(v.begin(), v.end(), v.begin(), [](int element){return 1+element;});
	dcpl::cout << "fin transform" << endl;
	int reducc = dcpl::reduce(v.begin(), v.end(), 100, [](int a, int b){return a+b;});
	int rank = 0;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
/*	for(auto ii:v){
		dcpl::cout << ii << endl;
	}*/
	if(rank == 0) cout << reducc << endl;
	return 0;
}
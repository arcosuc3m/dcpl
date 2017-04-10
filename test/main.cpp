#include <iostream>
#include <algorithm>
#include "dcpl.cpp"
using namespace std;
int main(){
	dcpl::DistributedVector<double> v{dcpl::ROBIN, 1};
	dcpl::DistributedVector<double> v2{dcpl::BLOCK};
	v.llenar("DATA");
	v2.llenar("DATA");
	int aux;

	dcpl::transform(v.begin(), v.end(), v2.begin(), [](double element){return 2*element;});
	std::vector<double> vlocal{};
	for(auto ii: v2){
		vlocal.push_back(ii);
	}
	MPI_Comm_rank(MPI_COMM_WORLD, &aux);
	if(aux == 0){
		for(auto ii: vlocal){
			cout << ii << endl;
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	
}
#include <iostream>
#include <algorithm>
#include "dcpl.cpp"
using namespace std;
int main(){
	dcpl::DistributedVector<int> v{dcpl::ROBIN, 1};	
	dcpl::DistributedVector<int> v2{dcpl::BLOCK};	
	v.llenar("DATA");	
	v2.llenar("DATA");	
	int aux;
	cout << "inicio transform" << endl;
	dcpl::transform(v.begin(), v.end(), v2.begin(), [](int element){return 2*element;});
	cout << "fin transform" << endl;
	std::vector<int> vlocal{};
	for(auto ii: v2){
		vlocal.push_back(ii);
	}
	MPI_Comm_rank(MPI_COMM_WORLD, &aux);
	if(aux == 0){
		cout << "====================" << endl;
		for(auto ii: vlocal){
			cout << ii << endl;
		}
	}
	cout << "FIN["<<aux<<"]" << endl;
	MPI_Barrier(MPI_COMM_WORLD);	
	return 0;	
}
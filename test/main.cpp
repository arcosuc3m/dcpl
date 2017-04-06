#include <iostream>
#include "dcpl.cpp"
using namespace std;
using namespace dcpl;
int main(){
	DistributedVector<double> v{ROBIN, 1};
	v.llenar("DATA");

	for(int ii = 0; ii < v.size()/2; ++ii){
		auto aux = v[ii];
		v[ii] = v[v.size()-(ii+1)];
		v[v.size()-(ii+1)] = aux;
	}

	for(int ii = 0; ii < v.size(); ++ii){
		cout << "v[" << ii << "]" << v[ii] << endl;
	}
	return 0;
}
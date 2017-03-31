#include <iostream>
#include "dcpl.cpp"
using namespace std;
using namespace dcpl;
int main(){
	DistributedVector<double> v{ROBIN, 1};
	v.llenar("DATA");
	v.write("DATA2");
	return 0;
}
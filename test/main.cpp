#include <iostream>
#include "dcpl.cpp"
using namespace std;
using namespace dcpl;
int main(){
	DistributedVector<int> v{ROBIN, 3};
	v.llenar("DATA");
	return 0;
}
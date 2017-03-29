#include <iostream>
#include "dcpl.cpp"
using namespace std;
using namespace dcpl;
int main(){
	DistributedVector<int> v{BLOCK};
	v.llenar("DATA");
	return 0;
}
#include <iostream>
#include <algorithm>
#include "dcpl.cpp"

int mymin (int a, int b){return (a<b)?a:b;}
int suma (int a){
	return a+1;
}
class clasechunga
{
public:
	clasechunga(){};
	~clasechunga(){};
	int operator()(int a){
		return a+1;
	}
	
};


int main(int argc, char** argv){
	dcpl::inicializador aux{argc, argv};
	dcpl::ifstream enteros("int.data");
	dcpl::ifstream doubles("double.data");

	std::vector<dcpl::DistributedVector<int>> VE{};

	std::vector<dcpl::DistributedVector<double>>VD{};

	std::vector<dcpl::ifstream> salidas{};
	for(int ii = 0; ii < 6; ii++){
		salidas.push_back(dcpl::ifstream{"salida,"s+to_string(ii)});
	}
	for(int ii = 0; ii < 3; ++ii){
		VE.push_back(dcpl::DistributedVector<int>{dcpl::OPTIMIZED});
		enteros.read(VE[ii], 100);
	}

	for(int ii = 0; ii < 3; ++ii){
		VD.push_back(dcpl::DistributedVector<double>{dcpl::OPTIMIZED});
		doubles.read(VD[ii], 100);
	}

	for(int ii = 0; ii < 3; ++ii){
		salidas[ii].write(VE[ii], 101);
	}

	for(int ii = 0; ii < 3; ++ii){
		salidas[ii+3].write(VD[ii], 101);
	}
	

	return EXIT_SUCCESS;
}

#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <numeric>
#include <chrono>

using namespace std;
using namespace chrono;

int main(int argc, char const *argv[])
{
	std::vector<double> v{};
	

	std::ifstream archivo{"double.data", std::ios::binary};
	auto tama = archivo.rdbuf()->in_avail()/sizeof(double);
	v.resize(tama);

	auto start = chrono::system_clock::now();
	archivo.read((char*) v.data(), tama);
	std::ofstream("double.out", std::ios::binary).write((char*) v.data(), v.size()*sizeof(double));
	auto end = chrono::system_clock::now();
	std::cout<< "IO: "<< std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;

	start = chrono::system_clock::now();
	std::transform(v.begin(), v.end(), v.begin(), [](double a){return a+1.0;});
	end = chrono::system_clock::now();

	std::cout<< "transform: "<< std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;

	start = chrono::system_clock::now();
	std::accumulate(v.begin(), v.end(), 0, [](double a, double b){return a+b;});
	end = chrono::system_clock::now();

	std::cout<< "reduce: "<< std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;

	start = chrono::system_clock::now();
	for(auto ii = v.begin(); ii != v.end(); ++ii){
		*ii++;
	}
	end = chrono::system_clock::now();

	std::cout<< "corchetes/get&set: "<< std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << endl;

	return 0;
}

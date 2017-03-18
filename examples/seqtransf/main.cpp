#include <iostream>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string.h>
#include <chrono>
//#include <>
using namespace std;
using namespace chrono;
int main(int argc, char** argv){
	if(argc < 2){
		cerr << "argumentos insuficientes" << endl;
		cerr << "USO: ./secuencial <ruta_al_archivo>" << endl;
		exit(-1);
	}
	ifstream archivo{argv[1], ios::binary};	//abrimos el archivo indicado en la ruta
	if(!archivo.is_open()){		// Si no lo hemos abierto,  error.
		cerr << "Error abriendo el archivo, puede que no exista." << endl;
		exit(0);
	}	
	int size_b = (*archivo.rdbuf()).in_avail();
	int size_elem = size_b/sizeof(int);
	vector<int> enteros{size_elem};
	enteros.resize(size_elem);
	time_point<system_clock> inicio, fin;
	inicio = system_clock::now();
	archivo.read((char*)(enteros.data()), size_b);
	fin = system_clock::now();
	duration<double> segundos = fin-inicio;
	cout << segundos.count() << endl;
	copy(enteros.begin(), enteros.end(), ostream_iterator<int>(cout, "\n"));	
}
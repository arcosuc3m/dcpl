#include <stdlib.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace std;
template <class TYPE>
void llenar_archivo(int elements, string ruta){
	vector<TYPE> v{};	
	v.resize(elements);	
	auto contador{0};
	for(auto& ii : v){
		ii = contador;
		contador++;
	}
	ofstream archivo{ruta, ios::out};
	archivo.write((char*)v.data(), v.size()*sizeof(TYPE));
	archivo.close();
}
int main(int argc, char **argv){
	if(argc == 2){
		llenar_archivo<int>(atoi(argv[1]), "int.data"s);
	}else if(argc == 3){
		llenar_archivo<double>(atoi(argv[1]), "double.data"s);
	}else{
		cout << "Mal argumento" << endl;
	}
	return EXIT_SUCCESS;
}
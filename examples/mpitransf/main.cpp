#include <iostream>
#include <unistd.h>
#include <vector>
#include "mpi.h"
using namespace std;


int num_procesos;


/**
	size es el tamaño del vector
	rank el rango del proceso
*/
int amount_per_process(int size, int rank){
	int elem_per_process {size/num_procesos};
	int elem_last_process{elem_per_process+size-(elem_per_process*num_procesos)};
	return (rank==num_procesos-1) ? elem_last_process:elem_per_process;	
}



int main() {
	//variables
	MPI_Init(NULL, NULL);
	auto rango = 0, displacement = 0;
	long long int file_size = 0;
	vector<int> buffer{};
	int blocklength;
	MPI_File file_descriptor;
	MPI_Datatype tipo_contiguo;
	MPI_Status status;
	//fin variables
	MPI_Comm_size(MPI_COMM_WORLD, &num_procesos);
	MPI_Comm_rank(MPI_COMM_WORLD, &rango);
	MPI_File_open(MPI_COMM_WORLD, "DATA", MPI_MODE_RDONLY, MPI_INFO_NULL, &file_descriptor);
	MPI_File_get_size(file_descriptor, &file_size);
	//vector_size = file_size/sizeof(int);
	blocklength = amount_per_process(file_size/sizeof(int), rango);
	displacement = (MPI_Aint)amount_per_process(file_size/sizeof(int), 0)*sizeof(int)*rango;
	//crear el tipo de datos hindexed (diferente por proceso)
	MPI_Type_contiguous(blocklength, MPI_INT, &tipo_contiguo);
	MPI_Type_commit(&tipo_contiguo);
	printf("[%d]: he creado un tipo contiguo con los datos\n\tlength: %d\n", rango, blocklength);
	usleep(rango*1000);
	//leer del fichero
	buffer.resize(blocklength);
	MPI_File_read_at(file_descriptor, displacement, buffer.data(), 1, tipo_contiguo, &status);
	cout << "==========="<< rango<<"==========="<< endl;
	for(const int& ii : buffer){
		cout << ii << endl;
	}
	MPI_Barrier(MPI_COMM_WORLD);
	return 0;
}

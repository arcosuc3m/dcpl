#include "mpi.h"
#include <vector>	//std::vector
#include <typeinfo>	//typeid
#include <iostream>	//iostream
#include <unistd.h>	//usleep
#include <chrono>	//usleep
using namespace std;

static int MPI_Comm_size_wrapper(){
	int aux;
	fflush(NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &aux);
	return aux;
}
static int MPI_Comm_rank_wrapper(){
	int aux;
	MPI_Comm_rank(MPI_COMM_WORLD, &aux);
	return aux;
}
namespace dcpl{
	/********TIPOS********/
	typedef char schedule_type;
	const schedule_type BLOCK = 0, ROBIN = 1;	//tipos de reparto

	typedef struct{
		int type = 0;		//splitting type (block, round robin...)
		int rr_param = 1;	//if the type is rr, chunk's size (ignored otherwise)

		int size = 0;		//size of the dataType which symbolizes the splitting in file (in elements of tipe double or int)
	} schedule_data; //datos del reparto

	int noop = MPI_Init(nullptr, nullptr); //TODO: preguntar como hacer esto con argc y argv
	/*********************/
	//vector
	template <typename T>
	class DistributedVector{
		private:
			/*****FIELDS*******/
			vector<T> contenido{}; 	//DATA
			schedule_data my_schedule{BLOCK, 0};//spliting schedule
			MPI_Datatype datatype;
			/*********************/

			static inline MPI_Datatype CHECK_TYPE(){
				return (std::is_same<int, T>::value )?(MPI_INT):MPI_DOUBLE;
			}
			/*				
				@param size -> size (in bytes) of the file U are reading.
			*/
			void build_datatype(int size){ //allow us to build a MPI datatype
				MPI_Datatype res;
				int num_proc{MPI_Comm_size_wrapper()};
				int rango{MPI_Comm_rank_wrapper()};
				int elements = size/sizeof(T);
				int amount_per_process = elements/num_proc;
				int blocklength, dis[1], last_length;
				int contador = 0;
				vector<int> blocklengths{};
				vector<MPI_Aint> displacements{};

			
				switch(my_schedule.type){
					case BLOCK:					
						//displacement = rank*elems for process 0
						dis[0] = rango*amount_per_process;						
						last_length = amount_per_process+elements%num_proc;
						blocklength = (rango != num_proc-1)?amount_per_process:last_length;
						MPI_Type_create_indexed_block(1, blocklength, dis, CHECK_TYPE(), &res);
						MPI_Type_commit(&res);
						this->my_schedule.size = blocklength;
						break;
					case ROBIN:
						auto init = chrono::system_clock::now();
						contador = rango*my_schedule.rr_param;
						//prueba rendimiento 200.000.000 rodajas = 3683 ms por proceso. (se supone paralelo)
						while(contador < elements){
							int length = (elements - contador < my_schedule.rr_param) ? (elements - contador) :( my_schedule.rr_param);
							my_schedule.size += length;
							blocklengths.push_back(length);
							displacements.push_back(contador*sizeof(T));
							contador += (my_schedule.rr_param*num_proc);							
						}
						auto fin = chrono::system_clock::now();
						if(rango == 0){
							cout << chrono::duration_cast<chrono::milliseconds>(fin-init).count() << endl;
						}
						MPI_Type_create_hindexed(blocklengths.size(), blocklengths.data(), displacements.data(), CHECK_TYPE(), &res);
						MPI_Type_commit(&res);						
						break;
				}
				this->datatype = res;
			}
		public:
			DistributedVector(schedule_type tipo){
				my_schedule.type = tipo;				
			};
			DistributedVector(schedule_type tipo, int rr_param){
				 this->my_schedule.type = tipo;
				 this->my_schedule.rr_param = rr_param;
			};
			/*
				@param path -> path to the file to read

			*/
			void llenar(const char* path){
				MPI_File file_descriptor;
				MPI_Offset file_size;					
				MPI_Status status;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &file_descriptor);
				MPI_File_get_size(file_descriptor, &file_size);
				build_datatype(file_size);
				contenido.resize(my_schedule.size);
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, datatype, "native", MPI_INFO_NULL);
				MPI_File_read(file_descriptor, contenido.data(), my_schedule.size, CHECK_TYPE(), &status);
				usleep(10000*MPI_Comm_rank_wrapper());
				cout << "=========" << MPI_Comm_rank_wrapper()<< "=========" << endl;
				for(auto ii : contenido){
					cout << ii << endl;
				}
				MPI_File_close(&file_descriptor);
				MPI_Barrier(MPI_COMM_WORLD);
			}
			void write(const char* path){
				MPI_File file_descriptor;
				MPI_Status status;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &file_descriptor);																
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, datatype, "native", MPI_INFO_NULL);
				MPI_File_write(file_descriptor, contenido.data(), my_schedule.size, CHECK_TYPE(), &status);				
			}
			~DistributedVector(){
				
			};	
	};

}


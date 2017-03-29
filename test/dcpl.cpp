#include "mpi.h"
#include <vector>	//std::vector
#include <typeinfo>	//typeid
#include <iostream>	//iostream
#include <unistd.h>	//usleep
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
		int type = 0;
		int rr_param = 1;
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
			MPI_Datatype TYPE = (typeid(T)==typeid(int))?(MPI_INT):MPI_DOUBLE;
			
			/*********************/

			/*
				@param schedule data -> specifies how the data is splitted 
				@param size -> size (in bytes) of the file U are reading.
			*/
			MPI_Datatype build_datatype(schedule_data re, int size){ //allow us to build a MPI datatype
				MPI_Datatype res;
				int num_proc{MPI_Comm_size_wrapper()};
				int rango{MPI_Comm_rank_wrapper()};
				int elements = size/sizeof(T);
				int amount_per_process = elements/num_proc;
				int blocklength;
				int dis[1], last_length;


				switch(re.type){
					case BLOCK:					
						//displacement = rank*elems for process 0
						dis[0] = rango*amount_per_process;						
						last_length = amount_per_process+elements%num_proc;
						blocklength = (rango != num_proc-1)?amount_per_process:last_length;
						MPI_Type_create_indexed_block(1, blocklength, dis, TYPE, &res);
						MPI_Type_commit(&res);
						break;
					case ROBIN:
						break;
				}
				return res;
			}
		public:
			DistributedVector(schedule_type tipo){
				my_schedule.type = tipo;
			};
			DistributedVector(schedule_type tipo, int rr_param){
			};
			/*
				@param path -> path to the file to read

			*/
			void llenar(const char* path){
				MPI_File file_descriptor;
				MPI_Datatype tipo;
				MPI_Offset file_size;
				MPI_Aint tipe_extent;
				MPI_Aint elebe;
				MPI_Status status;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &file_descriptor);
				MPI_File_get_size(file_descriptor, &file_size);
				tipo = build_datatype(this->my_schedule, file_size);
				MPI_Type_get_extent(tipo, &elebe, &tipe_extent);
				contenido.resize(tipe_extent/sizeof(T));
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, tipo, "native", MPI_INFO_NULL);
				MPI_File_read(file_descriptor, contenido.data(), tipe_extent, MPI_CHAR, &status);
				usleep(10000*MPI_Comm_rank_wrapper());
				cout << "=========" << MPI_Comm_rank_wrapper()<< "=========" << endl;
				for(auto ii : contenido){
					cout << ii << endl;
				}
				MPI_Barrier(MPI_COMM_WORLD);
			}
			~DistributedVector(){
				
			};	
	};

}


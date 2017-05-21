#include "mpi.h"
#include <vector>	//std::vector
#include <typeinfo>	//typeid
#include <iostream>	//iostream
#include <fstream>	//iostream
#include <unistd.h>	//usleep
#include <iterator>	//iterator tag
#include <chrono>	//medir tiempo
//#include <functional> //std::function
#include <string.h> //memcpy
using namespace std;

/*std::vector<int> repartir_inversamente(int total, vector<double> durations){
	return vector<int>{total};
}*/

namespace dcpl{
	/********TIPOS********/
	typedef char schedule_type;
	const schedule_type BLOCK = 0, ROBIN = 1, BENCHMARK = 2, OPTIMIZED = 3, AD_HOC=4;	//tipos de reparto
	const string PER_INFO_PATH = "./performance.info";

	typedef struct{
		int type = 0;		//splitting type (block, round robin...)
		int rr_param = 1;	//if the type is rr, chunk's size (ignored otherwise)
		std::vector<int> block_lengths{}; //is splitting type is optimized or ad-hoc, we store here how many elements has each process.

		int datatype_size = 0;		//size of the dataType which symbolizes the splitting in file (in elements of tipe double or int)
		int vector_size = 0;
	} schedule_data; //datos del reparto

	typedef struct{
		int rank = 0;
		int size = 0;		
	} MPI_context;
	MPI_context my_context;

	ostream *coutp = new ostream(nullptr);
	ostream& cout = *coutp;


	class inicializador{
		std::chrono::time_point<std::chrono::system_clock> start, end;
	public:
		inicializador(int argc, char** argv){
			start = chrono::system_clock::now();
			MPI_Init(&argc, &argv);
			MPI_Comm_rank(MPI_COMM_WORLD, &(my_context.rank));
			MPI_Comm_size(MPI_COMM_WORLD, &(my_context.size));
			if(my_context.rank == 0)memcpy(&cout, &std::cout, sizeof(std::cout));
		};
		~inicializador(){
			MPI_File fileper;
			//delete coutp;
			end = chrono::system_clock::now();
			//escribir el tiempo en un archivo: por ej. ./performance.info
			vector <double> tiempos{};
			tiempos.resize(my_context.size);
			tiempos[my_context.rank] = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
			std::cout << "Adiós[ " << my_context.rank << "]" << endl;
			for(int ii = 0; ii < my_context.size; ii++){
				MPI_Bcast(&tiempos[ii], 1, MPI_DOUBLE, ii, MPI_COMM_WORLD);
			}
			MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE, MPI_INFO_NULL, &fileper);
			string aux {};
			for(auto ii:tiempos){
				aux = aux + std::to_string(ii)+"\n";
			}
			MPI_File_write_all_begin(fileper, aux.c_str(), aux.size()+1, MPI_CHAR);
			MPI_Finalize();
		};		
	};

	/*****************************/


	/********VECTOR********/
	template <typename T>
	class DistributedVector{
		private:
			/*****FIELDS*******/
			vector<T> contenido{};	//DATA
			schedule_data my_schedule{BLOCK, 0};//spliting schedule
			MPI_Datatype datatype;
			T dummy; //field used in order to be able to return a reference in operator[]
			/********************/
			/*				
				@param size -> size (in bytes) of the file U are reading.
			*/
			void build_datatype(int size){ //allow us to build a MPI datatype
				MPI_Datatype res;				
				my_schedule.vector_size = size/sizeof(T);
				int amount_per_process = my_schedule.vector_size/my_context.size;
				int blocklength, dis[1], last_length;
				int contador = 0;
				vector<int> blocklengths{};
				vector<MPI_Aint> displacements{};			
				switch(my_schedule.type){
					case BLOCK:					
						//displacement = rank*elems for process 0
						dis[0] = my_context.rank*amount_per_process;						
						last_length = amount_per_process+my_schedule.vector_size%my_context.size;
						blocklength = (my_context.rank != my_context.size-1)?amount_per_process:last_length;
						MPI_Type_create_indexed_block(1, blocklength, dis, CHECK_TYPE(), &res);
						MPI_Type_commit(&res);
						this->my_schedule.datatype_size = blocklength;
						break;
					case ROBIN:
						//auto init = chrono::system_clock::now();
						contador = my_context.rank*my_schedule.rr_param;
						//prueba rendimiento 200.000.000 rodajas = 3683 ms por proceso. (se supone paralelo)
						while(contador < my_schedule.vector_size){
							int length = (my_schedule.vector_size - contador < my_schedule.rr_param) ? (my_schedule.vector_size - contador) :( my_schedule.rr_param);
							my_schedule.datatype_size += length;
							blocklengths.push_back(length);
							displacements.push_back(contador*sizeof(T));
							contador += (my_schedule.rr_param*my_context.size);							
						}
						//auto fin = chrono::system_clock::now();
						/*if(rango == 0){
							cout << chrono::duration_cast<chrono::milliseconds>(fin-init).count() << endl;
						}*/
						MPI_Type_create_hindexed(blocklengths.size(), blocklengths.data(), displacements.data(), CHECK_TYPE(), &res);
						MPI_Type_commit(&res);						
						break;
				}
				this->datatype = res;
			}
			/*
				@param pos -> position in the vector whose owner process wanna be known
				@return the rank of the process which has that element in memory
			*/
		public:
			static inline MPI_Datatype CHECK_TYPE(){
				return (std::is_same<int, T>::value )?(MPI_INT):MPI_DOUBLE;
			}
			int owner(int pos){				
				switch(my_schedule.type){
					case BLOCK:
					return (pos/(my_schedule.vector_size/my_context.size) >= my_context.size)? my_context.size-1 : pos/(my_schedule.vector_size/my_context.size);
					break;
					case ROBIN:
					return (pos/my_schedule.rr_param)%my_context.size;
					break;
				}
				return 0;
			}
			int global_to_local_pos(int pos){			
				switch(my_schedule.type){					
					case BLOCK:
						return (pos - my_context.rank*my_schedule.datatype_size);
					break;
					case ROBIN:
						//la posición será tan alta como el número de repartos enteros
						//se le añade la posición que ocupa en el último repartor
							//esta posición se sabe 
						int tama_reparto = my_schedule.rr_param*my_context.size;
						int repartos_enteros = pos / tama_reparto;
						int pos_in_chunk = pos - (repartos_enteros*tama_reparto) - (my_context.rank*my_schedule.rr_param);
						int res = repartos_enteros*my_schedule.rr_param + pos_in_chunk;
						return res;
					break;					
				}
				return 0;
			}
			DistributedVector(schedule_type tipo){//para bloque y benchmark
				my_schedule.type = tipo;
				if(tipo == BENCHMARK)
					my_schedule.type = dcpl::BLOCK;
				if(tipo == OPTIMIZED){
					ifstream aux{PER_INFO_PATH};
					if(!aux.is_open()){
						throw "No hay información de benchmark";
					}
					else{
						aux.close();
					}
				}
			};
			DistributedVector(schedule_type tipo, int rr_param){
				 this->my_schedule.type = tipo;
				 this->my_schedule.rr_param = rr_param;
			};
			DistributedVector (schedule_type tipo, vector<int> blocklengths){
				this->my_schedule.type = tipo;
				if(tipo != dcpl::AD_HOC) throw "Tipo de reparto equivocado.";
				this->my_schedule.blocklengths = blocklengths;
			}; //para ad-hoc

			T& operator[](int pos){						
				int local_pos = global_to_local_pos(pos);
				if(owner(pos) == my_context.rank){ //soy el que almacena el proceso
					MPI_Bcast(&contenido[local_pos], 1, CHECK_TYPE(), my_context.rank, MPI_COMM_WORLD);					
					return contenido[local_pos];
				}else{
					MPI_Bcast(&(this->dummy), 1, CHECK_TYPE(), owner(pos), MPI_COMM_WORLD);
					return this->dummy;
				}
			}
			/*
				return -> size of vector in elements of type T
			*/
			int size(){
				return my_schedule.vector_size;
			}
			/*
				@param path -> path to the file to read
			*/
			void llenar(const char* path, int length){
				MPI_File file_descriptor;
				MPI_Offset file_size;
				MPI_Status status;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &file_descriptor);
				MPI_File_get_size(file_descriptor, &file_size);
				file_size = ((unsigned int)file_size > length*sizeof(T)) ? length*sizeof(T) : file_size;
				build_datatype(file_size);
				contenido.resize(my_schedule.datatype_size);
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, datatype, "native", MPI_INFO_NULL);
				MPI_File_read(file_descriptor, contenido.data(), my_schedule.datatype_size, CHECK_TYPE(), &status);
				MPI_File_close(&file_descriptor);
				MPI_Barrier(MPI_COMM_WORLD);
			}
			/*
				@param path -> path to the file to write the vector
			*/
			void write(const char* path, int length){
				MPI_File file_descriptor;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &file_descriptor);
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, datatype, "native", MPI_INFO_NULL);
				length = this->my_schedule.datatype_size < length ? this->my_schedule.datatype_size : length;
				MPI_File_write_all_begin(file_descriptor, contenido.data(), length, CHECK_TYPE());
				MPI_File_close(&file_descriptor);
			}
			//funciones algoritmos
			template <class iterator, class unaryOperator>
			friend void transform(iterator first, iterator last, iterator result, unaryOperator op);
			template<class ForwardIt, class U, class BinaryOp>
			friend U reduce(ForwardIt first, ForwardIt last, U init, BinaryOp binary_op);
			

			~DistributedVector(){
			};
			//forward_iterator
			class iterator{
				DistributedVector& parent;
				int position;
			public:

				typedef T value_type;
				typedef T& reference;
				typedef T* pointer;
				typedef std::forward_iterator_tag iterator_category;
				typedef int difference_type;

				iterator(DistributedVector& p, int pos):parent{p},position{pos}{
				};
				T& operator*(){
					return parent[position];
				}
				T* operator->(){
					return &parent[position];
				}
				iterator& operator++(){
					position++;
					return *this;
				}
				iterator operator++(int unused){
					unused++;
					iterator copy = *this;
					++(*this);
					return copy;
				}
				bool operator==(iterator other){
					return this->position == other.position && &(this->parent) == &(other.parent);
				}
				bool operator!=(iterator other){
					return !(*this == other);
				}
				int getPosition(){
					return this->position;
				}
				DistributedVector& getParent(){
					return this->parent;
				}
				~iterator(){};
				
			};//fin iterator
			iterator begin(){
				iterator res {*this, 0};
				return res;
			}
			iterator end(){
				iterator res{*this, this->my_schedule.vector_size};
				return res;
			}
	}; //fin vector
	/////ifstream

	class ifstream{
		string path{};
	public:
		ifstream(string _path):path{_path}{};
		template <class T>
		ifstream& read(dcpl::DistributedVector<T>& in, int length){			
			in.llenar((this->path).c_str(), length);
			return *this;
		}
		template <class T>
		ifstream& write(DistributedVector<T>& in, int length){
			in.write((this->path).c_str(), length);
			return *this;
		}

		~ifstream(){};
		
	};


	/**********FUNCIONES**********/	
	template <class iterator, class unaryOperator>
	void transform(iterator first, iterator last, iterator result, unaryOperator op){		
		int contador = 0;
		MPI_Status status;
		while(first != last){			
			int emisor = first.getParent().owner(first.getPosition());		//proceso del que leer el dato con el que operaremos
			int receptor = result.getParent().owner(result.getPosition());	//proceso que operará con el dato fuente			
			auto argumento = first.getParent().contenido[0];
			if(emisor == receptor && my_context.rank == emisor){	//soy emisor y receptor
				//operación local en memoria
				result.getParent().contenido[result.getParent().global_to_local_pos(result.getPosition())] =
				 op(first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())]);

				result++;
				first++;
				contador++;
				continue;
			}
			if(my_context.rank == emisor){				
				MPI_Send(&(first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())]), 1, first.getParent().CHECK_TYPE(), receptor, 0, MPI_COMM_WORLD);				
			}
			if(my_context.rank == receptor){
				MPI_Recv(&argumento, 1, first.getParent().CHECK_TYPE(), emisor, 0, MPI_COMM_WORLD, &status);								
				result.getParent().contenido[result.getParent().global_to_local_pos(result.getPosition())] = op(argumento);								
			}
			++result;
			++first;
			contador++;
		}
	}
	

	
	template<class ForwardIt, class U, class Binary_op>
	U reduce(ForwardIt first, ForwardIt last, U init, Binary_op binary_op){		
		typedef struct{
			bool flag;
			U data;
		}flagged_data;
		flagged_data my_partial;

		bool primer = true;		
		U result;		
		for(;first!=last; first++){			
			if(first.getParent().owner(first.getPosition()) == my_context.rank){
				if(primer){
					my_partial.data = first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())];
					my_partial.flag = true;
					primer = false;
					continue;
				}
				my_partial.data = binary_op(my_partial.data, first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())]);
			}
		}
		//En este punto, todos han calculado su reduce parcial sin tener en cuenta init.		
		//puede haber procesos que no tengan un dato válido (no hayan hecho cálculos)
		//mandamos el dato e información sobre si es válido o no.
		std::vector<flagged_data> partials{};
		partials.resize(my_context.size);
		//mando mi dato a todos
		MPI_Bcast(&my_partial, sizeof(flagged_data), MPI_CHAR, my_context.rank, MPI_COMM_WORLD);
		for(unsigned int ii = 0; ii < partials.size();++ii){
			if((unsigned int)my_context.rank != ii)MPI_Bcast(&partials[ii], sizeof(flagged_data), MPI_CHAR, ii, MPI_COMM_WORLD);
		}
		partials[my_context.rank] = my_partial;
		
		//for(auto ii:partials) cout << ii.data << endl;
		
		result = init;		
		for(auto ii:partials){
			if(ii.flag) result = binary_op(result, ii.data);
		}
		return result;
	}


}//fin namespace
#include "mpi.h"
#include <vector>	//std::vector
#include <typeinfo>	//typeid
#include <iostream>	//iostream
#include <fstream>	//iostream
#include <unistd.h>	//usleep
#include <iterator>	//iterator tag
#include <chrono>	//medir tiempo
#include <numeric> //std::accumulate
#include <string.h> //memcpy
using namespace std;

std::vector<int> repartir_inversamente(int elementos, vector<int> durations){
	std::vector<double> in{};	
	in.resize(durations.size());
	std::vector<int> res{};	
	res.resize(durations.size());	
	std::transform(durations.begin(), durations.end(), in.begin(), [](int d){return 1.0/(double)d;});		
	double magic = std::accumulate(in.begin(), in.end(), 0.0, std::plus<double>());	
	magic = (double)elementos/magic;	
	std::transform(durations.begin(), durations.end(), res.begin(), [magic](int ele){return magic*(1.0/ele);});
	auto aux = std::accumulate(res.begin(), res.end(), 0.0);
	res[res.size()-1] += elementos - aux;
	return res;
}

namespace dcpl{
	/********TIPOS********/
	typedef char schedule_type;
	const schedule_type BLOCK = 0, ROBIN = 1, BENCHMARK = 2, OPTIMIZED = 3, AD_HOC=4;	//tipos de reparto
	const string PER_INFO_PATH = "./performance.info";

	typedef struct{
		int type = 0;		//splitting type (block, round robin...)
		int rr_param = 1;	//if the type is rr, chunk's size (ignored otherwise)
		std::vector<int> block_lengths{}; //is splitting type is optimized or ad-hoc, we store here how many elements each process has.

		int datatype_size = 0;		//size of the dataType which symbolizes the splitting in file (in elements of tipe double or int)
		int vector_size = 0;
	} schedule_data; //datos del reparto

	typedef struct{
		int rank = 0;
		int size = 0;
		vector<int> tiempos{};
	} MPI_context;
	MPI_context my_context;	//rango y número de procesos

	bool benchmark_flag = false;	//indica si se debe medir rendimiento.

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
			if(!benchmark_flag) return;
			end = chrono::system_clock::now();
			//escribir el tiempo en un archivo: por ej. ./performance.info
			vector <int> tiempos{};
			tiempos.resize(my_context.size);
			tiempos[my_context.rank] = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
			//std::cout << "Adiós[ " << my_context.rank << "]" << endl;
			for(int ii = 0; ii < my_context.size; ii++){
				MPI_Bcast(&tiempos[ii], 1, MPI_INT, ii, MPI_COMM_WORLD);
			}
			MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE|MPI_MODE_UNIQUE_OPEN|MPI_MODE_DELETE_ON_CLOSE, MPI_INFO_NULL, &fileper);
			MPI_File_close(&fileper);
			MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE|MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fileper);

			MPI_File_write_all_begin(fileper, tiempos.data(), tiempos.size(), MPI_INT);
			MPI_File_close(&fileper);
			
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
				int amount_per_process;
				int blocklength, dis[1], last_length;
				int contador = 0;
				vector<int> blocklengths{};
				vector<MPI_Aint> displacements{};


				switch(my_schedule.type){
					case BLOCK:	
						my_schedule.vector_size = size/sizeof(T);
						amount_per_process = my_schedule.vector_size/my_context.size;				
						//displacement = rank*elems for process 0
						dis[0] = my_context.rank*amount_per_process;						
						last_length = amount_per_process+my_schedule.vector_size%my_context.size;
						blocklength = (my_context.rank != my_context.size-1)?amount_per_process:last_length;
						MPI_Type_create_indexed_block(1, blocklength, dis, CHECK_TYPE(), &res);
						MPI_Type_commit(&res);
						this->my_schedule.datatype_size = blocklength;
						break;
					case ROBIN:
						my_schedule.vector_size = size/sizeof(T);
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
					case OPTIMIZED:
						my_schedule.block_lengths = repartir_inversamente(size/sizeof(T), my_context.tiempos);
						my_schedule.type = AD_HOC;
					//break; <- comentado intencionadamente
					case AD_HOC:	//sólo necesito ese, benchmark es bloque y optimized es este leyendo los datos en el constructor.
						my_schedule.vector_size = std::accumulate(my_schedule.block_lengths.begin(), my_schedule.block_lengths.end(), 0);
						int pos = my_context.rank == 0?0:my_context.rank;
						auto last = my_schedule.block_lengths.begin();
						std::advance(last, pos);
						int acumulado = std::accumulate(my_schedule.block_lengths.begin(), last, 0);
						MPI_Type_create_indexed_block(1, my_schedule.block_lengths[my_context.rank], &acumulado, CHECK_TYPE(), &res);
						MPI_Type_commit(&res);
						this->my_schedule.datatype_size = my_schedule.block_lengths[my_context.rank];
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
					case AD_HOC:
					int acumulado = 0;
					for(unsigned int ii=0; ii < my_schedule.block_lengths.size(); ++ii){
						if(pos >= acumulado && pos < acumulado+my_schedule.block_lengths[ii]){
							//cout << "OWNER " << ii << endl;
							return ii;
						};
						acumulado += my_schedule.block_lengths[ii];
					}
					break;
				}
				return 0;
			}
			int global_to_local_pos(int pos){			
				int tama_reparto, repartos_enteros, pos_in_chunk, res;
				switch(my_schedule.type){					
					case BLOCK:
						return (pos - my_context.rank*my_schedule.datatype_size);
					break;
					case ROBIN:
						//la posición será tan alta como el número de repartos enteros
						//se le añade la posición que ocupa en el último repartor
							//esta posición se sabe 
						tama_reparto = my_schedule.rr_param*my_context.size;
						repartos_enteros = pos / tama_reparto;
						pos_in_chunk = pos - (repartos_enteros*tama_reparto) - (my_context.rank*my_schedule.rr_param);
						res = repartos_enteros*my_schedule.rr_param + pos_in_chunk;
						return res;
					break;
					case AD_HOC:
					int acumulado = 0;
					if(my_context.rank == 0){
						acumulado = 0;
					}else{
						auto last = my_schedule.block_lengths.begin();
						std::advance(last, my_context.rank);
						acumulado = std::accumulate(my_schedule.block_lengths.begin(), last, 0);
					}
					//if(my_context.rank == 3)std::cout << "LOCAL_POS " << "|"<<pos - acumulado<<"|" << endl;
					return pos - acumulado;
					break;
				}
				return 0;
			}
			DistributedVector(schedule_type tipo){//para bloque y benchmark
				my_schedule.type = tipo;
				if(tipo == BENCHMARK){					
					my_schedule.type = dcpl::BLOCK;
					benchmark_flag = true;
				}
				if(tipo == OPTIMIZED){
					ifstream aux{PER_INFO_PATH};
					if(!aux.is_open()){
						throw "No hay información de benchmark";
					}
					else{
						MPI_File archivo;
						MPI_Status sta;
						my_context.tiempos.resize(my_context.size);
						//TODO leer archivo y calcular cuánto le corresponde a cada proceso.
						MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_RDONLY|MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &archivo);
						MPI_File_read(archivo, my_context.tiempos.data(), my_context.tiempos.size(), MPI_INT, &sta);
						
						aux.close();
					}
				}
			};
			DistributedVector(schedule_type tipo, int rr_param){ //para round robin
				 this->my_schedule.type = tipo;
				 this->my_schedule.rr_param = rr_param;
			};
			DistributedVector (schedule_type tipo, vector<int> blocklengths){ //para adhoc
				this->my_schedule.type = tipo;
				if(tipo != dcpl::AD_HOC || blocklengths.size() != (unsigned int) my_context.size){
					throw "Error de reparto";
				}
				this->my_schedule.block_lengths = blocklengths;
			}; //para ad-hoc

			T& operator[](int pos){						
				int local_pos = global_to_local_pos(pos);
				if(owner(pos) == my_context.rank){ //soy el que almacena el proceso
					//cout << "OPERATOR[]-dueño" << endl;
					MPI_Bcast(&contenido[local_pos], 1, CHECK_TYPE(), my_context.rank, MPI_COMM_WORLD);					
					return contenido[local_pos];
				}else{
					//cout << "OPERATOR[]-receptor" << endl;
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
				std::cout << "[" <<my_context.rank << "] = " << contenido.size();
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
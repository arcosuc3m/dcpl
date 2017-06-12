#include "mpi.h"
#include <vector>			//std::vector
#include <map>				//std::map
#include <typeinfo>			//typeid
#include <iostream>			//iostream
#include <fstream>			//iostream
#include <unistd.h>			//usleep
#include <iterator>			//iterator tag
#include <chrono>			//medir tiempo
#include <numeric>			//std::accumulate
#include <string.h> 		//memcpy
#include <functional>
#include <openssl/md5.h>	//md5
#include <stdio.h>
#include <limits.h>

#define NODE_NAME_LENGTH 16
#define DEBUG
//#define OPTIMIZED_INTERFACE

using namespace std;

inline void debug(std::string message){
	message = message;
	#ifdef	DEBUG
		auto rank{0};
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		usleep(rank*10000);
		std::cout<< "["s << rank << "]: " << message << endl;
	#endif
}

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
		std::vector<int> block_lengths{}; //is splitting type is optimized or ad-hoc, we store here how many elements each process has got..
		int datatype_size = 0;		//size of the dataType which symbolizes the splitting in file (in elements of tipe double or int)
		int vector_size = 0;
	} schedule_data; //datos del reparto

	typedef struct{
		int rank = 0;
		int size = 0;
		//vector<int> tiempos{};
		std::vector<int> tiempos{};

	} MPI_context;
	MPI_context my_context;	//rank, commuicator size & times when measuring performance.

	bool benchmark_flag = false;	//It sais if execution time will be wrotten in a file.

	ostream *coutp = new ostream(nullptr);
	ostream& cout = *coutp;

	char* node_names_hash(){
		//get my name (uname -n in shell)
		auto tuber = popen("uname -n", "r");
		char* buffer = (char*) calloc(my_context.size, sizeof(char)*NODE_NAME_LENGTH);
		char* my_string = buffer+NODE_NAME_LENGTH*my_context.rank;			
		fgets(my_string, NODE_NAME_LENGTH, tuber);		
		auto nombre_resumen = (const char*) MD5((const unsigned char*)my_string, strlen(my_string)+1, nullptr);
		memcpy(my_string, nombre_resumen, NODE_NAME_LENGTH);		
		for(int ii = 0; ii < my_context.size; ++ii){
			MPI_Bcast(&(buffer[ii*NODE_NAME_LENGTH]), NODE_NAME_LENGTH, MPI_CHAR, ii, MPI_COMM_WORLD);
		}
		return buffer;
	}

	class inicializador{
		std::chrono::time_point<std::chrono::system_clock> start, end;
	public:
		inicializador(int argc, char** argv){
			start = chrono::system_clock::now();
			MPI_Init(&argc, &argv);
			MPI_Comm_rank(MPI_COMM_WORLD, &(my_context.rank));
			MPI_Comm_size(MPI_COMM_WORLD, &(my_context.size));			
			debug("INICIALIZADO proceso nº "s+ to_string(my_context.rank)+" y hay "s+to_string(my_context.size)+" procesos."s);
			if(my_context.rank == 0)memcpy(&cout, &std::cout, sizeof(std::cout));
		};
		~inicializador(){
			MPI_File fileper;
			MPI_Status sta;
			//delete coutp;
			if(!benchmark_flag){
				auto error = MPI_Finalize();
				if(error != MPI_SUCCESS){
					debug("Error ejecutando MPI_Finalize.");
				}
				return;	
			} 
			end = chrono::system_clock::now();
			char * buffer = node_names_hash();

			//write the execution time for each node in a file.
			vector <int> tiempos{};
			tiempos.resize(my_context.size);
			tiempos[my_context.rank] = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
			//std::cout << "Adiós[ " << my_context.rank << "]" << endl;
			for(int ii = 0; ii < my_context.size; ++ii){
				MPI_Bcast(&tiempos[ii], 1, MPI_INT, ii, MPI_COMM_WORLD);
			}
			//we open the file and close it in order to overwrite existing data (MPI does not have O_TRUNC equivalent)
			MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE|MPI_MODE_UNIQUE_OPEN|MPI_MODE_DELETE_ON_CLOSE, MPI_INFO_NULL, &fileper);
			MPI_File_close(&fileper);
			//open the file
			MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE|MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fileper);
			int contador = 0;
			for(auto ii = tiempos.begin(); ii!=tiempos.end(); ++ii){				
				MPI_File_write_at(fileper, contador*(sizeof(int)+NODE_NAME_LENGTH), &(*ii), 1, MPI_INT, &sta);
				MPI_File_write_at(fileper, contador*(sizeof(int)+NODE_NAME_LENGTH)+sizeof(int), &buffer[NODE_NAME_LENGTH*contador], NODE_NAME_LENGTH, MPI_CHAR, &sta);
				++contador;
			}
			MPI_File_close(&fileper);
			free(buffer);
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
						/*Se comenta el break proque se utiliza la rutina de creación de un datatype de tipo ad-hoc para crear un vector cuando se elige balanceo de carga*/
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
					//dcpl::cout << "OWNER = " << ((pos/(my_schedule.vector_size/my_context.size) >= my_context.size)? my_context.size-1 : pos/(my_schedule.vector_size/my_context.size)) << endl;
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
			/*
				@param pos: the position in the conceptual vector
				@return an integer that says where in the local vector that element is stored
			*/
			int global_to_local_pos(int pos){			
				int tama_reparto, repartos_enteros, pos_in_chunk, res;
				switch(my_schedule.type){					
					case BLOCK:
						return (pos - my_context.rank*(my_schedule.vector_size/my_context.size));
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
					if(benchmark_flag) debug("Se ha iniciado modo benchmark.");
				}
				if(tipo == OPTIMIZED){
					ifstream aux{PER_INFO_PATH};
					if(!aux.is_open()){
						cout << "No existen datos de rendimiento, instanciando vector ["<< this<<"] en modo bloque" << endl;
						DistributedVector(dcpl::BLOCK);
					}
					else{
						char* buffer_file = (char*)calloc( my_context.size, NODE_NAME_LENGTH*sizeof(char));
						char* buffer_check;

						MPI_File archivo;
						MPI_Status sta;	
						MPI_File_open(MPI_COMM_WORLD, PER_INFO_PATH.c_str(), MPI_MODE_RDONLY|MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &archivo);
						for(int ii = 0; ii < my_context.size; ++ii){
							MPI_File_read_at(archivo, ii*(sizeof(int)+NODE_NAME_LENGTH)+sizeof(int), buffer_file+ii*NODE_NAME_LENGTH, NODE_NAME_LENGTH, MPI_CHAR, &sta);
						}
						//ya tenemos los resúmenes que hay en el archivo
						buffer_check = node_names_hash();
						std::vector<std::vector<char>> file{}, actual{};
						for(int ii = 0; ii < my_context.size; ++ii){
							file.push_back(std::vector<char>{buffer_file+ii*NODE_NAME_LENGTH, buffer_file+ii*NODE_NAME_LENGTH+NODE_NAME_LENGTH});
							actual.push_back(std::vector<char>{buffer_check+ii*NODE_NAME_LENGTH, buffer_check+ii*NODE_NAME_LENGTH+NODE_NAME_LENGTH});
						}
						#ifdef	DEBUG
							for(auto ii : file){
								cout << "||"s<< string(ii.begin(), ii.end()) << endl;
							}
						#endif
						bool is_per = std::is_permutation(file.begin(), file.end(), actual.begin());

						if(!is_per){
							cout << "performance.info non consistent, changing to block" << endl;
							this->my_schedule.type = BLOCK;
						}
						vector<int> perm_pattern {};
						if(is_per && !(file == actual)){
							for(int ii = 0; ii < my_context.size; ++ii){
								for (int jj = 0; jj < my_context.size; ++jj){
									if(actual[jj] == file[ii]){
										perm_pattern.push_back(jj-ii);
										break;
									}
								}
							}
						}

						my_context.tiempos.resize(my_context.size);
						std::vector<int> tiempos_aux{};
						tiempos_aux.resize(my_context.size);
						for(int ii = 0; ii < my_context.size; ++ii){
							MPI_File_read_at(archivo, ii*(sizeof(int)+NODE_NAME_LENGTH), tiempos_aux.data()+ii, 1, MPI_INT, &sta);
						}
						if(perm_pattern.size()!=0){
							for(int ii = 0; ii < my_context.size; ++ii){
								my_context.tiempos[ii] = tiempos_aux[ii+perm_pattern[ii]];
							}
						}else
							my_context.tiempos = tiempos_aux;
						for(auto ii:my_context.tiempos)
							cout << ii << endl;
						
						aux.close();
					}
				}
				debug("Instanciado vector con tipo: "s+to_string(my_schedule.type));
			};
			DistributedVector(schedule_type tipo, int rr_param){ //para round robin
				 this->my_schedule.type = tipo;
				 this->my_schedule.rr_param = rr_param;
				 debug("Instanciado vector de tipo: "s+ to_string(my_schedule.type)+" con rodaja: "s+to_string(this->my_schedule.rr_param));
			};
			DistributedVector (schedule_type tipo, vector<int> blocklengths){ //para adhoc
				this->my_schedule.type = tipo;
				if(tipo != dcpl::AD_HOC || blocklengths.size() != (unsigned int) my_context.size){
					throw "Error de reparto";
				}
				this->my_schedule.block_lengths = blocklengths;
				#ifdef DEBUG
				std::string a{"Instanciado vector de tipo: "};
				a+=to_string(my_schedule.type);
				a+= "\n"s;
				for(unsigned int ii = 0; ii < blocklengths.size(); ii++){
					a+= "blocklength["s+to_string(ii)+"]"s +" = " +to_string(blocklengths[ii]);
				}
				debug(a);
				#endif
			}; //para ad-hoc

			T& operator[](int pos){
				#ifdef OPTIMIZED_INTERFACE
				if(owner(pos) != my_context.rank){
					this->dummy = 0;
					return this->dummy;
				}
				return this->contenido[global_to_local_pos(pos)];
				#endif
				#ifndef OPTIMIZED_INTERFACE

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
				#endif
			}
			T get(int pos, int nodo){ //acceso de lectura, sólo se lee valor correcto en el nodo indicado				
				bool amSender, amReceiver;
				amSender = owner(pos) == my_context.rank;
				amReceiver = nodo == my_context.rank;
				if(nodo < 0 ){
					if (owner(pos) == my_context.rank)
						return this->contenido[global_to_local_pos(pos)];
					return 0;
				}
				if(amSender && amReceiver){
					return this->contenido[global_to_local_pos(pos)];
				}else if(amSender){
					MPI_Send(&(this->contenido[global_to_local_pos(pos)]), 1, CHECK_TYPE(), nodo, 0, MPI_COMM_WORLD);
					//std::cout<< "nodo: " << my_context.rank << "sent pos: " << pos <<endl;
					return 0;
				}else if(amReceiver){
					T value;
					MPI_Status nullStatus;
					MPI_Recv(&value, 1, CHECK_TYPE(), owner(pos), 0, MPI_COMM_WORLD, &nullStatus);
					//std::cout<< "nodo: " << my_context.rank << "received pos: " << pos <<endl;
					return value;
				}else{
					return 0;
				}
			}
			void set(int pos, T value){ //acceso de escritura
				if(!(owner(pos) == my_context.rank)) return;
				this->contenido[global_to_local_pos(pos)] = value;
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
				#ifdef DEBUG
				auto a = "PROCESO "s+to_string(my_context.rank)+" contiene\n"s;
				for(uint ii = 0; ii < contenido.size(); ii++){
					a+=to_string(contenido[ii])+"\n"s;
				}
				debug(a);
				#endif
				return;
			}
			/*
				@param path -> path to the file to write the vector
			*/
			void write(const char* path, int length){
				debug("Entrando en 	WRITE");
				MPI_File file_descriptor;
				MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &file_descriptor);
				MPI_File_set_view(file_descriptor, 0, MPI_CHAR, datatype, "native", MPI_INFO_NULL);
				length = this->my_schedule.vector_size < length ? this->my_schedule.vector_size : length;
				--length;
				switch(this->my_schedule.type){
					case ROBIN:
						if(owner(length) == my_context.rank){
							length = global_to_local_pos(length);
						}else{
							while(owner(length) != my_context.rank && length != -1){
								--length;						
							}						
							length = global_to_local_pos(length);
							if(length <0)length = 0;
						}
						break;
					default: //sólo serína bloque o ad-hoc (benchmark == bloque y optimized == ad-hoc)
						if(owner(length) < my_context.rank){
							length = -1;
						}else if(my_context.rank == owner(length)){
							length = global_to_local_pos(length);
						}else{
							length = my_schedule.datatype_size-1;
						}
						break;
					}

				debug("MPI_File_write_all_begin "s+to_string(length)+" elementos"s);
				MPI_File_write_all_begin(file_descriptor, contenido.data(), length+1, CHECK_TYPE());
				MPI_File_close(&file_descriptor);
			}
			//funciones algoritmos
			template <class iterator, class unaryOperator>
			friend iterator transform(iterator first, iterator last, iterator result, unaryOperator op);
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

				iterator(DistributedVector& p, int pos):parent{p},position{pos}{};
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
					return (this->position == other.position) && (&(this->parent) == &(other.parent));
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
	iterator transform(iterator first, iterator last, iterator result, unaryOperator op){				
		MPI_Status status;
		if((first == result) &&(first == first.getParent().begin()) && (last == first.getParent().end())){ //para el caso trivial
			debug("Haciendo transform simple."s);
			std::transform(first.getParent().contenido.begin(), first.getParent().contenido.end(), first.getParent().contenido.begin(), op);
			return last;
		}
		debug("transform"s);
		while(first != last){			
			int emisor = first.getParent().owner(first.getPosition());		//proceso del que leer el dato con el que operaremos
			int receptor = result.getParent().owner(result.getPosition());	//proceso que operará con el dato fuente			
			auto argumento = first.getParent().contenido[0];
			if(emisor == receptor && my_context.rank == emisor){	//soy emisor y receptor
				//operación local en memoria
				result.getParent().contenido[result.getParent().global_to_local_pos(result.getPosition())] =
				 op(first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())]);

				++result;
				++first;
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
		}
		return result;
	}
	/*template<class number, typename A, std::function<A(A,A)> O>
	class interOperator{
	public:
		static void addem(number* invec, number* inout, int* length, MPI_Datatype* dtype){
			for(int ii = 0; ii < *length; ++ii){
				inout[ii] = (*O)(inout[ii], invec[ii]);
			}
		}
	};*/

	
	template<class ForwardIt, class U, class Binary_op>
	U reduce(ForwardIt first, ForwardIt last, U init, Binary_op binary_op){		
		static auto _op=binary_op; //para que la clase interna pueda verlo.
		U my_partial;//mi resultado parcial.
		bool primer = true;//true si no he hecho cálculos, falso en otro caso.
		MPI_Op miop;//operación personalizada para usar con MPI_op_create
		MPI_Comm comm;//nuevo comunicador
		

		class interOperator{
		public:
			static void addem(U* invec, U* inout, int* length, MPI_Datatype* dtype){
				*dtype = *dtype;//para evitar unused variable warning
				for(int ii = 0; ii < *length; ++ii){
						inout[ii] = (_op(inout[ii], invec[ii]));
				}
			}
		};
		auto auxPointer = &(interOperator::addem);
		if(my_context.rank == 0){
			my_partial = init;
			primer = false;
		}

		while(first!=last){					
			if(first.getParent().owner(first.getPosition()) == my_context.rank){
				if(primer){
					my_partial = first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())];					
					primer = false;
					++first;
					continue;
				}
				my_partial = binary_op(my_partial, first.getParent().contenido[first.getParent().global_to_local_pos(first.getPosition())]);

			}
			++first;
		}
		U send = my_partial;
		U receive;

		cout << "my partial data " << my_partial << endl;		
		MPI_Op_create((MPI_User_function*) auxPointer, 1, &miop);
		MPI_Comm_split(MPI_COMM_WORLD, primer?MPI_UNDEFINED:1, my_context.rank, &comm); //sólo los procesos con valores válidos van al nuevo comunicador
		
		if(comm != MPI_COMM_NULL)
			MPI_Reduce(&send, &receive, 1, std::is_same<U, int>()?MPI_INT:MPI_DOUBLE, miop, 0, comm);		

		MPI_Bcast(&receive, 1, std::is_same<U, int>()?MPI_INT:MPI_DOUBLE, 0, MPI_COMM_WORLD); //recibo un bcast del 0
		if(comm != MPI_COMM_NULL)
			MPI_Comm_free(&comm);
		std::cout << "PROCESO: "<<my_context.rank << " reduce = " << receive << endl;
		return receive;

		/*//En este punto, todos han calculado su reduce parcial sin tener en cuenta init.		
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
		return result;*/
	}

}//fin namespace
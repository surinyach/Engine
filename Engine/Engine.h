#ifndef ENGINE_H
#define ENGINE_H

#include <iostream>
#include <string>
#include <tchar.h>
#include <thread>
#include <map>
#include <vector>
#include <immintrin.h>
#include <windows.h>
#include <psapi.h>

using namespace std;

struct Element
{
	int offset;
	char* value;
};

using Elements = vector<Element>;

struct Region
{
	char* BaseAddress;
	vector<Element> Elementos;
};
struct StoredRegion
{
	unsigned int RegionSize;
	char* BaseAddress;
	char* MemoryContent;
};

using Regions = vector<Region>;
using StoredRegions = vector<StoredRegion>;

using ProcessesIt = map <DWORD, basic_string<TCHAR>>::iterator;
using Processes = map<DWORD, basic_string<TCHAR>>;

class Engine
{

private:

	const int MAX_ADDRESSES = 500;

	struct Scan {
		string Input; // Valor a buscar en memoria en la iteracion actual. 

		DWORD ProcessID; // ID del proceso tratado en el escaner.
		int DataSize; // Tamaño del dato a tratar en el escaner.

		string DataType; // Tipo de datos a tratar en el escaner.
		string ModifyValue; // Nuevo valor para la posición de memoria en "ModifyAddress".

		bool First = true; // ¿Primera iteracion del escaner?
		bool KnownValue = true; // ¿Valor de búsqueda conocido?

		int ValueState; // 1: > ; 2: <
		char* ModifyAddress; // Direccion de memoria a modificar.
	};
	struct ThreadData
	{
		int threadNumber;
		Engine* instance;
		bool clearStructures;
		char* baseAddress;
		unsigned int regionSize;
	};
	struct WriteThreadData
	{
		Engine* instance; 
	};

	/* Variables */
	Scan Escaner;
	Processes Procesos;
	Regions Direcciones;
	bool TD_Bits; // ¿32 bits?

	// Fijacion de valores
	HANDLE WriteThread;
	HANDLE WriteEvent;
	WriteThreadData writeThreadData;
	map<char*, string> Persistentes;


	int CPUs;
	CRITICAL_SECTION Critical;
	atomic<int> TotalAddresses;
	vector<HANDLE> Threads;
	vector<ThreadData> threadData;
	vector<HANDLE> ThreadEvents;
	vector<HANDLE> MainEvents;

	/* Funciones */

	// Auxiliares
	static void ErrorExit(LPCTSTR message);

	bool compareInt(char* data, char* previous);

	bool compareValue(char* data, char* previous);

	void cleanStructures();

	bool is32BitProcess(HANDLE hProcess);

	static int CPUNumber();

	// Threads auxiliares
	void sincStructures(Region& Nueva);
	void knownScanFirstStructures(StoredRegion& nueva);

	bool compareBlockAVX2(char* previousValues, char* currentValues);
	bool compareBlock512(char* previousValues, char* currentValues);
	void inspectBlock(char* previousValues, char* currentValues, int offset, Elements& Elements);

	void threadObtainAllMemory(HANDLE& hProcess, ThreadData*& data, StoredRegions& RegionesGuardadas);
	void threadCompareEntireRegion(HANDLE& hProcess, StoredRegions& RegionesGuardadas, Regions& Regiones);
	void threadUpdateValues(HANDLE& hProcess, Regions& Regiones);


	// Threads
	static DWORD WINAPI ReadThreadFunction(LPVOID lpParam);
	static DWORD WINAPI WriteReadThreadFunction(LPVOID lpParam);

	// Listar procesos
	void obtainProcessIdentifiers();
	void obtainProcessName(DWORD processID);

	// Gestion de memoria
	void updateValues(HANDLE hProcess, char* baseAddress, unsigned int regionSize, Elements& elementos);
	void obtainAllMemory(HANDLE hProcess, char* baseAddress, unsigned int regionSize, StoredRegion& nueva);
	void compareEntireRegion(HANDLE hProcess, StoredRegion& actual, Regions& Regiones);

	void writeMemoryValue(HANDLE hProcess, char* modifyAddress, string input);

public:

	/* Constructora */
	Engine();

	/* Consultoras */
	Processes getProcessesNameAndID();

	int getProcessID();
	int getScanDataSize();
	int getAddressesNumber();
	int getMaxAddresses();
	bool getScanFirst();
	bool getScanKnownValue();
	string getSearchValue();
	string getScanDataType();
	string getInputValue();
	char* getModifyAddress();
	const Regions& getAddresses();

	/* Inputs */
	void insertProcessID(DWORD processID);
	void insertDataType(string dataType);
	void insertDataSize(int dataSize);
	void insertKnownValue(bool conocido);
	void insertSearchValue(string value);
	void insertValueState(int state);
	void insertScanFirst(bool first);
	void insertModifyAddress(char* modifyAddress);
	void insertModifyValue(string modifyValue);
	void insertPersistentValue(bool persistente);

	/* Procesos */
	void firstMemoryScan();
	void consecutiveMemoryScan();
	void modifyMemoryValue();
};
#endif
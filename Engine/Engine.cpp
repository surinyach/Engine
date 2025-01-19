#include "Engine.h"

/* Auxiliares privadas */
void Engine::ErrorExit(LPCTSTR message) {
	_tprintf(TEXT("%s\n"), message);
	DWORD errorMessageID = GetLastError();
	if (errorMessageID != 0)
	{
		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
		string message(messageBuffer, size);
		LocalFree(messageBuffer);
		cout << "WinApi Error Message: " << message << endl;
	}
	exit(1);
}

bool Engine::compareInt(char* data, char* previous)
{
	int value = *reinterpret_cast<int*>(data);

	if (value >= 0)
	{
		if (this->Escaner.KnownValue)
		{
			int iInput;
			iInput = stoi(this->Escaner.Input);
			if (iInput == value) return true;

		}
		else
		{
			int anterior = *reinterpret_cast<int*>(previous);
			if (this->Escaner.ValueState == 1)
			{
				if (value > anterior) return true;
			}
			else
			{
				if (value < anterior) return true;
			}
		}
	}
	return false;
}

bool Engine::compareValue(char* data, char* previous)
{
	switch (this->Escaner.DataSize) {
	case 4:
	{
		if (this->Escaner.DataType == "4 Bytes")
		{
			return compareInt(data, previous);
		}
		else
		{
		}
		break;
	}
	}
	return false;
}

int Engine::CPUNumber()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

void Engine::cleanStructures()
{
	this->TotalAddresses.store(0, memory_order_seq_cst);
	EnterCriticalSection(&this->Critical);
	this->Direcciones = Regions{};
	LeaveCriticalSection(&this->Critical);
}

bool Engine::is32BitProcess(HANDLE hProcess)
{
	BOOL isWow64 = FALSE;

	if (IsWow64Process(hProcess, &isWow64)) return isWow64;
	else ErrorExit(TEXT("Error obteniendo la arquitectura del proceso"));
}

/* Listar procesos */
void Engine::obtainProcessName(DWORD processID)
{
	TCHAR processName[MAX_PATH] = TEXT("<Desconocido>");
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processID);

	if (NULL != hProcess)
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
		{
			GetModuleBaseName(hProcess, hMod, processName, sizeof(processName) / sizeof(TCHAR));
			this->Procesos.insert({ processID, processName });

			CloseHandle(hProcess);
		}
	}
}
void Engine::obtainProcessIdentifiers()
{
	DWORD aProcesses[1024], cbNeeded;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		ErrorExit(TEXT("Error obteniendo los identificadores de proceso"));
	}

	DWORD procesosTotales = cbNeeded / sizeof(DWORD);
	cout << "Procesos totales: " << procesosTotales << endl;

	for (unsigned int i = 0; i < procesosTotales; ++i)
	{
		if (aProcesses[i] != 0)
		{
			obtainProcessName(aProcesses[i]);
		}
	}
}

/* Gestion de memoria */
void Engine::updateValues(HANDLE hProcess, char* baseAddress, unsigned int regionSize, Elements& elementos)
{
	char* buffer = new char[regionSize];
	SIZE_T bytesRead;
	if (!ReadProcessMemory(hProcess, baseAddress, buffer, regionSize, &bytesRead))
	{
		ErrorExit(TEXT("Error leyendo la memoria del proceso"));
	}
	else if (bytesRead != regionSize)
	{
		ErrorExit(TEXT("Se han obtenido menos elementos de los esperados"));
	}

	Elements auxElementos = Elements{};
	for (int i = 0; i < elementos.size(); ++i)
	{
		Element elemento = elementos[i];
		char* value = buffer + elemento.offset;
		if (compareValue(value, elemento.value))
		{
			memcpy(elemento.value, buffer + elemento.offset, this->Escaner.DataSize);
			auxElementos.push_back(elemento);
		}
	}

	elementos = move(auxElementos);
	delete[] buffer;

}
void Engine::obtainAllMemory(HANDLE hProcess, char* baseAddress, unsigned int regionSize, StoredRegion& nueva)
{
	SIZE_T bytesRead;
	if (!ReadProcessMemory(hProcess, baseAddress, nueva.MemoryContent, regionSize, &bytesRead))
	{
		ErrorExit(TEXT("Error leyendo la memoria del proceso"));
	}
	else if (bytesRead != regionSize)
	{
		ErrorExit(TEXT("Se han obtenido menos elementos de los esperados"));
	}
}

bool Engine::compareBlockAVX2(char* previousValues, char* currentValues)
{
	__m256i block1 = _mm256_load_si256((__m256i*)(previousValues));
	__m256i block2 = _mm256_load_si256((__m256i*)(currentValues));

	__m256i result = _mm256_cmpeq_epi8(block1, block2);

	int mask = _mm256_movemask_epi8(result);

	if (mask != -1)
	{
		return false;
	}

	return true;
}
bool Engine::compareBlock512(char* previousValues, char* currentValues)
{
	int result = memcmp(previousValues, currentValues, 512);
	if (result == 0) return true;
	else return false;
}
void Engine::inspectBlock(char* previousValues, char* currentValues, int offset, Elements& elements)
{
	bool primero = true;
	for (int i = 0; i < 512; i += this->Escaner.DataSize)
	{
		if (compareValue(currentValues + i, previousValues + i))
		{
			Element nuevo;
			nuevo.offset = i + offset;
			nuevo.value = new char[this->Escaner.DataSize];
			memcpy(nuevo.value, currentValues + i, this->Escaner.DataSize);
			elements.push_back(nuevo);
		}
	}
}

void Engine::compareEntireRegion(HANDLE hProcess, StoredRegion& actual, Regions& Regiones)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQueryEx(hProcess, actual.BaseAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == sizeof(MEMORY_BASIC_INFORMATION))
	{
		if ((mbi.State & MEM_COMMIT)
			&& (mbi.Protect & PAGE_READWRITE)
			&& !(mbi.Protect & PAGE_GUARD)
			&& !(mbi.Protect & PAGE_NOACCESS))
		{
			char* newValues = new char[mbi.RegionSize];
			char* baseAddress = (char*)mbi.BaseAddress;
			unsigned int regionSize = mbi.RegionSize;

			if (!ReadProcessMemory(hProcess, baseAddress, newValues, regionSize, NULL))
			{
				ErrorExit(TEXT("Error leyendo la memoria del proceso"));
			}

			Elements elements = Elements{};

			// Calcular las divisiones en bloques de 512 bytes
			int divisiones = regionSize / 512;

			for (int i = 0; i < divisiones; ++i)
			{
				int offset = 512 * i;
				if (!compareBlock512(actual.MemoryContent + offset, newValues + offset))
				{
					inspectBlock(actual.MemoryContent + offset, newValues + offset, offset, elements);
				}
			}

			int checkpoint = 512 * divisiones;

			unsigned int resto = regionSize - checkpoint;
			unsigned int ajuste = resto % 4; 
			unsigned int endRegion = regionSize - ajuste; 

			for (int i = checkpoint; i < endRegion; i += this->Escaner.DataSize)
			{
				if (compareValue(newValues + i, actual.MemoryContent + i))
				{
					Element nuevo;
					nuevo.offset = i;
					nuevo.value = new char[this->Escaner.DataSize];
					memcpy(nuevo.value, newValues + i, this->Escaner.DataSize);
					elements.push_back(nuevo);
				}
			}

			// Se han encontrado valores modificados segun el estado descrito.
			if (!elements.empty())
			{
				Region nueva;
				nueva.BaseAddress = actual.BaseAddress;
				nueva.Elementos = move(elements);

				Regiones.push_back(nueva);
				sincStructures(nueva);
			}

			delete[] actual.MemoryContent;
			delete[] newValues;
		}
	}
}

void Engine::writeMemoryValue(HANDLE hProcess, char* modifyAddress, string input)
{

	void* data = NULL;
	switch (this->Escaner.DataSize) {
	case 4:
	{
		int value = stoi(input);
		data = &value;
		break;
	}
	}

	SIZE_T bytesWritten;
	if (!WriteProcessMemory(hProcess, modifyAddress, data, (SIZE_T)(Escaner.DataSize), &bytesWritten))
	{
		ErrorExit(TEXT("Error escribiendo el nuevo valor en memoria"));
	}
}

/* Constructora */
Engine::Engine()
{
	this->Escaner.DataSize = 0;
	this->Escaner.DataType = "";
	this->Escaner.First = true;
	this->Escaner.KnownValue = true;
	this->Escaner.ModifyAddress = NULL;
	this->Escaner.ModifyValue = "";
	this->Escaner.ProcessID = 0;
	this->Escaner.ValueState = 0;
	this->TD_Bits = false;
	InitializeCriticalSection(&this->Critical);
	this->CPUs = CPUNumber();

	/* Crear todos los threads y sus eventos de sincronizacion */
	this->threadData = vector<ThreadData>(this->CPUs);
	this->Threads = vector<HANDLE>(this->CPUs);
	this->ThreadEvents = vector<HANDLE>(this->CPUs);
	this->MainEvents = vector<HANDLE>(this->CPUs);
	this->TotalAddresses = 0;
	for (int i = 0; i < this->CPUs; ++i)
	{
		this->threadData[i] = { i, this, false };
		this->Threads[i] = CreateThread(NULL, 0, ReadThreadFunction, &this->threadData[i], 0, NULL);
		this->ThreadEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		this->MainEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	/* Crear el thread para la persistencia de valores */
	this->writeThreadData = { this };
	this->WriteThread = CreateThread(NULL, 0, WriteReadThreadFunction, &this->writeThreadData, 0, NULL);
	this->WriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

/* Consultoras */
Processes Engine::getProcessesNameAndID()
{
	obtainProcessIdentifiers();
	return this->Procesos;
}

int Engine::getProcessID()
{
	return this->Escaner.ProcessID;
}
int Engine::getScanDataSize()
{
	return this->Escaner.DataSize;
}
int Engine::getAddressesNumber()
{
	return this->TotalAddresses;
}
int Engine::getMaxAddresses()
{
	return this->MAX_ADDRESSES;
}
bool Engine::getScanFirst()
{
	return this->Escaner.First;
}
bool Engine::getScanKnownValue()
{
	return this->Escaner.KnownValue;
}
string Engine::getSearchValue()
{
	return this->Escaner.Input;
}
string Engine::getScanDataType()
{
	return this->Escaner.DataType;
}
string Engine::getInputValue()
{
	return this->Escaner.Input;
}
char* Engine::getModifyAddress()
{
	return this->Escaner.ModifyAddress;
}
const Regions& Engine::getAddresses()
{
	return this->Direcciones;
}

/* Input */
void Engine::insertProcessID(DWORD processID)
{
	this->Escaner.ProcessID = processID;
	if (Procesos.find(processID) == Procesos.end()) {
		ErrorExit(TEXT("El proceso insertado no se encuentra presente en el sistema"));
	}
}
void Engine::insertDataType(string dataType)
{
	this->Escaner.DataType = dataType;
}
void Engine::insertDataSize(int dataSize)
{
	this->Escaner.DataSize = dataSize;
}
void Engine::insertKnownValue(bool conocido)
{
	this->Escaner.KnownValue = conocido;
}
void Engine::insertSearchValue(string value)
{
	this->Escaner.Input = value;
}
void Engine::insertValueState(int state)
{
	this->Escaner.ValueState = state;
}
void Engine::insertScanFirst(bool first)
{
	this->Escaner.First = first;
}
void Engine::insertModifyAddress(char* modifyAddress)
{
	this->Escaner.ModifyAddress = modifyAddress;
}
void Engine::insertModifyValue(string modifyValue)
{
	this->Escaner.ModifyValue = modifyValue;
}
void Engine::insertPersistentValue(bool persiste)
{
	EnterCriticalSection(&this->Critical);
	bool presente = this->Persistentes.find(this->Escaner.ModifyAddress) != this->Persistentes.end(); 
	if (persiste)
	{
		if (presente) this->Persistentes[this->Escaner.ModifyAddress] = this->Escaner.ModifyValue;
		else this->Persistentes.insert({ this->Escaner.ModifyAddress,  this->Escaner.ModifyValue });
	}
	else 
	{
		if (presente) this->Persistentes.erase(this->Escaner.ModifyAddress);
	}
	LeaveCriticalSection(&this->Critical);
	if (this->Persistentes.size() == 1) SetEvent(this->WriteEvent);
}

/* Procesos */
void Engine::modifyMemoryValue()
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, this->Escaner.ProcessID);
	if (hProcess != NULL) {
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQueryEx(hProcess, this->Escaner.ModifyAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == sizeof(MEMORY_BASIC_INFORMATION))
		{
			if ((mbi.State & MEM_COMMIT)
				&& (mbi.Protect & PAGE_READWRITE)
				&& !(mbi.Protect & PAGE_GUARD)
				&& !(mbi.Protect & PAGE_NOACCESS))
			{
				writeMemoryValue(hProcess, this->Escaner.ModifyAddress, this->Escaner.ModifyValue);
			}
		}
	}
}

/* Procesos */
void Engine::firstMemoryScan()
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, this->Escaner.ProcessID);
	MEMORY_BASIC_INFORMATION mbi;
	char* base = NULL;
	int threadCounter = 0;

	// Comprueba si el proceso objetivo es de 32 bits
	this->TD_Bits = is32BitProcess(hProcess);

	cleanStructures();

	if (hProcess != NULL)
	{
		while (VirtualQueryEx(hProcess, base, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) != 0 && (base + mbi.RegionSize > base)) {

			if (this->TD_Bits && (uintptr_t)base >= 0x7FFFFFFF) break;
	
			base = (char*)mbi.BaseAddress;
			unsigned int regionSize = mbi.RegionSize;

			if (this->TD_Bits)
			{
				uintptr_t maxRegionSize = 0x7FFFFFFF - (uintptr_t)base;
				if (regionSize > maxRegionSize) regionSize = (unsigned int)maxRegionSize;
			}

	
			if ((mbi.State & MEM_COMMIT)
				&& (mbi.Protect & PAGE_READWRITE)
				&& !(mbi.Protect & PAGE_GUARD)
				&& !(mbi.Protect & PAGE_NOACCESS))
			{

				if (threadCounter < this->CPUs)
				{
					this->threadData[threadCounter].baseAddress = base;
					this->threadData[threadCounter].regionSize = regionSize;
					this->threadData[threadCounter].clearStructures = true;
					SetEvent(this->ThreadEvents[threadCounter]);
					++threadCounter;
				}
				else
				{
					DWORD result = WaitForMultipleObjects(this->CPUs, this->MainEvents.data(), FALSE, INFINITE);
					DWORD index = result - WAIT_OBJECT_0;
					this->threadData[index].baseAddress = base;
					this->threadData[index].regionSize = regionSize;
					this->threadData[index].clearStructures = false;
					SetEvent(this->ThreadEvents[index]);
				}
			}

			base += mbi.RegionSize;
		}
		WaitForMultipleObjects(this->CPUs, this->MainEvents.data(), TRUE, INFINITE);
		CloseHandle(hProcess);
	}
}
void Engine::consecutiveMemoryScan()
{
	cleanStructures();

	for (int i = 0; i < this->ThreadEvents.size(); ++i)
	{
		SetEvent(this->ThreadEvents[i]);
	}
	WaitForMultipleObjects(this->CPUs, this->MainEvents.data(), TRUE, INFINITE);
}

/* Threads auxiliares */
void Engine::sincStructures(Region& nueva)
{
	int numeroElementos = nueva.Elementos.size();
	if (this->TotalAddresses < this->MAX_ADDRESSES)
	{
		EnterCriticalSection(&this->Critical);
		Region add;
		add.BaseAddress = nueva.BaseAddress;
		int elementosActuales = this->TotalAddresses;
		for (int i = 0; (elementosActuales < this->MAX_ADDRESSES) && (i < nueva.Elementos.size()); ++i)
		{
			add.Elementos.push_back(nueva.Elementos[i]);
			++elementosActuales;
		}
		this->Direcciones.push_back(add);
		LeaveCriticalSection(&this->Critical);
	}
	this->TotalAddresses.fetch_add(numeroElementos, memory_order_seq_cst);
}
void Engine::knownScanFirstStructures(StoredRegion& nueva)
{
	int numeroDirecciones = 0;
	Region nuevaRegion;
	nuevaRegion.BaseAddress = nueva.BaseAddress;
	int contador = this->TotalAddresses;
	for (int i = 0; i < nueva.RegionSize; i += this->Escaner.DataSize)
	{
		if (compareValue(nueva.MemoryContent + i, NULL))
		{
			if (contador < this->MAX_ADDRESSES)
			{
				Element elemento;
				elemento.offset = i;
				elemento.value = nueva.MemoryContent + i;
				nuevaRegion.Elementos.push_back(elemento);
				++contador;
			}
			++numeroDirecciones;
		}
	}
	if (contador != this->TotalAddresses)
	{
		EnterCriticalSection(&this->Critical);
		this->Direcciones.push_back(nuevaRegion);
		LeaveCriticalSection(&this->Critical);
	}
	if (numeroDirecciones != 0) this->TotalAddresses.fetch_add(numeroDirecciones, memory_order_seq_cst);
}

void Engine::threadUpdateValues(HANDLE& hProcess, Regions& Regiones)
{
	MEMORY_BASIC_INFORMATION mbi;
	Regions aux;
	for (int i = 0; i < Regiones.size(); ++i)
	{
		Region actual = Regiones[i];
		if (VirtualQueryEx(hProcess, actual.BaseAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == sizeof(MEMORY_BASIC_INFORMATION))
		{
			if ((mbi.State & MEM_COMMIT)
				&& (mbi.Protect & PAGE_READWRITE)
				&& !(mbi.Protect & PAGE_GUARD)
				&& !(mbi.Protect & PAGE_NOACCESS))
			{
				updateValues(hProcess, actual.BaseAddress, mbi.RegionSize, actual.Elementos);
			}

			if (!actual.Elementos.empty())
			{
				aux.push_back(actual);
				sincStructures(actual);
			}
		}
	}
	Regiones = move(aux);
}
void Engine::threadObtainAllMemory(HANDLE& hProcess, ThreadData*& data, StoredRegions& RegionesGuardadas)
{
	char* baseAddress = data->baseAddress;
	unsigned int regionSize = data->regionSize;

	StoredRegion nueva;
	nueva.BaseAddress = baseAddress;
	nueva.MemoryContent = new char[regionSize];
	nueva.RegionSize = regionSize;
	obtainAllMemory(hProcess, baseAddress, regionSize, nueva);
	RegionesGuardadas.push_back(nueva);
	if (this->Escaner.KnownValue) knownScanFirstStructures(nueva);
	else this->TotalAddresses.fetch_add(regionSize / this->Escaner.DataSize, memory_order_seq_cst);
}
void Engine::threadCompareEntireRegion(HANDLE& hProcess, StoredRegions& RegionesGuardadas, Regions& Regiones)
{
	for (int i = 0; i < RegionesGuardadas.size(); ++i)
	{
		compareEntireRegion(hProcess, RegionesGuardadas[i], Regiones);
	}
}

/* Threads */
DWORD WINAPI Engine::ReadThreadFunction(LPVOID lpParam)
{
	ThreadData* data = (ThreadData*)lpParam;
	Engine* instance = data->instance;
	int threadNumber = data->threadNumber;
	bool firstScan = instance->Escaner.First;
	bool clearStructures = data->clearStructures;
	bool knownValue = instance->Escaner.KnownValue;

	Regions Regiones;
	StoredRegions RegionesGuardadas;

	HANDLE hProcess = NULL;

	while (true)
	{
		DWORD event = WaitForSingleObject(instance->ThreadEvents[threadNumber], INFINITE);
		if (hProcess == NULL)
		{
			hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, instance->Escaner.ProcessID);
		}
		if (event == WAIT_OBJECT_0)
		{
			firstScan = instance->Escaner.First;
			clearStructures = data->clearStructures;
			knownValue = instance->Escaner.KnownValue;
	
			if (clearStructures)
			{
				RegionesGuardadas = StoredRegions{};
				Regiones = Regions{};
			}

			if (firstScan) instance->threadObtainAllMemory(hProcess, data, RegionesGuardadas);
			else if (Regiones.empty())
			{
				instance->threadCompareEntireRegion(hProcess, RegionesGuardadas, Regiones);
				RegionesGuardadas = StoredRegions{};
			}
			else instance->threadUpdateValues(hProcess, Regiones);

			SetEvent(instance->MainEvents[threadNumber]);
		}
	}
	return 0;
}

DWORD WINAPI Engine::WriteReadThreadFunction(LPVOID lpParam)
{
	WriteThreadData* data = (WriteThreadData*)lpParam;
	Engine* instance = data->instance;
	HANDLE hProcess = NULL;

	while (true)
	{
		if (instance->Persistentes.empty())
		{
			WaitForSingleObject(instance->WriteEvent, INFINITE);
		}

		if (hProcess == NULL) hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, instance->Escaner.ProcessID);
		MEMORY_BASIC_INFORMATION mbi; 

		EnterCriticalSection(&instance->Critical);
		map<char*, string>::iterator it = instance->Persistentes.begin(); 
		while (it != instance->Persistentes.end())
		{
			char* buffer = new char[instance->Escaner.DataSize];
			if(VirtualQueryEx(hProcess, it->first, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) 
			{
				if ((mbi.State & MEM_COMMIT)
					&& (mbi.Protect & PAGE_READWRITE)
					&& !(mbi.Protect & PAGE_GUARD)
					&& !(mbi.Protect & PAGE_NOACCESS))
				{
					instance->writeMemoryValue(hProcess, it->first, it->second);
				}
			}
			++it;
		}
		LeaveCriticalSection(&instance->Critical);
	}
}	
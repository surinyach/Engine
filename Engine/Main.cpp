#include <iostream>
#include <windows.h>
#include <psapi.h>
#include <tchar.h>
#include <map>
#include <cstring>
#include <string>
#include <set>
#include "Engine.h"

using namespace std;

typedef std::map <DWORD, basic_string<TCHAR>>::iterator ProcessesIt;
typedef std::map <DWORD, basic_string<TCHAR>> Processes;

Engine Motor;

/* Input */
static void InputProcess()
{	
	// Seleccionar el proceso con el que se quiere trabajar.
	cout << endl;
	cout << "Inserte el PID del proceso que quiere modificar:" << endl;
	DWORD processID; cin >> processID;
	Motor.insertProcessID(processID);
	std::cin.ignore();
}

static int ObtainDataTypeSize(string dataType)
{
	string types[4] = { "Byte", "2 Bytes", "4 Bytes", "8 Bytes" };

	for (int i = 0; i < 4; ++i) {
		if (types[i] == dataType)
		{
			switch (i) {
				case 0: return 1;
				case 1: return 2;
				case 2: return 4;
				default: return 8;
			}
		}
	}
	return 0;
}
static void InputDataType()
{
	cout << endl;
	string dataType;
	int dataSize;
	// Seleccionar el numero de bytes y el tipo de datos.
	cout << "Seleccione el tipo de datos que quiere buscar." << endl << "Los valores disponibles son: Byte, 2 Bytes, 4 Bytes, 8 Bytes:" << endl;
	std::cin.ignore();
	std::getline(std::cin, dataType);
	cout << "He introducido " << dataType << endl;
	dataSize = ObtainDataTypeSize(dataType);
	if (!dataSize)
	{
		cout << "Tipo de datos incorrecto" << endl;
		bool correct = false;
		while (!correct) {
			std::getline(std::cin, dataType);
			dataSize = ObtainDataTypeSize(dataType);
			if (dataSize != 0) correct = true;
		}
	}
	// Insertar valores en la clase
	Motor.insertDataType(dataType);
	Motor.insertDataSize(dataSize);
}

static void InputKnownValue()
{
	cout << endl;
	cout << "Inserte el valor que quiere buscar en memoria" << endl;
	string input; cin >> input;
	Motor.insertSearchValue(input);
}
static void InputValueState()
{
	cout << endl;
	cout << "Especifique el estado de su valor:" << endl;
	cout << "1. Incrementado" << endl << "2. Decrementado" << endl;
	cout << "Inserte el numero asociado al estado:" << endl;
	int state;  cin >> state;
	while (state > 2 || state < 0)
	{
		cout << "Estado incorrecto, pruebe otra vez:";
		cin >> state;

	}
	Motor.insertValueState(state);
}
static void ScanAllMemory()
{
	cout << endl;
	cout << "Obteniendo todos los valores de memoria..." << endl;
	cout << "En la siguiente busqueda, tendra que indicar si el valor ha incrementado o decrementado" << endl;
}
static void InputSearchValue()
{
	// Seleccionar el valor del dato que se quiere buscar
	cout << endl;

	bool primero = Motor.getScanFirst();
	int dataSize = Motor.getScanDataSize();

	if (primero)
	{
		cout << "Conoce el valor que quiere buscar? S/n" << endl;
		string known; cin >> known;
		while (known != "S" && known != "n")
		{
			cout << "Valor incorrecto. Inserte S/n, por favor." << endl;
			cin >> known;
		}

		if (known == "S") Motor.insertKnownValue(true);
		else Motor.insertKnownValue(false);
	}

	bool conocido = Motor.getScanKnownValue();

	// Dato conocido
	if (conocido) InputKnownValue();

	// Dato desconocido. No primer escaner.
	else if (!conocido && !primero) InputValueState();

	// Dato Desconocido. Primer escaner. 
	else ScanAllMemory(); 

	// Tipo de dato = String
	if (dataSize == -1)
	{
		string value = Motor.getSearchValue(); 
		Motor.insertDataSize(value.length());
	}
}
static void InputMemoryAddress()
{
	cout << endl;
	cout << "Inserte la direccion de memoria que quiere modificar:" << endl;
	cout << "Ejemplo de formato: 00007FFF1CDA5FA0" << endl;
	uintptr_t aux; cin >> hex >> aux;
	char* direccion = reinterpret_cast<char*>(aux);
	Motor.insertModifyAddress(direccion);
}
static void InputStoreValue()
{
	cout << endl;
	char* address = Motor.getModifyAddress();
	string dataType = Motor.getScanDataType();
	_tprintf(TEXT("Inserte el nuevo valor para la direccion de memoria %p\n"),address);
	cout << "El tipo de datos debe ser: " << dataType << endl;
	string modifyValue; cin >> modifyValue;
	Motor.insertModifyValue(modifyValue);
}
static void InputPersistentValue()
{
	cout << endl;
	cout << "Quiere que el valor introducido persista en memoria [s/N]" << endl;
	string persiste; cin >> persiste; 
	if (persiste == "s") Motor.insertPersistentValue(true);
	else if (persiste == "N") Motor.insertPersistentValue(false);
	else
	{
		cout << "Valor incorrecto. Introduzca s o N, por favor." << endl;
		while (persiste != "N" && persiste != "s") cin >> persiste; 
	}
}

static int InputOption()
{
	cout << endl;
	cout << "Seleccione la opcion que quiere llevar a cabo a continuacion" << endl;
	cout << "Las opciones disponibles son:" << endl << "1. Primera busqueda" << endl;
	cout << "2. Siguiente busqueda" << endl;
	cout << "3. Modificar valor de memoria" << endl;
	cout << "4. Salir" << endl << "Por favor, haga su seleccion con un numero del 1 al 4:" << endl;
	int option; cin >> option;

	while (option < 0 || option > 5)
	{
		cout << "Opción incorrecta, seleccione otra, por favor" << endl;
		cin >> option;
	}

	if (option == 2 && Motor.getScanFirst()) {
		cout << "Es necesario hacer un primer escaner para seleccionar esta opcion" << endl;
		while (option != 2)
		{
			cout << "Seleccione otra opción, por favor" << endl;
			cin >> option;
		}
	}
	else if (option == 3 && (Motor.getScanDataType() == "")) InputDataType();

	return option;
}

/* Output */
static void PrintProcessNameAndID() 
{
	Processes Procesos = Motor.getProcessesNameAndID();
	ProcessesIt iterador = Procesos.begin();

	while (iterador != Procesos.end())
	{
		basic_string<TCHAR> nombre = iterador->second;
		DWORD ID = iterador->first;
		_tprintf(TEXT("%s (PID: %u)\n"), nombre.c_str(), ID);
		++iterador;
	}
}
static void PrintInteger(char* data)
{
	int value;
	memcpy(&value, data, sizeof(int));
	cout << value << endl;
}
static void PrintMemoryValues()
{
	int numeroDirecciones = Motor.getAddressesNumber();
	int maximasDirecciones = Motor.getMaxAddresses();
	const Regions Direcciones = Motor.getAddresses();
	bool primero = Motor.getScanFirst(); 
	bool conocido = Motor.getScanKnownValue(); 

	int direccionesMostradas = 0;
	bool overflow = numeroDirecciones > maximasDirecciones;

	cout << endl << "Se han obtenido " << numeroDirecciones << " direcciones." << endl << endl;
	
	if (!primero || conocido)
	{
		for (int i = 0; i < Direcciones.size(); ++i)
		{
			const Region actual = Direcciones[i];
			char* baseAddress = actual.BaseAddress;
			int numeroElementos = actual.Elementos.size();
			for (int j = 0; j < numeroElementos; ++j)
			{
				_tprintf(TEXT("%p - "), baseAddress + actual.Elementos[j].offset);
				PrintInteger(actual.Elementos[j].value);
				++direccionesMostradas;
			}
		}
		if (overflow) cout << endl << "Se han mostrado las primeras " << direccionesMostradas << " direcciones" << endl;
	}
	

}

static void PrintNewScan()
{
	cout << endl;
	cout << "Ninguna direccion previamente listada contiene el nuevo valor" << endl;
	cout << "Empezando nuevo escaner..." << endl;
}
	
int main(void)
{
	
	PrintProcessNameAndID();
	InputProcess(); 

	// Escoger opcion tras seleccionar proceso.
	int opcion = 0;
	while (opcion != 4) 
	{
		opcion = InputOption();
		switch (opcion)
		{
			case 1:
			{
				// Primer escaneo
				_tprintf(TEXT("Nuevo escaner para el proceso: %d\n"), Motor.getProcessID());
				Motor.insertScanFirst(true);

				// Input de datos
				InputDataType(); 
				InputSearchValue();

				// Escaner de memoria
				Motor.firstMemoryScan();

				// Imprimir valores
				PrintMemoryValues(); 

				// Modificar variable de primer escaner
				Motor.insertScanFirst(false);
				break;
			}
			case 2:
			{
				// Escaneos posteriores al primero

				// Obtencion de variables
				bool knownValue = Motor.getScanKnownValue();

				// Input de datos 
				if (!knownValue) InputValueState();
				else InputSearchValue();

				// Escaner de memoria
				Motor.consecutiveMemoryScan();

				int numeroDirecciones = Motor.getAddressesNumber();
				if (numeroDirecciones > 0) PrintMemoryValues();
				else PrintNewScan();
				break;
			}
			case 3:
			{
				//Modificar el valor en memoria

				// Input de datos
				InputMemoryAddress();
				InputStoreValue();
				InputPersistentValue(); 

				// Modificar valor de memoria
				Motor.modifyMemoryValue();
				break;
			}
		}
	}

	return 0;
}
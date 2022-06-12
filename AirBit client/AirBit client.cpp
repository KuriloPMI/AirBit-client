// AirBit client.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

//Список разрешённых EUI:
//0x99,0x99,0xE8,0xEB,0x11,0x41,0x96,0x65
//0x00,0xAE,0xAE,0xFF,0xFE,0x00,0x00,0x00
//0x00,0x00,0xE8,0xEB,0x11,0x39,0xEF,0x7C
//0x00,0x00,0xE8,0xEB,0x11,0x41,0x9C,0x72
//0x00,0x00,0xE8,0xEB,0x11,0x41,0xA9,0x44
//0x72,0x76,0xFF,0x00,0x39,0x03,0x09,0x16
//0x64,0x7F,0xDA,0xFF,0xFE,0x00,0x5C,0x90
//0x00,0x00,0x94,0xE3,0x6D,0xCE,0xDC,0x39
//0x00,0x00,0x94,0xE3,0x6D,0xCE,0xDC,0x7B

#include <iostream>
#include <WS2tcpip.h>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")

//string readFile(const string& fileName)

void main(int argc, char* argv[]) // We can pass in a command line option!! 
{
	int n = 6000,//максимальное число пакетов
		timeno = 2000,//время в мс до объявления пакета потерянным
		ispush = 0,//
		sendOk = 0,//код ошибки при отправке или его отсутствие
		rcvOk = 0,//код ошибки при получении или длинна сообщения при отсутствии ошибок
		wsOk = 0,//пременная, которая определяет тип пакета
		packsent = 0,//пакетов отправлено всего
		packrcv = 0,//пакетов получено всего
		msgloop = 0;//переменная для зацикливания вектора jsonов

	int EUIval = 0;//значение от 0 до 8 соответвующее одному EUI, при распараллеливании у каждого потока должено быть своё значение

	char EUI[9][8] = {
		0x99,0x99,0xE8,0xEB,0x11,0x41,0x96,0x65,
	0x00,0xAE,0xAE,0xFF,0xFE,0x00,0x00,0x00,
	0x00,0x00,0xE8,0xEB,0x11,0x39,0xEF,0x7C,
	0x00,0x00,0xE8,0xEB,0x11,0x41,0x9C,0x72,
	0x00,0x00,0xE8,0xEB,0x11,0x41,0xA9,0x44,
	0x72,0x76,0xFF,0x00,0x39,0x03,0x09,0x16,
	0x64,0x7F,0xDA,0xFF,0xFE,0x00,0x5C,0x90,
	0x00,0x00,0x94,0xE3,0x6D,0xCE,0xDC,0x39,
	0x00,0x00,0x94,0xE3,0x6D,0xCE,0xDC,0x7B
	};

	ifstream ifs("packets.txt");//считываем значения из файла в вектор строк jsons
	string line;
	vector<string> jsons;
	while (getline(ifs, line))
	{
		jsons.push_back(line);
	}
	ifs.close();



	chrono::seconds maxtime(60);//максимальное время теста
	chrono::milliseconds minreply(timeno + 1),//минимальное время ожидания ответа
		                 maxreply(0),//максимальное время ожидания ответа
		                 timesum(0);//суммарное время ожидания ответа
	cout << "Specify test duration: 0" << endl
		<< "Specify packet amount: 1" << endl
		<< "Continue using default settings: anything else" << endl;
	string str;
	cin >> str;
	switch (str[0])
	{
	case '0':
	{
		int inp;
		cout << "Enter test duration time in seconds: ";
		cin >> inp;
		maxtime = chrono::seconds(inp);
		n = inp * 1000;
		break;
	}
	case '1':
	{
		int inp;
		cout << "Enter packet amount: ";
		cin >> inp;
		n = inp;
		maxtime = chrono::seconds(inp * timeno / 1000);
		break;
	}
	default:
		break;
	}

	////////////////////////////////////////////////////////////
	// INITIALIZE WINSOCK
	////////////////////////////////////////////////////////////

	// Structure to store the WinSock version. This is filled in
	// on the call to WSAStartup()
	WSADATA data;

	// To start WinSock, the required version must be passed to
	// WSAStartup(). This server is going to use WinSock version
	// 2 so I create a word that will store 2 and 2 in hex i.e.
	// 0x0202
	WORD version = MAKEWORD(2, 2);

	// Start WinSock
	wsOk = WSAStartup(version, &data);
	if (wsOk != 0)
	{
		// Not ok! Get out quickly
		cout << "Can't start Winsock! " << wsOk;
		return;
	}

	////////////////////////////////////////////////////////////
	// CONNECT TO THE SERVER
	////////////////////////////////////////////////////////////

	// Create a hint structure for the server
	sockaddr_in server;
	server.sin_family = AF_INET; // AF_INET = IPv4 addresses
	server.sin_port = htons(8001); // Little to big endian conversion
	inet_pton(AF_INET, "80.92.13.228", &server.sin_addr); // Convert from string to byte array

	// Socket creation, note that the socket type is datagram
	SOCKET out = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);


	setsockopt(out, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeno, sizeof(int));


	// Write out to that socket

	chrono::steady_clock::time_point startprogram = chrono::high_resolution_clock::now();//время начала теста
	//начало основного цикла
	for (int i = 0; (i < n) && ((timesum + ((packsent - packrcv) * chrono::milliseconds(timeno))) < maxtime); i++)
	{
		ispush = i % 10;
		if (ispush == 0)
		{
			char pull_request_text[12] = //pull_data
			{
				0x01,//Protocol version
				0x00, 0x00,//Token
				0x02,//PULL_DATA identifier
				//0x64,0x7F,0xDA,0xFF,0xFE,0x00,0x5C,0x90//Gateway EUI
			};
			for (int j = 4; j < 12; j++)//посимвольно копируем значения из массива char c EUI в массив с пакетом
				pull_request_text[j] = EUI[EUIval][j-4];
			cout << "pull: ";
			sendOk = sendto(out, pull_request_text, 12, 0, (sockaddr*)&server, sizeof(server));
		}
		else
		{
			char spush_text[1024] = //push_data
			{
				0x01,//Protocol version
				0x00, 0x00,//Token
				0x00,//PUSH_DATA identifier
				//0x64,0x7F,0xDA,0xFF,0xFE,0x00,0x5C,0x90//Gateway EUI
			};
			for (int j = 4; j < 12; j++)//посимвольно копируем значения из массива char c EUI в массив с пакетом
				spush_text[j] = EUI[EUIval][j - 4];

			int packet_len;

			int fwefwef = i % jsons.size();

			for (int j = 0; (j < 1012) && (jsons[fwefwef][j] != '\0'); j++)//посимвольно копируем значения из строки в векторе jsonов в массив char с пакетом
			{
				packet_len = j;
				
				spush_text[j + 12] = jsons[fwefwef][j];
			}

			cout << "push: ";
			sendOk = sendto(out, spush_text, packet_len + 13, 0, (sockaddr*)&server, sizeof(server));
		}

		chrono::steady_clock::time_point start = chrono::high_resolution_clock::now();//начинаем ждать ответ

		if (sendOk == SOCKET_ERROR)
		{
			cout << "That didn't work! " << WSAGetLastError() << endl;
		}
		else
		{
			cout << "packet sent" << endl;
			packsent++;
		}

		char buffer[2408];
		int len = sizeof(server);

		rcvOk = recvfrom(out, (char*)buffer, 2407, 0, (sockaddr*)&server, &len);

		chrono::steady_clock::time_point stop = chrono::high_resolution_clock::now();//дожидаемся ответа
		chrono::milliseconds duration = chrono::duration_cast<chrono::milliseconds>(stop - start);//вычисляем время ожидания

		if (rcvOk == SOCKET_ERROR)
		{
			cout << "read failed: ";
			switch (WSAGetLastError())
			{
			case WSAEMSGSIZE:
				cout << "buffer too small" << endl;
				break;
			case WSAETIMEDOUT:
				cout << "too long!" << endl;
				break;
			default:
				cout << "error #" << WSAGetLastError() << endl;
				break;
			}
		}
		else
		{
			buffer[rcvOk] = '\0';
			cout << " Server : " << buffer << endl << " Time : " << duration.count() << "ms" << endl;		
			packrcv++;
			if (duration < minreply)
				minreply = duration;
			if (duration > maxreply)
				maxreply = duration;
			timesum += duration;
		}
	}
	//конец основного цикла

	// Close the socket
	closesocket(out);

	// Close down Winsock
	WSACleanup();
	
	//результаты теста
	int avgwait = timesum.count() / packrcv,
		losperc = 100 - (100 * (float(packrcv) / float(packsent)));
	chrono::milliseconds exectime = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - startprogram);

	cout << endl << "Execution time:\t\t" << exectime.count() << "ms" << endl//время выполнения теста
		<< "Packets sent:\t\t" << packsent << endl//число отправленных пакетов
		<< "Packets recieved:\t" << packrcv << endl//число принятых пакетов
		<< "Loss percentage:\t" << losperc << '%' << endl//процент потерянных пакетов
		<< "Total time:\t\t" << timesum.count() << "ms" << endl//суммарное время ожидания пакетов(не учитывая потерянные)
		<< "Shortest wait time:\t" << minreply.count() << "ms" << endl//самое быстрое время ожидания ответа
		<< "Longest wait time:\t" << maxreply.count() << "ms" << endl//самое долгое время ожидания ответа
		<< "Avg wait time:\t\t" << avgwait << "ms" << endl;//среднее время ожидания ответа
}

// Запуск программы: CTRL+F5 или меню "Отладка" > "Запуск без отладки"
// Отладка программы: F5 или меню "Отладка" > "Запустить отладку"

// Советы по началу работы 
//   1. В окне обозревателя решений можно добавлять файлы и управлять ими.
//   2. В окне Team Explorer можно подключиться к системе управления версиями.
//   3. В окне "Выходные данные" можно просматривать выходные данные сборки и другие сообщения.
//   4. В окне "Список ошибок" можно просматривать ошибки.
//   5. Последовательно выберите пункты меню "Проект" > "Добавить новый элемент", чтобы создать файлы кода, или "Проект" > "Добавить существующий элемент", чтобы добавить в проект существующие файлы кода.
//   6. Чтобы снова открыть этот проект позже, выберите пункты меню "Файл" > "Открыть" > "Проект" и выберите SLN-файл.

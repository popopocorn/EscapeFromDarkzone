#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <string>

#pragma comment (lib, "WS2_32.LIB")

constexpr short SERVER_PORT = 3000;

enum class EventType {
	Input,
	Timeout,
};

enum class INPUT_KEY {
	UP = VK_UP,
	DOWN = VK_DOWN,
	LEFT = VK_LEFT,
	RIGHT = VK_RIGHT,

	W = 'W',
	A = 'A',
	S = 'S',
	D = 'D',

	E = 'E',
	G = 'G',
	I = 'I',

	KEY_1 = '1',
	KEY_2 = '2',
	KEY_3 = '3',
	KEY_4 = '4',

	SHIFT = VK_SHIFT,

	LBUTTON = VK_LBUTTON,
	RBUTTON = VK_RBUTTON,
};

enum class KEY_STATE
{
	NONE,
	HOLD,
	DOWN,
	UP
};

struct InputEvent {
	INPUT_KEY key;
	KEY_STATE state;
};

struct GameEvent {
	EventType type;
	InputEvent keyEvent;
};

void CALLBACK g_recv_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);
void CALLBACK g_send_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);

void print_error_message(int s_err)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, s_err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << lpMsgBuf << std::endl;
	while (true);   // 디버깅 용
	LocalFree(lpMsgBuf);
}

class EXP_OVER
{
public:
	EXP_OVER(long long id, char* mess) : _id(id)
	{
		ZeroMemory(&_send_over, sizeof(_send_over));

		auto  packet_size = 2 + strlen(mess);
		if (packet_size > 255) {
			std::cout << "MESSAGE TOO LONG";
			exit(-1);
		}
		_send_buffer[0] = static_cast<unsigned char>(packet_size);
		_send_buffer[1] = static_cast<unsigned char>(_id);
		_send_wsabuf[0].buf = _send_buffer;
		_send_wsabuf[0].len = static_cast<ULONG>(packet_size);
		strcpy_s(_send_buffer + 2, sizeof(_send_buffer) - 2, mess);
	}

	WSAOVERLAPPED	_send_over;
	long long		_id;
	char			_send_buffer[1024];
	WSABUF			_send_wsabuf[1];
};

class SESSION;

std::unordered_map<long long , SESSION> g_users;

class SESSION {
private:
	SOCKET			_c_socket;
	long long		_id;

	WSAOVERLAPPED	_recv_over;
	char			_recv_buffer[1024];
	WSABUF			_recv_wsabuf[1];
	char			_remain_buffer[2048];
	size_t			_remain_pos;

	void do_recv()
	{
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over, sizeof(_recv_over));
		_recv_over.hEvent = reinterpret_cast<HANDLE>(_id);
		auto ret = WSARecv(_c_socket, _recv_wsabuf, 1, NULL, &recv_flag, &_recv_over, g_recv_callback);
		if (0 != ret) {
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no) {
				print_error_message(err_no);
				exit(-1);
			}
		}
	}

public:
	SESSION() {
		std::cout << "DEFAULT SESSION CONSTRUCTOR CALLED!!\n";
		exit(-1);
	}
	SESSION(long long session_id, SOCKET s) : _id(session_id), _c_socket(s)
	{
		_recv_wsabuf[0].len = sizeof(_recv_buffer);
		_recv_wsabuf[0].buf = _recv_buffer;
		_remain_pos = 0;

		_recv_over.hEvent = reinterpret_cast<HANDLE>(session_id);

		do_recv();
	}
	~SESSION()
	{
		closesocket(_c_socket);
	}

	void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
	{
		// 넘겨받은 버퍼 처리

		memcpy(_remain_buffer + _remain_pos, _recv_buffer, num_bytes);
		_remain_pos += num_bytes;

		size_t curr_pos = 0;
		GameEvent ev;
		std::string sOutput;

		while (_remain_pos - curr_pos >= sizeof(GameEvent)) {
			memcpy(&ev, _remain_buffer + curr_pos, sizeof(GameEvent));

			sOutput.clear();
			// 콘솔 출력
			switch (ev.type) {
			case EventType::Input:
			{
				if (ev.keyEvent.key <= INPUT_KEY::RBUTTON) {
					sOutput += "BUTTON ";
					switch (ev.keyEvent.key) {
					case INPUT_KEY::LBUTTON:
						sOutput += "L ";
						break;
					default:
						sOutput += "R";
						break;
					}
				}
				else if (ev.keyEvent.key <= INPUT_KEY::SHIFT) {
					sOutput += "SHIFT ";
				}
				else if (ev.keyEvent.key <= INPUT_KEY::DOWN) {
					sOutput += "ARROW ";
					switch (ev.keyEvent.key) {
					case INPUT_KEY::LEFT:
						sOutput += "L ";
						break;
					case INPUT_KEY::UP:
						sOutput += "U ";
						break;
					case INPUT_KEY::RIGHT:
						sOutput += "R ";
						break;
					default:
						sOutput += " D";
						break;
					}
				}
				else if (ev.keyEvent.key <= INPUT_KEY::KEY_4) {
					sOutput += "NUM ";
				}
				else {
					sOutput += "ALPHABET ";
					switch (ev.keyEvent.key) {
					case INPUT_KEY::W:
						sOutput += "W ";
						break;
					case INPUT_KEY::A:
						sOutput += "A ";
						break;
					case INPUT_KEY::S:
						sOutput += "S ";
						break;
					case INPUT_KEY::D:
						sOutput += "D ";
						break;
					default:
						break;
					}
				}

				if (ev.keyEvent.state == KEY_STATE::DOWN) {
					sOutput += "DOWN. ";
				}
				else if (ev.keyEvent.state == KEY_STATE::UP) {
					sOutput += "UP. ";
				}
			}
			break;
			case EventType::Timeout:
			default:
				sOutput = "Timeout";
				break;
			}

			std::cout << sOutput << '\n';

			curr_pos += sizeof(GameEvent);
		}
		ZeroMemory(&ev, sizeof(GameEvent));

		memcpy(_remain_buffer, _remain_buffer + curr_pos, _remain_pos - curr_pos);
		_remain_pos = _remain_pos - curr_pos;

		do_recv();
	}

	void do_send(long long id, char* mess)
	{
		EXP_OVER* o = new EXP_OVER(id, mess);
		DWORD size_sent;
		WSASend(_c_socket, o->_send_wsabuf, 1, &size_sent, 0, &(o->_send_over), g_send_callback);
	}
};

void CALLBACK g_send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	EXP_OVER* o = reinterpret_cast<EXP_OVER*>(p_over);
	delete o;
}

void CALLBACK g_recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	auto my_id = reinterpret_cast<long long>(p_over->hEvent);
	g_users[my_id].recv_callback(err, num_bytes, p_over, flag);
}

int main()
{
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);;
	if (s_socket <= 0) std::cout << "ERRPR" << "원인";
	else std::cout << "Socket Created.\n";

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN));
	listen(s_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	long long client_id = 0;
	while (true) {
		auto c_socket = WSAAccept(s_socket,
			reinterpret_cast<sockaddr*>(&addr), &addr_size,
			NULL, NULL);
		g_users.try_emplace(client_id, client_id, c_socket);

		client_id++;
	}
	closesocket(s_socket);
	WSACleanup();
}
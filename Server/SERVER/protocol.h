constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;

constexpr int MAX_USER = 10000;

constexpr int W_WIDTH = 400;
constexpr int W_HEIGHT = 400;

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;

constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_ADD_PLAYER = 3;
constexpr char SC_REMOVE_PLAYER = 4;
constexpr char SC_MOVE_PLAYER = 5;

// CS_MOVE_PACKET inputs 비트 플래그
constexpr char MOVE_W = 0x01;
constexpr char MOVE_S = 0x02;
constexpr char MOVE_A = 0x04;
constexpr char MOVE_D = 0x08;

#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned char size;
	char	type;
	char	name[NAME_SIZE];
};

//struct CS_MOVE_PACKET {
//	unsigned char size;
//	char	type;
//	char	direction;  // 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT
//	unsigned	move_time;
//};
//struct CS_MOVE_PACKET {
//	unsigned char size;
//	char          type;
//	float         x, y, z;        // direction 제거, 좌표로 대체
//	unsigned int  move_time;
//};
// 클라이언트 -> 서버: 입력 정보 전송 (서버가 좌표 계산)
struct CS_MOVE_PACKET {
	unsigned char size;
	char          type;
	char          inputs;       // WASD 비트 플래그 (MOVE_W, MOVE_S, MOVE_A, MOVE_D)
	float         yaw;          // 카메라 Yaw (도 단위) — 이동 방향 계산용
	unsigned int  move_time;    // 패킷 순서 보정용 
};

//struct SC_LOGIN_INFO_PACKET {
//	unsigned char size;
//	char	type;
//	short	id;
//	short	x, y;
//};
struct SC_LOGIN_INFO_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;        // short → float
};

//struct SC_ADD_PLAYER_PACKET {
//	unsigned char size;
//	char	type;
//	short	id;
//	short	x, y;
//	char	name[NAME_SIZE];
//};
struct SC_ADD_PLAYER_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;        // short → float
	char          name[NAME_SIZE];
};

struct SC_REMOVE_PLAYER_PACKET {
	unsigned char size;
	char	type;
	short	id;
};

//struct SC_MOVE_PLAYER_PACKET {
//	unsigned char size;
//	char	type;
//	short	id;
//	short	x, y;
//	unsigned int move_time;
//};
struct SC_MOVE_PLAYER_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;        // short → float
	unsigned int  move_time;
};

#pragma pack (pop)
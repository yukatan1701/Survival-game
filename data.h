#define MOVE_UP 0
#define MOVE_DOWN 1
#define MOVE_LEFT 2
#define MOVE_RIGHT 3
#define MOVE_NONE 4
#define CL_QUIT_CODE 2
#define SR_END_CODE 3
#define SR_CHEST_CODE 4
#define SR_DEATH_CODE 5
#define SR_HEALTH_CHGD_CODE 6
#define SR_WIN_CODE 7
#define CHEST_INTERVAL 5
#define HEALTH_REDUCE 1 
#define MAX_HEALTH 10
#define GOOD_CHEST_ID 100
#define BAD_CHEST_ID 101
#define CHEST_CHAR '@'
#define GOOD_CHEST_VAL 0
#define BAD_CHEST_VAL 1

typedef struct client_data {
	char id;
	char move;
	char code;
	char other;
} Data;

typedef struct basic_data {
	char players;
	char width;
	char height;
	char id;
} BasicData;

typedef struct player {
	char id;
	char x, y;
	char active;
	char health;
} Player;

typedef struct map_cell {
	char id;
	char image;
} MapCell;

void print_data(FILE *fd, Data data)
{
	fprintf(fd, "Id: %d, move: %d, code: %d, other: %d.\n", 
	       data.id, data.move, data.code, data.other);
}

BasicData unpack_basic(int data)
{
	BasicData basic_data = {0, 0, 0};
	basic_data.players = data & 0xFF;
	data >>= 8;
	basic_data.width = data & 0xFF;
	data >>= 8;
	basic_data.height = data & 0xFF;
	data >>= 8;
	basic_data.id = data && 0xFF;
	return basic_data;
}

Data unpack(int data)
{
	Data data_pack = {0, 0, 0};
	data_pack.id = data & 0xFF;
	data >>= 8;
	data_pack.move = data & 0xFF;
	data >>= 8;
	data_pack.code = data & 0xFF;
	data >>= 8;
	data_pack.other = data & 0xFF;
	return data_pack;
}

int pack_basic(BasicData data, int i)
{
	int package = i;
	package <<= 8;
	package |= data.height;
	package <<= 8;
	package |= data.width;
	package <<= 8;
	package |= data.players;
	return package;
}

int pack(Data data)
{
	int package = data.other;
	package <<= 8;
	package |= data.code;
	package <<= 8;
	package |= data.move;
	package <<= 8;
	package |= data.id;
	return package;
}

int pack_player(Player pl)
{
	int pack = 0;
	pack |= pl.health;
	pack <<= 8;
	pack |= pl.active;
	pack <<= 8;
	pack |= pl.y;
	pack <<= 8;
	pack |= pl.x;
	pack <<= 8;
	pack |= pl.id;
	return pack;
}

Player unpack_player(int pack)
{
	Player pl;
	pl.id = pack & 0xFF;
	pack >>= 8;
	pl.x = pack & 0xFF;
	pack >>= 8;
	pl.y = pack & 0xFF;
	pack >>= 8;
	pl.active = pack & 0xFF;
	pack >>= 8;
	pl.health = pack & 0xFF;
	return pl;
}

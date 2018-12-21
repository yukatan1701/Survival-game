#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "data.h"

BasicData basic_data;
MapCell **table;
int *client_sockets;
Player *list;
int fd[2], *forks;
int last_alive = -1;

void allocation_error()
{
	puts("Failed to allocate memory.");
	exit(1);
}

int open_server(int port)
{
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    int socket_option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
			&socket_option, sizeof(socket_option));
    struct sockaddr_in server_address;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
    server_address.sin_family = AF_INET;
    bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    listen(server_socket, 5);
	return server_socket;
}

int accept_client(int server_socket, struct sockaddr_in *client)
{
    socklen_t size = sizeof(*client);
    return accept(server_socket,(struct sockaddr *) client, &size);
}

int *connect_players(int port, int players)
{
	int server_socket = open_server(port);
	int *client_sockets = malloc(sizeof(int) * players);
	if (client_sockets == NULL)
		allocation_error();
	for (int i = 0; i < players; i++) {
		struct sockaddr_in client;
        client_sockets[i] = accept_client(server_socket, &client);
        char *addr = inet_ntoa(client.sin_addr);
        int port = ntohs(client.sin_port);
        printf("Connected: %s %d\n", addr, port);
    }
	return client_sockets;
}

void read_args(int argc, char *argv[], int *port, int *players, 
               char **map_name)
{
	// args: 1 - count of users, 2 - map_name, 3 - port
	if (argc < 2) {
		puts("Too few arguments.");
		exit(1);
	}	
	const int max_players = 10;
	*players = atoi(argv[1]);
	if (*players > max_players) {
		puts("The maximum number of players is 10.");
		exit(1);
	}
	if (*players < 1) {
		puts("Too few players (min = 1).");
		exit(1);
	}
	if (argv[2] == NULL) {
		char path[] = "1.map";
		*map_name = malloc(sizeof(path) + 1);
		memset(*map_name, 0, sizeof(path) + 1);
		strcpy(*map_name, path);
	} else {
		*map_name = malloc(strlen(argv[2]) + 1);
		memset(*map_name, 0, strlen(argv[2]) + 1);
		strcpy(*map_name, argv[2]);
	}
	if (argv[3] == NULL)
		*port = 8080;
	else {
		*port = atoi(argv[3]);
		if (*port < 1024) {
			puts("Ports in the range of 0-1023 are reserved by the system.");
			exit(1);
		}
	}
}

void close_clients(int **client_sockets, int players)
{
	for (int i = 0; i < players; i++) {
		close((*client_sockets)[i]);
	}
	free(*client_sockets);
	*client_sockets = NULL;
}

void send_to_all(Data reply, int *client_sockets, int players)
{
	//int plain_reply = pack(reply);
	for (int i = 0; i < players; i++) {
		if (write(client_sockets[i], &reply, sizeof(Data)) < 0) {
			perror("Failed to write to socket");
			exit(1);
		}
	}
}

void death_message(int dead_id) {
	Data death_info = {dead_id, 0, SR_DEATH_CODE, 0};
	send_to_all(death_info, client_sockets, basic_data.players);
}

int check_pos(int x, int y, MapCell **table) {	
	if (table[y][x].image != '#' && ((unsigned) table[y][x].id >= 10))
		return 1;
	return 0;
}

void game_over() {
	Data end_info = {last_alive, 0, SR_END_CODE, 0};
	send_to_all(end_info, client_sockets, basic_data.players);
	close(fd[0]);
	close(fd[1]);
	for (int i = 0; i < basic_data.players; i++)
		kill(forks[i], SIGTERM);
	puts("Game over!");
	close_clients(&client_sockets, basic_data.players);
	exit(0);
}

int check_alive() {
	int alive = 0, max_health = list[0].health;
	if (last_alive == -1)
		last_alive = 0;
	for (int i = 0; i < basic_data.players; i++) {
		alive += list[i].active;
		if (list[i].active && list[i].health > max_health) {
			max_health = list[i].health; 
			last_alive = i;
		}
	}
	if (alive == 1) {
		alive = 0;
	}
	return alive;
}

void reduce_health(int id, int direction) {
	int i = 0, end = basic_data.players;
	if (id != -1) {
		i = id;
		end = i + 1;
	}
	for ( ; i < end; i++) {
		if (list[i].active) {
			if (direction < 0)
				list[i].health -= HEALTH_REDUCE;
			else
				list[i].health += HEALTH_REDUCE;
		}
		if (list[i].health <= 0) {
			printf("Player %d is dead.\n", i);
			list[i].active = 0;
			death_message(i);
		}
	}
}

Data check_data(Data data, Player *list, MapCell **table)
{
	Data reply = {data.id, MOVE_NONE, data.code, 0};
	int move = data.move, id = data.id;
	int old_x = list[id].x, old_y = list[id].y;
	switch (move) {
		case MOVE_UP:
			if (check_pos(old_x, old_y - 1, table)) {
				list[id].y -= 1;
				reply.move = MOVE_UP;
			}
			break;
		case MOVE_DOWN:
			if (check_pos(old_x, old_y + 1, table)) {
				list[id].y += 1;
				reply.move = MOVE_DOWN;
			}
			break;
		case MOVE_LEFT:
			if (check_pos(old_x - 1, old_y, table)) {
				list[id].x -= 1;
				reply.move = MOVE_LEFT;
			}
			break;
		case MOVE_RIGHT:
			if (check_pos(old_x + 1, old_y, table)) {
				list[id].x += 1;
				reply.move = MOVE_RIGHT;
			}
			break;
	}
	int new_y = list[id].y, new_x = list[id].x;
	table[old_y][old_x].image = ' ';
	table[old_y][old_x].id = -1;
	int cell_id = table[new_y][new_x].id;
	if (cell_id == GOOD_CHEST_ID || cell_id == BAD_CHEST_ID) {
		reply.code = SR_HEALTH_CHGD_CODE;
		if (cell_id == GOOD_CHEST_ID) {
			reduce_health(id, 1);
			reply.other = GOOD_CHEST_VAL;
		} else {
			reduce_health(id, -1);
			reply.other = BAD_CHEST_VAL;
		}
	}
	if (check_alive() == 0)
		game_over();
	//printf("Health (id = %d): %d\n", id, list[id].health);
	table[new_y][new_x].image = '0' + id;
	table[new_y][new_x].id = id;
	return reply;
}

// генерация аптечки
void chest_alarm(int var) {
	int width = basic_data.width, height = basic_data.height;
	time_t t;
	srand((unsigned) time(&t));
	int chest_type = rand() % 2;
	printf("Chest created. Type: %d.\n", chest_type);
	int chest_x = 0, chest_y = 0; 
	while (1) {
		chest_x = rand() % width;
		chest_y = rand() % height;
		MapCell *cur = &table[chest_y][chest_x];
		if (cur->image != ' ')
			continue;
		if (cur->id == -1) {
			if (chest_type == 0) {
				cur->id = GOOD_CHEST_ID;
			}
			else {
				cur->id = BAD_CHEST_ID;
			}
			break;
		}
	}
	Data chest_info = {(char) chest_x, (char) chest_y, SR_CHEST_CODE, chest_type}; 
	send_to_all(chest_info, client_sockets, basic_data.players);
	reduce_health(-1, -1);
	if (check_alive() == 0)
		game_over();
	alarm(CHEST_INTERVAL);
}

void start_game(int *client_sockets, int players, Player *list, MapCell **table)
{
	pipe(fd);
	forks = malloc(sizeof(int) * players);
	for (int i = 0; i < players; i++) {
		if ((forks[i] = fork()) == 0) {
			// read from socket and write to pipe
			int user_data;
			while (1) {
				if (read(client_sockets[i], &user_data, 4) < 0) {
					perror("Failed to read from client");
					kill(getppid(), SIGTERM);
				}
				if (write(fd[1], &user_data, 4) < 0) {
					perror("Failed to write to pipe");
					kill(getppid(), SIGTERM);
				};
			}
		}
	}
	signal(SIGALRM, chest_alarm);
	alarm(CHEST_INTERVAL);
	int client_data;
	while (1) {
		if (read(fd[0], &client_data, 4) < 0) {
			perror("Failed to read from pipe");
			exit(1);
		}
		Data data = unpack(client_data);
		Data reply = check_data(data, list, table);
		send_to_all(reply, client_sockets, players);
		if (reply.code == SR_END_CODE) {
			break;
		}
	}
	game_over();
}

char gc(FILE *stream) {
	char ch = fgetc(stream);
	while (ch != '#' && ch != ' ' && ch != EOF)
		ch = fgetc(stream);
	return ch;
}

char **load_map(BasicData *data, char *map_name)
{
	char dir[] = "maps/";
	int path_len = strlen(map_name) + strlen(dir);
	char *path = malloc(sizeof(char) * (path_len + 1));
	if (path == NULL)
		allocation_error();
	memset(path, 0, path_len + 1);
	sprintf(path, "%s%s", dir, map_name);
	FILE *map_file = fopen(path, "r");
	if (map_file == NULL) {
		perror("Failed to load map");
		exit(1);
	}
	int height, width;
	fscanf(map_file, "%d %d\n", &height, &width);
	data->width = width;
	data->height = height;
	char **map = malloc(sizeof(char *) * height);
	if (map == NULL)
		allocation_error();
	char ch;
	for (int i = 0; i < height; i++) {
		map[i] = malloc(sizeof(char) * (width + 10));
		if (map[i] == NULL)
			allocation_error();
		memset(map[i], 0, width + 1);
		for (int j = 0; j < width; j++) {
			ch = gc(map_file);
			map[i][j] = ch;
		}
	}	
	fclose(map_file);
	return map;
}

void print_map(MapCell **map, int height, int width) {
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			putchar(map[i][j].image);
		}
		putchar('\n');
	}
}

void send_map(char **map, BasicData data, int dest_socket)
{
	for (int i = 0; i < data.height; i++) {
		if (write(dest_socket, map[i], data.width) < 0) {
			perror("Failed to send map");
			exit(1);
		}
	}
}

MapCell **map_to_table(char **map, int height, int width)
{
	MapCell **table = malloc(sizeof(MapCell *) * height);
	if (table == NULL)
		allocation_error();
	for (int i = 0; i < height; i++) {
		table[i] = malloc(sizeof(MapCell) * width);
		if (table[i] == NULL)
			allocation_error();
		for (int j = 0; j < width; j++) {
			MapCell cell;
			cell.id = -1;
			cell.image = map[i][j];
			table[i][j] = cell;
		}
	}
	return table;
}

char **free_map(char **map, int height)
{
	for (int i = 0; i < height; i++)
		free(map[i]);
	free(map);
	return NULL;
}

MapCell **send_basic_data(BasicData *data, int *client_sockets, int players, char *map_name)
{
	char **map = load_map(data, map_name);
	for (int i = 0; i < players; i++) {
		int plain_data = pack_basic(*data, i);
		if (write(client_sockets[i], &plain_data, 4) < 0) {
			perror("Failed to send basic info");
			exit(1);
		}
		send_map(map, *data, client_sockets[i]);
	}
	MapCell **table = map_to_table(map, data->height, data->width);
	map = free_map(map, data->height);
	return table;
}

void generate_random_position(int id, MapCell **table, char *x, char *y, 
                              BasicData data)
{
	int width = data.width, height = data.height;
	time_t t;
	srand((unsigned) time(&t));
	while (1) {
		int rand_x = rand() % width;
		int rand_y = rand() % height;
		MapCell *cur = &table[rand_y][rand_x];
		if (cur->image != ' ')
			continue;
		if (cur->id == -1) {
			cur->id = id;
			*x = (char) rand_x;
			*y = (char) rand_y;
			break;
		}
	}
}

Player *generate_player_list(MapCell **table, BasicData data)
{
	int count = data.players;
	Player *list = malloc(sizeof(Player) * count);
	if (list == NULL)
		allocation_error();
	for (int i = 0; i < count; i++) {
		Player pl;
		pl.id = i;
		generate_random_position(i, table, &(pl.x), &(pl.y), data);
		pl.active = 1;
		pl.health = MAX_HEALTH;
		list[i] = pl;
	}
	return list;
}

void print_player_list(Player *list, int count) {
	Player cur;
	puts("Player list:");
	for (int i = 0; i < count; i++) {
		cur = list[i];
		printf("Id: %d | Positon: (%d, %d) | Health: %d | Active: %d\n",
		       cur.id, cur.x, cur.y, cur.health, cur.active);
	}
}

void send_player_list(int *client_sockets, Player *list, int count)
{
	puts("Send list...\n");
	for (int i = 0; i < count; i++) {
		int *full_pack = malloc(sizeof(int) * count);
		if (full_pack == NULL)
			allocation_error();
		for (int j = 0; j < count; j++) {
			full_pack[j] = pack_player(list[j]);
		}
		if (write(client_sockets[i], full_pack, 4 * count) < 0) {
			perror("Failed to send user info");
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	int port, players;
	char *map_name = NULL;
	read_args(argc, argv, &port, &players, &map_name);
	client_sockets = connect_players(port, players);
	basic_data.id = -1;
	basic_data.players = players;
	table = send_basic_data(&basic_data, client_sockets, players, map_name);
	//print_map(table, basic_data.height, basic_data.width);
	list = generate_player_list(table, basic_data);
	print_player_list(list, players);
	send_player_list(client_sockets, list, players);
	start_game(client_sockets, players, list, table);
}

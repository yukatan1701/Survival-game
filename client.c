#include <sys/time.h>
//#include <fcntl.h>
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
#include <curses.h>
#include <locale.h>
#include "data.h"

void allocation_error()
{
	puts("Failed to allocate memory.");
	exit(1);
}

void init_terminal()
{
	setlocale(LC_ALL, "");
	if (!initscr())
		exit(1);
	cbreak();
	noecho();
	nonl();
	meta(stdscr, TRUE);
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_WHITE, COLOR_BLUE);
	}
	attrset(COLOR_PAIR(1));
	bkgdset(COLOR_PAIR(1));
	clear();
}

void reset_terminal()
{
	bkgdset(COLOR_PAIR(0));
	clear();
	refresh();
	endwin();
	nodelay(stdscr, FALSE);
}

int open_client(int port, char *ip)
{
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    int socket_option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
	           &socket_option, sizeof(socket_option));
    struct hostent *host = gethostbyname(ip);
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&(server_address.sin_addr), host->h_addr_list[0], 
	       sizeof(server_address.sin_addr));
	//printf("%s\n", inet_ntoa(server_address.sin_addr));
	puts("Connected succesfully.\nWaiting for other players...");
    if (connect(server_socket, (struct sockaddr *) &server_address, 
	            sizeof(server_address)) < 0) {
        perror("Failed to connect");
        exit(1);
    }
	//fcntl(server_socket, O_NONBLOCK);
	return server_socket;
}

void set_basic_positions(Player *list, BasicData game_info)
{
	int basic_x = 0, basic_y = 0;
	for (int i = 0; i < game_info.players; i++) {
		int cur_x = list[i].x, cur_y = list[i].y;
		mvaddch(cur_y, cur_x, ('0' + i) | A_BOLD);
		if (i == game_info.id) {
			basic_x = cur_x;
			basic_y = cur_y;
		}
	}
	move(basic_y, basic_x);
}

void print_health(BasicData game_info, int value) {
	mvwprintw(stdscr, game_info.height, 0, "Health: %-10d", value);	
}

void paint_map(char **map, BasicData game_info)
{
	int width = game_info.width, height = game_info.height;
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			mvaddch(i, j, map[i][j]);
	print_health(game_info, MAX_HEALTH);	
}

void refresh_map(Data data, Player *list, char **map, BasicData game_info)
{
	int main_id = game_info.id;
	int id = data.id, move = data.move, code = data.code, other = data.other;
	if (code == SR_CHEST_CODE) {
		int x = id, y = move;
		list[main_id].health -= HEALTH_REDUCE;	
		mvaddch(y, x, CHEST_CHAR);
		print_health(game_info, list[main_id].health);	
		return;
	}
	if (code == SR_DEATH_CODE) {
		list[id].active = 0;
		return;
	}
	if (code == SR_HEALTH_CHGD_CODE) {
		//print_data(stderr, data);
		if (id == main_id) {
			if (other == 0) {
				list[main_id].health += HEALTH_REDUCE;
			} else {
				list[main_id].health -= HEALTH_REDUCE;
			}
			print_health(game_info, list[main_id].health);	
		}
	}
	int old_x = list[id].x, old_y = list[id].y;
	switch (move) {
		case MOVE_NONE:
			return;
		case MOVE_UP:
			list[id].y -= 1;
			break;
		case MOVE_DOWN:
			list[id].y += 1;
			break;
		case MOVE_LEFT:
			list[id].x -= 1;
			break;
		case MOVE_RIGHT:
			list[id].x += 1;
			break;
	}
	int new_y = list[id].y, new_x = list[id].x;
	mvaddch(old_y, old_x, ' ');
	mvaddch(new_y, new_x, ('0' + id) | A_BOLD);
	//map[old_y][old_x] = ' ';
	//map[new_y][new_x] = '*';
	if (main_id == id)
		move(new_y, new_x);
}

void start_game(int server_socket, Player *list, char **map, BasicData game_info)
{
	init_terminal();
	paint_map(map, game_info);
	set_basic_positions(list, game_info);
	int flag = 1;
	fd_set rfds;
	struct timeval tv;
	int retval;
	Data data = {game_info.id, 0, 0, 0};
	while (flag) {
		int ch = getch();
		char move = 0;
		//char game_over = 0;
		if (ch != ERR) {
			// if user is alive
			switch (ch) {
				case 033:
					flag = 0;
					//game_over = 255;
					break;
				case KEY_UP:
					move = MOVE_UP;
					break;
				case KEY_DOWN:
					move = MOVE_DOWN;
					break;
				case KEY_LEFT:
					move = MOVE_LEFT;
					break;
				case KEY_RIGHT:
					move = MOVE_RIGHT;
					break;
			}
			if (list[(int) game_info.id].active != 0) {
				Data client_data = {(char) game_info.id, move, 0};
				//print_data(stderr, client_data);
				int plain_data = pack(client_data);
				if (write(server_socket, &plain_data, 4) < 0) {
					perror("Failed to send move");
					exit(1);
				}
			}
		//	if (game_over != 0)
		//		break;
		}
		FD_ZERO(&rfds);
		FD_SET(server_socket, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec  = 0;
		retval = select(server_socket + 1, &rfds, NULL, NULL, &tv);
		if (retval) {
			int server_data;
			if (read(server_socket, &server_data, 4) < 0) {
				perror("Failed to read from socket");
				exit(1);
			}
			data = unpack(server_data);
			//printf("%d %d %d", data.id, data.move, data.code);
			if (data.code == SR_END_CODE) {
				break;
			}
			//print_data(stderr, data);
			refresh_map(data, list, map, game_info);
		} else {
			/*if (getpeername(server_socket, NULL, NULL) < 0) {
				game_info.id = 99;
				break;
			}*/
		}
	}
	reset_terminal();
	if (game_info.id == data.id) {
		printf("You won the game!\n");
	} else if (game_info.id == 99) {
		printf("Server is lost.\n");
	} else {
		printf("Player %d won the game!\n", data.id);
	}
}

void read_args(int argc, char *argv[], char **ip, int *port)
{
	if (argc < 2) {
		puts("Too few arguments");
		exit(1);
	}
	*ip = argv[1];
	if (argv[2] == NULL)
		*port = 8080;
	else {
		*port = atoi(argv[2]);
		if (*port < 1024) {
			puts("Ports in the range of 0-1023 are reserved by the system.");
			exit(1);
		}
	}
}
BasicData read_basic_data(int server_socket)
{
	int plain_data;
	if (read(server_socket, &plain_data, 4) < 0) {
		perror("Failed to read plain data");
		exit(1);
	}
	BasicData game_info = unpack_basic(plain_data);
	return game_info;
}

void print_basic_data(BasicData data)
{
	printf("Players: %d\nWidth: %d\nHeight: %d\n", 
	       data.players, data.width, data.height);
}

char **download_map(int server_socket, BasicData data) {
	int width = data.width;
	int height = data.height;
	char **map = malloc(sizeof(char *) * height);
	if (map == NULL)
		allocation_error();
	for (int i = 0; i < height; i++) {
		map[i] = malloc(sizeof(char) * (width + 1));
		if (map[i] == NULL)
			allocation_error();
		memset(map[i], 0, width + 1);
		if (read(server_socket, map[i], width) < 0) {
			perror("Failed to load map");
			exit(1);
		}
	}
	return map;
}


void print_map(char **map, int height, int width) {
	for (int i = 0; i < height; i++) {
		puts(map[i]);
	}
}

Player *download_player_list(int server_socket, BasicData data)
{
	int count = data.players;
	Player *list = malloc(sizeof(Player) * count);
	if (list == NULL)
		allocation_error();
	int *full_pack = malloc(sizeof(int) * count);
	if (full_pack == NULL)
		allocation_error();
	if (read(server_socket, full_pack, count * 4) < 0) {
		perror("Failed to download user info");
		exit(1);
	}
	for (int i = 0; i < count; i++) {
		list[i] = unpack_player(full_pack[i]);
		list[i].health = MAX_HEALTH;
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

int main(int argc, char *argv[])
{
	// args: 1 - server ip, 2 - port
	char *ip;
	int port;
	read_args(argc, argv, &ip, &port);
	int server_socket = open_client(port, ip);
	BasicData game_info = read_basic_data(server_socket);
	//print_basic_data(game_info);
	char **map = download_map(server_socket, game_info);
	//print_map(map, game_info.height, game_info.width);
	Player *player_list = download_player_list(server_socket, game_info);
	//print_player_list(player_list, game_info.players);
	printf("Current client id: %d\n", game_info.id);
	start_game(server_socket, player_list, map, game_info);
	close(server_socket);
	return 0;
}

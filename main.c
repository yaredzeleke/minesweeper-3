#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ncurses.h>

struct winsize size;

static int bomb_cell_ratio = 4;

int grid_rows = 6;
int grid_cols = 5;
#define GRID_Y                                  3
#define GRID_X                                  4

#define CELL_LENGTH 1
#define CELL_WIDTH 1
#define START_X (CELL_WIDTH / 2 + 1)
#define START_Y (CELL_LENGTH / 2 + 1)


int status_rows = 1;
int status_cols = 37;
#define STATUS_Y                                GRID_Y 
#define STATUS_X                                GRID_X - 2

int controls_rows = 4;
int controls_cols = 37;
#define CONTROLS_X			GRID_X


#define MAX_GRID_ROWS ((size.ws_row) - (controls_rows) - (status_rows) - (STATUS_Y))

#define OFFSET(board, x, y) (board + (((y-START_Y) / (CELL_LENGTH + 1)) * grid_cols + ((x - START_X) / (CELL_WIDTH + 1))))

static WINDOW *status;
static WINDOW *grid;
static WINDOW *controls;

static char *playboard = NULL;
static char *puzzle = NULL;
static int *locations = NULL;
static int bombs = 0;
static int bombflags = 0;

static sigset_t caught_signals;

// sets up the interface for the game
// used also when terminal is resized
static void init(void) 
{
	keypad(stdscr, true);
	initscr();			/* Start curses mode 		*/
	raw();				/* Line buffering disabled	*/
	noecho();			/* Don't echo() while we do getch */

	ioctl(fileno(stdout), TIOCGWINSZ, &size);
	status = newwin(status_rows, size.ws_col - STATUS_X, STATUS_X, STATUS_Y);
	grid = newwin(MAX_GRID_ROWS, size.ws_col - GRID_X, GRID_X, GRID_Y);
	controls = newwin(controls_rows, size.ws_col - CONTROLS_X, size.ws_row - controls_rows, CONTROLS_X);


	wprintw(controls, "CONTROLS\n");
	wprintw(controls, " h - left\t");
	wprintw(controls, " j - down\t");
	wprintw(controls, " k - up\t");
	wprintw(controls, " l - right\n\n");

	refresh();

}

// 
static void create_puzzle(void) {

	free(puzzle);
	free(locations);
	free(playboard);

	puzzle = malloc((grid_rows * grid_cols + 1) * sizeof(char));
	playboard = malloc((grid_rows * grid_cols + 1) * sizeof(char));

	memset(puzzle, '0', (grid_rows * grid_cols + 1) * sizeof(char));

	bombs = (grid_rows * grid_cols) / bomb_cell_ratio / 2;

	wprintw(status, "bomb(s) = %d, flag(s) = %d", bombs, bombflags);
	wrefresh(status);

	locations = malloc(bombs * sizeof(int));
	int i = 0;

	for(i = 0; i < grid_rows * grid_cols; i++) {
		playboard[i] = '#';
	}
	playboard[grid_rows * grid_cols] = '\0';

	srand(time(NULL));

	i = 0;

	while(i < bombs) 
	{
		int random = rand() % (grid_rows * grid_cols) + 1;
		if(puzzle[random] != 'b') {

			puzzle[random] = 'b';
			locations[i++] = random;

			// top left
			if(random % grid_cols != 0 && random - grid_cols - 1 >= 0 && puzzle[random - grid_cols - 1] != 'b') {
				puzzle[random - grid_cols - 1]++;
			} 
			// top
			if(random - grid_cols >= 0 && puzzle[random - grid_cols] != 'b') {
				puzzle[random - grid_cols]++;
			}
			// top right
			if(((random + 1) % grid_cols != 0) && (random - grid_cols + 1) >= 0 && puzzle[random - grid_cols + 1] != 'b') {
				puzzle[random - grid_cols + 1]++;
			}
			// left
			if(random % grid_cols != 0 && random - 1 >= 0 && puzzle[random - 1] != 'b') {
				puzzle[random - 1]++;
			}
			// right
			if((random + 1) % grid_cols != 0 && random + 1 < grid_rows * grid_cols && puzzle[random + 1] != 'b') {
				puzzle[random + 1]++;
			}
			// bottom left
			if(random % grid_cols != 0 && random + grid_cols - 1 < grid_rows * grid_cols && puzzle[random + grid_cols - 1] != 'b') {
				puzzle[random + grid_cols - 1]++;
			}
			// bottom
			if(random + grid_cols < grid_rows * grid_cols && puzzle[random + grid_cols] != 'b') {
				puzzle[random + grid_cols]++;
			}
			// bottom right
			if((random + 1) % grid_cols != 0 && random + grid_cols + 1 < grid_rows * grid_cols && puzzle[random + grid_cols + 1] != 'b') {
				puzzle[random + grid_cols + 1]++;
			}
		}

	}
}

static void draw_row(int r) {
	int j = 0;
	for(j = 0; j < grid_cols + 1; j++) {
		waddch(grid, '|');
		if(j < grid_cols)
			waddch(grid, playboard[r * grid_cols + j]);
	}
	waddch(grid, '\n');
}


static void create_grid(void)
 {
        int i, j;
	wmove(grid, 0, 0);

	for(i = 0; i < grid_rows + 1; i++) {
		for(j = 0; j < grid_cols + 1; j++) {
			waddch(grid, '+');
			if(j < grid_cols) 
				waddch(grid, '-');
		}
		waddch(grid, '\n');
		if(i < grid_rows) {
			draw_row(i);
		}
	}

}

static void clean(int sig) {
	endwin();

	free(locations);
	free(puzzle);
	free(playboard);
}

static void clean2(void) {
	endwin();

	free(locations);
	free(puzzle);
	free(playboard);
}

int main(void) {

	atexit(clean2);

	int key, x = START_X, y = START_Y;
	bool run = true;
	bool playing = true;

	struct sigaction act;

	int const sig[] = {
		SIGINT, SIGQUIT, SIGPWR, SIGHUP, SIGTERM
	};

	int i = 0;
	int open = 0;

	sigemptyset(&caught_signals);

	for(i = 0; i < sizeof(sig); i++) {
		sigaddset(&caught_signals, sig[i]);
	}

	act.sa_handler = clean;
	act.sa_mask = caught_signals;
	act.sa_flags = 0;

	for(i = 0; i < sizeof(sig); i++) {
		sigaction(sig[i], &act, NULL);
	}



	init();
	wrefresh(controls);
	wrefresh(status);


	create_puzzle();
	create_grid();

	wmove(grid, y, x);	
	wrefresh(grid);

	while(run) {
		key = getch();	

		if(playing) {
			switch(key) {
				case 'h':
				if(x > START_X) {
					x -= CELL_WIDTH + 1;
				}
				break;
				case 'l':
				if(x < grid_cols * 2 - CELL_WIDTH) {
					x += CELL_WIDTH + 1;
				}
				break;
				case 'j':
				if(y < (grid_rows * 2) - CELL_LENGTH) {
					y += CELL_LENGTH + 1;
				}
				break;
				case 'k':
				if(y > START_Y) {
					y -= CELL_LENGTH + 1;
				}
				break;
				case 'q':
				run = false;
				break;
				case 'r':
				werase(status);
				wprintw(status, "Enter rows: ");
				wrefresh(status);
				echo();
				wscanw(status, "%d", &grid_rows);
				werase(status);
				wprintw(status, "Enter cols: ");
				wrefresh(status);
				wscanw(status, "%d", &grid_cols);
				create_puzzle();
				create_grid();
				wrefresh(grid);

				werase(status);
				wprintw(status, "bomb(s) = %d, flag(s) = %d", bombs, bombflags);
				wrefresh(status);
				noecho();

				break;
				case 'f':
				if(*OFFSET(playboard, x, y) == '#') { 
					*(OFFSET(playboard, x, y)) = 'f';
					if(bombflags < bombs - 1 && (*(OFFSET(puzzle, x, y))) == 'b') {
						bombflags++;
						werase(status);
						wprintw(status, "bomb(s) = %d, flag(s) = %d", bombs, bombflags);
						wmove(grid, y, START_X - 1 - (CELL_WIDTH / 2));
						draw_row((y - START_Y) / (CELL_LENGTH + 1));
					} else {
						werase(status);
						wprintw(status, "You win!");
						playing = false;
						open = 0;
					}
				}
				break;
				case '\n':
				if(*(OFFSET(playboard, x, y)) == '#') {
					open++;
					if((*(OFFSET(playboard, x, y)) = *(OFFSET(puzzle, x, y))) == 'b') {
						werase(status);
						wprintw(status, "You lose!");
						playing = false;
						open = 0;
					} else if(open == grid_rows * grid_cols - bombs) {
						werase(status);
						wprintw(status, "You win!");
						playing = false;
						open = 0;
					}
				} else if(*(OFFSET(playboard, x, y)) == 'f') {
					*(OFFSET(playboard, x, y)) = '#';
					if(*OFFSET(puzzle, x, y) == 'b') {
						bombflags--;
						werase(status);
						wprintw(status, "bomb(s) = %d, flag(s) = %d", bombs, bombflags);
					}
				}
				wmove(grid, y, START_X - 1 - (CELL_WIDTH / 2));
				draw_row((y - START_Y) / (CELL_LENGTH + 1));

				break;
				case KEY_RESIZE:

				endwin();
				refresh();
				clear();

				init();
				create_grid();
				wrefresh(controls);
				wrefresh(grid);
				wrefresh(status);

				break;
			}
		} else {
			create_puzzle();
			create_grid();
			wrefresh(grid);

			werase(status);
			wprintw(status, "bomb(s) = %d, flag(s) = %d", bombs, bombflags);
			wrefresh(status);
			playing = true;	

		}

		wrefresh(status);
		wmove(grid, y, x);	
		wrefresh(grid);
	}


	return 0;
}

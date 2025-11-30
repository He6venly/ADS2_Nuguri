#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32 // 윈도우의 경우
#include <conio.h>
#include <windows.h>
#define kbhit _kbhit
void clrscr(){
    system("cls");
}
void usleep(int us){
    Sleep(us/1000); // Linux의 usleep = Sleep*1000이므로, usleep을 기준으로 함
}

void enable_raw_mode() { // 터미널 초기 설정
    SetConsoleOutputCP(65001);
}
void disable_raw_mode() {}

#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// 터미널 설정
struct termios orig_termios;

// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 비동기 키보드 입력 확인
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void clrscr(){
    printf("\x1b[2J\x1b[H");
}

#endif

// 맵 및 게임 요소 정의 (수정된 부분)
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20
#define MAX_STAGES 2
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];
int player_x, player_y;
int stage = 0;
int score = 0;
int life = 3; //목숨 전역변수

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;



// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input, int *game_over); //game_over 변수 포인터 추가
void move_player(char input);
void move_enemies();
void check_collisions();
void textcolor(int color);
int title();
void openingUI();

int main() {
    srand(time(NULL));
    enable_raw_mode();

    // title 화면에서 0 선택 시 게임 종료, 1 선택 시 title 함수 내부에서 openingUI 함수 호출
    if(title() == 0){ 
        disable_raw_mode();
        return 0;
    }

    load_maps();
    init_stage();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) {
            c = getchar();
            if (c == 'q') {
                game_over = 1;
                continue;
            }
            if (c == '\x1b') {
                getchar(); // '['
                switch (getchar()) {
                    case 'A': c = 'w'; break; // Up
                    case 'B': c = 's'; break; // Down
                    case 'C': c = 'd'; break; // Right
                    case 'D': c = 'a'; break; // Left
                }
            }
        } else {
            c = '\0';
        }

        update_game(c, &game_over); //인자로 주소값 넘겨줌
        draw_game();
        usleep(90000);

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            } else {
                game_over = 1;
                clrscr(); // printf("\x1b[2J\x1b[H");에서 Windows도 호환되게
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);
            }
        }
    }

    disable_raw_mode();
    return 0;
}

// 맵 파일 로드
void load_maps() {
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0;
            strncpy(map[s][r], line, MAP_WIDTH + 1);
            r++;
        }
    }
    fclose(file);
}


// 현재 스테이지 초기화
void init_stage() {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') {
                player_x = x;
                player_y = y;
            } else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){x, y, (rand() % 2) * 2 - 1};
                enemy_count++;
            } else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){x, y, 0};
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    clrscr(); // printf("\x1b[2J\x1b[H");에서 Windows도 호환되게
    printf("Stage: %d | Score: %d\n", stage + 1, score);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for(int y=0; y < MAP_HEIGHT; y++) {
        for(int x=0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            } else {
                display_map[y][x] = cell;
            }
        }
    }
    
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for(int x=0; x< MAP_WIDTH; x++){
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input, int *game_over) { //메인에서 넘겨준 주소값 가르키는 포인터
    move_player(input);
    move_enemies();
    check_collisions(game_over); //포인터를 check_collisions에 넘김
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') next_y++; break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
            }
            break;
    }

    if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') player_x = next_x;
    
    if (on_ladder && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 
    else {
        if (is_jumping) {
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            velocity_y++;

            if (velocity_y < 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] == '#') {
                velocity_y = 0;
            } else if (next_y < MAP_HEIGHT) {
                player_y = next_y;
            }
            
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        } else {
            if (floor_tile != '#' && floor_tile != 'H') {
                 if (player_y + 1 < MAP_HEIGHT) player_y++;
                 else init_stage();
            }
        }
    }
    
    if (player_y >= MAP_HEIGHT) init_stage();
}


// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        } else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions(int *game_over) { //포인터 받아서
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) { //적과 겹치면
            score = (score > 50) ? score - 50 : 0;
            life--; //목숨 -1
            if (life > 0) { //목숨 남았으면 맵 초기화
                init_stage();
            } else { //아니면(게임오버면) game_over = 1
                *game_over = 1;
            }
            return; //게임오버하면 코인 충돌 감지는 돌아가면 안되니까 리턴으로 종료
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
        }
    }
}

void textcolor(int color){ // 화면 출력 시 색상 설정
    switch (color){
        case 1:
        printf("\033[33m"); // 어두운 노랑(갈색과 유사!)
        break;
   default:
        printf("\033[0m"); // 기본값으로 리셋
        break;
    }
    fflush(stdout);
}

// 코드 실행 시 1 또는 0 중 선택에 따라 게임 시작 혹은 종료
int title(){
    char choice;
    // 무한 루프로 사용자가 유효한 입력(1 또는 0)을 할 때까지 타이틀 화면을 반복 출력하고 입력 기다림
    while(1) {
        clrscr();
        textcolor(1);

        printf("       ____                        ____   \n");
        printf("      /    \\                     /     \\ \n");
        printf("     /      \\                   /       \\  \n");
        printf("================================================\n");

        printf("\n");
        textcolor(0);
        printf("               N U G U R I G A M E              \n");

        printf("\n");
        printf("                  1. Game Start                  \n");
        printf("                  0. Exit                        \n");

        printf("\n");
        printf("Ready to play? (1/0): ");

        printf("\n");
        textcolor(1);
        printf("================================================\n");

        // raw mode 이용하여 단일 키 입력 받기(kbhit 활용)
        enable_raw_mode();
        // 키 입력이 있을 때까지 대기
        while (!kbhit()) usleep(1000);
        // 입력된 키 가져오기
        choice = getchar();
        // 터미널 설정 원래대로 되돌림
        disable_raw_mode();

        // 입력된 문자에 따른 게임 분기 처리
        // 1 선택 시 openingUI 보여준 후 메인 함수로 1을 반환하여 게임 루프 시작하도록 지시
        if(choice == '1'){
            openingUI();
            return 1;
        }
        // 0 선택 시 메인 함수로 0을 반환하여 프로그램을 종료하도록 지시
        else if(choice == '0'){
            return 0;
        }
        // 1이나 0이 아니면 루프가 다시 돌아 처음부터 화면을 다시 출력하고 입력을 기다림
    }
}

void openingUI() {

    clrscr(); // 화면 초기화
    textcolor(1); // 색상 설정

    printf("       ____                        ____   \n");
    printf("      /    \\                     /     \\ \n");
    printf("     /      \\                   /       \\  \n");
    printf("================================================\n");

    printf("\n");
    textcolor(0);
    
    printf("          ████     ████   █    █  ██████ \n");
    printf("         █        █    █  ██  ██  █            \n");
    printf("         █        ██████  █ ██ █  ██████       \n");
    printf("         █  ████  █    █  █    █  █            \n");
    printf("          ████ █  █    █  █    █  ██████       \n");

    printf("\n");

    printf("     ██████  ██████   ████   ██████  ██████  \n");
    printf("     █         ██    █    █  █    █    ██    \n");
    printf("     ██████    ██    ██████  ██████    ██   \n");
    printf("          █    ██    █    █  █    █    ██     \n");
    printf("     ██████    ██    █    █  █     █   ██ \n");
    
    printf("\n");
    textcolor(1);

    printf("================================================\n");
    textcolor(0); // 기본값으로 초기화
    getchar();
}


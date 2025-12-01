#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) // 윈도우의 경우, 추가: 다른 OS(Linux, MacOS)의 경우에는 다른 분기로 넘어가기 위해 직접 #elif로 지정해주기 위해서 #ifdef에서 #if를 사용.
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

//디아블로때와 동일하게 window / 리눅스계열 getch로 함수이름 통일해서 분기로 처리
int getch() {
    return _getch();
}

#elif defined(__APPLE__) || defined(__MACH__) || defined(__linux__) // 리눅스, MacOS의 경우 분기
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

int getch() {
    return getchar();
}

void Beep(int frequency, int duration){
    (void)frequency;
    printf("\a");
    fflush(stdout);
    usleep(duration*1700);
}

#else
    #error "Unsupported platform!" // 현재는 Windows, MacOS, Linux를 제외한 다른 OS는 지원하지 않기 때문에 이외의 OS로 실행 시 에러를 뱉음.
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
void check_collisions(int *game_over);
void textcolor(int color);
int title();
void openingUI();
void gameoverUI(int final_score);
void restart_game(int *game_over);
void gameclearUI(int final_score);
void cleanBuf();
void game_over_sound();
void move_sound();
void get_coin_sound();
void hit_enemy_sound();
//버퍼비우기용
void cleanBuf() {
	int c;
	while ((c = getchar()) != '\n' && c != EOF);
}


int main() {
    srand(time(NULL));

    enable_raw_mode(); // rawmode를 title 호출 로직 뒤로 옮김.

    // title 화면에서 0 선택 시 게임 종료, 1 선택 시 title 함수 내부에서 openingUI 함수 호출
    if(title() == 0){ 
        disable_raw_mode();
        return 0;
    }

    load_maps();
    init_stage();

    clrscr();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) {
            c = getch();
            if (c == 'q') {
                game_over = 1;
                continue;
            }
            if (c == '\x1b') {
                getch(); // '['
                switch (getch()) {
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
                clrscr(); //UI화면 출력 찌꺼기 방지 위해 호출전 한번만 클리어
                gameclearUI(score); // gameclearUI 함수 호출
            }
        }
    }
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
            
            if (r < MAP_HEIGHT) {
                line[strcspn(line, "\n\r")] = 0;
                strncpy(map[s][r], line, MAP_WIDTH + 1);
                r++;
            }
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
    //clrscr(); // printf("\x1b[2J\x1b[H");에서 Windows도 호환되게
    printf("\033[H"); //플리커링 방지 >> clrscr를 호출하는 대신 커서만 맨 앞으로 이동시켜서 다시 덮어서 그려버리기

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
    int prev_x = player_x, prev_y = player_y;
    int moved_by_input = 0; // 키보드를 눌렀는지 확인하는 변수
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
        case 'a': next_x--; moved_by_input = 1; break;
        case 'd': next_x++; moved_by_input = 1; break;
        case 'w': if (on_ladder) next_y--; moved_by_input = 1; break;
        case 's':
            if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#'){
                next_y++;
                moved_by_input = 1;
            }
            break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
                moved_by_input = 1;
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

            if (velocity_y < 0) { //위로 올라갈 때
                for (int k = -1; k >= velocity_y; k--) { //위로 올라가는 속도(한 번에 이동하는 거리만큼) 크기만큼 k값 하나씩 감소하면서 사이 검사
                    if (player_y + k >= 0 && map[stage][player_y + k][player_x] == '#') { //플레이어 위치에서 점프 높이만큼 증가하다가 벽 감지하면
                    player_y = player_y + k + 1; //해당 벽 밑으로 위치 조정
                    velocity_y = 0; //속도 0
                    break; //종료
                    }
                } // ->검사문에 안걸림 = 벽 없음
                if (next_y >= 0 && map[stage][next_y][player_x] != '#') player_y = next_y; //따라서 그냥 점프
                else velocity_y = 0; 
            }else { //점프 눌렀을 때 올라가는 경우가 아니면 -> 떨어질 때
                if (velocity_y > 0) { //떨어지는 속도가 빨라서 중간에 벽을 건너뛰었는지 검사문, 아래로 낙하하고 있을 때
                    for (int k = 1; k <= velocity_y; k++) { //가는 길목(k)에 벽(#)이 있거나 바닥, 떨어지는중이면 한 번에 이동하는 칸만큼 반복
                        if (player_y + k <= MAP_HEIGHT && map[stage][player_y + k][player_x] == '#') { //이동값이 맵 바닥보다 작고(=맵 안)플레이어 y에 k만큼 더한게 벽이면
                            player_y = player_y + k - 1; //벽 바로 위에서 멈춤 -> player_y + k가 벽 위치라 거기서 -1만큼하면 벽 위
                            is_jumping = 0; //착지 처리
                            velocity_y = 0; //속도 0
                            break; //종료
                        } // -> 결론 : 떨어질 때 가속도 붙으면 한번에 여러칸 이동 -> 이동하는 칸 사이만큼 반복하면서 벽 감지하면 벽 위에 멈춤
                    }
                } // ->검사문에 안걸림 = 바닥 없음
                if (is_jumping) player_y = next_y; //점프중이면 그냥 다음 위치로
            }
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') { //바로 한칸 아래가 벽이면 점프끝 -> 없으면 버그날 확률 높음
                is_jumping = 0;
                velocity_y = 0;
            }
            if (is_jumping) velocity_y++; //밑으로 이동하는 것 까지 끝내고 떨어지는 속도 증가
        }else { //점프중이 아님 -> 중력으로 낙하
            if (floor_tile != '#' && floor_tile != 'H') { //아래칸 바닥 타일이 벽이나 사라디가 아니면
                if (player_y + 1 < MAP_HEIGHT) player_y++; //맵 높이보다 작으면 y값 증가시켜 한칸 떨어지게 
                else init_stage(); //맵 바닥 경계아래로 떨어지려 하면 초기화
            }
        }
    }
    //사운드 검사
    if (moved_by_input && (player_x != prev_x || player_y != prev_y)) { // 플레이어의 위치가 변경되었고 키를 같이 눌러야(중력으로 인해 떨어질 경우에도 마찬가지로 플레이어 위치가 변경되기 때문에) 움직임 소리가 나도록 함.
        move_sound();
    }
    //가속도 검사?
    if (player_y >= MAP_HEIGHT) { //가속 너무 높은 상태로 바닥으로 직행하면 이전 코드는 바로 스테이지가 초기화됨 -> 낙사 로직인지모르겠는데 일단 바닥 위로 이동 처리
        player_y = MAP_HEIGHT - 2;
        is_jumping = 0;
        velocity_y = 0;
    }
}


// 적 이동 로직
void move_enemies() {
    //공중에 떠있는 적 이동로직 추가하기
    //현재 로직이 지금 다음칸 발 밑이 비어있으면 (공백이면) 뒤로 돌아가는 로직. >> 땅 위에 있는 경우에만 낭떠러지인지 검사하는 식으로 하면 될듯.

    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;

        //기본 충돌체크는 그대로
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#') { // ||로 y축 검사하는거 아래로 옮기기
            enemies[i].dir *= -1;
            continue;
        }
        
        //내가 땅위인지 확인. 내 발밑이 #이고 MAP HEIGHT를 벗어나지 않으면 땅위
        int isGround = enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == '#';


        //isGround가 1이면(True면) == 땅바닥에 있는 몬스터면 원래있던 낭떠러지 검사 및 뒤로 돌아가는 로직 그대로 수행
        if(isGround) {
            if((enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
                enemies[i].dir *= -1;
                continue;
            }
        }

        //땅위 아니면 공백이여도 자유롭게 이동
        enemies[i].x = next_x;
    }
}

// 충돌 감지 로직
void check_collisions(int *game_over) { //포인터 받아서
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) { //적과 겹치면
            hit_enemy_sound();
            score = (score > 50) ? score - 50 : 0;
            life--; //목숨 -1
            if (life > 0) { //목숨 남았으면 맵 초기화
                init_stage();
            } else { //아니면(게임오버면) game_over = 1
                game_over_sound();
                restart_game(game_over); //check_collisions 안에 구현하면 복잡할 거 같아서 따로 restart_game함수 구현
            }
            return; //적과 충돌하면 코인 충돌은 돌아가면 안되서 return으로 종료
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
            get_coin_sound();
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

        textcolor(1);
        printf("================================================\n");
        textcolor(0);

        printf("Ready to play? (1/0): ");

        // 키 입력이 있을 때까지 대기
        while (!kbhit()) usleep(1000);
        // 입력된 키 가져오기
        choice = getch();

        cleanBuf(); //입력버퍼는 지우기

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
    getch();
}

void gameoverUI(int final_score) {
    clrscr();
    textcolor(1);

    printf("        ____                        ____   \n");
    printf("       /    \\                     /     \\ \n");
    printf("      /      \\                   /       \\  \n");
    printf("================================================\n");

    printf("\n");
    textcolor(0);

    printf("          ████     ████   █    █  ██████ \n");
    printf("         █        █    █  ██  ██  █            \n");
    printf("         █        ██████  █ ██ █  ██████       \n");
    printf("         █  ████  █    █  █    █  █            \n");
    printf("          ████ █  █    █  █    █  ██████       \n");

    printf("\n");

    printf("       ████   █      █  ██████  ██████        \n");
    printf("      █    █   █    █   █       █     █         \n");
    printf("      █    █   █    █   ██████  ██████          \n");
    printf("      █    █    █  █    █       █    █            \n");
    printf("       ████      ██     ██████  █     █   █ █ █      \n");

    printf("\n");
    printf("                              최종 점수: %d \n", final_score);
    printf("\n");
    textcolor(1);
    printf("================================================\n");
    textcolor(0);

     printf("                              RESTART? (Y/N) : ");


}

void restart_game(int *game_over) { //여기도 game_over포인터 받아서
    char re;

    gameoverUI(score);

     // 키 입력이 있을 때까지 대기
    while (!kbhit()) usleep(1000);
    // 입력된 키 가져오기
    re = getch();

    if (re == 'y' || re == 'Y') { //y/Y 입력시 재시작
        life = 3;
        score = 0;
        stage = 0;
        init_stage();

        clrscr();
    }
    
    else { //종료
        *game_over = 1;
    }
}

void gameclearUI(int final_score){ // score를 인수로 받기 위해 int score
    clrscr();
    textcolor(1);

    printf("        ____                        ____   \n");
    printf("       /    \\                     /     \\ \n");
    printf("      /      \\                   /       \\  \n");
    printf("================================================\n");

    printf("\n");
    textcolor(0);

    printf("          ████     ████   █    █  ██████ \n");
    printf("         █        █    █  ██  ██  █            \n");
    printf("         █        ██████  █ ██ █  ██████       \n");
    printf("         █  ████  █    █  █    █  █            \n");
    printf("          ████ █  █    █  █    █  ██████       \n");

    printf("\n");

    printf("      █████  █      ██████   ████   ██████   █    \n");
    printf("     █       █      █       █    █  █     █  █    \n");
    printf("     █       █      ██████  ██████  ██████   █    \n");
    printf("     █       █      █       █    █  █    █        \n");
    printf("      █████  █████  ██████  █    █  █     █  █    \n");

    printf("\n");

    printf("                               최종 점수: %d\n", final_score);

    printf("\n");
    textcolor(1);

    printf("================================================\n");
    textcolor(0);
    getch();
}


/*
SOUND 모음
Beep 함수는 (Frequency, Duration)을 인자로 받는데, Frequency의 경우 소리의 높낮이를 결정하는 요소로, 클수록 음이 높다. Duration의 경우는 이 소리를 어느정도 이어지게 할 지 정하는데, 단위는 ms이다.(1000ms=1s)
이 Beep 함수는 <windows.h>에 있다. 따라서 Linux용에서는 따로 만들어줘야 함. 그리고 소리가 끝나고 다음 라인(코드)으로 넘어가기 때문에, 리눅스에도 마찬가지로 딜레이를 넣을 필요가 있다. 마지막으로 리눅스에서는 Frequencey나 Duration 같은 기능은 내부 라이브러리로는 제어할 수 없어서 윈도우의 형식만 가져옴.
*/

// GAME OVER 됐을 때의 사운드
void game_over_sound() {
    for (int f = 2000; f >= 100; f -= 100) {
        Beep(f, 30);
    }
    // 소리가 높은 곳에서 낮은 곳으로 내려가면서 0.03초 동안 내도록 만들어 옛날 레트로 게임의 효과음처럼 만들었다.
}

void move_sound(){
    Beep(300, 10);
} // move_player 함수에서 사용된다. 움직임+키보드 입력 시 소리가 나게 했으며, 특별하지 않은 소리로 하여 거슬림 없는 소리 정도로 만들었다. 자세한 로직 설명은 함수에 있다.

void get_coin_sound(){
    for (int f = 1800; f <= 2000; f += 100) {
        Beep(f, 30);
    }
} // check_collisions 함수에서 사용된다. 코인과 충돌 감지시 소리를 나게 했으며, 윈도우의 경우 경쾌한 소리를 주었다. 움직임과 다르게 3번 소리를 주게 해서 다르게 표현했다.

void hit_enemy_sound(){
    Beep(800, 40);
    Beep(400, 60);
}  // check_collisions 함수에서 사용된다. 적과 충돌 감지시 소리를 나게 했으며, 낮은 음으로 2번 소리를 주게 해서 다르게 표현했다.
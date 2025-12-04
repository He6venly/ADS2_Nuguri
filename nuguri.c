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

    //커서 왔다갔다와 깜빡거리는거 없애기 위해서 커서 삭제하는 로직 추가
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE); //디아블로와 동일. 현재 창(콘솔)의 핸들(제어권) 얻어오기.
    CONSOLE_CURSOR_INFO cursor; //커서info라는 window.h 구조체 받아오기. 커서에 대한 설정정보 구조체임.
    cursor.dwSize = 100; //커서 사이즈 정해주기. 이거 안써주니까 커서 숨기기 반영이 안됨 ;;
    cursor.bVisible = FALSE; // 그안에있는 bVisible로 커서 숨기라고(false)로 설정
    SetConsoleCursorInfo(consoleHandle, &cursor); //설정 적용하기. & 붙이는 이유는 방금 setConsoleCursorInfo가 구조체 포인터 주소값을 파라미터로 받기 때문.

    //윈도우는 cmd창 끄면 자동으로 커서 다시 보이기로 세팅되기때문에 disable row mode에서 다시켜줄필요 X
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
void disable_raw_mode() { 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    //리눅스용 커서 다시 보이기
    printf("\x1b[?25h"); //h는 켜기 (high)
    fflush(stdout);
}
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    //리눅스용 커서 숨기기
    printf("\x1b[?25l");  //터미널에게 내리는 명령 , x1b = escape 문자. 이문자 나오면 이제부터 명령어가 시작되는거라고 알려주는 깃발?
                          // ?25 << 커서를 지칭하는 고유 번호, h는 켜기 (high), l(low)는 끄기를 의미함. 고로 ?25l << 커서 꺼라.
    fflush(stdout);
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
    usleep(duration*300);
}

#else
    #error "Unsupported platform!" // 현재는 Windows, MacOS, Linux를 제외한 다른 OS는 지원하지 않기 때문에 이외의 OS로 실행 시 에러를 뱉음.
#endif

// 맵 및 게임 요소 정의 (수정된 부분), 동적할당 위해 전역변수 int로 교체?
int MAP_WIDTH = 0;  
int MAP_HEIGHT = 0;
int MAX_STAGES = 0;
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
char ***map; //char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1]; 동적 맵 할당을 위해 3중 포인터로 변경 (3차원 배열이니 3중 포인터)
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

//방향 공중점프용 전역변수 세팅, 지역변수하니까 버그가 자꾸 생김..
int j_dirction = 0; // 점프중의 방향 지정할 변수 (0 = 제자리 / 1 = 오른쪽 / -1 = 왼쪽으로 설계할 예정, 왼쪽은 x좌표 -1해야하니까 이렇게 구성)
int now_direction = 0; //방금전까지 향하던 방향(입력키?) 저장해둘 변수 (0 = 제자리 / 1 = 오른쪽 / -1 = 왼쪽 )
int stopSecond = 0; // 입력 없이 제자리일 경우 체크하기 위해 진짜 짧은 시간동안 입력 없으면 멈추게 하기 위한 임시변수

// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input, int *game_over); //game_over 변수 포인터 추가, 점프상태 변수 추가
void move_player(char input, int *game_over);
void move_enemies();
void check_collisions(int x, int y, int *game_over, int enemyCheck);
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

//동적맵관련 함수
void scanMap(); //맵파일 읽어와서 변수에 넣어주기
void dynamicMap(); //동적맵 할당 함수
void dynamicMap_free(); //동적맵 free전용 함수

//맵 읽어와서 사이즈 분석해서 길이 높이 스테이지 갯수 결정해주는 함수
//뭐 존재하는 맵 중 최대길이/최대높이만큼 할당해야하니까.
void scanMapSize() {
    //여긴 loadMap과 동일
    FILE *file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }

    char line[1000]; //한줄 읽어올 함수 대충 1000으로 길게 잡기

    int width = 0;
    int height = 0;
    int maxWidth = 0;
    int maxHeight = 0; //현재 읽는 스테이지 길이 높이 저장용이랑 malloc은 맵파일 전체중 최대크기 맵으로 할당해야하니 max변수도 추가
    int stageCount = 1; //스테이지 카운트는 1부터

    //여기도 load_map이랑 동일, 분석해서 길이 높이 구하는것만 목적이니 스테이지 조건검사는 없어도 됨.
    while(fgets(line, sizeof(line), file)) {

        int length = strlen(line); // 한줄 길이 저장. 이게 width가 되는것

        //윈도우와 리눅스 둘다 개행 문자 처리하기 위해 \n\r 다해주기
        //line[strcspn(line, "\n\r")] = 0; -> 이걸로 하니까 윈도우는 되는데 리눅스에서는 맵 로딩 안됨.
        while(length > 0 && (line[length-1] == '\r' || line[length-1] == '\n')) {
            line[length-1] = '\0';
            length--;
        }
        
        if(length == 0) { //맵은 빈줄 한줄로 다음 스테이지 구분하니까 0이면 다음 스테이지 구분자구나! 로 간주
            //스테이지 구분선에 진입했고, height변수에 뭔가 저장되어 있으면 (그니까 스테이지 한개를 다 읽었다는 말이겠죠?)
            if(height > 0) {
                stageCount++; //그때 스테이지 카운트 +1 하고
                if(height > maxHeight) { //지금 읽은 크기가 최대값인지 비교후
                    maxHeight = height; //최대값이면 갱신해주고
                }
                height = 0; //스테이지별로 높이 저장해주는 height는 0으로 초기화하는 방식.
            }
            continue;
        }
        
        //한줄 끝내고 최대 width(너비) 갱신, 맵이 width가 더 긴 맵을 만들수도 있으니까 꼼꼼히 검사
        if(length > maxWidth) {
            maxWidth = length;
        }

        //마지막 루프전에 높이 +1씩 계속해주기, 스테이지 끝나면 위에서 0으로 어차피 처리하고 continue하니까..
        height++;
    }

    //파일 맨끝에 공백이 없을때 갱신이 안되는 버그가 있어서 파일 끝났으면 스페이스바 없어도 끝나기전에 최대높이 한번더 갱신해주기
    if(height > 0) {
        if(height > maxHeight) {
            maxHeight = height;
        }
    }

    //이제 다 검사했으면 전역변수에 저장
    MAP_WIDTH = maxWidth;
    MAP_HEIGHT = maxHeight;
    MAX_STAGES = stageCount;

    fclose(file); //닫는거 잊지말기~
}   

/* 12-04 선효 -> 동적 맵 할당 설명 가이드

    일단 3중 포인터 구조부터
    1차원 char* = 문자열 한 줄 (가로줄 하나, width)
    2차원 char** = 이 한줄들이 모인 하나의 스테이지(세로값(height)가 곧 2차원 배열이 되는 느낌)
    3차원 char*** = 이 스테이지(2차원 배열들)이 모인 배열 (전체 맵 데이터, stages)
    
    대충 이런 느낌이고

    첫번째 스테이지 할당을 예시로 설명할게요

    >> map = (char ***)malloc(sizeof(char **) * MAX_STAGES);

    char*** 형변환은 왜?
    -> malloc은 기본적으로 void 타입 메모리 공간 주소를 반환하는데, 얘는 얘가 뭔지도, 그냥 이만큼 공간 할당 하라니까 하는거지
    뭔 주소인지도 뭔 타입인지도 모름. 그러니까 정확히 우리지금 char타입 맵 만든다, 3중 포인터 값이다. 라고 명시해주는 느낌..?
    >> 근데이거 솔직히 저도 긴가민가 해요. 찾아보니 C언어에서는 자동으로 변환이 된다고도 하고?

    malloc은 힙에 이만큼 메모리 자리좀 빌려다오.. 하는 건데 데이터 크기를 명확히 잡아줘야겠죠?
    sizeof(char**) -> 이거면 char** 의 데이터 크기를 의미. 근데 포인터니까 주소값이겠죠? -> 64비트기준으로 주소값당 8바이트 고정
    지금 저희 스테이지가 총 4개니까 8*4 = 32바이트만큼 할당요청.
    스테이지 1, 2, 3, 4 각각으로 바로 갈수있는 주소를 적어둘 메모리 공간을 할당한다는 느낌이에요

    다음으로 map[i] = (char **)malloc(sizeof(char *) * MAP_HEIGHT); 면 그럼 char ** (1개의 해당 스테이지) 안에서 연산하겠죠?
    -> sizeof(char*) * Height니까 현재 스테이지(char**)가 높이가 얼마인가?  -> height가 20이면 20줄짜리 스테이지다 라는 의미.

    마지막으로 map[i][k] = (char *)malloc(sizeof(char) * MAP_WIDTH + 1); -> 이제 char * (해당 스테이지 안의 한 줄)
    -> 이제 몇 스테이지 몇번째 줄인지까지 접근했으니까, 맵을 찍어야되잖아요? 그래서 이제 sizeof(char)로 char (1바이트)짜리 크기로 할당해서 맵을 쭉 찍는거임
    얼마만큼? MAP_WDITH만큼. MAP_WIDTH는 뭐다? 처음에 맵 분석할때 MAP파일 전체 스테이지중 최대 사이즈만큼 되어있다.
    +1은 왜? >> 계속 설명했지만 C는 문자열 끝날때 널문자 필수. 라인 한줄씩 찍으니까 라인 끝날때마다 +1로 널문자 넣어줄 공간 할당.
*/

//동적 맵 할당 함수
void dynamicMap() {
    map = (char ***)malloc(sizeof(char **) * MAX_STAGES); //일단 스테이지 갯수부터 할당(첫번째 차원 포인터)
                                                        // 스테이지는 2차원 배열을 원소로 가지는 배열임. 앞에서 분석해둔 스테이지 갯수만큼 곱해서 총 스테이지 갯수만큼 스테이지 메모리부터 할당.
                                                        // 이 친구는 주소값을 가지니까 64비트 기준 개당 8바이트씩, 8 * 스테이지 갯수만큼 메모리 할당.
    if (map == NULL) {
        printf("맵 할당 오류"); // 없으면 걍 종료
        exit(0);
    } 

    //이제 2차원, 맵 높이(c언어 2차원 배열에서는 행에 해당) 할당해주기
    for(int i = 0; i < MAX_STAGES; i++) {
        map[i] = (char **)malloc(sizeof(char *) * MAP_HEIGHT); //이제 2차원, 스테이지 안에 있는 제일 큰 맵 높이만큼 메모리 할당 (앞에서 MaxHeight를 MAP_HEGIHT로 해놨음.)
        if (map[i] == NULL) {
            printf("맵 할당 오류"); // 없으면 걍 종료
            exit(0);
        }

        //2차원 반복문 안에서 각 행의 width만큼 할당 (여기가 3차원, 열에 해당) 할당해주기
        for(int k = 0; k < MAP_HEIGHT; k++) {
            map[i][k] = (char *)malloc(sizeof(char) * MAP_WIDTH + 1); // 각 스테이지 문자열 끝에 항상 \0(끝문자)가 오니까 + 1 해주는거 잊지말기!!!
            if (map[i][k] == NULL) {
                printf("맵 할당 오류"); // 없으면 걍 종료
                exit(0);
            }
            
            //최대크기보다 작은 맵이 들어왔을때 해당 칸에 읽어올게 없으면 다 공백으로 세팅해주기 (화면 지우는 느낌?)
            //아근데 솔직히 3중포문이라 맘에 안들긴 함.. ㅠ
            for (int j = 0; j < MAP_WIDTH; j++) {
                map[i][k][j] = ' '; 
            }
            
            //문자열 끝에 널문자 사뿐하게
            map[i][k][MAP_WIDTH] = '\0';
        }   
    }
}

//맵 할당 해제. 프로그램 종료할때만 해주면 될 듯
void dynamicMap_free() {
    //위에서 할당했던 스테이지 갯수(1차원) -> 맵 높이(2차원) -> 맵 넓이(3차원)의 역순으로 닫아주면 됨
    // width -> height -> stages 순으로 닫으니 2중배열을 stage -> height 순으로 감싸주기
    for(int i = 0; i < MAX_STAGES; i++) {
        for(int k = 0; k < MAP_HEIGHT; k++) {
            free(map[i][k]);
        }
        free(map[i]);
    }
    free(map);
}

int main() {
    srand(time(NULL));
    enable_raw_mode(); // rawmode를 title 호출 로직 뒤로 옮김.

    scanMapSize(); //맵 할당하려고 읽어오기
    dynamicMap(); //동적 할당

    // title 화면에서 0 선택 시 게임 종료, 1 선택 시 title 함수 내부에서 openingUI 함수 호출
    if(title() == 0){ 
        disable_raw_mode();
        dynamicMap_free(); //종료할때 free 꼭!!
        return 0;
    }

    load_maps();
    init_stage();

    clrscr();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) {
            while (kbhit()) {  //버퍼에 입력 쌓이는데 한 프레임당 kbhit이 입력값 하나만 들고옴 -> 버퍼에 쌓여서 떼도 움직임
                int temp = getch(); // 입력을 임시로 저장해두고
                
                // 키가 자꾸 씹히니까 일단 임시 저장한게 왼쪽 오른쪽 이동이면 바로 공중점프 로직에 반영
                if (temp == 'a') {
                    now_direction = -1;
                    stopSecond = 0;
                } else if (temp == 'd') {
                    now_direction = 1;
                    stopSecond = 0;
                }
                //c = getch() 원래 로직이니 temp를 그냥 넣어주기
                c = temp;

                if (c == 'q') { //while문으로 한 프레임에 버퍼 비울때까지 가져와서 입력 안밀림 -> 버퍼에서 읽어와서 c에 계속 덮어쓴다고 생각
                    game_over = 1;
                    continue;
                }
                if (c == '\x1b') {
                    getch(); // '['
                    switch (getch()) { //다른 키 입력할 때 define A B C D 해서 하는 거 같은데 화살표 구현떄문에 남긴건가
                        case 'A': c = 'w'; break; // Up
                        case 'B': c = 's'; break; // Down
                        case 'C': c = 'd'; break; // Right
                        case 'D': c = 'a'; break; // Left
                    }
                }
            }
        } else {
            c = '\0';
        }

        update_game(c, &game_over); //인자로 주소값 넘겨줌, 점프했는지 안했는지 상태 넘겨줌
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
    char line[MAP_WIDTH + 4]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨 , +2로 남겨두니까 /r/n의 윈도우와 리눅스 처리 방식 차이에 따라서
                              // 1바이트 차이로 리눅스 환경에서는 맵 로딩이 정상적으로 되지 않는 버그 발생. +4정도로 넉넉하게 잡아줌.
                              // \r\n은 윈도우는 \r을 없애고 \n으로 자동 변환시켜서 가져오는데, 리눅스는 \r\n을 그대로 가져오기에 발생하는 문제였음.

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

    printf("Stage: %d | Score: %d | Life: ", stage + 1, score);
    for (int i = 0; i < life; i++) {
        printf("♥ ");
    }
    for (int i = 0; i < 3 - life; i++) {
        printf("  ");
    }
    printf("\n");
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
void update_game(char input, int *game_over) { //메인에서 넘겨준 주소값 가르키는 포인터, 점프상태 추가
    move_player(input, game_over);
    move_enemies();
    check_collisions(player_x, player_y, game_over, 1); //포인터를 check_collisions에 넘김 ->점프로직 사이검사때문에 플레이어 x, y값 추가
}

// 플레이어 이동 로직
void move_player(char input, int *game_over) {
    int prev_x = player_x, prev_y = player_y;
    int moved_by_input = 0; // 키보드를 눌렀는지 확인하는 변수
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];
    on_ladder = (current_tile == 'H');

    int isBottomLadder = (player_y + 1 < MAP_HEIGHT && map[stage][player_y + 1][player_x] == 'H'); // 지금 내가있는 바닥(#) 밑이 사다리임? 을 저장하는 변수

    /* 
        12-02 공중에서 이동하는 관성 로직이 있어야 할 듯
        실제 너구리 게임을 보니, 점프 중간엔 방향을 바꿀 수 없고 오른쪽, 왼쪽 점프, 제자리 점프만 존재.
        그러면 점프 시점에 오른/왼쪽 키 입력이 있는지 체크하고 점프입력이 들어올때 오른쪽 왼쪽 키가 입력중이라면 해당 방향으로 1칸씩 자동으로 이동하게 하면 될 듯.
        중간에 떼도 해당 방향 점프를 취소할 수 없고, 벽에 부딛히거나 땅에 착지하면 관성을 다시 0으로 만드는 식으로 구현.
    */
    
    // 0 제자리 1 오른쪽 2 왼쪽
    if(input == 'a') {
        now_direction = -1; //왼쪽, 좌표 -1해야하니까
        stopSecond = 0; //입력이니까 0으로 초기화

    } else if (input == 'd') {
        now_direction = 1; //오른쪽, 좌표 +1해야하니
        stopSecond = 0; //이하동문

    } else if (input == '\0') {  //입력이 없다면 (상시상태, main에서 c가 입력이 없다면 \0으로 처리되기에 이렇게 검사)
        stopSecond++; //main에서 usleep(90000), 즉 0.09초마다 화면 업데이트 중이니, 0.09초마다 stopSecond가 1씩 올라가는 구조.
        if(stopSecond > 2) { // 다시 말해 1보다 크면 == 입력없이 0.1초 지나면 제자리점프
            now_direction = 0; // >> 방향 정지로 설정
        }
    }

    switch (input) {
        case 'a': next_x--; moved_by_input = 1; break;
        case 'd': next_x++; moved_by_input = 1; break;
        case 'w': if (on_ladder) next_y--; moved_by_input = 1; break;
        case 's':
            if ((on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') || isBottomLadder) { //내 밑이 사다리인지도 동시에 체크
                next_y++;
                moved_by_input = 1;
            }
            break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder || isBottomLadder)) {
                is_jumping = 1;
                velocity_y = -2;
                moved_by_input = 1;
                j_dirction = now_direction; // 점프로직 맨 마지막에 점프 방향 세팅해주기
            }
            break;
    }

    //>> 그니까 점프 중이 아닐때만 next_x << (키보드 입력)으로 이동. 아닐경우에는 j_direction으로 이동처리
    if (!is_jumping) {
        if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') {
            player_x = next_x;

            check_collisions(player_x, player_y, game_over, 0);
        }
    }


    //사다리처리 ~~ 바닥이 사다리일때도 추가(12/02)
    if ((on_ladder || (input == 's' && isBottomLadder)) && (input == 'w' || input == 's')) {
        if(next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
            check_collisions(player_x, player_y, game_over, 1); //사다리 이동중 충돌 체크, 적 만나면 죽어야함
            j_dirction = 0;
            now_direction = 0; //사다리 타면 관련 변수 다 초기화
        }
    }
    //여기부터 점프 로직 y축검사.
    else {
        if (is_jumping) {
            //제자리 점프중에 오른쪽 왼쪽 입력으로 이동할수있게 하기
            if (input == 'a') j_dirction = -1;
            else if (input == 'd') j_dirction = 1;

            //기존에 있던 점프처리 (y축검사)
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            if (velocity_y < 0) { //위로 올라갈 때
                for (int k = -1; k >= velocity_y; k--) { //위로 올라가는 속도(한 번에 이동하는 거리만큼) 크기만큼 k값 하나씩 감소하면서 사이 검사
                    if (player_y + k >= 0 && map[stage][player_y + k][player_x] == '#') { //플레이어 위치에서 점프 높이만큼 증가하다가 벽 감지하면
                        player_y = player_y + k + 1; //해당 벽 밑으로 위치 조정
                        velocity_y = 0; //속도 0
                        break; //종료
                    }
                    check_collisions(player_x, player_y + k, game_over, 1); //위의 충돌검사 y축 이후에 검사하도록 옮김
                } // ->검사문에 안걸림 = 벽 없음

                //버그수정. 점프해서 위의 벽 검사에서  y+1에 벽이 있는지 검사하는데, for문 밖에서 y+2에 벽이 없으면 된다고 판단하고 벽 뚫고 점프함.
                //if문 한번 더 씌워서 충돌이 없을때만 이동하게 바꿨음.
                if (velocity_y != 0) {
                    if (next_y >= 0 && map[stage][next_y][player_x] != '#') {
                        player_y = next_y; //따라서 그냥 점프
                    } else {
                        velocity_y = 0;
                    }
                }

            } else { //점프 눌렀을 때 올라가는 경우가 아니면 -> 떨어질 때
                if (velocity_y > 0) { //떨어지는 속도가 빨라서 중간에 벽을 건너뛰었는지 검사문, 아래로 낙하하고 있을 때
                    for (int k = 1; k <= velocity_y; k++) { //가는 길목(k)에 벽(#)이 있거나 바닥, 떨어지는중이면 한 번에 이동하는 칸만큼 반복

                        /* 여기있던 땅 검사 로직 순서 변경했음 */

                        if (player_y + k <= MAP_HEIGHT && map[stage][player_y + k][player_x] == '#') { //이동값이 맵 바닥보다 작고(=맵 안)플레이어 y에 k만큼 더한게 벽이면
                            player_y = player_y + k - 1; //벽 바로 위에서 멈춤 -> player_y + k가 벽 위치라 거기서 -1만큼하면 벽 위
                            is_jumping = 0; //착지 처리
                            velocity_y = 0; //속도 0
                            j_dirction = 0; //착지할때 관성 제거해주기

                            check_collisions(player_x, player_y, game_over, 1); // 착지할때 적 있으면 죽어야하니 1

                            break; //종료
                        } // -> 결론 : 떨어질 때 가속도 붙으면 한번에 여러칸 이동 -> 이동하는 칸 사이만큼 반복하면서 벽 감지하면 벽 위에 멈춤

                        check_collisions(player_x, player_y + k, game_over, 0); // 그냥 낙하중일때는 코인만 검사하게
                    }
                } // ->검사문에 안걸림 = 바닥 없음
                if (is_jumping) player_y = next_y; //점프중이면 그냥 다음 위치로
            }

             /* 
                12/03 --- 선효
                점프관성 추가하면서 계속 버그나서 검사로직 순서자체를 변경
                1. 적 충돌감지보다 벽검사를 먼저하게 순서 변경 
                2. 대각선 벽 관통 버그 수정을 위해 착지 판정 순서 교체
                3. wall coner 로직에서 착지할 바닥도 모서리로 인식해서 공중에서 멈추는 버그 발생하여 교체.
                4. 이렇게 계속 구조를 수정하다보니 x축을 1씩 공중에서 밀어주는 로직 실행 중 사각지대가 계속 발생하여 코인(C)가 관통하여 먹어지지 않는 버그도 발생
                5. 2중으로 꼼꼼하게 충돌검사를 시행하여 가는 경로 사각지대도 검사해주는 방법을 일단 사용... ㅠㅠ
            */

            // Y축 처리 다음부터 X축 좌우 점프시 관성으로 이동 검사
            //대각전용검사추가
            //제자리가 아니라 점프방향이 존재한다면
            if (j_dirction != 0) {
                int jumpNext_X = player_x + j_dirction;
                // 옆면이 벽임?
                int isSideWall = (map[stage][player_y][jumpNext_X] == '#');
                

                //옆면 안 막혔으면 일단 이동
                if (jumpNext_X >= 0 && jumpNext_X < MAP_WIDTH && !isSideWall) {
                    player_x = jumpNext_X;
                    //충돌검사
                    check_collisions(player_x, player_y, game_over, 1);
                    
                    //y+1도 검사해서 예를 들어 3칸 점프한다면 내 머리 바로 위칸(y+1)좌표에 있는 아이템도 먹게 충돌검사 해주기
                    check_collisions(player_x, player_y + 1, game_over, 0);

                    //이게 맞나 ㅋㅋㅋㅋㅋ 일단 y+2까지 3단으로 검사 (점프력 3이니)
                    check_collisions(player_x, player_y + 2, game_over, 0);
                } else {
                    j_dirction = 0; // 벽에 부딪히면 관성 즉시 정지
                }
            }

            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') { //바로 한칸 아래가 벽이면 점프끝 -> 없으면 버그날 확률 높음
                is_jumping = 0;
                velocity_y = 0;
                j_dirction = 0; //착지할때 관성 제거해주기
            }
            if (is_jumping) velocity_y++; //밑으로 이동하는 것 까지 끝내고 떨어지는 속도 증가

        } else { //점프중이 아님 -> 걷는 중간에 중력으로 낙하
            if (floor_tile != '#' && floor_tile != 'H') { //아래칸 바닥 타일이 벽이나 사라디가 아니면
                if (player_y + 1 < MAP_HEIGHT) player_y++; //맵 높이보다 작으면 y값 증가시켜 한칸 떨어지게 
                else init_stage(); //맵 바닥 경계아래로 떨어지려 하면 초기화

            //점프관성 제거위해 else 달았음.
            } else {
                j_dirction = 0; //착지할때 관성 제거해주기
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
void check_collisions(int x, int y, int *game_over, int enemyCheck) { //포인터 받아서 -> 원래 전역변수 player_x, player_y에 바로 접근해 검사하던 거를 점프때 player_x, player_y + k값 받아서 검사 가능하게 변경
    /*
        enemyCheck라는 0,1 T/F표현 변수 추가해서, 충돌 로직 검사할때마다
        내가 지금 몬스터 충돌까지 생각할지(바닥에 닿을때나 사다리 타있을때 기준)
        그냥 단순 점프 공중 관성이동(경로)이여서 코인만 검사하면 될지 분기를 정하기 위해 enemyCheck 변수 추가해서
        이동 로직 함수에서 충돌 검사 함수를 불러 쓸 때마다 몬스터 검사를 할지 안 할지를 0과 1로 제어하는 방식으로 해봤음.
    */
    
    if(enemyCheck) {
        for (int i = 0; i < enemy_count; i++) {
            if (x == enemies[i].x && y == enemies[i].y) { //적과 겹치면
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
    }

    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && x == coins[i].x && y == coins[i].y) {
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

        while(kbhit()) getch(); //입력키 처리

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
    while(1){

    re = getch();

    if (re == 'y' || re == 'Y') { //y/Y 입력시 재시작
        life = 3;
        score = 0;
        stage = 0;
        init_stage();

        clrscr();
        break;
    }
    else if (re == 'n' || re == 'N'){ //종료
        *game_over = 1;
        break;
    }
    
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
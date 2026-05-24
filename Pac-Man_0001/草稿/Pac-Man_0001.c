#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define WIDTH 19
#define HEIGHT 15
#define TILE_SIZE 48
#define UI_HEIGHT 60

// 原始地图模板（永远不会被修改，用于重新开始）
// 新地图布局（大小仍为 19x15）
#define HEIGHT 21
#define WIDTH  31

const char original_map[HEIGHT][WIDTH + 1] = {
    "###############################",
    "#.#.................#......##.#",
    "#..#.##.##.#..##.##..#.#..##..#",
    "#....##....#...#.##.....#.....#",
    "#.#.##.#..##.......##.##..#.#.#",
    "#.#.##.....##.....##.....#..#.#",
    "#.............##..............#",
    "#..#..#.#.##.....##.##..#.#...#",
    "#.##.##....o###o....##.#...#..#",
    "#......##.#..##.##.#..........#",
    "#.#..............#....#.##.##.#",
    "#.#.#.##.##.##.#..##.##.#.##..#",
    "#.......#......#....##......o.#",
    "#.##.##.##.##.#.##..#.##.##.#.#",
    "#.........##.....#..........#.#",
    "#.####.##....###...##.##.###..#",
    "#....#..##.......##....##.....#",
    "#.##.....##...#.##........##..#",
    "#.##.#..#..#...##...##.##.##..#",
    "#.............................#",
    "###############################"
};

// 当前工作地图（会被修改，豆子被吃后变为空格）
char map[HEIGHT][WIDTH + 1];

int score = 0;
int pellets = 0;
int gameOver = 0;
int win = 0;

typedef struct {
    int x;
    int y;
} Point;

Point player = { 1, 1 };
Point ghost = { 9, 7 };

// 幽灵移动间隔（秒），原代码约640ms移动一次
#define GHOST_MOVE_DELAY 0.64f
float ghostTimer = 0.0f;

// 判断是否为墙
static inline int isWall(int x, int y) {
    return map[y][x] == '#';
}

// 初始化/重置游戏
void initGame(void) {
    // 恢复地图到初始状态（逐行 memcpy，包括字符串终止符）
    for (int y = 0; y < HEIGHT; y++) {
        memcpy(map[y], original_map[y], WIDTH + 1);
    }

    // DEBUG: 打印地图以便确认复制是否正确
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", map[y]);
    }

    score = 0;
    pellets = 0;
    gameOver = 0;
    win = 0;
    player.x = 1;
    player.y = 1;
    // 优先放在地图靠上的开放位置（避免放在墙里）
    ghost.x = 9;
    ghost.y = 4;
    ghostTimer = 0.0f;

    // 如果幽灵初始化在墙里，则寻找距离首选点最近的非墙位置（避免被卡住）
    if (isWall(ghost.x, ghost.y)) {
        int bestDist = INT_MAX;
        int prefX = 9, prefY = 4;
        for (int yy = 0; yy < HEIGHT; yy++) {
            for (int xx = 0; xx < WIDTH; xx++) {
                if (!isWall(xx, yy) && !(xx == player.x && yy == player.y)) {
                    int d = abs(xx - prefX) + abs(yy - prefY);
                    if (d < bestDist) {
                        bestDist = d;
                        ghost.x = xx;
                        ghost.y = yy;
                    }
                }
            }
        }
    }

    // 统计豆子数量（'.' 为普通豆子，'o' 为大能量豆，也计数）
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (map[y][x] == '.' || map[y][x] == 'o') pellets++;
        }
    }

    // DEBUG: 打印玩家与幽灵初始位置
    printf("Player: (%d,%d)  Ghost: (%d,%d)  Pellets: %d\n", player.x, player.y, ghost.x, ghost.y, pellets);
}

// 检查坐标是否可以移动（不越界、不是墙）
int canMove(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
    return !isWall(x, y);
}

// 幽灵AI：向玩家移动
void moveGhost(void) {
    int bestDist = 1000;
    Point choices[4];
    int count = 0;

    const Point dirs[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (int i = 0; i < 4; i++) {
        int nx = ghost.x + dirs[i].x;
        int ny = ghost.y + dirs[i].y;
        if (canMove(nx, ny)) {
            int dist = abs(player.x - nx) + abs(player.y - ny);
            if (dist < bestDist) {
                bestDist = dist;
                count = 0;
                choices[count++] = (Point){ nx, ny };
            }
            else if (dist == bestDist) {
                choices[count++] = (Point){ nx, ny };
            }
        }
    }
    // 如果没有找到任何拉近玩家的方向，则回退到任意可移动方向以避免卡住
    if (count == 0) {
        for (int i = 0; i < 4; i++) {
            int nx = ghost.x + dirs[i].x;
            int ny = ghost.y + dirs[i].y;
            if (canMove(nx, ny)) {
                choices[count++] = (Point){ nx, ny };
            }
        }
    }

    if (count > 0) {
        ghost = choices[rand() % count];
    }
}

// 绘制游戏画面
void drawGame(void) {
    ClearBackground(BLACK);

    // 绘制地图
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int pixelX = x * TILE_SIZE;
            int pixelY = y * TILE_SIZE + UI_HEIGHT;

            if (map[y][x] == '#') {
                DrawRectangle(pixelX, pixelY, TILE_SIZE, TILE_SIZE, BLUE);
            }
            else if (map[y][x] == '.') {
                int r = TILE_SIZE / 8;
                if (r < 3) r = 3;
                DrawCircle(pixelX + TILE_SIZE / 2, pixelY + TILE_SIZE / 2, r, WHITE);
            }
            else if (map[y][x] == 'o') {
                int r = TILE_SIZE / 4;
                if (r < 6) r = 6;
                DrawCircle(pixelX + TILE_SIZE / 2, pixelY + TILE_SIZE / 2, r, LIGHTGRAY);
            }
        }
    }

    // 绘制吃豆人（黄色圆）
    DrawCircle(player.x * TILE_SIZE + TILE_SIZE / 2,
        player.y * TILE_SIZE + UI_HEIGHT + TILE_SIZE / 2,
        TILE_SIZE / 2 - 2, YELLOW);

    // 绘制幽灵（红色圆）
    DrawCircle(ghost.x * TILE_SIZE + TILE_SIZE / 2,
        ghost.y * TILE_SIZE + UI_HEIGHT + TILE_SIZE / 2,
        TILE_SIZE / 2 - 2, RED);

    // 绘制UI文字
    int uiFont = 20;
    DrawText(TextFormat("Score: %d  Pellets: %d", score, pellets), 10, 10, uiFont, WHITE);

    if (gameOver) {
        const char *msg = "GAME OVER - Press R to restart, Q to quit";
        int tw = MeasureText(msg, uiFont);
        DrawText(msg, (WIDTH * TILE_SIZE - tw) / 2, (HEIGHT * TILE_SIZE) / 2 + UI_HEIGHT, uiFont, RED);
    }
    else if (win) {
        const char *msg = "YOU WIN! - Press R to restart, Q to quit";
        int tw = MeasureText(msg, uiFont);
        DrawText(msg, (WIDTH * TILE_SIZE - tw) / 2, (HEIGHT * TILE_SIZE) / 2 + UI_HEIGHT, uiFont, GREEN);
    }
}

int main(void) {
    srand((unsigned)time(NULL));

    // 初始化窗口
    InitWindow(WIDTH * TILE_SIZE, HEIGHT * TILE_SIZE + UI_HEIGHT, "Pac-Man (Raylib)");
    SetTargetFPS(60);

    initGame();

    while (!WindowShouldClose()) {
        // ----- 输入处理 -----
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
            break;  // 退出游戏
        }
        if (IsKeyPressed(KEY_R)) {
            initGame();  // 重新开始
        }

        // 玩家移动（仅在游戏进行中）
        if (!gameOver && !win) {
            int nextX = player.x;
            int nextY = player.y;
            int moved = 0;

            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) { nextY--; moved = 1; }
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { nextY++; moved = 1; }
            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) { nextX--; moved = 1; }
            if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { nextX++; moved = 1; }

            if (moved && canMove(nextX, nextY)) {
                player.x = nextX;
                player.y = nextY;

                // 吃豆子（小豆子 '.' = 10 分，大豆子 'o' = 50 分）
                char cur = map[player.y][player.x];
                if (cur == '.' || cur == 'o') {
                    map[player.y][player.x] = ' ';  // 移除豆子
                    score += (cur == 'o') ? 50 : 10;
                    pellets--;
                    if (pellets == 0) {
                        win = 1;
                    }
                }
            }
        }

        // ----- 游戏逻辑更新 -----
        if (!gameOver && !win) {
            ghostTimer += GetFrameTime();
            if (ghostTimer >= GHOST_MOVE_DELAY) {
                ghostTimer = 0.0f;
                moveGhost();
                // 检查幽灵是否抓到玩家
                if (ghost.x == player.x && ghost.y == player.y) {
                    gameOver = 1;
                }
            }
        }

        // ----- 渲染 -----
        BeginDrawing();
        drawGame();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
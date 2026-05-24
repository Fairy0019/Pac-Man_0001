#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

// ========== 地图配置 ==========
#define WIDTH  31
#define HEIGHT 21
#define TILE_SIZE 48        // 所有图片都是 48x48，直接1:1绘制
#define UI_HEIGHT 60

// 原始地图（19x15 已改为 31x21）
const char original_map[HEIGHT][WIDTH + 1] = {
    "###############################",
    "# #..o..............#......##.#",
    "#..#.##.##.#..#..##..#.#..##..#",
    "#....##....#...#..#.....#.....#",
    "#.#.#..#..##.......##.##..#.#.#",
    "#.#.##.....##.....##..o..#..#.#",
    "#.............##..............#",
    "#..#..#.#.##.....##.##..#.#...#",
    "#.##.##....o###......#.#...#..#",
    "#......##.#..##.##.#..........#",
    "#.#..............#....#.##.##.#",
    "#.#.#.##..#.##.#..##.##.#.##..#",
    "#.......#......#....##......o.#",
    "#.##.##.##.##.#.##..#.##.##.#.#",
    "#....o....##.....#..........#.#",
    "#.####.##....###...##.##.###..#",
    "#....#..##.......##....##.....#",
    "#.##.....##...#.##........##..#",
    "#.##.#..#..#...##...##.##.##..#",
    "#.............................#",
    "###############################"
};

char map[HEIGHT][WIDTH + 1];        // 当前工作地图
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
float ghostTimer = 0.0f;
#define GHOST_MOVE_DELAY 0.64f

// ========== 图片纹理 ==========
// 用户请根据实际文件位置修改下面的路径（在 main 函数中）
Texture2D pacmanTex[3];     // 吃豆人三帧动画
Texture2D ghostTex;         // 幽灵
Texture2D wallTex;          // 墙
Texture2D dotTex;           // 普通豆子
Texture2D energyDotTex;     // 能量豆

// 动画控制
int currentPacmanFrame = 0;
float animTimer = 0.0f;
const float ANIM_INTERVAL = 0.1f;   // 每0.1秒切换一帧
const int frameSequence[] = { 0, 1, 2, 1 };  // 循环序列：张→半→闭→半
int seqIndex = 0;

// ========== 辅助函数 ==========
static inline int isWall(int x, int y) {
    return map[y][x] == '#';
}

int canMove(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
    return !isWall(x, y);
}

// 幽灵AI（向玩家移动）
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

// 初始化游戏
void initGame(void) {
    // 恢复地图
    for (int y = 0; y < HEIGHT; y++) {
        memcpy(map[y], original_map[y], WIDTH + 1);
    }

    score = 0;
    pellets = 0;
    gameOver = 0;
    win = 0;
    player.x = 1;
    player.y = 1;
    ghost.x = 9;
    ghost.y = 4;
    ghostTimer = 0.0f;

    // 幽灵避墙
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

    // 统计豆子数量
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (map[y][x] == '.' || map[y][x] == 'o') pellets++;
        }
    }

    // 重置动画
    currentPacmanFrame = 0;
    seqIndex = 0;
    animTimer = 0.0f;
}

// 更新吃豆人动画
void updatePacmanAnimation(float deltaTime) {
    animTimer += deltaTime;
    if (animTimer >= ANIM_INTERVAL) {
        animTimer = 0.0f;
        seqIndex = (seqIndex + 1) % 4;
        currentPacmanFrame = frameSequence[seqIndex];
    }
}

// ========== 绘制全部游戏元素 ==========
void drawGame(void) {
    ClearBackground(BLACK);

    // 1. 绘制地图（墙、豆子、能量豆都用图片）
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int pixelX = x * TILE_SIZE;
            int pixelY = y * TILE_SIZE + UI_HEIGHT;
            Rectangle dstRect = { pixelX, pixelY, TILE_SIZE, TILE_SIZE };

            if (map[y][x] == '#') {
                // 墙壁图片
                DrawTexturePro(wallTex, (Rectangle) { 0, 0, TILE_SIZE, TILE_SIZE },
                    dstRect, (Vector2) { 0, 0 }, 0.0f, WHITE);
            }
            else if (map[y][x] == '.') {
                // 普通豆子图片
                DrawTexturePro(dotTex, (Rectangle) { 0, 0, TILE_SIZE, TILE_SIZE },
                    dstRect, (Vector2) { 0, 0 }, 0.0f, WHITE);
            }
            else if (map[y][x] == 'o') {
                // 能量豆图片
                DrawTexturePro(energyDotTex, (Rectangle) { 0, 0, TILE_SIZE, TILE_SIZE },
                    dstRect, (Vector2) { 0, 0 }, 0.0f, WHITE);
            }
        }
    }

    // 2. 绘制吃豆人（动画帧）
    Rectangle pacmanSrc = { 0, 0, TILE_SIZE, TILE_SIZE };
    Rectangle pacmanDst = {
        player.x * TILE_SIZE,
        player.y * TILE_SIZE + UI_HEIGHT,
        TILE_SIZE, TILE_SIZE
    };
    DrawTexturePro(pacmanTex[currentPacmanFrame], pacmanSrc,
        pacmanDst, (Vector2) { 0, 0 }, 0.0f, WHITE);

    // 3. 绘制幽灵
    Rectangle ghostSrc = { 0, 0, TILE_SIZE, TILE_SIZE };
    Rectangle ghostDst = {
        ghost.x * TILE_SIZE,
        ghost.y * TILE_SIZE + UI_HEIGHT,
        TILE_SIZE, TILE_SIZE
    };
    DrawTexturePro(ghostTex, ghostSrc, ghostDst, (Vector2) { 0, 0 }, 0.0f, WHITE);

    // 4. UI 文字
    int uiFont = 20;
    DrawText(TextFormat("Score: %d  Pellets: %d", score, pellets), 10, 10, uiFont, WHITE);

    if (gameOver) {
        const char* msg = "GAME OVER - Press R to restart, Q to quit";
        int tw = MeasureText(msg, uiFont);
        DrawText(msg, (WIDTH * TILE_SIZE - tw) / 2, (HEIGHT * TILE_SIZE) / 2 + UI_HEIGHT, uiFont, RED);
    }
    else if (win) {
        const char* msg = "YOU WIN! - Press R to restart, Q to quit";
        int tw = MeasureText(msg, uiFont);
        DrawText(msg, (WIDTH * TILE_SIZE - tw) / 2, (HEIGHT * TILE_SIZE) / 2 + UI_HEIGHT, uiFont, GREEN);
    }
}

// ========== 主函数 ==========
int main(void) {
    const char* basePath = GetApplicationDirectory();
    srand((unsigned)time(NULL));
    InitWindow(WIDTH * TILE_SIZE, HEIGHT * TILE_SIZE + UI_HEIGHT, "Pac-Man (Raylib)");
    SetTargetFPS(60);

    // 加载图片到全局纹理数组/变量
    pacmanTex[0] = LoadTexture(TextFormat("%sresources/pacman_0.png", basePath));
    pacmanTex[1] = LoadTexture(TextFormat("%sresources/pacman_1.png", basePath));
    pacmanTex[2] = LoadTexture(TextFormat("%sresources/pacman_2.png", basePath));

    ghostTex = LoadTexture(TextFormat("%sresources/ghost_normal.png", basePath));
    // 如果需要蓝色幽灵，可以单独存一个变量，比如 ghostBlueTex，这里为了简化先只用白鬼
    wallTex = LoadTexture(TextFormat("%sresources/wall.png", basePath));
    dotTex = LoadTexture(TextFormat("%sresources/gold coin.png", basePath));
    energyDotTex = LoadTexture(TextFormat("%sresources/energydot.png", basePath));

    // 调试输出：检查纹理ID是否有效（有效纹理的 id > 0）
    TraceLog(LOG_INFO, "pacman_0 id: %d", pacmanTex[0].id);
    TraceLog(LOG_INFO, "pacman_1 id: %d", pacmanTex[1].id);
    TraceLog(LOG_INFO, "pacman_2 id: %d", pacmanTex[2].id);
    TraceLog(LOG_INFO, "ghost id: %d", ghostTex.id);
    TraceLog(LOG_INFO, "wall id: %d", wallTex.id);
    TraceLog(LOG_INFO, "dot id: %d", dotTex.id);
    TraceLog(LOG_INFO, "energydot id: %d", energyDotTex.id);
    TraceLog(LOG_INFO, "basePath: %s", basePath);

    // 设置像素风格过滤
    for (int i = 0; i < 3; i++) SetTextureFilter(pacmanTex[i], TEXTURE_FILTER_POINT);
    SetTextureFilter(ghostTex, TEXTURE_FILTER_POINT);
    SetTextureFilter(wallTex, TEXTURE_FILTER_POINT);
    SetTextureFilter(dotTex, TEXTURE_FILTER_POINT);
    SetTextureFilter(energyDotTex, TEXTURE_FILTER_POINT);


    initGame();

    while (!WindowShouldClose()) {
        // 输入处理
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_R)) initGame();

        // 玩家移动（仅在游戏中）
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

                char cur = map[player.y][player.x];
                if (cur == '.' || cur == 'o') {
                    map[player.y][player.x] = ' ';
                    score += (cur == 'o') ? 50 : 10;
                    pellets--;
                    if (pellets == 0) win = 1;
                }
            }
        }

        // 幽灵移动
        if (!gameOver && !win) {
            ghostTimer += GetFrameTime();
            if (ghostTimer >= GHOST_MOVE_DELAY) {
                ghostTimer = 0.0f;
                moveGhost();
                if (ghost.x == player.x && ghost.y == player.y) gameOver = 1;
            }
        }

        // 更新吃豆人动画（即使在游戏结束或胜利时也停止更新，可放在条件内）
        if (!gameOver && !win) {
            updatePacmanAnimation(GetFrameTime());
        }

        // 渲染
        BeginDrawing();
        drawGame();
        EndDrawing();
    }

    // 卸载纹理
    for (int i = 0; i < 3; i++) UnloadTexture(pacmanTex[i]);
    UnloadTexture(ghostTex);
    UnloadTexture(wallTex);
    UnloadTexture(dotTex);
    UnloadTexture(energyDotTex);

    CloseWindow();
    return 0;
}
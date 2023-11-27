#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define BLOCK_WIDTH 10
#define BLOCK_HEIGHT 20
#define SYSTEM_CYCLE 16667
#define GAME_CYCLE 30

typedef enum {
    RESETTING,
    FALLING,
    LANDING,
    DELETING,
    GAMEOVER,
} Situation;

int blockGrid[BLOCK_HEIGHT][BLOCK_WIDTH];
int game_counter = 0;
Situation situation = RESETTING;
int deletedFlag[BLOCK_HEIGHT];

int debug_msg[10];

char getPushedKey(){
    struct termios trm_save,trm;
    // 端末の属性を取得
    tcgetattr(STDIN_FILENO,&trm);
    trm_save = trm;
    // 端末モードの変更
    trm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&trm);
    int fd = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, fd | O_NONBLOCK);
    // 入力の受取
    char c = getchar();
    // 端末モードを戻す
    tcsetattr(STDIN_FILENO, TCSANOW, &trm_save);
    fcntl(STDIN_FILENO, F_SETFL, fd);

    return c;
}

typedef struct mino{
    int width;
    int x;
    int y;
    int x_min;
    int x_max;
    int y_min;
    int y_max;
    int* grid;
} Mino;
// player
Mino mino;
// template
Mino imino;
Mino omino;
Mino tmino;
Mino jmino;
Mino lmino;
Mino smino;
Mino zmino;

void initializeMino(Mino* mino,int* grid,int width){
    mino->width = width;
    mino->grid = (int*)malloc(sizeof(int) * (width * width));
    for(int i = 0;i < width * width;i++){
        mino->grid[i] = grid[i];
    }
    mino->x_min = 3;
    mino->x_max = 0;
    mino->y_min = 3;
    mino->y_max = 0;
    for(int i = 0;i < width;i++){
        for(int j = 0;j < width;j++){
            if(!grid[i*width + j]) continue;
            mino->x_min = mino->x_min < j ? mino->x_min : j;
            mino->x_max = mino->x_max > j ? mino->x_max : j;
            mino->y_min = mino->y_min < i ? mino->y_min : i;
            mino->y_max = mino->y_max > i ? mino->y_max : i;
        }
    }
}
void initializeAllMino(){
    // Imino
    {
        int grid[] = {
            0,0,0,0,
            1,1,1,1,
            0,0,0,0,
            0,0,0,0,
        };
        initializeMino(&imino,grid,4);
    }
    // Omino
    {
        int grid[] = {
            0,0,0,0,
            0,1,1,0,
            0,1,1,0,
            0,0,0,0,
        };
        initializeMino(&omino,grid,4);
    }
    // Tmino
    {
        int grid[] = {
            0,0,0,
            1,1,1,
            0,1,0 
        };
        initializeMino(&tmino,grid,3);
    }
    // Jmino
    {
        int grid[] = {
            1,0,0,
            1,1,1,
            0,0,0 
        };
        initializeMino(&jmino,grid,3);
    }
    // Lmino
    {
        int grid[] = {
            0,0,1,
            1,1,1,
            0,0,0 
        };
        initializeMino(&lmino,grid,3);
    }
    // Smino
    {
        int grid[] = {
            0,1,1,
            1,1,0,
            0,0,0 
        };
        initializeMino(&smino,grid,3);
    }
    // Zmino
    {
        int grid[] = {
            1,1,0,
            0,1,1,
            0,0,0 
        };
        initializeMino(&zmino,grid,3);
    }

}
void copyMino(Mino* a,Mino* b){
    a->width = b->width;
    a->x = b->x;
    a->y = b->y;
    a->x_min = b->x_min;
    a->x_max = b->x_max;
    a->y_min = b->y_min;
    a->y_max = b->y_max;
    a->grid = (int*)malloc(sizeof(int)*(b->width * b->width));
    for(int i = 0;i < (b->width * b->width);i++){
        a->grid[i] = b->grid[i];
    }
}
void printGame(){
    system("clear");  // Unix環境
    for(int i = 0;i <= BLOCK_HEIGHT ;i++){
        printf("%2d ",i);
        for(int j = -1;j <= BLOCK_WIDTH;j++){
            int y = i - mino.y;
            int x = j - mino.x;
            if(i == BLOCK_HEIGHT  || j == -1 || j == BLOCK_WIDTH) printf("■ ");
            else if(0 <= i && i < BLOCK_HEIGHT && 
                    0 <= j && j < BLOCK_WIDTH &&
                    blockGrid[i][j] == 1) printf("□ ");
            else if(0 <= i && i < BLOCK_HEIGHT && 
                    0 <= j && j < BLOCK_WIDTH &&
                    blockGrid[i][j] == 2) printf("＊");
            else if( (0 <= x && x < mino.width) && 
                    (0 <= y && y <  mino.width) &&
                    mino.grid[( y )  * mino.width + x]) printf("◆ ");
            else printf("  "); 
        }
        printf("\n");
    }
}
// dx[i],dy[i]だけずらした場合にミノがめり込まないか判定し めり込まないならiを返す
int checkFittable(int* newMinoGrid,int* dx,int* dy,int n){
    for(int k = 0;k < n;k++){
        int flag = 1;
        for(int i = 0;i < mino.width;i++){
            for(int j = 0;j < mino.width;j++){
                if(!newMinoGrid[i * mino.width + j]) continue; 
                int x = mino.x + j + dx[k];
                int y = mino.y + i + dy[k];
                if(x < 0 || BLOCK_WIDTH <= x || y >= BLOCK_HEIGHT || blockGrid[y][x]){
                        flag = 0;
                        break;
                }
            }
            if(!flag) break;
        }
        if(flag) {
            return k;
            break;
        }
    }
    return -1;
}
void cwRotate(){
    int* newMinoGrid = (int*)malloc(sizeof(int) * (mino.width * mino.width));
    // 回転前の行列A  回転後の行列Bとする
    // B[i][j] = A[n - 1 - j][i] に変更
    for(int i = 0;i < mino.width;i++){
        for(int j = 0;j < mino.width;j++){
            newMinoGrid[i*mino.width + j] = mino.grid[(mino.width - 1 - j)*mino.width + i];
        }
    }
    // 回転可能かどうか判定
    // 回転したミノがめり込むなら、左、右、上の順に1ますずらした場合にめり込むか再判定
    // どの場合もめり込むなら回転はしない
    int dx[] = {0,-1,1,0};
    int dy[] = {0,0,0,-1};
    int ok = checkFittable(newMinoGrid,dx,dy,4);
    if(ok == -1) return ;
    mino.x += dx[ok];
    mino.y += dy[ok];

    for(int i = 0;i < mino.width * mino.width;i++) mino.grid[i] = newMinoGrid[i];
    int n = mino.width - 1;
    int xmin = mino.x_min;
    int xmax = mino.x_max;
    int ymin = mino.y_min;
    int ymax = mino.y_max;
    mino.x_min = n - ymax;
    mino.x_max = n - ymin;
    mino.y_min = xmin;
    mino.y_max = xmax;
}
void ccwRotate(){
    int* newMinoGrid = (int*)malloc(sizeof(int) * (mino.width * mino.width));
    for(int i = 0;i < mino.width;i++){
        for(int j = 0;j < mino.width;j++){
            newMinoGrid[i*mino.width + j] = mino.grid[j*mino.width + (mino.width - 1 - i)];
        }
    }
    int dx[] = {0,-1,1,0};
    int dy[] = {0,0,0,-1};
    int ok = checkFittable(newMinoGrid,dx,dy,4);
    if(ok == -1) return ;
    mino.x += dx[ok];
    mino.y += dy[ok];

    for(int i = 0;i < mino.width * mino.width;i++) mino.grid[i] = newMinoGrid[i];
    int n = mino.width - 1;
    int xmin = mino.x_min;
    int xmax = mino.x_max;
    int ymin = mino.y_min;
    int ymax = mino.y_max;
    mino.x_min = ymin;
    mino.x_max = ymax;
    mino.y_min = n - xmax;
    mino.y_max = n - xmin;
}
// ミノをdx,dyだけ動かした場合に、壁やブロックと重なるかどうか判定
int  checkMinoOver(int dx,int dy){
    for(int i = 0;i < mino.width;i++){
        for(int j = 0;j < mino.width;j++){
            if(!mino.grid[i*mino.width + j]) continue;
            int y = mino.y + i + dy;
            int x = mino.x + j + dx;
            if(x < 0 || BLOCK_WIDTH <= x) return 1;
            if(y >= BLOCK_HEIGHT) return 1;
            if(blockGrid[y][x]) return 1;
        }
    }
    return 0;
}
void checkMinoClls(){
    // ミノが他のブロックや壁に衝突した場合に修正する
    if(mino.x + mino.x_min < 0) mino.x += 1;
    if(mino.x + mino.x_max >= BLOCK_WIDTH) mino.x -= 1;
    for(int i = 0;i < mino.width;i++){
        for(int j = 0;j < mino.width;j++){

        }
    }
    if(checkMinoOver(0,0)) {
        // game_counter = 1;
        mino.y -= 1;
    }
}
void stopMino(){
    for(int i = 0;i < mino.width;i++){
        for(int j = 0;j < mino.width;j++){
            if(!mino.grid[i * mino.width + j]) continue;
            int x = mino.x + j;
            int y = mino.y + i;
            blockGrid[y][x] = 1;
        }
    }
}
void checkDeleteMino(){
    for(int i = 0;i < BLOCK_HEIGHT;i++) deletedFlag[i] = 0;
    for(int i = 0;i < BLOCK_HEIGHT;i++){
        int flag = 1;
        for(int j = 0;j < BLOCK_WIDTH;j++){
            if(!blockGrid[i][j]){
                flag = 0;
                break;
            }
        }
        if(flag) {
            situation = DELETING;
            deletedFlag[i] = 1;
            for(int j = 0;j < BLOCK_WIDTH;j++) blockGrid[i][j] = 2;
        }
    }
}
void deleteMino(){
    // deletedFlagが立っている行を順に（下から）消していく
    int cnt = 0; //消した行の数
    for(int i = BLOCK_HEIGHT - 1;i >= 0;i--){
        if(deletedFlag[i]) {
            cnt++;
            continue;
        } 
        for(int j = 0;j < BLOCK_WIDTH;j++) blockGrid[i + cnt][j] = blockGrid[i][j];
    }
    for(int i = 0;i < cnt;i++){
        for(int j = 0;j < BLOCK_WIDTH;j++){
            blockGrid[i][j] = 0;
        }
    }
}
void moveMino(){
    //落下
    if(game_counter == 0) mino.y += 1;
    // 着地判定
    if(checkMinoOver(0,0)){
        situation = LANDING;
        mino.y -= 1;
        stopMino();
        checkDeleteMino();
        return ;
    }
    //key入力の受取・ミノの操作
    char c = getPushedKey();
    if(c == 'd' && !checkMinoOver(1,0)) mino.x += 1;
    if(c == 'a' && !checkMinoOver(-1,0)) mino.x -= 1;
    if(c == 's' && !checkMinoOver(0,1)) {
        mino.y += 1;
        game_counter = 1;
    }
    //強制落下
    if(c == ' ') {
        int counter = 0;
        while(1){
            if(checkMinoOver(0,counter)) break;
            counter++;
        }
        situation = LANDING;
        mino.y += counter - 1;
        stopMino();
        checkDeleteMino();
        return ;
    }
    if(c == 'j') ccwRotate();
    if(c == 'k') cwRotate();
}
void resetMino(){
    game_counter = 1;
    static Mino* minos[7] = {&imino,&omino,&tmino,&jmino,&lmino,&smino,&zmino};

    int idx = rand() % 7;
    copyMino(&mino,minos[idx]);
    mino.x = BLOCK_WIDTH / 2 - (mino.width / 2);
    mino.y = -mino.y_min;
    debug_msg[2] = mino.y_min;

    if(checkMinoOver(0,0)) situation = GAMEOVER;

}
int main(void){
    initializeAllMino();
    srand(time(NULL));

    int cnt = 0;
    copyMino(&mino,&tmino);
    while(1){
        game_counter = (game_counter + 1) % GAME_CYCLE;

        switch(situation){
            case RESETTING:
                situation = FALLING;
                resetMino();
                debug_msg[0] += 1;
                break;
            case LANDING:
                // if(game_counter == 0) situation = RESETTING;
                situation = RESETTING;
                break;
            case FALLING:
                moveMino();
                break;
            case DELETING:
                if(game_counter == 0) {
                    deleteMino();
                    situation = RESETTING;
                }
                break;
            case GAMEOVER:
                printGame();
                printf("Game Over \n");
                exit(0);
                break;
        }

        printGame();
        printf("   counter : %d\n",cnt++);
        printf("   x : %d, y : %d\n",mino.x,mino.y);
        printf("   ");
        for(int i = 0;i < 10;i++){
            printf("%d ",debug_msg[i]);
        }
        printf("\n");
        usleep(SYSTEM_CYCLE);
    }

}
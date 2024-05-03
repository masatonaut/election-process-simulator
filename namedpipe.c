#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

int main() {
    const char *fifoPath = "/tmp/voter_fifo"; // FIFOファイルのパス
    int fifoFd; // FIFOのファイルディスクリプタ
    int voterID;
    char voteResult[20];

    // Named pipe (FIFO)の作成
    if (mkfifo(fifoPath, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // ランダムなIDの読み込みと検証
    while (scanf("%d", &voterID) > 0) {
        // 20%の確率で投票不可と判定
        if (rand() % 100 < 20) {
            sprintf(voteResult, "%d cannot vote\n", voterID);
        } else {
            sprintf(voteResult, "%d can vote\n", voterID);
        }

        // Named pipeを開いて結果を書き込む
        fifoFd = open(fifoPath, O_WRONLY);
        if (fifoFd == -1) {
            perror("open");
            continue;
        }
        write(fifoFd, voteResult, sizeof(voteResult));
        close(fifoFd);
    }

    // FIFOの削除
    unlink(fifoPath);
    
    return 0;
}

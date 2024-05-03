#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main() {
    int pipefd[2]; // Pipe用のファイルディスクリプタ
    pid_t pid;     // プロセスID
    int voterCount = 10; // 選挙人数
    int voterID;

    // Pipeの作成
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 子プロセスの生成
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // 子プロセス
        close(pipefd[1]); // 書き込み用のエンドを閉じる

        printf("Child (Checker) process reading IDs:\n");
        while (read(pipefd[0], &voterID, sizeof(voterID)) > 0) {
            printf("Voter ID: %d\n", voterID);
        }
        close(pipefd[0]); // 読み込み完了後、読み込み用のエンドを閉じる
        exit(EXIT_SUCCESS);
    } else {
        // 親プロセス
        close(pipefd[0]); // 読み込み用のエンドを閉じる
        srand(time(NULL)); // 乱数の初期化

        // ランダムなIDの生成と送信
        for (int i = 0; i < voterCount; i++) {
            voterID = rand() % 10000;  // 0から9999のランダムなID
            write(pipefd[1], &voterID, sizeof(voterID));
        }
        close(pipefd[1]); // 全てのIDを送信した後、書き込み用のエンドを閉じる
        wait(NULL); // 子プロセスの終了を待つ
    }

    return 0;
}

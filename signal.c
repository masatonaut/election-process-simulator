#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

// シグナルハンドラの定義
void signalHandler(int signum) {
    printf("Signal with number %i has arrived\n", signum);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_voters>\n", argv[0]);
        return 1;
    }

    int number_of_voters = atoi(argv[1]);
    int pipefd[2];
    pid_t pid;
    int buffer;

    // Pipeの生成
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // シグナルハンドラの設定
    signal(SIGUSR1, signalHandler);

    // 子プロセスの生成
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // 子プロセス（検査メンバー）
        close(pipefd[1]);  // 書き込み用のエンドを閉じる

        // 親プロセスに準備完了を通知
        sleep(1);
        kill(getppid(), SIGUSR1);
        printf("Child process is ready, signal sent to parent.\n");

        // Pipeからデータを読み込み、画面に表示
        while (read(pipefd[0], &buffer, sizeof(int)) > 0) {
            printf("Received ID: %d\n", buffer);
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    } else {
        // 親プロセス（プレジデント）
        close(pipefd[0]);  // 読み込み用のエンドを閉じる

        // 子プロセスの準備完了を待つ
        pause();
        printf("Parent process received ready signal from child.\n");

        srand(time(NULL));  // 乱数のシード設定

        // 各選挙人に対して識別番号を生成し、Pipeに書き込み
        for (int i = 0; i < number_of_voters; i++) {
            buffer = rand();  // ランダムな識別番号を生成
            write(pipefd[1], &buffer, sizeof(int));
        }

        close(pipefd[1]);  // データの書き込みが完了したらエンドを閉じる
        wait(NULL);  // 子プロセスの終了を待つ
        printf("Parent process ended\n");
    }

    return 0;
}

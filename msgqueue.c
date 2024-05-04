#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

// シグナルハンドラの定義
void signalHandler(int signum) {
    printf("Signal with number %i has arrived\n", signum);
}

void secondChildHandler(int signum) {
    printf("secondChild: Signal %d received, now starting to read from FIFO.\n", signum);
}

int main(int argc, char *argv[]) {
    // コマンドライン引数の確認
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_voters>\n", argv[0]);
        return 1;
    }

    int number_of_voters = atoi(argv[1]);  // 選挙人数の読み取り
    int pipefd[2], fd;  // パイプとファイルディスクリプタの定義
    pid_t firstChild, secondChild;  // 子プロセスのPID
    int buffer;  // 識別番号を格納するバッファ
    char *fifo = "/tmp/myfifo";  // FIFOのパス

    // Pipeの生成
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // FIFOの生成
    mkfifo(fifo, 0666);

    // シグナルハンドラの設定
    signal(SIGUSR1, signalHandler);

    // firstChildプロセスの生成
    firstChild = fork();
    if (firstChild == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (firstChild == 0) {
        // firstChildプロセス
        close(pipefd[1]);  // 書き込み用のエンドを閉じる
        printf("firstChild started, pipe for writing closed.\n");

        // 親プロセスに準備完了を通知
        sleep(1);  // 準備に少し時間を与える
        kill(getppid(), SIGUSR1);  // 親プロセスにシグナルを送信
        printf("Child process is ready, signal sent to parent.\n");

        // secondChildプロセスの生成
        secondChild = fork();
        if (secondChild == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (secondChild == 0) {
            // secondChildプロセスのシグナルハンドラ設定
            signal(SIGUSR1, secondChildHandler);
            // secondChildプロセス
            int read_fd = open(fifo, O_RDONLY);
            printf("secondChild started, waiting to read from FIFO.\n");
            pause(); // SIGUSR1 を待つ

            char vote_info[256];
            while (read(read_fd, vote_info, sizeof(vote_info)) > 0) {
                printf("secondChild reads: %s\n", vote_info);
            }
            close(read_fd);
            printf("secondChild ends.\n");
            exit(EXIT_SUCCESS);
        } else {
            // firstChild continues here
            char vote_info[256];
            int write_fd = open(fifo, O_WRONLY);
            printf("firstChild writing to FIFO.\n");
            srand(time(NULL));
            for (int i = 0; i < number_of_voters; i++) {
                read(pipefd[0], &buffer, sizeof(int));
                float chance = (float)rand() / RAND_MAX;
                sprintf(vote_info, "ID %d: %s", buffer, (chance < 0.2) ? "cannot vote" : "can vote");
                write(write_fd, vote_info, sizeof(vote_info));
            }
            close(pipefd[0]);
            close(write_fd);
            kill(secondChild, SIGUSR1);  // secondChildにデータの準備ができたことを通知
            wait(NULL);  // Wait for secondChild to finish
            printf("firstChild ends.\n");
            exit(EXIT_SUCCESS);
        }
    } else {
        // 親プロセス（プレジデント）
        close(pipefd[0]);  // 読み込み用のエンドを閉じる
        printf("Parent process starting.\n");

        srand(time(NULL));  // 乱数のシード設定
        for (int i = 0; i < number_of_voters; i++) {
            buffer = rand();  // ランダムな識別番号を生成
            write(pipefd[1], &buffer, sizeof(int));
        }

        close(pipefd[1]);  // データの書き込みが完了したらエンドを閉じる
        wait(NULL);  // firstChildの終了を待つ
        printf("Parent process ended.\n");
    }

    unlink(fifo);  // FIFOを削除
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/msg.h>    // メッセージキュー操作のためのヘッダー
#include <sys/sem.h>
#include <string.h>     // strstr関数のためのヘッダー
#include <errno.h>      // エラー番号定義のためのヘッダー

// メッセージキュー用の構造体定義
struct message {
    long msg_type;
    char msg_text[100];
};

// #if !defined(_SEM_SEMUN_UNDEFINED)
// union semun {
//     int val;
//     struct semid_ds *buf;
//     unsigned short *array;
// };
// #endif

// シグナルハンドラの定義
void signalHandler(int signum) {
    printf("Signal with number %i has arrived\n", signum);
}

void secondChildHandler(int signum) {
    printf("secondChild: Signal %d received, now starting to read from FIFO.\n", signum);
}

int init_semaphore() {
    int semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    union semun arg;
    arg.val = 1; // セマフォの初期値 (1は一度に1つのプロセスのみ許可)
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl failed");
        exit(EXIT_FAILURE);
    }
    return semid;
}

void P(int semid) {
    struct sembuf sop = {0, -1, 0};  // P操作
    if (semop(semid, &sop, 1) == -1) {
        perror("P operation failed");
        exit(EXIT_FAILURE);
    }
}

void V(int semid) {
    struct sembuf sop = {0, 1, 0};  // V操作
    if (semop(semid, &sop, 1) == -1) {
        perror("V operation failed");
        exit(EXIT_FAILURE);
    }
}

void write_log(const char *filename, const char *entry) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s\n", entry);
    fclose(file);
}

int main(int argc, char *argv[]) {
    // コマンドライン引数の確認
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_voters>\n", argv[0]);
        return 1;
    }

    int number_of_voters = atoi(argv[1]);  // 選挙人数の読み取り
    int semid = init_semaphore();
    char *log_file = "./leave_log.txt";

    int pipefd[2], fd;  // パイプとファイルディスクリプタの定義
    pid_t firstChild, secondChild;  // 子プロセスのPID
    int buffer;  // 識別番号を格納するバッファ
    char *fifo = "/tmp/myfifo";  // FIFOのパス
    int msgid;
    struct message msg;

    // Pipeの生成
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // FIFOファイルの生成前に既存のファイルを確認し、存在する場合は削除
    struct stat st;
    if (stat(fifo, &st) == 0) {
        // ファイルが存在する場合、削除を試みる
        if (unlink(fifo) == -1) {
            perror("Failed to remove existing fifo");
            exit(EXIT_FAILURE);
        }
    }

    // FIFOの生成
    if (mkfifo(fifo, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // シグナルハンドラの設定
    signal(SIGUSR1, signalHandler);

    // メッセージキューの生成
    msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

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
            int vote_result;
            srand(time(NULL));
            while (read(read_fd, vote_info, sizeof(vote_info)) > 0) {
                printf("Processing vote for: %s\n", vote_info); // 追加：読み込んだ情報のログ出力
                if (strstr(vote_info, "can vote")) {
                    vote_result = (rand() % 6) + 1;  // 1から6の投票結果
                    sprintf(msg.msg_text, "%s, voted: %d", vote_info, vote_result);
                    msg.msg_type = 1;
                    if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                        perror("msgsnd failed");
                    }
                    printf("Voted info sent to queue: %s\n", msg.msg_text); // 追加：キューへの書き込みログ
                }
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
        // メッセージキューから投票結果を読み取り
        printf("Parent process reading from message queue.\n");
        int rc;
        while ((rc = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT)) >= 0) {
            printf("Received from message queue: %s\n", msg.msg_text);
        }
        if (rc == -1 && errno != ENOMSG) {
            perror("msgrcv error"); // Improved error message
        }

        msgctl(msgid, IPC_RMID, NULL);

        // 親プロセスが子プロセスを起動した後、一人のメンバーが部屋を離れるシミュレーション
        P(semid);  // セマフォを取得し、部屋を離れる
        char time_buffer[100];
        time_t now = time(NULL);
        strftime(time_buffer, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
        write_log(log_file, "Member leaves the room at: ");
        write_log(log_file, time_buffer);

        sleep(5);  // 外にいることをシミュレート

        now = time(NULL);
        strftime(time_buffer, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
        write_log(log_file, "Member returns to the room at: ");
        write_log(log_file, time_buffer);
        V(semid);  // セマフォを解放し、部屋に戻る

        printf("Operation logged in %s\n", log_file);
        printf("Parent process ended.\n");
        exit(0);
    }
    unlink(fifo);  // FIFOを削除
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#define FIFO_PATH "voter_fifo"
#define SOCKET_PATH "/tmp/voter_socket"
#define SEM_NAME "/leave_semaphore"
#define LOG_FILE "leave_log.txt"

// Function declarations
void setup_fifo();
void setup_socket(int *sockfd, struct sockaddr_un *address);
void log_time(FILE *log_file, int id, const char *action);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number of voters>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int number_of_voters = atoi(argv[1]);
    int pfds[2], fd, pid1, pid2, server_sock, client_sock;
    char buffer[128], voter_info[128];
    sem_t *sem;
    FILE *log_file;
    struct sockaddr_un server_addr;

    // Set up FIFO and UNIX domain socket
    setup_fifo();
    setup_socket(&server_sock, &server_addr);

    // Create semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("Semaphore open failed");
        exit(EXIT_FAILURE);
    }

    // Open log file for time logging
    log_file = fopen(LOG_FILE, "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    // Create a pipe
    if (pipe(pfds) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Accept a connection
    client_sock = accept(server_sock, NULL, NULL);
    if (client_sock < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    // Create first child process
    if ((pid1 = fork()) == 0) {
        close(pfds[1]);
        fd = open(FIFO_PATH, O_WRONLY);
        srand(getpid());

        while (read(pfds[0], buffer, sizeof(buffer)) > 0) {
            int can_vote = rand() % 5; // 20% chance to not be allowed to vote
            sprintf(voter_info, "Voter ID: %s, %s", buffer, (can_vote == 0) ? "cannot vote" : "can vote");
            write(fd, voter_info, strlen(voter_info) + 1);
        }

        close(pfds[0]);
        close(fd);
        exit(0);
    }

    // Create second child process
    if ((pid2 = fork()) == 0) {
        fd = open(FIFO_PATH, O_RDONLY);
        while (read(fd, voter_info, sizeof(voter_info)) > 0) {
            int vote = (rand() % 6) + 1; // Generate a vote between 1 and 6
            sprintf(buffer, "%s - Vote: %d", voter_info, vote);
            write(client_sock, buffer, strlen(buffer) + 1);
        }

        close(fd);
        exit(0);
    }

    // Parent process
    for (int i = 0; i < number_of_voters; i++) {
        sprintf(buffer, "%d", i);
        write(pfds[1], buffer, strlen(buffer) + 1);
    }
    close(pfds[1]);

    // Log leave times with semaphore
    sem_wait(sem);
    log_time(log_file, 1, "left");
    sleep(rand() % 3 + 1); // Simulate time out of the room
    log_time(log_file, 1, "returned");
    sem_post(sem);

    // Read data from the socket
    while (read(client_sock, buffer, sizeof(buffer)) > 0) {
        printf("President reads: %s\n", buffer);
    }

    fclose(log_file);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    close(server_sock);
    close(client_sock);
    unlink(SOCKET_PATH);
    wait(NULL);
    wait(NULL);

    return 0;
}

void setup_fifo() {
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

void setup_socket(int *sockfd, struct sockaddr_un *address) {
    *sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address->sun_family = AF_UNIX;
    strcpy(address->sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH);

    if (bind(*sockfd, (struct sockaddr *)address, sizeof(*address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    listen(*sockfd, 5);
}

void log_time(FILE *log_file, int id, const char *action) {
    time_t now = time(NULL);
    fprintf(log_file, "Member %d %s at %s", id, action, ctime(&now));
}

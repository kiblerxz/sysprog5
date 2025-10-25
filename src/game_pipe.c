#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

static ssize_t xread(int fd, void *buf, size_t n){
    size_t got=0; while(got<n){
        ssize_t r = read(fd, (char*)buf+got, n-got);
        if(r==0) return 0; if(r<0){ if(errno==EINTR) continue; return -1; }
        got+=r;
    } return (ssize_t)got;
}
static ssize_t xwrite(int fd, const void *buf, size_t n){
    size_t put=0; while(put<n){
        ssize_t r = write(fd, (const char*)buf+put, n-put);
        if(r<=0){ if(errno==EINTR) continue; return -1; }
        put+=r;
    } return (ssize_t)put;
}

static int next_guess_linear(int *tried, int N){
    for(int i=1;i<=N;i++) if(!tried[i]) return i;
    return -1;
}

static void play_setter(int w_ctrl, int r_guess, int w_reply, int N){
    int start=1; xwrite(w_ctrl, &start, sizeof(start)); // сказать "начинай"
    int target = 1 + rand()%N;
    for(;;){
        int g;
        if(xread(r_guess, &g, sizeof(g)) <= 0) break;
        int eq = (g==target) ? 1 : 0;
        xwrite(w_reply, &eq, sizeof(eq));
        if(eq) break;
    }
}

static int play_guesser(int r_ctrl, int w_guess, int r_reply, int N, int attempt_log){
    int go=0; xread(r_ctrl, &go, sizeof(go));
    int *tried = calloc((size_t)N+1, sizeof(int));
    if(!tried){ perror("calloc"); exit(1); }
    int attempts=0;
    while(1){
        int g = next_guess_linear(tried,N);
        if(g<0){ fprintf(stderr,"no candidates\n"); free(tried); return attempts; }
        tried[g]=1; attempts++;
        xwrite(w_guess, &g, sizeof(g));
        int eq=0; xread(r_reply, &eq, sizeof(eq));
        if(attempt_log) printf("guess %d -> %s\n", g, eq?"EQUAL":"NO");
        if(eq){ free(tried); return attempts; }
    }
}

int main(int argc, char **argv){
    int N=100, rounds=10;
    if(argc>=2) N = atoi(argv[1])>0?atoi(argv[1]):100;
    if(argc>=3) rounds = atoi(argv[2])>0?atoi(argv[2]):10;

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    int p2c_ctrl[2], p2c_guess[2], c2p_reply[2];
    if(pipe(p2c_ctrl)<0 || pipe(p2c_guess)<0 || pipe(c2p_reply)<0){ perror("pipe"); return 1; }

    pid_t child = fork();
    if(child<0){ perror("fork"); return 1; }

    if(child==0){
        // child uses: read from p2c_*, write to c2p_reply
        close(p2c_ctrl[1]); close(p2c_guess[1]); close(c2p_reply[0]);

        for(int r=1;r<=rounds;r++){
            if(r%2==1){
                // child = guesser
                int attempts = play_guesser(p2c_ctrl[0], p2c_guess[1], c2p_reply[0], N, 1);
                printf("[child] round %d attempts=%d\n", r, attempts);
            }else{
                // child = setter
                play_setter(c2p_reply[1], p2c_guess[0], p2c_ctrl[1], N);
                printf("[child] round %d done (setter)\n", r);
            }
        }
        close(p2c_ctrl[0]); close(p2c_guess[0]); close(c2p_reply[1]);
        return 0;
    }else{
        // parent uses: write to p2c_*, read from c2p_reply
        close(p2c_ctrl[0]); close(p2c_guess[0]); close(c2p_reply[1]);

        for(int r=1;r<=rounds;r++){
            if(r%2==1){
                // parent = setter
                play_setter(p2c_ctrl[1], p2c_guess[0], c2p_reply[1], N);
                printf("[parent] round %d done (setter)\n", r);
            }else{
                // parent = guesser
                int attempts = play_guesser(c2p_reply[0], p2c_guess[1], p2c_ctrl[0], N, 1);
                printf("[parent] round %d attempts=%d\n", r, attempts);
            }
        }
        close(p2c_ctrl[1]); close(p2c_guess[1]); close(c2p_reply[0]);

        int st=0; waitpid(child,&st,0);
        return 0;
    }
}

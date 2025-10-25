#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

static pid_t peer = -1;
static volatile sig_atomic_t got_guess = 0;
static volatile sig_atomic_t last_guess = 0;
static volatile sig_atomic_t result_equal = -1;

static void on_rt(int signo, siginfo_t *si, void *u){ (void)u; (void)signo; last_guess = si->si_value.sival_int; got_guess = 1; }
static void on_usr1(int s){ (void)s; result_equal = 1; }
static void on_usr2(int s){ (void)s; result_equal = 0; }

static int next_guess_linear(int *tried, int N){ for(int i=1;i<=N;i++) if(!tried[i]) return i; return -1; }

static void setup_handlers(void){
    sigset_t mask; sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1); sigaddset(&mask, SIGUSR2); sigaddset(&mask, SIGRTMIN);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    struct sigaction srt = {0}; srt.sa_sigaction = on_rt; srt.sa_flags = SA_SIGINFO; sigaction(SIGRTMIN, &srt, NULL);
    struct sigaction su1 = {0}, su2 = {0}; su1.sa_handler = on_usr1; su2.sa_handler = on_usr2;
    sigaction(SIGUSR1, &su1, NULL); sigaction(SIGUSR2, &su2, NULL);
}

# 123
static void play_round_setter(int N){
    int target = 1 + rand()%N;
    got_guess = 0; result_equal = -1;
    sigset_t empty; sigemptyset(&empty);
    for(;;){
        while(!got_guess) sigsuspend(&empty);
        got_guess = 0;
        int g = last_guess;
        if(g == target){ kill(peer, SIGUSR1); break; }
        else            { kill(peer, SIGUSR2); }
    }
}

static int play_round_guesser(int N, int attempt_log){
    int *tried = calloc((size_t)N+1, sizeof(int));
    if(!tried){ perror("calloc"); exit(1); }
    sigset_t empty; sigemptyset(&empty);
    int attempts = 0;
    for(;;){
        int g = next_guess_linear(tried, N);
        if(g < 0){ free(tried); return attempts; }
        tried[g] = 1; attempts++;
        union sigval sv; sv.sival_int = g;
        if(sigqueue(peer, SIGRTMIN, sv) != 0){ perror("sigqueue"); free(tried); exit(1); }
        result_equal = -1;
        while(result_equal == -1) sigsuspend(&empty);
        if(attempt_log) printf("guess %d -> %s\n", g, (result_equal==1?"EQUAL":"NO"));
        if(result_equal == 1){ free(tried); return attempts; }
    }
}

int main(int argc, char **argv){
    setvbuf(stdout, NULL, _IOLBF, 0);
    int N = 100, rounds = 10;
    if(argc>=2){ int t=atoi(argv[1]); if(t>0) N=t; }
    if(argc>=3){ int t=atoi(argv[2]); if(t>0) rounds=t; }
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    pid_t child = fork();
    if(child < 0){ perror("fork"); return 1; }
    if(child == 0){
        peer = getppid();
        setup_handlers();
        for(int r=1;r<=rounds;r++){
            if(r%2==1){ int a=play_round_guesser(N,1); printf("[child]  round %d: attempts=%d\n", r, a); }
            else      { play_round_setter(N);          printf("[child]  round %d: done (setter)\n", r); }
        }
        return 0;
    }else{
        peer = child;
        setup_handlers();
        for(int r=1;r<=rounds;r++){
            if(r%2==1){ play_round_setter(N);          printf("[parent] round %d: done (setter)\n", r); }
            else      { int a=play_round_guesser(N,1); printf("[parent] round %d: attempts=%d\n", r, a); }
        }
        int st=0; waitpid(child,&st,0);
        return 0;
    }
}

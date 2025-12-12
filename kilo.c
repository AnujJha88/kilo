#include<unistd.h>
#include<errno.h>
#include<termios.h>
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>


#define CTRL_KEY(k) ((k)&(0x1f))


struct termios orig;

void die(const char* s){
    perror(s);
    exit(1);
}


void disableRawMode(){
   if( tcsetattr(STDIN_FILENO,TCSAFLUSH, &orig)==-1)die("tcsetattr");
}

void enableRawMode(){
    if(tcgetattr(STDIN_FILENO,&orig)==-1)die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw=orig;
    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &=~(OPOST);
    raw.c_cflag |=(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1;
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1)die("tcsetattr");
}

char editorReadKey(){
    int nread;
    char c;
    while((nread=read(STDIN_FILENO,&c,1))!=1){
        if(nread==-1 && errno != EAGAIN) die("read");

    }
    return c;
}

void editorProcessKey(){
    char c=editorReadKey();
    switch(c){
        case CTRL_KEY(q):
        exit(0);
        break;
    }
}

int main(){

    enableRawMode();

    while(1){
        editorProcessKey();
    }

    return 0;
}

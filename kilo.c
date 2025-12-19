#include<unistd.h>
#include<errno.h>
#include<termios.h>
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<sys/ioctl.h>

/// DEFINES
#define CTRL_KEY(k) ((k)&(0x1f))

/// DATA

struct editorConfig{
    int screenRows;
    int screenCols;
    struct termios orig;
};

struct editorConfig E;



///TERMINAL


void die(const char* s){
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);

    perror(s);
    exit(1);
}


void disableRawMode(){
   if( tcsetattr(STDIN_FILENO,TCSAFLUSH, &E.orig)==-1)die("tcsetattr");
}

void enableRawMode(){
    if(tcgetattr(STDIN_FILENO,&E.orig)==-1)die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw=E.orig;
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
int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i=0;
    if(write(STDOUT_FILENO,"\x1b[6n",4)!=4)return -1;

  while(i<sizeof(buf)-1){
        if(read(STDIN_FILENO,&buf[i],1)!=1)break;
        if(buf[i]=='R')break;
        i++;
    }
    buf[i]='\0';
    if(buf[0]!='\x1b'||buf[1]!='[')return -1;
    if(sscanf(&buf[2],"%d,%d",rows,cols)!=2)return -1;
         return 0;
}

int getWindowSize(int* rows,int* cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==-1||ws.ws_col==0){
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12)!=12)return -1;
               return getCursorPosition(rows,cols);
    }
    else{
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;
    }
}


/// APPEND BUFFER

struct abuf{
    char* b;
    int len;
};


#define ABUF_INIT {NULL,0} // an empty append buffer

void abAppend(struct abuf* ab, int len, const char* s){
    char* new= realloc(ab->b, ab->len+len);//make sure we have enough space

    if(new==NULL) return;
    //now we use memcpy to copy the new string into the buffer

    memcpy(&new[ab->len], s,len);
    ab->b=new;
    ab->len+=len;

}

void abFree(struct abuf *ab){
    free(ab->b);
}


///OUTPUT



void editorDrawRows(struct abuf *ab){
    int y;
    for(y=0;y<E.screenRows;y++){
        //write(STDOUT_FILENO,"~",3);
        //if(y<E.screenRows-1)write(STDOUT_FILENO,"\r\n",2);
        abAppend(ab, "~",1);
        if(y<E.screenRows-1){
            abAppend(ab,"\r\n",2);
        }
    }


}

void editorRefresh(){
    //write(STDOUT_FILENO,"\x1b[2J",4);//clear screen fully. keeps the cursor at bottom
    //write(STDOUT_FILENO,"\x1b[H",3);//move cursor to top left
   // editorDrawRows();
    //write(STDOUT_FILENO,"\x1b[H",3);//move cursor to top left

    struct abuf ab=ABUF_INIT;
      abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[2J",4);
    abAppend(&ab, "\x1b[H",3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H",3);

      abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);

}


/// INPUT

void editorProcessKey(){
    char c=editorReadKey();
    switch(c){
        case CTRL_KEY('q'):
        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        exit(0);
        break;
    }
}

///INIT

void initEditor(){
if(getWindowSize(&E.screenRows,&E.screenCols)==-1 )die("getWindowSize");

}

int main(){

    enableRawMode();
    initEditor();

    while(1){
        editorRefresh();
        editorProcessKey();
    }

    return 0;
}

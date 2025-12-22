#include<unistd.h>
#include<errno.h>
#include<termios.h>
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<sys/ioctl.h>
#include<string.h>

/// DEFINES
#define CTRL_KEY(k) ((k)&(0x1f))

#define KILO_VERSION "0.0.1"
enum editorKey{
    ARROW_LEFT=1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};
/// DATA

struct editorConfig{
    int cx,cy;
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

int  editorReadKey(){
    int nread;
    char c;
    while((nread=read(STDIN_FILENO,&c,1))!=1){
        if(nread==-1 && errno != EAGAIN) die("read");

    }
    if(c=='\x1b'){
        char seq[3];
        if(read(STDIN_FILENO, &seq[0],1)!=1)return '\x1b';
        if(read(STDIN_FILENO, &seq[1],1)!=1)return '\x1b';
        if(seq[0]=='['){
            if(seq[1]>='0' && seq[1]<='9'){
                if(read(STDIN_FILENO,&seq[2],1)!=1){
                    return '\x1b';
                }
                if(seq[2]=='~'){
                    switch(seq[1]){
                        case '5':return PAGE_UP;
                        case '6':return PAGE_DOWN;
                    }
                }

            if(read(STDIN_FILENO,&seq[2],1)!=1){
                return '\x1b';
            }

                            }

            else{
            switch (seq[1]){
                case 'A':return ARROW_UP;
                case 'B':return ARROW_DOWN;
                case 'C':return ARROW_RIGHT;
                case 'D':return ARROW_LEFT;
            }
        }
        }
        return '\x1b';
    }
    else {return c;}
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

void abAppend(struct abuf* ab,const char* s,int len){
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
        if(y==E.screenRows/3){
            char welcome[80];
            int welcomelen=snprintf(welcome,sizeof(welcome),"Kilo editor --version %s",KILO_VERSION);
            if (welcomelen>E.screenCols) welcomelen=E.screenCols;
            int padding=(E.screenCols-welcomelen)/2;
            if(padding){
                abAppend(ab,"~",1);
                padding--;
            }
            while(padding--) abAppend(ab," ",1);
            abAppend(ab,welcome,welcomelen);
        }
        else{
        abAppend(ab, "~",1);
        }
        abAppend(ab, "\x1b[K",3);
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
      abAppend(&ab, "\x1b[?25l", 6);// hide cursor
//more efficient to clear screen one line at a time
   //abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H",3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,E.cx+1);//this is ironically not a print to console but it mutates the buffer we give it
    abAppend(&ab,buf,strlen(buf));

      abAppend(&ab, "\x1b[?25h", 6);//show cursor

    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);

}


/// INPUT

void editorMoveCursor(int key){
    switch(key){
        case ARROW_LEFT:
        if(E.cx!=0)E.cx--;
        break;
        case ARROW_DOWN:
        if(E.cy!=E.screenCols-1)E.cy++;
        break;
        case ARROW_UP:
        if(E.cy!=0)E.cy--;
        break;
        case ARROW_RIGHT:
        if(E.cx!=E.screenRows-1)E.cx++;
        break;
    }
}

void editorProcessKey(){
    int c=editorReadKey();
    switch(c){
        case CTRL_KEY('q'):
        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        exit(0);
        break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times=E.screenRows;
                while(times--)editorMoveCursor(c==PAGE_UP?ARROW_UP:ARROW_DOWN);
            }
        break;
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
        editorMoveCursor(c);
        break;
    }
}

///INIT

void initEditor(){
    E.cx=0;
    E.cy=0;
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

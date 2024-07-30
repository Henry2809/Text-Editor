#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h> // how a termial interacts with a user & program running on a compute
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h> // to get window size
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h> // allows functions to accept an indefinite number of arguments
#include <fcntl.h>

/***** Feature test macro - compiler complains about getline() *****/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

/***** defines *****/
#define CTRL_KEY(k) ((k) & 0x1f) // if k is A which is 65 then 65 & 31. 0x1f = 0001 1111
#define ONREE_VERSION "0.0.1"
#define ONREE_TAB_STOP 8 // length of the tab stop as a constant
#define ONREE_QUIT_TIMES 3 // require the user to press ctrl-q 3 more times in order to quit w/o saving
#define HL_HIGHLIGHT_NUMBERS (1<<0) // shifting 1 to the left by 0 position, result 1
#define HL_HIGHLIGHT_STRINGS (1<<1) // resutl 2


struct editorSyntax {
        char *filetype; // filetype field is the name of the filetype that will be displayed to the user in the status bar
        char **filematch; // an array of strings, where each string contains a pattern to match a filename against
        char **keywords;
        char *singleline_comment_start;
        char *multiline_comment_start; // "/*"
        char *multiline_comment_end;// "*/""
        int flags; // bit field that will contain flags for whether to highlight numbers and whether to highlight strings for that filetype
};

/*** filetypes ***/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL }; // for the filematch
// common C types as secondary keywords so end each one with | char to color differently
char *CH_HLk_keywords[] = {"switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|" , NULL};


struct editorSyntax HLDB[] = { // HLDB means highlight database
        { // this is 1 struct in an array of struct
                "c", // filetype field
                C_HL_extensions, // filematch field
                CH_HLk_keywords,
                "//", "/*", "*/",
                HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS 
        },
};
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0])) // store the length of HLDB


enum editorKey{ // defined my own data types
        BACKSPACE = 127,
        ARROW_LEFT = 1000, // give a larger int so it doesn't conflict with w a s d. The rest will get increment by 1 
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        DEL_KEY, // <esc>[3~, 
        HOME_KEY, // <esc>[1~, <esc>[7~, <esc>[H, or <esc>OH, depebds on the OS. will cover all cases
        END_KEY, // <esc>[4~, <esc>[8~, <esc>[F, or <esc>OF
        PAGE_UP, // <esc>[5~ 
        PAGE_DOWN // <esc>[6~
        
};

enum editorHighlight{ // hl array will contain int from 0 to 255, this is either a char part of str, comment or number. This enum containing the value the hl array can contain
        HL_NORMAL = 0, // normal text
        HL_COMMENT, // single line comment
        HL_MLCOMMENT, // for multi line comments
        HL_KEYWORD1, // for actual keywords
        HL_KEYWORD2, // for common types names
        HL_STRING, // color string
        HL_NUMBER, // numbers only
        HL_MATCH
};

/***** data *****/
typedef struct erow{ // data type for storing a row of text in the edito
        int idx; // each erow knows its onw index within the file
        int size;
        int rsize; // tab size
        char *chars; // position in the actual text stored in the chars array of erow
        char *render; // tab char to draw on the screen, processed(copy) version of 'chars'. Represent the position in the rendered(displayed) version of a text row, where tab chars take up multiple cols
        unsigned char *hl; // for highlight the entire strings, keywords, comments of each line. Highlighting for each row of text before display it and then rehighlight a line whenever it gets changed. Each char in the array will correspond to a char in render
        int hl_open_comment; // whether the row ends in an unclosed multi-line comment
} erow; // editor row


struct editorConfig{
        int cx, cy; // for moving the cursor around. cx - is horizontal coor(column) index into chars, cy - vertical coor(row)
        int rx; // index into render field. If there are tabs, then E.rx is greater then E.cx by how many extra spaces those tabs take up when rendered
        int rowoff; // vertical scrolling, scroll through the whole file
        int coloff; // horizontal scrolling
        int screenrows;
        int screencols;
        int numrows;
        erow *row; // array of errow struct, to store multiple line
        int dirty; // keep track of whether the text loaded to editor differs from what's in the file. Warn the user they might lose unsaved changes when try to quit, (1) appear, (0) disappea
        char *filename; // for display filename in status bar, save a copy of filename here when a file is opened
        char statusmsg[80]; // display message to the use
        time_t statusmsg_time; // timestamp for the message display to user, so that can erase it after the message it's been displayed
        struct editorSyntax *syntax;
        struct termios orig_termios;
};

struct editorConfig E;


/***** Append Buffer *****/
struct abuf{
        char *b;
        int len;
};

#define ABUF_INIT {NULL, 0} // represent an empty buffer b set NUll, len set to 0

// Terminal
void enableRawMode();
void disableRawMode();
void die(const char *s);
int editorReadKey();
int getWindowSize(int *rows, int *cols);
int getCursorPosition(int *rows, int *cols);
// Input
void editorProcessKeypress();
void editorMoveCursor(int key);
char *editorPrompt(char *prompt, void(*callback)(char *, int));
// Output 
void editorRefreshScreen();
void editorDrawRows(struct abuf *ab);
void editorScroll();
void editorDrawStatusBar(struct abuf *ab);
void editorSetStatusMessage(const char *fmt, ...);
void editorDrawMessageBar(struct abuf *ab);
// Init
void initEditor();
// Append buffer
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);
// File I/O
void editorOpen(char *filename);
char *editorRowsToString(int *buflen);
void editorSave();
//Find
void editorFind();
void editorFindCallback(char *query, int key);
// Row Operation
void editorInsertRow(int at, char *s, size_t len);
void editorUpdateRow(erow *row);
int editorRowCxToRx(erow *row, int cx);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowDelChar(erow *row, int at);
void editorFreeRow(erow * row);
void editorDelRow(int at);
void editorRowAppendString(erow *row, char *s, size_t len);
int editorRowRxToCx(erow *row, int rx);
// Editor Operations
void editorInsertChar(int c);
void editorDelChar();
void editorInsertNewLine();
// Syntax highlighting
void editorUpdateSyntax(erow *row);
int editorSyntaxToColor(int hl);
int is_separator(int c);
void editorSelectSyntaxHighlight();



int main(int argc, char *argv[]){
        enableRawMode();
        initEditor();
        
        if(argc >= 2){
                editorOpen(argv[1]);
        }

        editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

        while(1){
                // char c = '\0';
                // if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");// read from standard input & store it in c until eof && error is not due to unavaible resource
                // if(iscntrl(c)){ // if it's contorl char(non-printable), printf to screen. ASCII 0-31, 127 are control char
                //         printf("%d\r\n", c); // add carriage return so the cursor move back to the begining of the current line
                // }
                // else{
                //         printf("%d ('%c')\r\n", c, c);
                // }
                
                // if(c == CTRL_KEY('q')) break;        
                editorRefreshScreen(); // ouput
                editorProcessKeypress(); // input


        }

        return 0;
}
/***** terminal *****/
// turn off echoing 
void enableRawMode(){
        if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr"); // get attributes of the terminal associated with the fd & store it in ori_termios
        atexit(disableRawMode); // register a function to be called automatically when the program exit. Restore the terminal to its origianl state
        struct termios raw = E.orig_termios; // assgined to raw to make a copy before making changes
        
        // These flags prolly turned off, this is to make sure it's turned off
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        
        // raw.c_iflag = ~(ICRNL | IXON); // (1) disable CR(carriage return) and NL(new line). (2)disable ctrl-s(stop data from transmitted until press on ctrl-q
        // (1) clear echo, bitwise AND. (2) process input char by char. (3) disable ctrl-v(wait to type a char then send that cha
        // (4) turn of ctrl-c and ctrl-z by ISIG
        raw.c_oflag &= ~(OPOST); // turn off all output processing
        raw.c_cflag |= (CS8); // set the char size CS to 8 bits per byte
        raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

        // set timeout for read. Returns if it doesn't get any input 
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1; // 100 milliseconds

        /* TCSAFLUSH spefifies when to apply changes(flush - clear & apply new setting right away).
                - It waits for all pending output to be written to the terminal, 
                and also discards any input that hasn’t been read.
                - &raw where the new setting being set
        */
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); 
}

// disable raw mode at exit
void disableRawMode(){
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
                die("tcsetattr");
        }
}

void die(const char *s){
        // clear the screen on exit, errors will not be printed
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        perror(s);
        exit(1);
}

int getWindowSize(int *rows, int *cols){
        struct winsize ws;
        
        // if success icotl place cols & rows to ws struct
        if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
                
                /* ioctl is not guarantee on all systems. This stategy is to position the 
                cursor at the bottom right of the screen, then use escape sequences query the position of the
                cursor. This tells how many rows & cols there must be on the screen 
                
                C command - cursor forward, moves the cursor to the right
                B - cursor down, moves the cursor down
                999 is an argu to ensure the cursor reaches the right & bottom edges of the screen
                */        

                if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
                return getCursorPosition(rows, cols); // the hard way
        }
        else{
                // return mutiple values in C, update row & col
                *cols = ws.ws_col;
                *rows = ws.ws_row;
                return 0;
        }
}




int getCursorPosition(int *rows, int *cols) {
        char buf[32];
        unsigned int i = 0;
        /* n command - Device status report, query the terminal for status information
        6 - to ask for the cursor position
        */
        if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
       
        // keep reading until get to to R 
        while (i < sizeof(buf) - 1) {
                if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
                if (buf[i] == 'R') break;
                i++;
        }
        buf[i] = '\0';
        // printf("\r\n&buf[1]: '%s'\r\n", &buf[1]); // skip the 1st char in buf
        // From here it shoulb be in the form: <esc>[24;80
        if (buf[0] != '\x1b' || buf[1] != '[') return -1;
        if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1; // parsing to int to rows & cols, buf[2] is where to get the data

        return 0;
}




// wait for 1 keypress and returns it. Also handle escape sequence
int editorReadKey(){
        int nread;
        char c;
        while((nread = read(STDIN_FILENO, &c, 1)) != 1){ // read the 1st char 
                if(nread == -1 && errno != EAGAIN) die("read");
        }

        /* In the begining when press on an arrow key it sends bytes as input to the program(turned it off)
        These bytes are in the form: '\x1n', '[', followd by an 'A', 'B', 'C', or 'D' depends on which 4 arrow keys pressed
        Now lets replace w a s d with arrow key */
        if(c == '\x1b'){ // make sure the 1st char in the escape sequence
                char seq[3];

                // if read an escape char, immediately read 2 more bytes into seq, return escape key if one of these read times out
                if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
                if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

                if(seq[0] == '['){

                        // PAGE_UP & PAGE_DONW keys, in the form <esc>[5~
                        if(seq[1] >= '0' && seq[1] <= '9'){
                                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b'; // read the tilde into seq[2]
                                if(seq[2] == '~'){ // if it is a tilde
                                        switch(seq[1]){ // test the digit 
                                                case '1': return HOME_KEY;
                                                case '3': return DEL_KEY; // not doing anything
                                                case '4': return END_KEY;
                                                case '5': return PAGE_UP;
                                                case '6': return PAGE_DOWN;
                                                case '7': return HOME_KEY;
                                                case '8': return END_KEY;
                                        }
                                }
                        }
                        else{
                                switch(seq[1]){
                                        case 'A': return ARROW_UP; // up arrow
                                        case 'B': return ARROW_DOWN; // down arrow
                                        case 'C': return ARROW_RIGHT; // right arrow
                                        case 'D': return ARROW_LEFT; // left arrow
                                        case 'H': return HOME_KEY;
                                        case 'F': return END_KEY;
                                }
                        }
                }
                
                else if(seq[0] == 'O'){ // case for HOME_KEY & END_KEY
                        switch(seq[1]){
                                case 'H': return HOME_KEY;
                                case 'F': return END_KEY;
                        }

                }

                return '\x1b'; // if not match return this 
        }
        
        else{
                return c; // return char regularly
        }
}

/* INPUT
map various Ctrl key combinations and other special keys to 
different editor functions, and insert any alphanumeric and 
other printable keys’ characters into the text that is being edited.
*/
void editorProcessKeypress(){
        static int quit_times = ONREE_QUIT_TIMES; // keep track of # of times the user must press ctrl-Q to quit

        int c = editorReadKey();
        switch(c){
                case '\r':
                        editorInsertNewLine();
                        break;

                case CTRL_KEY('q'):
                        if(E.dirty && quit_times > 0){ // each time 
                                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                                "Press Ctrl-Q %d more times to quit", quit_times);
                                quit_times--;
                                return; // exit when quit times == 0
                        }

                        // clear the screen on exit, errors will not be printed
                        write(STDOUT_FILENO, "\x1b[2J", 4);
                        write(STDOUT_FILENO, "\x1b[H", 3);        
                        exit(0);
                        break;

                case CTRL_KEY('s'): // mapped editorSave() to ctrl-s
                        editorSave();
                        break;
                        
                case HOME_KEY:
                        E.cx = 0; // move to left of screen
                        break;
                case END_KEY: 
                        if(E.cy < E.numrows){
                                E.cx = E.row[E.cy].size; // move the cursor to the end of the current line
                        }
                        break;

                case CTRL_KEY('f'):
                        editorFind();
                        break;

                case BACKSPACE:
                case CTRL_KEY('h'): // sends the control code 8, it's orginally what the backspace char would send back in the day
                case DEL_KEY:
                        if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
                        editorDelChar();
                        break;
                
                case PAGE_UP:
                case PAGE_DOWN: 
                        { // scope to declare variables inside switch
                                if(c == PAGE_UP){ 
                                        E.cy = E.rowoff;
                                }
                                else if(c == PAGE_DOWN){
                                        E.cy = E.rowoff + E.screenrows - 1;
                                        if(E.cy > E.numrows) E.cy = E.numrows;
                                }

                                int times = E.screenrows;
                                while(times--){ // move up or down depends on which key pressed, ex: move up 20 times
                                        editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                                }
                        }
                        break;
                // when any of the arrow key pressed, the code will fall through editorMoveCursor()
                case ARROW_UP:
                case ARROW_DOWN:
                case ARROW_LEFT:
                case ARROW_RIGHT:
                        editorMoveCursor(c);
                        break;

                case CTRL_KEY('l'): // use to refresh the screen after any keypress
                case '\x1b': // ignore the escape key bc there are many esapce sequeces that arn't handling
                        break;
                
                // This will allow any keypresses that is not mapped to another editor function to be inserted directly into the text being edited
                default: 
                        editorInsertChar(c);
                        break;
        }

        quit_times = ONREE_QUIT_TIMES; // if the user press any key other than ctrl_Q, quit_times will reset back to 3
}

void editorMoveCursor(int key){
        /* Check if the cursor is on an actual line, meaning if there's text on that line */
        erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy]; 

        switch(key){
                case ARROW_LEFT: 
                        /* all the if are to prevent the cursor go past the right & bottom of the screen. 
                        Prevent E.cx & E.cy values go to the negatives */
                        if(E.cx != 0){
                                E.cx--;
                        }
                        else if(E.cy > 0){ // allow the user to press <- at the begining of the line to move to the end of the previous line
                                E.cy--;
                                E.cx = E.row[E.cy].size;
                        }
                        break;
                case ARROW_RIGHT: // also for horizontal scroll
                        /* check if the cursor at a col is to the left of the line before move cursor to the right
                        Make sure the cursor is not beyond the end of the row */
                        if(row && E.cx < row->size){ 
                                E.cx++;
                        }
                        // allow the user to press -> at the end of a line to go to the beginning of the next line.
                        else if(row && E.cx == row->size){ // end of the file is 1 char after the last cha
                                E.cy++; // new row
                                E.cx = 0; // col
                        }

                        break;
                case ARROW_UP:
                        if(E.cy != 0){
                                E.cy--;
                        }
                        break;
                case ARROW_DOWN: // also for vertcal scroll
                        if(E.cy < E.numrows){
                                E.cy++; // down is plus
                        }
                        break;
        }

        // Snap the cursor to end of line(when arrow down, the cursor will be at the end of the next line)
        row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy]; // if the cursor is past the total # row in a file
        int rowlen = row ? row->size : 0; // if there's a valid row, get the size of that row
        if(E.cx > rowlen){ // if the col is > rowlen
                E.cx = rowlen;
        }

}

/*function that displays a prompt in the status bar, and lets the user input a line of text after the prompt. Also, this function will support (incremental search), meaning the file is searched after each keypress when the user is typing in their search query. This function will take a callback function as an argument. Call this this callback function after each keypress, passing the current search query inputted by the user and the last key they presses */
char *editorPrompt(char *prompt, void(*callback)(char *, int)){
        size_t bufsize = 128;
        char *buf = malloc(bufsize); // buf to store user input

        size_t buflen = 0;
        buf[0] = '\0';

        while(1){
                editorSetStatusMessage(prompt, buf); // set the status bar, prompt expects a string %s, which is where the user's input will be displayed
                editorRefreshScreen(); // refresh the screen

                int c = editorReadKey();

                if( c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
                        if(buflen != 0) buf[--buflen] = '\0'; // delete a char if the condition is true
                }
                else if(c == '\x1b'){
                        editorSetStatusMessage("");
                        if(callback) callback(buf, c);
                        free(buf);
                        return NULL;
                }
                else if(c == '\r'){ // when the user press ENTER & their input is not empty. The status bar message is cleared and the input is returned
                        if(buflen != 0){
                                editorSetStatusMessage("");
                                if(callback) callback(buf, c);
                                return buf;
                        }
                }
                else if(!iscntrl(c) && c < 128){ // if the input is a printable chars & isn't a special keys in EditorKey enum, append it to  buf
                        if(buflen == bufsize - 1){ // reallocate size if needed before append to buf
                                bufsize *= 2;
                                buf = realloc(buf, bufsize);
                        }
                        buf[buflen++] = c; // append to buf when it is a printable char
                        buf[buflen] = '\0';
                }
                
                if(callback) callback(buf, c);
        }
}



/***** Output *****/
void editorRefreshScreen(){
        editorScroll();

        struct abuf ab = ABUF_INIT;
        /* write 4 bytes out the terminal. 1st byte is \x1b(escape char-27), 3 bytes is [2J
                J - erase in display. 2 is the argu. means to clear the entire screen
        */
        abAppend(&ab, "\x1b[?25l", 6); // set mode - hide cursor before refreshing the screen
        // abAppend(&ab, "\x1b[2J", 4); // instead of clear the entire screen, let's clear each line as draw. See editorDrawrows()
        abAppend(&ab, "\x1b[H", 3); // 3 bytes long. H command to position the cursor, default para is 1 (<esc>[1;1H), 1 row 1 col

        editorDrawRows(&ab);
        editorDrawStatusBar(&ab);
        editorDrawMessageBar(&ab);
        
        // This is to move the cursor to the position store in E.cx & E.cy
        char buf[32];
        /* [ - to start the escape sequence. H - cmd to move the cursor to specific position
        Format a str & store into buf, also convert 0-indexed to 1 that the terminal uses 
        substract E.coloff to fix the cursor position, before isn't position properly(it does not want to go back when pressed)*/
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1); 
        abAppend(&ab, buf, strlen(buf));

        abAppend(&ab, "\x1b[?25h", 6); // reset mode - show the cursor again after the refresh finishes 

        write(STDOUT_FILENO, ab.b, ab.len); // write buffer content all at once out to standard output
        abFree(&ab);
}



// print the correct number of tildes for the height of the terminal
void editorDrawRows(struct abuf *ab){
        int y;
        // screenrow is set by initEditor() when getWindowSize() is called
        for (y = 0; y < E.screenrows; y++){
                int filerow = y + E.rowoff; // to get the # row of the file at each y position, also use this as an index into E.row
                if(filerow >= E.numrows){ // beyond the text that needs to be displayed
                        /* Only display welcome message when the program start with no argus.
                        not when a user open a file*/
                        if(E.numrows == 0 && y == E.screenrows / 3){ // check if the current row is 1/3 of the way down the screen
                                char welcome[80];
                                int welcomelen = snprintf(welcome, sizeof(welcome), "ONREE Editor --- Version %s", ONREE_VERSION);
                                if (welcomelen > E.screencols) welcomelen = E.screencols;
                                
                                /*Center the welcome message. Divide the screen by 2 then subtract half of the string's length from that
                                Basically how far from the left edge of the screen it should be printing*/
                                int padding = (E.screencols - welcomelen) / 2;
                                if(padding){ // if there is any padding 
                                        abAppend(ab, "~", 1); // append tilde first 
                                        padding--;
                                }
                                // use the padding-- above for this one. Continue to execute as long as padding is != 0. Decrement by 1 each time. postfux dec. current value of padding is used first
                                while(padding--) abAppend(ab, " ", 1);
                                
                                abAppend(ab, welcome, welcomelen);
                        }
                        else{
                                abAppend(ab, "~", 1); // else append tildes to the 1st col of each row
                        }
               
                }
                else{ // this is for displaying a row of text 
                        int len = E.row[filerow].rsize - E.coloff; // get the length of the current row
                        if(len < 0) len = 0; // if the user scroll hori. past the end of the file, set len to 0 so nothing is displayed
                        if(len > E.screencols) len = E.screencols; // if the text is longer than the screen width, truncate it
                       
                        unsigned char *hl = &E.row[filerow].hl[E.coloff];
                        int current_color = -1;

                        char *c = &E.row[filerow].render[E.coloff];
                        int j;
                        for(j = 0; j < len; j++){

                                if(iscntrl(c[j])){
                                        char sym = (c[j] <= 26) ? '@' + c[j] : '?'; // translate to printable char by adding @, letters of the alphabet comes after the @ char
                                        abAppend(ab, "\x1b[7m", 4); // switch to inverted color before printing the translated symbol
                                        abAppend(ab, &sym, 1);
                                        abAppend(ab, "\x1b[m", 3); // turn off inverted colors. This will turn off all text formatting including colors
                                        if(current_color != -1){ // print the escape sequence for the current color
                                                char buf[16];
                                                int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                                                abAppend(ab, buf, clen);
                                        }
                                }
                                else if(hl[j] == HL_NORMAL){
                                        if(current_color != -1){
                                                abAppend(ab, "\x1b[39m", 5); // set color back to normal before printing
                                                current_color = -1; // -1 default text color
                                        }
                                       
                                        abAppend(ab, &c[j], 1); // append the current digit to the buffer
                                }
                                else{
                                        int color = editorSyntaxToColor(hl[j]);
                                        if(color != current_color){ 
                                                // when the color changes, print out the escape sequence for that color adn set to new color
                                                current_color = color;
                                                char buf[16];
                                                int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                                                abAppend(ab, buf, clen);
                                        }
                                        
                                        abAppend(ab, &c[j], 1);
                                }
                        }
                        abAppend(ab, "\x1b[39m", 5); // after done looping all the chars, reset the text color to default
                }
                
                /* k - erase in line, erases part of the current line to the right of the cursor. 0 is default param. so it's just <esc>[K */ 
                abAppend(ab, "\x1b[K", 3); 
                abAppend(ab, "\r\n", 2); // print a new line after the last row it draws

        }
}


void editorScroll(){
        /* Set the value of E.rx same as E.cx. Also replace all instances of E.cx with E.rx because scrolling 
        should take into account the characters that are actually rendered to the screen, and the rendered position of the cursor.*/
        E.rx = E.cx; 

        if(E.cy < E.numrows){
                E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
        }

        // rowoff is always at the top, == 0
        if(E.cy < E.rowoff){ // if the cursor is above the visible window, scroll up to where the cursor is
                E.rowoff = E.cy;
        }
        if(E.cy >= E.rowoff + E.screenrows){ // if cursor is past the bottom of the visible window. Use screerow to know what's at the bottom of the screen
                E.rowoff = E.cy - E.screenrows + 1; // visible from rowoff to E.cy, i.e: 2 to 11
        }
        if(E.rx < E.coloff){
                E.coloff = E.rx;
        }
        if(E.rx >= E.coloff + E.screencols){
                E.coloff = E.rx - E.screencols + 1;
        }
}


void editorDrawStatusBar(struct abuf *ab){
        /* m command is to select graphic rendition, 1(bold), 4(underscore)
        5(blink), 7 (for inverted color) */
        abAppend(ab, "\x1b[7m", 4);
        
        char status[80], rstatus[80];
        // write eveything to status buffer. 
        int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", 
                E.filename ? E.filename : "[No Name]", E.numrows,
                E.dirty ? "(modified)" : "");
        /* add 1 to since E.cy is 0-indexed. After printing the first status string, 
        keep printing spaces until get to the point where if we printed the second status string, it would end up against the right edge of the screen. */
        int rlen = snprintf(rstatus, sizeof(rstatus), "File Type: %s | %d/%d",
        E.syntax ? E.syntax->filetype : "no filetype", E.cy + 1, E.numrows);

        if(len > E.screencols) len = E.screencols;
        abAppend(ab, status, len);

        while (len < E.screencols) {
                if(E.screencols - len == rlen){
                        abAppend(ab, rstatus, rlen); // print status str & break out of loop
                        break;
                }
                else{
                        abAppend(ab, " ", 1); // else keep adding space until == rlen
                        len++;
                }
        }
        abAppend(ab, "\x1b[m", 3); // <esc>[m to go back to normal text formatting.
        abAppend(ab, "\r\n" , 2); // a new line after the staus bar line
}


void editorSetStatusMessage(const char *fmt, ...){
        va_list ap; // to handle argus list
        va_start(ap, fmt); // initilizes ap variable and get additional agu after fmt
        vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap); // works like snprintf, ap contain additional argu
        va_end(ap);
        E.statusmsg_time = time(NULL); // returns the number of seconds that have passed since midnight, January 1, 1970 as an integer
}

void editorDrawMessageBar(struct abuf *ab) {
        abAppend(ab, "\x1b[K", 3); // first clear any message before displaying new one
        int msglen = strlen(E.statusmsg);
        if (msglen > E.screencols) msglen = E.screencols; // make sure the message will fit the screen
        // message will disappear when press a key after 5 seconds
        if (msglen && time(NULL) - E.statusmsg_time < 5) // time(null) will always changes as time progress, E.staustime_mgs will remains constant once it's set
                abAppend(ab, E.statusmsg, msglen); // display the message iff' the message is < 5 seconds old
}



// Init
void initEditor(){
        E.cx = 0, E.cy = 0; // cursor start from the top left of the screen
        E.rx = 0;
        E.rowoff = 0; // default scroll to the top of the file by default
        E.coloff = 0; 
        E.numrows = 0;
        E.row = NULL; // initialized ptr to NULL
        E.dirty = 0;
        E.filename = NULL; // stay NULL if a file isn't opened (which what happend when this program run w/o argus.)
        E.statusmsg[0] = '\0'; // no message will be displayed by default
        E.statusmsg_time = 0; // timestamp when set the message
        E.syntax = NULL; // NULL means there's no filetype for the current file and no highlight should be done
        
        // update screenrows & screencols
        if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
        E.screenrows -= 2; // so that editorDrawRows() doesn’t try to draw a line of text at the bottom of the screen.
}



/***** Append Buffer *****/
/* This is to do one big write(), to make sure the whole screen update at once
        - replace all write() with code that appends str to a buf & then write this buf out at the end
*/
void abAppend(struct abuf *ab, const char *s, int len) {
        // ab->b is the str to be reallocated(new size). ask realloc to give a block of memory of the current str + the size of str appending
        char *new = realloc(ab->b, ab->len + len); 
        
        if (new == NULL) return;
        memcpy(&new[ab->len], s, len); // copy content of s to new
        // update the struct
        ab->b = new;
        ab->len += len;
}
void abFree(struct abuf *ab) {
        free(ab->b); // free mem for allocated st
}


/***** File I/O *****/
void editorOpen(char *filename){
        free(E.filename);
        E.filename = strdup(filename);

        editorSelectSyntaxHighlight();

        FILE *fp = fopen(filename, "r");
        if(!fp) die("fopen");

        char *line = NULL; // string
        size_t linecap = 0; // line capacity
        ssize_t linelen; // # char returns from getline()
        
        while ((linelen = getline(&line, &linecap, fp)) != -1){ // read an entire file into E.row
                while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == 'r')){
                        linelen--; // remove \n and \r if at the end of string
                }

                editorInsertRow(E.numrows, line, linelen);
        }

        free(line);
        fclose(fp);
        E.dirty = 0; 
}

// Function that converts array of erow structs into a single str that is ready to be written out to a file
char *editorRowsToString(int *buflen){
        int totlen = 0;
        int j;
        for(j = 0; j < E.numrows; j ++){ // loop through every rows and get the size of each row, +1 for newline cha
                totlen += E.row[j].size + 1;
        }
        *buflen = totlen; // will update len from editorSave()

        char *buf = malloc(totlen);
        char *p = buf;
        for(j = 0; j < E.numrows; j++){
                memcpy(p, E.row[j].chars, E.row[j].size); // copy str to buf
                p += E.row[j].size; // advance the ptr, p still point to the start of the buffer, buf + 5
                *p = '\n'; // add a new line
                p++; // buf + 6
        }
        return buf;
}

// function to write the string returned by editorRowsToString() to disk
void editorSave(){
        if(E.filename == NULL){
                E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
                if(E.filename == NULL){
                        editorSetStatusMessage("Save Aborted");
                        return;
                }
                editorSelectSyntaxHighlight();
        }

        int len;
        char *buf = editorRowsToString(&len); // returns new len after the function call, len is pointing to totlen

        int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
        if(fd != -1){ // return -1 on erroe
                /* set the file size to specified length  If the file is larger than that, 
                 it will cut off any data at the end of the file to make it that length. 
                 If the file is shorter, it will add 0 bytes at the end to make it that length.*/
                if(ftruncate(fd, len) != -1){ // return -1 on erro
                        if (write(fd, buf, len) == len){ // return # of bytes told to write
                                // if error occurs or not
                                close(fd);
                                free(buf);
                                E.dirty = 0;
                                editorSetStatusMessage("%d bytes written to disk", len);
                                return;
                        }
                }
                close(fd);
        }

        free(buf);
        editorSetStatusMessage("Cannot save! I/O error: %s", strerror(errno)); // returns human-readable string for that error code
}

// Find
/* Restore cursor position when cancelling search: When the user presses Escape to cancel a search, the cursor need to go back to where it was when they started the search. To do that, save their cursor position and scroll position, and restore those values after the search is cancelled. */
void editorFind(){
        int save_cx = E.cx;
        int saved_cy = E.cy;
        int saved_coloff = E.coloff;
        int saved_rowoff = E.rowoff;
        
        char *query = editorPrompt("Search: %s (ESC / Arrows / Enter)", editorFindCallback);
        
        if(query){ // user complete the search
                free(query);
        }
        else{ // user pressed Escape, restore the values
                E.cx = save_cx;
                E.cy = saved_cy;
                E.coloff = saved_coloff;
                E.rowoff = saved_rowoff;
        }
}

/* search feature: When the user types a search query and presses Enter. Loop through all the rows of the file, and if a row contains their query string, move the cursor to the match
Search forward and backward: The ↑ and ← keys will go to the previous match, and the ↓ and → keys will go to the next match. */
void editorFindCallback(char *query, int key){
        static int last_match = -1; // contain the index of the row the last match was on, -1 if there was no last match
        static int direction = 1; // store the direction of the match: 1 searching forward, -1 searching backward

        static int saved_hl_line; // to know which line need to be restored
        static char *saved_hl = NULL; // points to NULL if there's nothing to restored

        if(saved_hl){
                memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
                free(saved_hl);
                saved_hl = NULL; // set back to NULL after restore
        }

        if (key == '\r' || key == '\x1b') { // leaving search mode so return immediately instead of doing another search
                last_match = -1;
                direction = 1;
                return;
        }
        else if(key == ARROW_RIGHT || key == ARROW_DOWN){
                direction = 1;
        }
        else if(key == ARROW_LEFT || key == ARROW_UP){
                direction = -1;
        }
        else{ // reset values back to the original
                last_match = -1; 
                direction = 1;
        }

        if(last_match == -1) direction = 1;
        int current = last_match; // store the of the current row searching

        // else after any other keypress, do another seach for the current query string
        int i;
        for(i = 0; i < E.numrows; i++){
                current += direction;
                if(current == -1) current = E.numrows - 1; // set to the last row
                else if(current == E.numrows) current = 0; // set to the first row

                erow *row = &E.row[current];
                char *match = strstr(row->render, query); // query is a substr of row->render, return a ptr point to the 1st char in substr matched
                if(match){
                        last_match = current; // if it's match, the user presses the arrow keys, it'll start the next search from that point, also update last_match
                        E.cy = current;
                        E.cx = editorRowRxToCx(row, match - row->render); // covert to an index
                        E.rowoff = E.numrows; /* set row offset to scroll to the bottom of the file. Which will cause editorScroll() to scroll upwards at the next screen refresh so that the matching line will be at the very top of the screen */

                        saved_hl_line = current;
                        saved_hl = malloc(row->rsize);
                        memcpy(saved_hl, row->hl, row->rsize);
                        memset(&row->hl[match - row->render], HL_MATCH, strlen(query)); // match - row->render is the index into render of the match, so use that as the index into hl
                        break;
                }
        }
}


/***** Row Operation *****/
/* This function allocate space for a new erow, and then copy the given str to a new erow at the end of the E.row array 
It will now be able to insert a row at the index specified by the new at argument. */
void editorInsertRow(int at, char *s, size_t len){
        if(at < 0 || at > E.numrows) return;

        E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // reallocate for one more erow
        memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at)); // make room at the specified index for the new row, shift current row down

        for(int j = at + 1; j <= E.numrows; j++) E.row[j].idx++; // update the index of each row that was displaced by the insert or delete operation

        E.row[at].idx = at; // initialize idx to the row’s index in the file at the time it is inserted

        E.row[at].size = len; // update the size of the current row
        E.row[at].chars = malloc(len + 1); // allocate memory 
        memcpy(E.row[at].chars, s, len); // copy the str to newly allocated memory
        E.row[at].chars[len] = '\0'; // make the end of a st
        
        E.row[at].rsize = 0;
        E.row[at].render = NULL;
        E.row[at].hl = NULL;
        E.row[at].hl_open_comment = 0;
        editorUpdateRow(&E.row[at]);

        E.numrows++; // update the newly row, reprent 1 row with text
        E.dirty++; // incremnet bc make changes to text
}

// this function uses the chars str of an erow to fill in the contents of the render str
void editorUpdateRow(erow *row) {
        int tabs = 0;
        int j;
        for(j = 0; j < row->size; j++){
                if(row->chars[j] == '\t') tabs++; // go through chars of the row & count the tabs in order to know how much memory to allocate for rende
        }

        free(row->render);
        row->render = malloc(row->size + tabs*(ONREE_TAB_STOP - 1) + 1); // allocate mem with tabs
        
        int idx = 0;
        for (j = 0; j < row->size; j++) {
                if(row->chars[j] == '\t'){ // if the current char is a tab, append one space bc each tab must advance the cursor forward at least 1 col
                        row->render[idx++] = ' ';
                        while(idx % ONREE_TAB_STOP != 0) row->render[idx++] = ' ';// then apppend spaces til get to the tab stop, which is a col that divisible by 8
                }
                else{
                        row->render[idx++] = row->chars[j]; // copy each char to rende
                }
        }
        row->render[idx] = '\0';
        row->rsize = idx; // update the size of row

        editorUpdateSyntax(row); // editorUpdateRow updating the render array whenever the text of row changes, so this is where hl array need to be update
}

// function that converts a chars index into a render index
int editorRowCxToRx(erow *row, int cx){
        int rx = 0;
        int j;
        for (j = 0; j < cx; j++) { // loop through all the chars to the left fo cx
                if (row->chars[j] == '\t') {// if it's a tab
                /* use rx % KILO_TAB_STOP to find out how many columns we are to the right of the last tab stop, 
                then subtract that from KILO_TAB_STOP - 1 to find out how many columns we are to the left of the next tab stop
                Add that amount to rx to get just to the left of the next tab stop,*/
                        rx += (ONREE_TAB_STOP - 1) - (rx % ONREE_TAB_STOP);
                }
                rx++; // to get right on the next tab stop
        }
        return rx;
}

// function that inserts a single character into an erow, at a given position.
void editorRowInsertChar(erow *row, int at, int c){
        if (at < 0 || at > row->size) at = row->size; // validate the index want to insert the char into, at can go 1 char past the end of st
        row->chars = realloc(row->chars, row->size + 2); // allocate 1 more byte fo chars of the erow (2 bc for the null)
        // increment the size of the chars array, then assign the character to its position in the array
        memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
        row->size++;
        row->chars[at] = c; // place a char at a certain position
        editorUpdateRow(row); // so that render & rsize fields get updated with new row content
        E.dirty++; // incremnet bc make changes to text
}

// Simple backspacing: function which deletes a char in an erow
void editorRowDelChar(erow *row, int at){
        if(at < 0 || at >= row->size) return;
        memmove(&row->chars[at], &row->chars[at+1], row->size - at); // move the next char to the current cha
        row->size--;
        editorUpdateRow(row);
        E.dirty++;
}


/* The 2 functions below is implementing backspacing at the start of a line. When the user backspace at the begining of a line, append the contents
of that line to the previous line, and then delete the current line. This backspaces the implicit \n char in the between the 2 lines to join them into 1 */
void editorFreeRow(erow * row){
        free(row->render);
        free(row->chars);
        free(row->hl);
}

void editorDelRow(int at){
        if(at < 0 || at >= E.numrows) return;
        editorFreeRow(&E.row[at]); // free the memory owned by the row
        memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at -1)); // shift all rows after deleted row 1 position to the left
        E.numrows--;
        E.dirty++;
}

// function which append a str to the end of a row
void editorRowAppendString(erow *row, char *s, size_t len){
        row->chars = realloc(row->chars, row->size + len + 1); // the row new size is including the null byte, +1
        memcpy(&row->chars[row->size], s, len); // copy the given str to the end of the contents of row->chars
        row->size += len; // update to the new length
        row->chars[row->size] = '\0'; // terminate the str with \0
        editorUpdateRow(row); // udpate the row's copy version & its rsize
        E.dirty++;
}


// function converting render index into chars index before assigning it to E.cx
int editorRowRxToCx(erow *row, int rx){
        int cur_rx = 0;
        int cx;
        for(cx = 0; cx < row->size; cx++){ // go through each char in a row
                if(row->chars[cx] == '\t'){ // if it's tab
                        cur_rx += (ONREE_TAB_STOP - 1) - (cur_rx % ONREE_TAB_STOP); // adjust cur_rx to account for the width of the tab. Calculate the spaces needed to reach the next tab
                }
                cur_rx++;

                if(cur_rx > rx) return cx; // return the current position in the char array of the row when hit the tab 
        }
        return cx;
}


/***** Editor Operations *****/
/* This func take a char and use editorRowInsertChar() to insert that character into the position that the cursor is at
 editorInsertChar() doesn’t have to worry about the details of modifying an erow, 
 and editorRowInsertChar() doesn’t have to worry about where the cursor is.
*/
void editorInsertChar(int c){
        if (E.cy == E.numrows) { // if the cursor is at the end of the current row, a new empty row will be appended
                editorInsertRow(E.numrows, "", 0);
        }
        editorRowInsertChar(&E.row[E.cy], E.cx, c); // else insert a char at a specify location
        E.cx++;
}


// also handle the case where the cursor is at the begining of a line
void editorDelChar(){
        if(E.cy == E.numrows) return; // if the cursor past the ned of the file, there's nothing to delelte. Return immediately
        if(E.cx == 0 && E.cy == 0) return; // if the cursor is at the begining of the first line, there's nothing to do, return immediately

        erow *row = &E.row[E.cy]; // else get the row the cursor is currently on
        if(E.cx > 0){ // if there's no char to the left, the cursor at begining of the
                editorRowDelChar(row, E.cx - 1); // delete it and move cursor  1 to the left
                E.cx--;
        }
        else{ // else E.cx == 0
                E.cx = E.row[E.cy - 1].size; // set cursor hori. position to the end of the previous line
                editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
                editorDelRow(E.cy);
                E.cy--;
        }
}


void editorInsertNewLine(){
        if(E.cx == 0){ // if at begining of the line, insert a new blank row before the line currently on
                editorInsertRow(E.cy, "", 0); 
        }
        else{ // else split the lien currenlty on into rows
                erow *row = &E.row[E.cy];
                // pass the chars on the current row that are to the right of the cursor. It will create a new row after the current one containing the chars to the right of the curso
                editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx); 
                row = &E.row[E.cy]; // update the current row to contain only the chars to the left of the curso
                row->size = E.cx; // set the size of the current row to the curso
                row->chars[row->size] = '\0';
                editorUpdateRow(row); // update the copy of the current row
        }
        E.cy++; // move to the next row
        E.cx = 0;
}

/*** syntax highlighting ***/
void editorUpdateSyntax(erow *row){
        row->hl = realloc(row->hl, row->size); // this might be the new row or the row might be bigger then the last time highlighted. hl array is the same as render array, so use rsize as the amoutn of memory to allocate for hl
        memset(row->hl, HL_NORMAL, row->size);

        if(E.syntax == NULL) return; // rturn immediately after memset()ting the entire line to HL_NORMAL.

        char **keywords = E.syntax->keywords;

        char *scs = E.syntax->singleline_comment_start;
        char *mcs = E.syntax->multiline_comment_start;
        char *mce = E.syntax->multiline_comment_end;

        int scs_len = scs ? strlen(scs) : 0;
        int mcs_len = mcs ? strlen(mcs) : 0;
        int mce_len = mce ? strlen(mce) : 0;


        int prev_sep = 1; // keep track of whether the previous char was a separator, 1 is true consider the begining of the line to be a separtor
        int in_string = 0; // keep track of whether currently inside a string. If inside, keep highlighting the current character as a string until hit the closing quote
        int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment); // initialize in_comment to true if the previous row has an unclosed multi-line comment. If that’s the case, then the current row will start out being highlighted as a multi-line comment.

        int i = 0;
        while(i < row->size){ // go through each char in a line
                char c = row->render[i]; // get the current char from a row
                unsigned char prev_hl = (i > 0)? row->hl[i-1] : HL_NORMAL;

                // single-line comments should not be recognized inside multi-line comments
                if(scs_len && !in_string && !in_comment){ // check if not in a string
                        // Compare the beginning of the current line (row->render) with the single-line comment start delimiter (scs)
                        if(!strncmp(&row->render[i], scs, scs_len)){
                                // &row->hl[i] - Start highlighting from the current character. row->size - 1 till the last char
                                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                                break;
                        }
                }

                if(mcs_len && mce_len && !in_string){ // validate the start & end of comment and if it's not in a string
                        if(in_comment){ // if inside multi-line comment
                                row->hl[i] = HL_MLCOMMENT; // highlight the current char
                                if(!strncmp(&row->render[i], mce, mce_len)){ // check if at the end of a multi-line
                                        memset(&row->hl[i], HL_MLCOMMENT, mce_len); // highlight the whole mce string
                                        i += mce_len; 
                                        in_comment = 0;
                                        prev_sep = 1;
                                        continue;
                                }
                                else{ // if not at the end of the comment, consume the current char whici already highlighted
                                        i++;
                                        continue;
                                }
                        }
                        else if(!strncmp(&row->render[i], mcs, mcs_len)){ // if not in a multi-line comment, check to see if at the begining of the comment
                                memset(&row->hl[i], HL_MLCOMMENT, mcs_len); // highlight the whole mcs string
                                i += mcs_len;
                                in_comment = 1; // set to true
                                continue;
                        }
                }


                if(E.syntax->flags & HL_HIGHLIGHT_STRINGS){
                        if(in_string){
                                row->hl[i] = HL_STRING;
                                if(c == '\\' && i + 1 < row->rsize){ // if current char is backflash and there's 1 more char after it, then highlight the char after the backflash and consume it
                                        row->hl[i+1] = HL_STRING;
                                        i += 2; // consume both char at once
                                        continue;
                                }
                                if(c == in_string) in_string = 0; // if match the opening quote(the end of string)
                                i++;
                                prev_sep = 1;
                                continue;
                        }
                        else{
                                if(c == '"' || c == '\''){ // if not in a string, check if at the beginning of one by checking for a double- or single-quote
                                        in_string = c; // store quote in string
                                        row->hl[i] = HL_STRING; // highligh it
                                        i++; // consume it
                                        continue;
                                }
                        }
                }


                if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){ // check if numbers should be highlighted for the current filetype
                        if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)){ // A . character that comes after a character that just highlighted as a number will now be considered part of the number
                                row->hl[i] = HL_NUMBER;
                                i++; // consume the char currently highlighted
                                prev_sep = 0; // indicate in the middle of highlighting something
                                continue;
                        }
                }

                if(prev_sep){
                        int j;
                        for(j = 0; keywords[j]; j++){
                                int klen = strlen(keywords[j]); // store the len of keyword
                                int kw2 = keywords[j][klen - 1] == '|'; // check to see if the last char of keyword is |, then decrement
                                if(kw2) klen--;

                                // check if keyword exist at the current position in the text & check if a separator char comes after the keyword
                                if(!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])){
                                        memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen); // highlight the whole keyworde depends on the value of kw2
                                        i += klen; // consume the entire word, then move on to the next index
                                        break; // break bc still inside the inner loop. Everytime break out of this, the if statement below will runs to continue the iteration
                                }
                        }
                        if(keywords[j] != NULL){
                                prev_sep = 0; // 0 means current char is part of the keyword(not a separator)
                                continue;
                        }
                }

                prev_sep = is_separator(c);
                i++;
        }

        int changed = (row->hl_open_comment != in_comment);
        row->hl_open_comment = in_comment; // set the value of the current row’s hl_open_comment to whatever state in_comment got left in after processing the entire row. This tells whether the row ended as an unclosed multi-line comment or not
        if(changed && row->idx + 1 < E.numrows){
                editorUpdateSyntax(&E.row[row->idx + 1]); // editorUpdateSyntax() keeps calling itself with the next line, the change will continue to propagate to more and more lines until one of them is unchanged
        }
}


int editorSyntaxToColor(int hl){
        switch(hl){
                case HL_COMMENT: // cyan
                case HL_MLCOMMENT: return 36; // cyan
                case HL_KEYWORD1: return 33; // yellow 
                case HL_KEYWORD2: return 32; // green
                case HL_STRING: return 35; // magenta
                case HL_NUMBER: return 31; // foreground red
                case HL_MATCH: return 34; // color blue, purple on vscode terminal
                default: return 37; // forground white
        }
}


// function that takes a char and return true if it's considered a separator char
int is_separator(int c){
        return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

/* function that tries to match the current filename to one of the filematch fields in the HLDB. If one matches, it’ll set E.syntax to that filetype. Call this function whenever E.filename changes. This is in editorOpen() and editorSave() */
void editorSelectSyntaxHighlight(){
        E.syntax = NULL;
        if(E.filename == NULL) return; // if there's no filename, there's no filetype

        char *ext = strrchr(E.filename, '.'); // locate the the last occurence of char

        for(unsigned int j = 0; j < HLDB_ENTRIES; j++){
                struct editorSyntax *s = &HLDB[j]; // s point to the current editorSyntax structure in the HLDB array.
                unsigned int i = 0;
                while(s->filematch[i]){
                        int is_ext = (s->filematch[i][0] == '.');
                        // if pattern starts with '.', then it's a file extension pattern, compare to see if filename ends with that extension. If it’s not a file extension pattern, then we just check to see if the pattern exists anywhere in the filename, using strstr()
                        if((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i]))){
                                E.syntax = s; // if match all the rules, set it to editorSyntax struct

                                // rehighlight the entire file after setting E.syntax in editorSelectSyntaxHighlight(). The highlighting immediately changes when the filetype changes.
                                int filerow;
                                for(filerow = 0; filerow < E.numrows; filerow++){
                                        editorUpdateSyntax(&E.row[filerow]);
                                }

                                return;
                        }
                        i++;
                }       
        }
}
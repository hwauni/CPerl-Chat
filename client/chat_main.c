#include "chat_main.h"

mlist *message_list;
ulist *user_list;
WINDOW *log_win, *show_win, *ulist_win, *chat_win;
int sock;
pthread_t rcv_pthread;
int usr_state;

int main(int argc, char *argv[])
{
    char str[MESSAGE_BUFFER_SIZE];
    char id[ID_SIZE];
    char srvname[ID_SIZE];
    char *first_scr = "Enter your id: ";
    char *srv_name_scr = "Server Name: ";
    msgst ms;

    // 처음 사용자의 상태를 로그아웃 상태로 셋팅
    usr_state = USER_LOGOUT_STATE;

    // Locale 환경 셋팅
    set_env();

    // Ncurses 환경 초기화
    initscr();
    cbreak();
    refresh();

    // 첫 실행 화면 출력
    mvwprintw(stdscr, LINES/2, (COLS - strlen(first_scr))/2, first_scr);
    mvwprintw(stdscr, LINES/2 + 2, (COLS - strlen(srv_name_scr))/2 - 1, srv_name_scr);

    //커서를 맨앞으로 이동
    wmove(stdscr, LINES/2, ((COLS - strlen(first_scr))/2) + strlen(first_scr) + 1);
    // 아이디 값을 받아서 저장
    getstr(id);
    memcpy(ms.id, id, strlen(id));
    //커서를 맨앞으로 이동
    wmove(stdscr, LINES/2 + 2, ((COLS - strlen(srv_name_scr))/2 - 1) + strlen(srv_name_scr) +1);
    getstr(srvname);
    ms.id[strlen(id)] = '\0';

    // 출력할 메시지 및 아이디 목록을 관리할 리스트 할당 및 초기화
    message_list = (mlist *)malloc(sizeof(mlist));
    init_mlist(message_list);
    user_list = (ulist *)malloc(sizeof(ulist));
    init_ulist(user_list);

    // 로그&Top10 출력창 생성
    log_win = create_window(LINES - 40, COLS - 17, 0, 16);
    // 메시지 출력창 생성
    show_win = create_window(LINES - 17, COLS - 17, 13, 16); 
    // 사용자 목록창 생성 
    ulist_win = create_window(LINES - 4, 15, 0, 0); 
    // 사용자 입력창 생성
    chat_win = create_window(3, COLS - 1, LINES - 3, 0);

    connect_server();
    ms.state = MSG_NEWUSER_STATE;
    strcpy(ms.message, str);
    write(sock, (char *)&ms, sizeof(msgst));


    while(1) {
        // 커서위치 초기화
        move(LINES - 2, 1);
        mvwgetstr(chat_win, 1, 1, str);
        // 입력이 완료되면 문자열을 화면에서 지우기 위해 사용자 입력창을 재 생성 한다.
        delwin(chat_win);
        chat_win = create_window(3, COLS - 1, LINES - 3, 0);

        // 아무값이 없는 입력은 무시
        if(!strlen(str)) 
            continue;

        if(str[0] == '/') {
            if(!strcmp("/connect", str)) {
                // 이미 사용자 로그인 상태이면 접속하지 않기 위한 처리를 함.
                if(usr_state == USER_LOGIN_STATE) {
                    insert_mlist(message_list, "already connected!");
                    update_show_win(message_list);
                    continue;
                }

                // 사용자 목록 창과 리스트에 남아있는 사용자 목록을 초기화 한다.
                clear_ulist(user_list);
                update_ulist_win(user_list);

                // 접속 시도
                if(connect_server() < 0) {
                    continue;
                }
                // TCP 접속이 완료되고 서버에게 새로운 사용자라는 것을 전달한다.
                // 이때 알리는 동시에 아이디 값을 같이 전달하게 되어 서버에서 사용자 목록에 추가되게 된다(아이디는 이미 위에서 저장됨).
                ms.state = MSG_NEWUSER_STATE;
                write(sock, (char *)&ms, sizeof(msgst));
            } else if(!strcmp("/disconnect", str)) {
                // 접속을 끊기 위해 메시지를 받는 쓰레드를 종료하고 읽기/쓰기 소켓을 닫는다.
                pthread_cancel(rcv_pthread);
                shutdown(sock, SHUT_RDWR);
                // 사용자 목록 초기화
                clear_ulist(user_list);
                update_ulist_win(user_list);
                insert_mlist(message_list, "disconnected!");
                update_show_win(message_list);
                usr_state = USER_LOGOUT_STATE;
                continue;
            } else if(!strcmp("/clear", str)) {
                // 메시지 출력창에 있는 메시지를 모두 지운다.
                clear_mlist(message_list);
                update_show_win(message_list);
                continue;
            } else if(!strcmp("/exit", str)) {
                break;
            } 
        } else {
            // 로그인 상태일때만 서버에 메시지를 전달하게 된다.
            if(usr_state == USER_LOGIN_STATE) {
                ms.state = MSG_DATA_STATE;
                strcpy(ms.message, str);
                write(sock, (char *)&ms, sizeof(msgst));
            }
        }
    }

    clear_mlist(message_list);
    free(message_list);
    clear_ulist(user_list);
    free(user_list);
    close(sock);
    endwin();

    return 0;
}

void print_error(char* err_msg)
{
    char buf[MESSAGE_BUFFER_SIZE];

    strcpy(buf, "ERROR: ");
    strcat(buf, err_msg);

    insert_mlist(message_list, buf);
    update_show_win(message_list);
}

int connect_server()
{
    struct sockaddr_in srv_addr;
    int thr_id;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(!sock) {
        print_error("socket error!\n");
        return -1;
    }
    memset(&srv_addr, 0x0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    srv_addr.sin_port = htons(atoi(SERVER_PORT));
    if(connect(sock, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        print_error("connect error!\n");
        close(sock);
        return -1;
    }

    // 메시지를 받는 역할을 하는 쓰레드 생성
    thr_id = pthread_create(&rcv_pthread, NULL, rcv_thread, (void *)&sock);
    if(thr_id < 0) {
        print_error("pthread_create error");
        close(sock);
        return -1;
    }

    return 0;
}

void *rcv_thread(void *data) {
    int read_len;
    msgst ms;
    char message_buf[ID_SIZE + MESSAGE_BUFFER_SIZE];

    while(1) {
        read_len = read(sock, (char *)&ms, sizeof(msgst));
        if(read_len <= 0) {
            pthread_exit(0);
        } else {
            // 서버로 부터 온 메시지의 종류를 구별한다.
            switch(ms.state) {
                // 서버가 클라이언트에게 알림 메시지를 전달 받을 때
                case MSG_ALAM_STATE:
                    strcpy(message_buf, ms.message);
                    break;
                    // 서버로 부터 사용자들의 메시지를 전달 받을 때
                case MSG_DATA_STATE:
                    strcpy(message_buf, ms.id);
                    strcat(message_buf, ": ");
                    strcat(message_buf, ms.message);
                    break;
                    // 서버로 부터 새로운 사용자에 대한 알림.
                case MSG_NEWUSER_STATE:
                    mvwprintw(ulist_win, 1, 1, ms.id);
                    wrefresh(ulist_win);
                    insert_ulist(user_list, ms.id);
                    update_ulist_win(user_list);
                    if(usr_state == USER_LOGIN_STATE) {
                        strcpy(message_buf, ms.id);
                        strcat(message_buf, "님이 입장하셨습니다!");
                        break;
                    } 
                    wrefresh(chat_win);
                    continue;
                    // 서버로 부터 연결 해제된 사용자에 대한 알림.
                case MSG_DELUSER_STATE:
                    delete_ulist(user_list, ms.id);
                    update_ulist_win(user_list);
                    strcpy(message_buf, ms.id);
                    strcat(message_buf, "님이 퇴장하셨습니다!");
                    break;
                    // 서버로 부터 사용자 목록 모두 받은 후 끝이라는 것을 받음.
                case MSG_ENDUSER_STATE:
                    usr_state = USER_LOGIN_STATE;
                    continue;
            }
        }
        // 서버로 부터 받은 메시지를 가공 후 메시지 출력창에 업데이트.
        insert_mlist(message_list, message_buf);
        update_show_win(message_list);
        wrefresh(chat_win);
    }
}

void init_ulist(ulist *lptr)
{
    lptr->count = 0;
    lptr->head = NULL;
    lptr->tail = NULL;
}

void insert_ulist(ulist *lptr, char *id)
{
    p_unode tmp_nptr;
    p_unode new_nptr = (p_unode)malloc(sizeof(unode));

    memset(new_nptr->id, 0x0, sizeof(new_nptr->id));
    memcpy(new_nptr->id, id, strlen(id));

    if(!lptr->head) {
        lptr->head = lptr->tail = new_nptr;
        new_nptr->next = NULL;
        new_nptr->prev = NULL;
    } else {
        lptr->tail->next = new_nptr;
        new_nptr->prev = lptr->tail;
        lptr->tail = new_nptr;
        new_nptr->next = NULL;
    }

    if(lptr->count > (LINES - 7)) {
        tmp_nptr = lptr->head->next;
        free(lptr->head);
        lptr->head = tmp_nptr;
        return;
    }

    lptr->count++;
}

void clear_ulist(ulist *lptr)
{
    p_unode cnode = lptr->head, tnode;

    while(cnode) {
        tnode = cnode->next;
        free(cnode);
        cnode = tnode;
        lptr->count--;
    }

    if(lptr->count == 0) {
        init_ulist(lptr);
    } else {
        printw("Oops!! Memory leak!!\n");
        refresh();
    }
}

void delete_ulist(ulist *lptr, char *key)
{
    p_unode cnode = lptr->head, tnode;

    while(cnode) {
        if(!strcmp(cnode->id, key)) {
            if(!cnode->prev) {
                if(!cnode->next) {
                    lptr->head = NULL;
                    lptr->tail = NULL;
                } else {
                    lptr->head = lptr->tail = cnode->next;
                    cnode->next->prev = NULL;
                }
            } else {
                if(!cnode->next) {
                    cnode->prev->next = NULL;
                    lptr->tail = cnode->prev;
                } else {
                    cnode->next->prev = cnode->prev;
                    cnode->prev->next = cnode->next;
                }
            }
            lptr->count--;
            free(cnode);
            return;
        }
        cnode = cnode->next;
    }

}

void init_mlist(mlist *lptr)
{
    lptr->count = 0;
    lptr->head = NULL;
    lptr->tail = NULL;
}

void insert_mlist(mlist *lptr, char *msg)
{
    p_mnode tmp_nptr;
    p_mnode new_nptr = (p_mnode)malloc(sizeof(mnode));

    memset(new_nptr->msg, 0x0, sizeof(new_nptr->msg));
    memcpy(new_nptr->msg, msg, strlen(msg));

    if(!lptr->head) {
        lptr->head = lptr->tail = new_nptr;
        new_nptr->next = NULL;
        new_nptr->prev = NULL;
    } else {
        lptr->tail->next = new_nptr;
        new_nptr->prev = lptr->tail;
        lptr->tail = new_nptr;
        new_nptr->next = NULL;
    }

    if(lptr->count > (LINES - 20)) {
        tmp_nptr = lptr->head->next;
        free(lptr->head);
        lptr->head = tmp_nptr;
        return;
    }

    lptr->count++;
}

void clear_mlist(mlist *lptr)
{
    p_mnode cnode = lptr->head, tnode;

    while(cnode) {
        tnode = cnode->next;
        free(cnode);
        cnode = tnode;
        lptr->count--;
    }

    if(lptr->count == 0) {
        init_mlist(lptr);
    } else {
        printw("Oops!! Memory leak!!\n");
        refresh();
    }
}

void update_show_win(mlist *list)
{
    int i, msg_cnt = list->count;
    p_mnode cnode = list->tail;

    delwin(show_win);
    //show_win = create_window(LINES - 4, COLS - 17, 0, 16); 
    show_win = create_window(LINES - 17, COLS - 17, 13, 16);

    for(i = 0; i < msg_cnt && cnode; i++) {
        mvwprintw(show_win, LINES - (19 + i), 1, cnode->msg);
        cnode = cnode->prev;
    }

    wrefresh(show_win);
}

void update_ulist_win(ulist *list)
{
    int i, usr_cnt = list->count;
    p_unode cnode = list->head;

    delwin(ulist_win);
    ulist_win = create_window(LINES - 4, 15, 0, 0); 

    for(i = 0; i < usr_cnt && cnode; i++) {
        mvwprintw(ulist_win, 1 + i, 1, cnode->id);
        cnode = cnode->next;
    }

    wrefresh(ulist_win);
}

WINDOW *create_window(int h, int w, int y, int x)
{
    WINDOW *win;

    win = newwin(h, w, y, x);
    box(win, 0, 0);
    wrefresh(win);

    return win;
}

void set_env()
{
    char *lc;

    lc = getenv("LC_CTYPE");
    if(lc != NULL) {
        setlocale(LC_CTYPE, lc);
    } else if(lc = getenv("LC_ALL")) {
        setlocale(LC_CTYPE, lc);
    } else {
        setlocale(LC_CTYPE, "");
    }
}

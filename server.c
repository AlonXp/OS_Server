#include "segel.h"
#include "request.h"
#include "server.h"

#define SCHEDALG_MAX_LEN 8


Queue wait_queue;
int num_workers = 0;
pthread_cond_t cond_work;
pthread_cond_t cond_wait;
void Update(Queue_Element q, struct timeval dispatch_time);
pthread_mutex_t m;
struct thread_info{
    int index;
    int num_dynamic;
    int num_static;
    int total_requests;
    pthread_t pt;
};

struct queue_element{
    int data;
    struct timeval arrival_time;
    struct timeval dispatch_time;
    Queue_Element next;
};
struct queue{
    Queue_Element first, last;
    int maxsize;
    int size;

};
Thread* pthread_arr;
void getargs(int *port,int *worker_thread, int* max_request, char * schedalg, int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *worker_thread = atoi(argv[2]);
    *max_request = atoi(argv[3]);
    strcpy(schedalg, argv[4]);
}
void* main_thread(void* a){
    Thread temp = (Thread)a;
    int thread_num = temp->index;
    while(1) {
        pthread_mutex_lock(&m);
        while(Getsize(wait_queue) == 0)
            pthread_cond_wait(&cond_work, &m);
        Queue_Element toHandle = popFirst(wait_queue);
        if(toHandle == NULL)
        {
            pthread_mutex_unlock(&m);
            continue;
        }
        num_workers+=1;
        pthread_mutex_unlock(&m);
        int connfd = GetFd(toHandle);
        struct timeval ht, dispatch, arrive_time = getArriveTime(toHandle);
        gettimeofday(&ht, NULL);
        timersub(&ht, &arrive_time, &dispatch);
        Update(toHandle, dispatch);
        requestHandle(connfd, pthread_arr[thread_num], toHandle);
        free(toHandle);
	Close(connfd);
        pthread_mutex_lock(&m);
        num_workers-=1;
        pthread_cond_signal(&cond_wait);
        pthread_mutex_unlock(&m);
    }

}
void blockCase(struct sockaddr_in clientaddr, int listenfd, int max_request,
               char schedalg[])
{
    while (1) {
        int clientlen = sizeof(clientaddr);
        Queue_Element new = malloc(sizeof(*new));
        int connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        struct timeval arrTime;
        gettimeofday(&arrTime, NULL);
        pthread_mutex_lock(&m);
        if(Getsize(wait_queue) + num_workers == max_request){
            while (Getsize(wait_queue) + num_workers == max_request)
                pthread_cond_wait(&cond_wait, &m);
        }
        InsertLast(wait_queue, connfd,arrTime,(struct timeval){0}, new);
        pthread_cond_signal(&cond_work);
        pthread_mutex_unlock(&m);
    }
}

void dtCase(struct sockaddr_in clientaddr, int listenfd, int max_request,
            char schedalg[])
{
    while (1) {
        int clientlen = sizeof(clientaddr);
        int connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t * ) & clientlen);
        struct timeval arrTime;
        gettimeofday(&arrTime, NULL);
        pthread_mutex_lock(&m);
        if (Getsize(wait_queue) + num_workers == max_request) {
            pthread_mutex_unlock(&m);
            Close(connfd);
            continue;
        }
        InsertLast(wait_queue, connfd,arrTime, (struct timeval){0},NULL);
        pthread_cond_signal(&cond_work);
        pthread_mutex_unlock(&m);
    }
}

void randomCase(struct sockaddr_in clientaddr, int listenfd, int max_request,
                char schedalg[])
{
    while (1) {
        int clientlen = sizeof(clientaddr);
        int connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t * ) & clientlen);
        struct timeval arrTime;
        gettimeofday(&arrTime, NULL);
        pthread_mutex_lock(&m);
        if(Getsize(wait_queue) + num_workers == max_request)
        {
            if(Getsize(wait_queue) == 0)
            {
                pthread_mutex_unlock(&m);
		Close(connfd);
                continue;
            }
            else
            {
                int amount_to_drop = ceil(0.3*(Getsize(wait_queue)));
                for (int i = 0; i < amount_to_drop; ++i) {
                    srand(time(0));
                    int to_drop = rand()% Getsize(wait_queue);
                    Queue_Element cur = removeTh(wait_queue, to_drop);
                    Close(GetFd(cur));
                    free(cur);
                }
            }
        }
        InsertLast(wait_queue, connfd,arrTime, (struct timeval){0},NULL);
        pthread_cond_signal(&cond_work);
        pthread_mutex_unlock(&m);
    }
}

void dhCase(struct sockaddr_in clientaddr, int listenfd, int max_request,
            char schedalg[])
{
    while (1) {
        Queue_Element to_drop = NULL;
        int clientlen = sizeof(clientaddr);
        Queue_Element new = malloc(sizeof(*new));
        int connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t * ) & clientlen);
        struct timeval arrTime;
        gettimeofday(&arrTime, NULL);
        pthread_mutex_lock(&m);
        if(Getsize(wait_queue) + num_workers == max_request)
        {
            if(Getsize(wait_queue) == 0)
            {
                pthread_mutex_unlock(&m);
		Close(connfd);
                continue;
            }
            else
            {
                to_drop = popFirst(wait_queue);
                Close(GetFd(to_drop));
            }
        }
        InsertLast(wait_queue, connfd,arrTime,(struct timeval){0},new);
        pthread_cond_signal(&cond_work);
        pthread_mutex_unlock(&m);
        if(to_drop != NULL)
            free(to_drop);
    }
}
int main(int argc, char *argv[])
{
    int listenfd, port, worker_thread, max_request;
    struct sockaddr_in clientaddr;
    char schedalg[SCHEDALG_MAX_LEN] = {0};
    getargs(&port, &worker_thread, &max_request, schedalg, argc, argv);
    wait_queue = Createqueue(max_request);
    pthread_arr = malloc(sizeof(Thread) * worker_thread);
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&cond_work, NULL);
    pthread_cond_init(&cond_wait, NULL);
    for(int i = 0; i < worker_thread; i++){
        pthread_arr[i] = (Thread)malloc(sizeof(struct thread_info));
        pthread_arr[i]->index = i;
        pthread_arr[i]->num_dynamic = 0;
        pthread_arr[i]->num_static = 0;
        pthread_arr[i]->total_requests = 0;
        pthread_create(&pthread_arr[i]->pt, NULL,main_thread, (void *)pthread_arr[i]);
    }

    listenfd = Open_listenfd(port);
    if(!strcmp(schedalg, "block"))
        blockCase(clientaddr, listenfd, max_request, schedalg);
    else if(!strcmp(schedalg, "dt"))
        dtCase(clientaddr, listenfd, max_request, schedalg);
    else if(!strcmp(schedalg, "random"))
        randomCase(clientaddr, listenfd, max_request, schedalg);
    else
        dhCase(clientaddr, listenfd, max_request, schedalg);
}

Queue Createqueue(int size){
    Queue new = malloc(sizeof(*new));
    new->first = NULL;
    new->last = NULL;
    new->maxsize = size;
    new->size = 0;
    return new;
}

void InsertLast(Queue q, int new_data, struct timeval arrTime, struct timeval dispatch_time, Queue_Element t){
    Queue_Element new;
    if(t == NULL)
        new = malloc(sizeof(*new));
    else
        new = t;
    new->data = new_data;
    new->arrival_time = arrTime;
    new->dispatch_time = dispatch_time;
    new->next = NULL;
    q->size+=1;
    if(q->size == 1){
        q->first = new;
        q->last = new;
        return;
    }
    q->last->next = new;
    q->last = new;
}
void Update(Queue_Element q, struct timeval dispatch_time){
    q->dispatch_time = dispatch_time; }
Queue_Element popFirst(Queue q){
    if(q->size == 0)
        return NULL;
    Queue_Element toreturn =  q->first;
    q->size-=1;
    if(q->first == q->last){
        q->first = NULL;
        q->last = NULL;
        return toreturn;
    }
    q->first = toreturn->next;
    return toreturn;
}
//ari
Queue_Element removeTh(Queue q, int th)
{
    if(q->size == 0 || th >= q->size || th < 0)
        return NULL;
    if(q->size == 1){
        return popFirst(q);
    }
    Queue_Element cur =  q->first;
    Queue_Element prev = NULL;
    q->size--;
    for (int i = 0; i < th; ++i) {
        prev = cur;
        cur = cur->next;
    }
    if(cur == q->first)
    {
        q->first = cur->next;
        return cur;
    }
    if(cur == q->last)
    {
        q->last = prev;
        prev->next = NULL;
        return cur;
    }
    prev->next = cur->next;
    return cur;
}

int Getsize(Queue q){
    return q->size;
}
int GetFd(Queue_Element elem){
    return elem->data;
}
//void RemoveByData(Queue q, int data){
//    Queue_Element toremove;
//    q->size-=1;
//    if(q->first->data == data && q->last->data == data){
//        free(q->first);
//        q->first= NULL;
//        q->last = NULL;
//        return;
//    }
//    if(q->first->data == data){
//        toremove = q->first;
//        q->first = toremove->next;
//        free(toremove);
//        return;
//    }
//    Queue_Element helper = q->first;
//    while (helper->next != NULL ){
//        if(helper->next->data == data){
//            toremove = helper->next;
//            helper->next = toremove->next;
//            if(q->last == toremove)
//                q->last = helper;
//            free(toremove);
//            return;
//        }
//        helper = helper->next;
//    }
//}

//Queue_Element getLast(Queue q)
//{
//    if(q == NULL || Getsize(q) == 0)
//        return NULL;
//    return q->last;
//}
struct timeval getArriveTime( Queue_Element e)
{
    return e->arrival_time;
}
struct timeval getDispatchTime( Queue_Element e)
{
    return e->dispatch_time;
}

void incRequests(Thread thread, char * field_to_inc)
{
    if(strcmp(field_to_inc, "total") == 0)
        thread->total_requests++;
    else if(strcmp(field_to_inc, "static") == 0)
        thread->num_static++;
    else if(strcmp(field_to_inc, "dynamic") == 0)
        thread->num_dynamic++;
}
int getSumOfRequests(Thread thread, char * field)//field most be dynamic || static || total
{
    if(strcmp(field, "total") == 0)
        return thread->total_requests;
    else if(strcmp(field, "static") == 0)
        return thread->num_static;
    else if(strcmp(field, "dynamic") == 0)
        return thread->num_dynamic;
    else
        return -1;
}
int getThreadNum(Thread thread)
{
    if(thread == NULL)
        return -1;
    return thread->index;
}
int getIndex(Thread thread){
    if(thread == NULL)
        return -1;
    return thread->index;
}
int getMaxSize(Queue q)
{
    return q->maxsize;
}
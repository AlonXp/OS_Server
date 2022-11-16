//
// Created by alonx on 04/06/2022.
//

#ifndef UNTITLED27_SERVER_H
#define UNTITLED27_SERVER_H
typedef struct queue_element* Queue_Element;
typedef struct queue* Queue;
typedef struct request_info *Request_info;
typedef struct thread_info *Thread;
Queue Createqueue(int size);
void InsertLast(Queue q, int new_data, struct timeval arrTime, struct timeval handleTime, Queue_Element t);
Queue_Element popFirst(Queue q);
Queue_Element getLast(Queue q);
int Getsize(Queue q);
int getMaxSize(Queue q);
int GetFd(Queue_Element elem);
void RemoveByData(Queue q, int data);
Queue_Element removeTh(Queue q, int th);
struct timeval getArriveTime( Queue_Element e);
struct timeval getDispatchTime( Queue_Element e);
//Thread:
void incRequests(Thread thread, char * field_to_inc);//field most be dynamic || static || total
int getSumOfRequests(Thread thread, char * field);//field most be dynamic || static || total, returns -1 if not
int getIndex(Thread thread);

int getThreadNum(Thread thread);

#endif //UNTITLED27_SERVER_H
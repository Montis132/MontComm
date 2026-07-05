#ifndef _UTIL_LIST_H_
#define _UTIL_LIST_H_

#include "CommonInclude.h"

typedef struct _LIST_NODE{
    struct _LIST_NODE *Prev;
    struct _LIST_NODE *Next;
}
LIST_NODE;

#define LIST_NODE_INIT(_NODE_)  \
        do{ \
            (_NODE_)->Prev = (_NODE_); \
            (_NODE_)->Next = (_NODE_); \
        }while(0)
    
#define LIST_IS_EMPTY(_NODE_HEAD_) \
        ((_NODE_HEAD_)->Prev == (_NODE_HEAD_) && (_NODE_HEAD_)->Next == (_NODE_HEAD_))
    
#define LIST_ADD_TAIL(_NODE_ADD_, _NODE_HEAD_)  \
        do{ \
            (_NODE_HEAD_)->Prev->Next = (_NODE_ADD_); \
            (_NODE_ADD_)->Prev = (_NODE_HEAD_)->Prev; \
            (_NODE_HEAD_)->Prev = (_NODE_ADD_); \
            (_NODE_ADD_)->Next = (_NODE_HEAD_); \
        }while(0)
    
#define LIST_ENTRY(_NODE_, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_) \
        ((_NODE_)->Next != (_NODE_) ? \
        ((_NODE_STRUCT_TYPE_ *)((char *)(_NODE_) - (size_t)(&((_NODE_STRUCT_TYPE_ *)0)->_NODE_OFFSET_NAME_))) : \
        NULL)
    
#define LIST_FIRST_ENTRY(_HEAD_, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_) \
        LIST_ENTRY((_HEAD_)->Next, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_)
    
#define LIST_FOR_EACH(_NODE_HEAD_, _NODE_LOOP_, _NODE_TMP_, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_) \
        for((_NODE_LOOP_) = LIST_ENTRY((_NODE_HEAD_)->Next, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_), \
                (_NODE_TMP_) = (_NODE_LOOP_) ? LIST_ENTRY((_NODE_LOOP_)->_NODE_OFFSET_NAME_.Next, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_) : NULL; \
            (_NODE_LOOP_) != NULL && (&(_NODE_LOOP_)->_NODE_OFFSET_NAME_ != _NODE_HEAD_); \
            (_NODE_LOOP_) = (_NODE_TMP_), \
                (_NODE_TMP_) = (_NODE_TMP_) ? LIST_ENTRY((_NODE_TMP_)->_NODE_OFFSET_NAME_.Next, _NODE_STRUCT_TYPE_, _NODE_OFFSET_NAME_) : NULL)
    
#define LIST_DEL_NODE(_NODE_) \
        do{ \
            (_NODE_)->Prev->Next = (_NODE_)->Next; \
            (_NODE_)->Next ->Prev = (_NODE_)->Prev; \
        }while(0)

#define LIST_HEAD_COPY(_NEW_HEAD_, _OLD_HEAD_) \
        do { \
            (_NEW_HEAD_)->Next = (_OLD_HEAD_)->Next; \
            (_NEW_HEAD_)->Next->Prev = (_NEW_HEAD_); \
            (_NEW_HEAD_)->Prev = (_OLD_HEAD_)->Prev; \
            (_NEW_HEAD_)->Prev->Next = (_NEW_HEAD_); \
        }while(0)

#endif /* _UTIL_LIST_H_ */

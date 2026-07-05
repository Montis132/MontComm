#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_

#include "CommonInclude.h"

#define UTLog(Fmt, ...)                 printf("----UTLog----[%s-%d]:" Fmt, __func__, __LINE__, ##__VA_ARGS__)

#endif /* _UNIT_TEST_H_ */

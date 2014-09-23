#ifndef TESTS_H
#define TESTS_H

#include <stdbool.h>

#define ASSERT_RET(_v) if(!(_v)){ printf("@ %s:%d `" #_v "`", __FILE__, __LINE__); puts(""); return false; }
#define ASSERT_MSG_RET(_v, ...) if(!(_v)){ printf("error: " __VA_ARGS__); puts(""); return false; }
#define ASSERT_MSG_RET_VAL(_v, _val, ...) if(!(_v)){ printf("error: " __VA_ARGS__); puts(""); return _val; }

bool test_write_tga();
bool test_scale();

#endif

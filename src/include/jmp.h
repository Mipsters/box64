#ifndef _JMP_H_
#define _JMP_H_

#include <setjmp.h>

#ifdef __BIONIC__
#define JMP_BUF jmp_buf
#else
#define JMP_BUF struct __jmp_buf_tag
#endif

#endif
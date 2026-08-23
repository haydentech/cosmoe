#ifndef _BACKEND_INPUT_STATE_H_
#define _BACKEND_INPUT_STATE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void _set_backend_lock_modifiers(uint32_t modifiers);
void _clear_backend_lock_modifiers(void);
int _get_backend_lock_modifiers(uint32_t* modifiers);

#ifdef __cplusplus
}
#endif

#endif

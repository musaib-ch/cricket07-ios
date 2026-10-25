/* Standalone guest-memory regression. No game files or generated code. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../runtime/runtime.h"

int main(void) {
    CPU cpu;
    memset(&cpu, 0xA5, sizeof cpu);
    CPU original = cpu;
    assert(!runtime_try_init(NULL));
#if defined(RC_TEST_GUEST_MAP_FAILURE)
    assert(!runtime_try_init(&cpu));
    assert(g_mem == NULL);
    assert(memcmp(&cpu, &original, sizeof cpu) == 0);
    assert(!runtime_try_init(&cpu)); /* failure remains retryable */
#else
    assert(runtime_try_init(&cpu));
    assert(g_mem != NULL);
    assert(cpu.esp == GUEST_STACK_TOP);
    assert(cpu.fpu_cw == 0x027f);
    assert(cpu.fs_base == GUEST_TIB);
    assert(R32(GUEST_TIB) == 0xffffffffu);
    assert(R32(GUEST_TIB + 0x18) == GUEST_TIB);
    assert(R32(GUEST_TIB + 0x04) == GUEST_STACK_TOP);
    assert(R32(GUEST_TIB + 0x08) == GUEST_STACK_TOP - GUEST_STACK_SIZE);
    assert(!runtime_try_init(&cpu)); /* never replace live memory */
    assert(cpu.esp == GUEST_STACK_TOP);
#endif
    return 0;
}

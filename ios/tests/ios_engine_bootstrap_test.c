/* Test the orchestration without loading game files or linking the engine. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/EngineBootstrap.h"

static int calls[8], count;
static int imports_ok = 1, recorder_ok = 1, memory_ok = 1;

uint32_t host_register_import_handlers(void) {
    calls[count++] = 1;
    return imports_ok ? 42u : 0u;
}
int cricket07_engine_select_ios_recorder(void) {
    calls[count++] = 2;
    return recorder_ok;
}

int runtime_try_init(CPU *c) {
    calls[count++] = 3;
    if (!memory_ok) return 0;
    memset(c, 0, sizeof *c);
    c->esp = 0x00300000u;
    return 1;
}

int main(void) {
    assert(!cricket07_engine_guest_cpu());
    assert(!cricket07_engine_host_prepared());
#if defined(TEST_IMPORT_FAILURE)
    imports_ok = 0;
    assert(!cricket07_engine_prepare_host());
    assert(!cricket07_engine_host_prepared());
    assert(count == 1 && calls[0] == 1);
    imports_ok = 1;
    assert(cricket07_engine_prepare_host());
    assert(count == 3 && calls[1] == 1 && calls[2] == 2);
#elif defined(TEST_RECORDER_FAILURE)
    recorder_ok = 0;
    assert(!cricket07_engine_prepare_host());
    assert(!cricket07_engine_host_prepared());
    assert(count == 2 && calls[0] == 1 && calls[1] == 2);
    recorder_ok = 1;
    assert(cricket07_engine_prepare_host());
    assert(count == 4 && calls[2] == 1 && calls[3] == 2);
#else
    assert(cricket07_engine_prepare_host());
    assert(count == 2 && calls[0] == 1 && calls[1] == 2);
#endif
    assert(cricket07_engine_host_prepared());
    int before = count;
    assert(cricket07_engine_prepare_host());
    assert(count == before); /* no duplicate import/backend mutation */
#if defined(TEST_MEMORY_FAILURE)
    memory_ok = 0;
    assert(!cricket07_engine_prepare_guest());
    assert(!cricket07_engine_guest_cpu());
    assert(count == before + 1 && calls[before] == 3);
    memory_ok = 1;
#endif
    assert(cricket07_engine_prepare_guest());
    assert(cricket07_engine_guest_cpu());
    assert(cricket07_engine_guest_cpu()->esp == 0x00300000u);
    int ready_count = count;
    assert(cricket07_engine_prepare_guest());
    assert(count == ready_count); /* do not remap or reset the guest CPU */
    return 0;
}

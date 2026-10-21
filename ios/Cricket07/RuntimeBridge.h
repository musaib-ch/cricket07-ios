#ifndef CRICKET07_RUNTIME_BRIDGE_H
#define CRICKET07_RUNTIME_BRIDGE_H
/// Returns 1 when the linked C runtime function registry passes a deterministic smoke test.
/// Does not allocate the 4 GiB guest address space or execute Cricket 07.
int cricket07_runtime_linked(void);
int cricket07_runtime_registry_smoke_test(void);
#endif

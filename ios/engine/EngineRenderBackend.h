#ifndef CRICKET07_ENGINE_RENDER_BACKEND_H
#define CRICKET07_ENGINE_RENDER_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

/* Select the existing render recorder for an eventual iOS game launch.
 * This is intentionally opt-in: it does not initialize the guest runtime,
 * load game files, create a Metal device, or start the game thread.
 * Returns 1 only when built with the iOS frame transport and the recorder
 * exposes both init and present callbacks. Otherwise leaves the backend
 * unchanged and returns 0. Must be called before Direct3D device creation.
 */
int cricket07_engine_select_ios_recorder(void);
int cricket07_engine_ios_recorder_selected(void);

#ifdef __cplusplus
}
#endif
#endif

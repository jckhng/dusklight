# KMSDRM EGL Probe

This probe is for testing the Dusklight PortMaster KMSDRM path without running
the full game.

It does the following:

1. Forces `SDL_VIDEODRIVER=kmsdrm`.
2. Creates a small SDL KMSDRM window.
3. Reads SDL's KMSDRM window properties:
   - `device_index`
   - `drm_fd`
   - `gbm_device`
4. Calls `eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_device, NULL)`.
5. Calls `eglInitialize`.
6. Calls `eglChooseConfig` for several GLES window config shapes.
7. Destroys the SDL window and exits.

## How To Run

Copy the whole `kmsdrm-egl-probe/` folder to the device, then run:

```sh
cd kmsdrm-egl-probe
./run-kmsdrm-egl-probe.sh
```

Send back:

```text
kmsdrm-egl-probe.log
```

## Expected Results

If the device has no DRM/KMS node, expect:

```text
path /dev/dri/card0 missing
SDL_Init(SDL_INIT_VIDEO) failed: kmsdrm not available
```

That means this firmware cannot use SDL KMSDRM. Dusklight must use the fbdev
sentinel path on that device.

If SDL KMSDRM works, expect:

```text
SDL_Init ok current_video_driver=kmsdrm
SDL_CreateWindow ok
SDL KMSDRM properties: device_index=0 drm_fd=... gbm_device=0x...
```

If EGL GBM also works, expect:

```text
eglGetPlatformDisplayEXT(GBM, gbm_device)=0x...
eglInitialize(GBM) ok version=...
eglChooseConfig ... count=...
probe result=0
```

That is the signal needed before implementing Dawn/Aurora GBM surface support.

#include <dawn/webgpu.h>
#include <stdio.h>
#include <string.h>

typedef struct ProbeResult {
    WGPURequestAdapterStatus status;
    WGPUAdapter adapter;
    WGPUStringView message;
} ProbeResult;

static const char* backend_name(WGPUBackendType backend) {
    switch (backend) {
    case WGPUBackendType_Null:
        return "Null";
    case WGPUBackendType_Vulkan:
        return "Vulkan";
    case WGPUBackendType_OpenGL:
        return "OpenGL";
    case WGPUBackendType_OpenGLES:
        return "OpenGLES";
    default:
        return "Other";
    }
}

static void print_string_view(const char* label, WGPUStringView view) {
    printf("%s=", label);
    if (view.data == NULL) {
        printf("(null)\n");
        return;
    }
    if (view.length == WGPU_STRLEN) {
        printf("%s\n", view.data);
        return;
    }
    printf("%.*s\n", (int)view.length, view.data);
}

static void request_adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                                     void* userdata1, void* userdata2) {
    (void)userdata2;
    ProbeResult* result = (ProbeResult*)userdata1;
    result->status = status;
    result->adapter = adapter;
    result->message = message;
}

static void test_backend(WGPUInstance instance, WGPUBackendType backend) {
    ProbeResult result = {
        .status = WGPURequestAdapterStatus_CallbackCancelled,
        .adapter = NULL,
        .message = WGPU_STRING_VIEW_INIT,
    };
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.powerPreference = WGPUPowerPreference_HighPerformance;
    options.backendType = backend;
    options.featureLevel = WGPUFeatureLevel_Compatibility;

    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_WaitAnyOnly;
    callback.callback = request_adapter_callback;
    callback.userdata1 = &result;

    WGPUFuture future = wgpuInstanceRequestAdapter(instance, &options, callback);
    WGPUFutureWaitInfo wait = WGPU_FUTURE_WAIT_INFO_INIT;
    wait.future = future;
    WGPUWaitStatus wait_status = wgpuInstanceWaitAny(instance, 1, &wait, 5000000000ull);

    printf("backend=%s wait_status=%d completed=%d request_status=%d adapter=%p\n", backend_name(backend),
           wait_status, wait.completed, result.status, (void*)result.adapter);
    print_string_view("message", result.message);

    if (result.adapter != NULL) {
        WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
        WGPUStatus info_status = wgpuAdapterGetInfo(result.adapter, &info);
        printf("adapter_info_status=%d\n", info_status);
        print_string_view("vendor", info.vendor);
        print_string_view("architecture", info.architecture);
        print_string_view("device", info.device);
        print_string_view("description", info.description);
        printf("adapter_backend=%s adapter_type=%d vendor_id=0x%04x device_id=0x%04x\n",
               backend_name(info.backendType), info.adapterType, info.vendorID, info.deviceID);
        wgpuAdapterInfoFreeMembers(info);
        wgpuAdapterRelease(result.adapter);
    }
}

int main(void) {
    WGPUInstanceFeatureName features[] = {
        WGPUInstanceFeatureName_TimedWaitAny,
    };
    WGPUInstanceDescriptor descriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
    descriptor.requiredFeatureCount = 1;
    descriptor.requiredFeatures = features;

    WGPUInstance instance = wgpuCreateInstance(&descriptor);
    printf("instance=%p\n", (void*)instance);
    if (instance == NULL) {
        return 1;
    }

    test_backend(instance, WGPUBackendType_OpenGLES);
    test_backend(instance, WGPUBackendType_OpenGL);
    test_backend(instance, WGPUBackendType_Vulkan);
    test_backend(instance, WGPUBackendType_Null);

    wgpuInstanceRelease(instance);
    return 0;
}

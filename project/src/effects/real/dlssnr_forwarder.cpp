#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <nvsdk_ngx.h>

#include <new>

namespace {

using Init = NVSDK_NGX_Result(NVSDK_CONV*)(unsigned long long, const wchar_t*, ID3D12Device*, NVSDK_NGX_Version, const NVSDK_NGX_Parameter*);
using Create = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
using Evaluate = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
using Release = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Handle*);
using Shutdown = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12Device*);

constexpr unsigned long long kGenericApplicationId = 0x24480451ull;
constexpr NVSDK_NGX_Feature kNeuralRenderingFeature = static_cast<NVSDK_NGX_Feature>(18);

struct Context
{
    HMODULE model;
    Init init;
    Create create;
    Evaluate evaluate;
    Release release;
    Shutdown shutdown;
    bool initialized;
};

[[nodiscard]] bool Complete(const Context& context) noexcept
{
    return context.init != nullptr && context.create != nullptr && context.evaluate != nullptr && context.release != nullptr && context.shutdown != nullptr;
}

void Destroy(Context* context) noexcept
{
    if (context == nullptr)
        return;
    if (context->model != nullptr)
        (void)::FreeLibrary(context->model);
    delete context;
}

} // namespace

extern "C" __declspec(dllexport) DWORD __cdecl DscreenDlssnrOpen(const wchar_t* modelPath, void** outContext) noexcept
{
    if (modelPath == nullptr || outContext == nullptr)
        return ERROR_INVALID_PARAMETER;
    *outContext = nullptr;
    HMODULE model = ::LoadLibraryExW(modelPath, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (model == nullptr)
        return ::GetLastError();
    Context* context = new (std::nothrow) Context{
        model,
        reinterpret_cast<Init>(::GetProcAddress(model, "NVSDK_NGX_D3D12_Init_Ext")),
        reinterpret_cast<Create>(::GetProcAddress(model, "NVSDK_NGX_D3D12_CreateFeature")),
        reinterpret_cast<Evaluate>(::GetProcAddress(model, "NVSDK_NGX_D3D12_EvaluateFeature")),
        reinterpret_cast<Release>(::GetProcAddress(model, "NVSDK_NGX_D3D12_ReleaseFeature")),
        reinterpret_cast<Shutdown>(::GetProcAddress(model, "NVSDK_NGX_D3D12_Shutdown1")),
        false
    };
    if (context == nullptr)
    {
        (void)::FreeLibrary(model);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    if (!Complete(*context))
    {
        Destroy(context);
        return ERROR_PROC_NOT_FOUND;
    }
    *outContext = context;
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) NVSDK_NGX_Result NVSDK_CONV DscreenDlssnrInit(void* opaque, const wchar_t* dataPath, ID3D12Device* device,
                                                                                const NVSDK_NGX_Parameter* parameters) noexcept
{
    Context* context = static_cast<Context*>(opaque);
    if (context == nullptr)
        return NVSDK_NGX_Result_FAIL_NotInitialized;
    volatile NVSDK_NGX_Result result = context->init(kGenericApplicationId, dataPath, device, NVSDK_NGX_Version_API, parameters);
    context->initialized = !NVSDK_NGX_FAILED(result);
    return result;
}

extern "C" __declspec(dllexport) NVSDK_NGX_Result NVSDK_CONV DscreenDlssnrCreate(void* opaque, ID3D12GraphicsCommandList* list,
                                                                                  const NVSDK_NGX_Parameter* parameters, NVSDK_NGX_Handle** handle) noexcept
{
    Context* context = static_cast<Context*>(opaque);
    if (context == nullptr || !context->initialized)
        return NVSDK_NGX_Result_FAIL_NotInitialized;
    volatile NVSDK_NGX_Result result = context->create(list, kNeuralRenderingFeature, parameters, handle);
    return result;
}

extern "C" __declspec(dllexport) NVSDK_NGX_Result NVSDK_CONV DscreenDlssnrEvaluate(void* opaque, ID3D12GraphicsCommandList* list,
                                                                                    const NVSDK_NGX_Handle* handle,
                                                                                    const NVSDK_NGX_Parameter* parameters) noexcept
{
    Context* context = static_cast<Context*>(opaque);
    if (context == nullptr || !context->initialized)
        return NVSDK_NGX_Result_FAIL_NotInitialized;
    volatile NVSDK_NGX_Result result = context->evaluate(list, handle, parameters, nullptr);
    return result;
}

extern "C" __declspec(dllexport) NVSDK_NGX_Result NVSDK_CONV DscreenDlssnrRelease(void* opaque, NVSDK_NGX_Handle* handle) noexcept
{
    Context* context = static_cast<Context*>(opaque);
    if (context == nullptr || !context->initialized)
        return NVSDK_NGX_Result_FAIL_NotInitialized;
    volatile NVSDK_NGX_Result result = context->release(handle);
    return result;
}

extern "C" __declspec(dllexport) NVSDK_NGX_Result NVSDK_CONV DscreenDlssnrClose(void* opaque, ID3D12Device* device) noexcept
{
    Context* context = static_cast<Context*>(opaque);
    if (context == nullptr)
        return NVSDK_NGX_Result_FAIL_NotInitialized;
    NVSDK_NGX_Result result = static_cast<NVSDK_NGX_Result>(1);
    if (context->initialized)
    {
        volatile NVSDK_NGX_Result shutdown = context->shutdown(device);
        result = shutdown;
        context->initialized = false;
    }
    Destroy(context);
    return result;
}

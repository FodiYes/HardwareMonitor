#pragma once

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric> 
#include <string>
#include <pdh.h>
#include <pdhmsg.h>

#pragma comment(lib, "pdh.lib")

ULONGLONG FT2ULL(FILETIME ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return li.QuadPart;
}


typedef struct nvmlDevice_st* nvmlDevice_t;
typedef struct nvmlUtilization_st { unsigned int gpu; unsigned int memory; } nvmlUtilization_t;
typedef int(*nvmlInit_t)();
typedef int(*nvmlShutdown_t)();
typedef int(*nvmlDeviceGetHandleByIndex_t)(unsigned int, nvmlDevice_t*);
typedef int(*nvmlDeviceGetUtilizationRates_t)(nvmlDevice_t, nvmlUtilization_st*);
typedef int(*nvmlDeviceGetTemperature_t)(nvmlDevice_t, int, unsigned int*);


typedef void* (*ADL_MAIN_MALLOC_CALLBACK)(int);
typedef struct AdapterInfo {
    int iSize; int iAdapterIndex; char strUDID[256]; int iBusNumber; int iDeviceNumber; int iFunctionNumber; int iVendorID;
    char strAdapterName[256]; char strDisplayName[256]; int iPresent; int iExist; char strDriverPath[256]; char strDriverPathExt[256];
    char strPNPString[256]; int iOSDisplayIndex;
} AdapterInfo;
typedef struct ADLPMActivity {
    int iSize; int iEngineClock; int iMemoryClock; int iVddc; int iActivityPercent;
    int iCurrentPerformanceLevel; int iCurrentBusSpeed; int iCurrentBusLanes; int iMaximumBusLanes; int iReserved;
} ADLPMActivity;

typedef int(*ADL_Main_Control_Create_t)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int(*ADL_Main_Control_Destroy_t)();
typedef int(*ADL_Adapter_NumberOfAdapters_Get_t)(int*);
typedef int(*ADL_Adapter_AdapterInfo_Get_t)(AdapterInfo*, int);
typedef int(*ADL_Overdrive5_CurrentActivity_Get_t)(int, ADLPMActivity*);

void* __stdcall ADL_Main_Memory_Alloc(int iSize) { return malloc(iSize); }

class Hardware {
private:
    ULONGLONG lastIdle = 0;
    ULONGLONG lastKernel = 0;
    ULONGLONG lastUser = 0;

    static const int CPU_BUFFER_SIZE = 10;
    std::vector<float> cpuBuffer;
    int bufferIndex = 0;

    enum GpuSource { SOURCE_NONE, SOURCE_NVIDIA, SOURCE_AMD, SOURCE_PDH };
    GpuSource gpuSource = SOURCE_NONE;

    PDH_HQUERY gpuQuery = NULL;
    PDH_HCOUNTER gpuCounter = NULL;
    bool pdhGpuInit = false;

    HMODULE hNvml = nullptr;
    nvmlDevice_t nvidiaDevice = nullptr;
    nvmlDeviceGetUtilizationRates_t nvmlGetUsage = nullptr;
    nvmlDeviceGetTemperature_t nvmlGetTemp = nullptr;

    HMODULE hAdl = nullptr;
    int amdAdapterIndex = -1;
    ADL_Overdrive5_CurrentActivity_Get_t adlGetActivity = nullptr;

    float target_cpu = 0.0f;
    float target_gpu = 0.0f;
    float target_ram = 0.0f;

public:
    float cpu_load = 0.0f;
    float gpu_load = 0.0f;
    float gpu_temp = 0.0f;
    float ram_usage = 0.0f; 
    float ram_percent = 0.0f; 

    Hardware() {
        cpuBuffer.resize(CPU_BUFFER_SIZE, 0.0f);
        InitCPU();

        if (InitNvidia()) gpuSource = SOURCE_NVIDIA;
        else if (InitAMD()) gpuSource = SOURCE_AMD;
        else if (InitUniversalGPU()) gpuSource = SOURCE_PDH;
    }

    ~Hardware() {
        if (pdhGpuInit) PdhCloseQuery(gpuQuery);
        if (hNvml) FreeLibrary(hNvml);
        if (hAdl) {
            auto adlDestroy = (ADL_Main_Control_Destroy_t)GetProcAddress(hAdl, "ADL_Main_Control_Destroy");
            if (adlDestroy) adlDestroy();
            FreeLibrary(hAdl);
        }
    }

    float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    void InitCPU() {
        FILETIME fIdle, fKernel, fUser;
        GetSystemTimes(&fIdle, &fKernel, &fUser);
        lastIdle = FT2ULL(fIdle);
        lastKernel = FT2ULL(fKernel);
        lastUser = FT2ULL(fUser);
    }

    void MeasureCPU() {
        FILETIME fIdle, fKernel, fUser;
        GetSystemTimes(&fIdle, &fKernel, &fUser);

        ULONGLONG nowIdle = FT2ULL(fIdle);
        ULONGLONG nowKernel = FT2ULL(fKernel);
        ULONGLONG nowUser = FT2ULL(fUser);

        ULONGLONG deltaIdle = nowIdle - lastIdle;
        ULONGLONG deltaKernel = nowKernel - lastKernel;
        ULONGLONG deltaUser = nowUser - lastUser;

        // Formula: Total = Kernel + User. (Kernel includes Idle time already)
        ULONGLONG totalSystem = deltaKernel + deltaUser;

        float rawPercent = 0.0f;
        if (totalSystem > 0) {
            ULONGLONG used = totalSystem - deltaIdle;
            rawPercent = (float)used * 100.0f / (float)totalSystem;
        }

        rawPercent = std::max(0.0f, std::min(100.0f, rawPercent));

        cpuBuffer[bufferIndex] = rawPercent;
        bufferIndex = (bufferIndex + 1) % CPU_BUFFER_SIZE;

        float sum = std::accumulate(cpuBuffer.begin(), cpuBuffer.end(), 0.0f);
        target_cpu = sum / CPU_BUFFER_SIZE;

        lastIdle = nowIdle;
        lastKernel = nowKernel;
        lastUser = nowUser;
    }

    bool InitNvidia() {
        hNvml = LoadLibrary(L"nvml.dll");
        if (!hNvml) hNvml = LoadLibrary(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
        if (!hNvml) return false;

        auto nvmlInit = (nvmlInit_t)GetProcAddress(hNvml, "nvmlInit_v2");
        if (!nvmlInit) nvmlInit = (nvmlInit_t)GetProcAddress(hNvml, "nvmlInit");
        auto nvmlGetHandle = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(hNvml, "nvmlDeviceGetHandleByIndex_v2");
        nvmlGetUsage = (nvmlDeviceGetUtilizationRates_t)GetProcAddress(hNvml, "nvmlDeviceGetUtilizationRates");
        nvmlGetTemp = (nvmlDeviceGetTemperature_t)GetProcAddress(hNvml, "nvmlDeviceGetTemperature");

        if (nvmlInit && nvmlGetHandle && nvmlGetUsage && nvmlInit() == 0) {
            return (nvmlGetHandle(0, &nvidiaDevice) == 0);
        }
        return false;
    }

    bool InitAMD() {
        hAdl = LoadLibrary(L"atiadlxx.dll");
        if (!hAdl) hAdl = LoadLibrary(L"atiadlxy.dll");
        if (!hAdl) return false;

        auto adlCreate = (ADL_Main_Control_Create_t)GetProcAddress(hAdl, "ADL_Main_Control_Create");
        auto adlNumAdapters = (ADL_Adapter_NumberOfAdapters_Get_t)GetProcAddress(hAdl, "ADL_Adapter_NumberOfAdapters_Get");
        auto adlGetInfo = (ADL_Adapter_AdapterInfo_Get_t)GetProcAddress(hAdl, "ADL_Adapter_AdapterInfo_Get");
        adlGetActivity = (ADL_Overdrive5_CurrentActivity_Get_t)GetProcAddress(hAdl, "ADL_Overdrive5_CurrentActivity_Get");

        if (adlCreate && adlCreate(ADL_Main_Memory_Alloc, 1) == 0) {
            int numAdapters = 0;
            adlNumAdapters(&numAdapters);
            if (numAdapters > 0) {
                std::vector<AdapterInfo> infos(numAdapters);
                if (adlGetInfo(infos.data(), sizeof(AdapterInfo) * numAdapters) == 0) {
                    for (int i = 0; i < numAdapters; i++) {
                        ADLPMActivity act = { 0 }; act.iSize = sizeof(ADLPMActivity);
                        if (infos[i].iPresent && adlGetActivity(infos[i].iAdapterIndex, &act) == 0) {
                            amdAdapterIndex = infos[i].iAdapterIndex;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool InitUniversalGPU() {
        if (PdhOpenQuery(NULL, 0, &gpuQuery) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounter(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) {
                PdhAddCounter(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter);
            }
            PdhCollectQueryData(gpuQuery);
            pdhGpuInit = true;
            return true;
        }
        return false;
    }

    void Update(float deltaTime) {
        static float sensorTimer = 0.0f;
        sensorTimer += deltaTime;

        if (sensorTimer >= 0.1f) {
            sensorTimer = 0.0f;

            MeasureCPU();

            float rawGpu = 0.0f;
            if (gpuSource == SOURCE_NVIDIA) {
                nvmlUtilization_st rates;
                if (nvmlGetUsage(nvidiaDevice, &rates) == 0) rawGpu = (float)rates.gpu;
                unsigned int temp = 0;
                if (nvmlGetTemp && nvmlGetTemp(nvidiaDevice, 0, &temp) == 0) gpu_temp = (float)temp;
            }
            else if (gpuSource == SOURCE_AMD) {
                ADLPMActivity act = { 0 }; act.iSize = sizeof(ADLPMActivity);
                if (adlGetActivity(amdAdapterIndex, &act) == 0) rawGpu = (float)act.iActivityPercent;
            }
            else if (gpuSource == SOURCE_PDH && pdhGpuInit) {
                PdhCollectQueryData(gpuQuery);
                PDH_FMT_COUNTERVALUE_ITEM* pItems = NULL;
                DWORD dwBufferSize = 0, dwItemCount = 0;
                PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
                if (dwBufferSize > 0) {
                    pItems = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(dwBufferSize);
                    if (pItems) {
                        PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
                        double maxLoad = 0.0;
                        for (DWORD i = 0; i < dwItemCount; i++) if (pItems[i].FmtValue.doubleValue > maxLoad) maxLoad = pItems[i].FmtValue.doubleValue;
                        rawGpu = (float)maxLoad;
                        free(pItems);
                    }
                }
            }
            target_gpu = std::max(0.0f, std::min(100.0f, rawGpu));

            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            ram_usage = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.f * 1024.f * 1024.f);
            target_ram = (float)memInfo.dwMemoryLoad;
        }

        cpu_load = Lerp(cpu_load, target_cpu, deltaTime * 5.0f);
        gpu_load = Lerp(gpu_load, target_gpu, deltaTime * 5.0f);
        ram_percent = Lerp(ram_percent, target_ram, deltaTime * 5.0f);
    }
};
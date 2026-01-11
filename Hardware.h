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

#ifndef PDH_CSTATUS_VALID
#define PDH_CSTATUS_VALID ((DWORD)0x00000000L)
#endif
#ifndef PDH_CSTATUS_NEW_DATA
#define PDH_CSTATUS_NEW_DATA ((DWORD)0x00000001L)
#endif

typedef struct nvmlDevice_st* nvmlDevice_t;
typedef struct nvmlUtilization_st { unsigned int gpu; unsigned int memory; } nvmlUtilization_t;
typedef struct nvmlMemory_st { unsigned long long total; unsigned long long free; unsigned long long used; } nvmlMemory_t;

typedef int(*nvmlInit_t)();
typedef int(*nvmlShutdown_t)();
typedef int(*nvmlDeviceGetHandleByIndex_t)(unsigned int, nvmlDevice_t*);
typedef int(*nvmlDeviceGetUtilizationRates_t)(nvmlDevice_t, nvmlUtilization_st*);
typedef int(*nvmlDeviceGetTemperature_t)(nvmlDevice_t, int, unsigned int*);
typedef int(*nvmlDeviceGetMemoryInfo_t)(nvmlDevice_t, nvmlMemory_t*);

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

typedef struct ADLMemoryInfo {
    long long iMemorySize; char strMemoryType[256]; long long iMemoryBandwidth;
} ADLMemoryInfo;

typedef int(*ADL_Main_Control_Create_t)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int(*ADL_Main_Control_Destroy_t)();
typedef int(*ADL_Adapter_NumberOfAdapters_Get_t)(int*);
typedef int(*ADL_Adapter_AdapterInfo_Get_t)(AdapterInfo*, int);
typedef int(*ADL_Overdrive5_CurrentActivity_Get_t)(int, ADLPMActivity*);
typedef int(*ADL_Adapter_MemoryInfo_Get_t)(int, ADLMemoryInfo*);

void* __stdcall ADL_Main_Memory_Alloc(int iSize) { return malloc(iSize); }


inline ULONGLONG FT2ULL(FILETIME ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return li.QuadPart;
}

class Hardware {
private:
    PDH_HQUERY cpuQuery = NULL;
    PDH_HCOUNTER cpuCounter = NULL;

    static const int CPU_BUFFER_SIZE = 10;
    std::vector<float> cpuBuffer;
    int bufferIndex = 0;

    enum GpuSource { SOURCE_NONE, SOURCE_NVIDIA, SOURCE_AMD, SOURCE_PDH };
    GpuSource gpuSource = SOURCE_NONE;

    PDH_HQUERY gpuQuery = NULL;
    PDH_HCOUNTER gpuCounter = NULL;
    bool pdhGpuInit = false;
    std::vector<BYTE> pdhRawBuffer; 

    HMODULE hNvml = nullptr;
    nvmlDevice_t nvidiaDevice = nullptr;
    nvmlDeviceGetUtilizationRates_t nvmlGetUsage = nullptr;
    nvmlDeviceGetTemperature_t nvmlGetTemp = nullptr;
    nvmlDeviceGetMemoryInfo_t nvmlGetMem = nullptr;

    HMODULE hAdl = nullptr;
    int amdAdapterIndex = -1;
    ADL_Overdrive5_CurrentActivity_Get_t adlGetActivity = nullptr;
    ADL_Adapter_MemoryInfo_Get_t adlGetMemInfo = nullptr;
    long long amdTotalVram = 0;

    float target_cpu = 0.0f;
    float target_gpu = 0.0f;
    float target_ram_load = 0.0f;

public:
    float cpu_load = 0.0f;

    float gpu_load = 0.0f;
    float gpu_temp = 0.0f;
    float gpu_vram_used = 0.0f;
    float gpu_vram_total = 0.0f;

    float ram_usage_gb = 0.0f;
    float ram_percent = 0.0f;
    float ram_total_gb = 0.0f;

    Hardware() {
        cpuBuffer.resize(CPU_BUFFER_SIZE, 0.0f);
        InitCPU();

        if (InitNvidia()) gpuSource = SOURCE_NVIDIA;
        else if (InitAMD()) gpuSource = SOURCE_AMD;
        else if (InitUniversalGPU()) gpuSource = SOURCE_PDH;

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            ram_total_gb = (float)memInfo.ullTotalPhys / (1024.f * 1024.f * 1024.f);
        }
    }

    ~Hardware() {
        if (cpuQuery) PdhCloseQuery(cpuQuery);
        if (pdhGpuInit) PdhCloseQuery(gpuQuery);

        if (hNvml) {
            auto nvmlShutdown = (nvmlShutdown_t)GetProcAddress(hNvml, "nvmlShutdown");
            if (nvmlShutdown) nvmlShutdown();
            FreeLibrary(hNvml);
        }

        if (hAdl) {
            auto adlDestroy = (ADL_Main_Control_Destroy_t)GetProcAddress(hAdl, "ADL_Main_Control_Destroy");
            if (adlDestroy) adlDestroy();
            FreeLibrary(hAdl);
        }
    }

    float Lerp(float a, float b, float t) {
        if (std::abs(b - a) < 0.01f) return b;
        return a + (b - a) * t;
    }

    void InitCPU() {
        cpuQuery = NULL;
        cpuCounter = NULL;

        FILETIME idle, kernel, user;
        GetSystemTimes(&idle, &kernel, &user);
    }


    void MeasureCPU() {
        static ULONGLONG lastIdle = 0;
        static ULONGLONG lastKernel = 0;
        static ULONGLONG lastUser = 0;
        static bool inited = false;

        FILETIME idleFT, kernelFT, userFT;
        if (!GetSystemTimes(&idleFT, &kernelFT, &userFT)) return;

        ULONGLONG idle = FT2ULL(idleFT);
        ULONGLONG kernel = FT2ULL(kernelFT);
        ULONGLONG user = FT2ULL(userFT);

        if (!inited) {
            lastIdle = idle;
            lastKernel = kernel;
            lastUser = user;
            inited = true;
            target_cpu = 0.0f;
            return;
        }

        ULONGLONG idleDiff = idle - lastIdle;
        ULONGLONG totalDiff = (kernel - lastKernel) + (user - lastUser);

        lastIdle = idle;
        lastKernel = kernel;
        lastUser = user;

        if (totalDiff == 0) return;

        double cpu = (1.0 - (double)idleDiff / (double)totalDiff) * 100.0;

        if (cpu < 0.0) cpu = 0.0;
        if (cpu > 100.0) cpu = 100.0;

        target_cpu = (float)cpu;
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
        nvmlGetMem = (nvmlDeviceGetMemoryInfo_t)GetProcAddress(hNvml, "nvmlDeviceGetMemoryInfo");

        if (nvmlInit && nvmlGetHandle && nvmlGetUsage && nvmlInit() == 0) {
            if (nvmlGetHandle(0, &nvidiaDevice) == 0) {
                if (nvmlGetMem) {
                    nvmlMemory_t mem = { 0 };
                    if (nvmlGetMem(nvidiaDevice, &mem) == 0) {
                        gpu_vram_total = (float)mem.total / (1024.f * 1024.f * 1024.f);
                    }
                }
                return true;
            }
        }
        FreeLibrary(hNvml); hNvml = nullptr;
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
        adlGetMemInfo = (ADL_Adapter_MemoryInfo_Get_t)GetProcAddress(hAdl, "ADL_Adapter_MemoryInfo_Get");

        if (adlCreate && adlCreate(ADL_Main_Memory_Alloc, 1) == 0) {
            int numAdapters = 0;
            adlNumAdapters(&numAdapters);
            if (numAdapters > 0) {
                std::vector<AdapterInfo> infos(numAdapters);
                if (adlGetInfo(infos.data(), sizeof(AdapterInfo) * numAdapters) == 0) {
                    for (int i = 0; i < numAdapters; i++) {
                        ADLPMActivity act = { 0 }; act.iSize = sizeof(ADLPMActivity);
                        if (infos[i].iPresent && adlGetActivity && adlGetActivity(infos[i].iAdapterIndex, &act) == 0) {
                            amdAdapterIndex = infos[i].iAdapterIndex;

                            if (adlGetMemInfo) {
                                ADLMemoryInfo memVal = { 0 };
                                if (adlGetMemInfo(infos[i].iAdapterIndex, &memVal) == 0) {
                                    gpu_vram_total = (float)memVal.iMemorySize / (1024.f * 1024.f * 1024.f);
                                }
                            }
                            return true;
                        }
                    }
                }
            }
        }
        FreeLibrary(hAdl); hAdl = nullptr;
        return false;
    }

    bool InitUniversalGPU() {
        if (PdhOpenQuery(NULL, 0, &gpuQuery) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounter(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) {
                if (PdhAddCounter(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) {
                    PdhCloseQuery(gpuQuery);
                    return false;
                }
            }
            PdhCollectQueryData(gpuQuery);
            pdhGpuInit = true;
            return true;
        }
        return false;
    }

    void MeasureGPU() {
        float rawGpu = 0.0f;
        float rawVramUsed = 0.0f;

        if (gpuSource == SOURCE_NVIDIA) {
            if (nvmlGetUsage) {
                nvmlUtilization_st rates;
                if (nvmlGetUsage(nvidiaDevice, &rates) == 0) rawGpu = (float)rates.gpu;
            }
            if (nvmlGetTemp) {
                unsigned int temp = 0;
                if (nvmlGetTemp(nvidiaDevice, 0, &temp) == 0) gpu_temp = (float)temp;
            }
            if (nvmlGetMem) {
                nvmlMemory_t mem = { 0 };
                if (nvmlGetMem(nvidiaDevice, &mem) == 0) {
                    rawVramUsed = (float)mem.used / (1024.f * 1024.f * 1024.f);
                }
            }
        }
        else if (gpuSource == SOURCE_AMD) {
            if (adlGetActivity) {
                ADLPMActivity act = { 0 }; act.iSize = sizeof(ADLPMActivity);
                if (adlGetActivity(amdAdapterIndex, &act) == 0) rawGpu = (float)act.iActivityPercent;
            }
        }
        else if (gpuSource == SOURCE_PDH && pdhGpuInit) {
            PdhCollectQueryData(gpuQuery);

            DWORD dwBufferSize = 0, dwItemCount = 0;
            PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);

            if (dwBufferSize > 0) {
                if (pdhRawBuffer.size() < dwBufferSize) pdhRawBuffer.resize(dwBufferSize);

                PDH_FMT_COUNTERVALUE_ITEM* pItems = (PDH_FMT_COUNTERVALUE_ITEM*)pdhRawBuffer.data();

                PDH_STATUS status = PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);

                if (status == ERROR_SUCCESS) {
                    double maxLoad = 0.0;
                    for (DWORD i = 0; i < dwItemCount; i++) {
                        if (pItems[i].FmtValue.CStatus == PDH_CSTATUS_VALID) {
                            if (pItems[i].FmtValue.doubleValue > maxLoad) maxLoad = pItems[i].FmtValue.doubleValue;
                        }
                    }
                    rawGpu = (float)maxLoad;
                }
            }
        }
        target_gpu = std::max(0.0f, std::min(100.0f, rawGpu));
        gpu_vram_used = rawVramUsed;
    }

    void Update(float deltaTime) {
        static float sensorTimer = 0.0f;
        sensorTimer += deltaTime;

        if (sensorTimer >= 0.2f) {
            sensorTimer = 0.0f;

            MeasureCPU();
            MeasureGPU();

            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                ram_usage_gb = (float)(memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.f * 1024.f * 1024.f);
                target_ram_load = (float)memInfo.dwMemoryLoad;
            }
        }

        float smoothSpeed = deltaTime * 5.0f;
        cpu_load = Lerp(cpu_load, target_cpu, smoothSpeed);
        gpu_load = Lerp(gpu_load, target_gpu, smoothSpeed);
        ram_percent = Lerp(ram_percent, target_ram_load, smoothSpeed);
    }
};
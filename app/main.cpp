#include <iostream>
#include <cpuid.h>
#include <cstdint>
#include <array>
#include <bitset>
#include <string>
#include <cstring>



namespace {
enum : uint32_t { EAX, EBX, ECX, EDX };

using cpuid_t = std::array<uint32_t, 4>;

void cpuid(cpuid_t& data, uint32_t leaf, uint32_t subleaf = 0u) {
    __get_cpuid_count(leaf, subleaf, &data[EAX], &data[EBX], &data[ECX], &data[EDX]);
}

uint32_t getBits(uint32_t src, uint32_t begin, uint32_t end) noexcept {
    uint32_t mask = (1u << (end - begin + 1u)) - 1u;
    return (src >> begin) & mask;
}
}

int main(int argc, char* argv[]) {
    cpuid_t regs;

    auto printData = [&out = std::cout](const char* name, auto&& data) {
      out << name << ": " << data << '\n';
    };

    auto newLine = [&out = std::cout]() {
      out << '\n';
    };

    // Fn0000_0000_EAX (num std fn)
    printData("Fn", "0000_0000");
    cpuid(regs, 0);

    int numStdFunc = regs[EAX];

    // Fn0000_0000_E[D,C,B]X (vendor name 12 bytes)
    char vendor[13]{};
    std::memcpy(vendor, &regs[EBX], 4);
    std::memcpy(vendor + 4, &regs[EDX], 4);
    std::memcpy(vendor + 8, &regs[ECX], 4);

    printData("numStdFunc", numStdFunc);
    printData("vendor", vendor);

    newLine();

    // Fn0000_0001_EAX (Family, Model, Stepping Identifiers)
    printData("Fn", "0000_0001");
    cpuid(regs, 1);
    
    auto stepping = getBits(regs[EAX], 0, 3);
    auto baseModel = getBits(regs[EAX], 4, 7);
    auto baseFamily = getBits(regs[EAX], 8, 11);
    // EAX[12, 15] - Reserved
    auto extModel = getBits(regs[EAX], 16, 19);
    auto extFamily = getBits(regs[EAX], 20, 27);
    // EAX[28, 31] - Reserved

    auto cpuFamily = baseFamily + extFamily;
    auto cpuModel = baseModel | (extModel << 4);

    printData("stepping", stepping);
    // printData("baseModel", baseModel);
    // printData("baseFamily", baseFamily);
    // printData("extModel", extModel);
    // printData("extFamily", extFamily);
    printData("cpuFamily", cpuFamily);
    printData("cpuModel", cpuModel);
    
    // Fn0000_0001_EBX (LocalApicId, LogicalProcessorCount, CLFlush)
    auto brandId = getBits(regs[EBX], 0, 7);
    auto CLFlush = getBits(regs[EBX], 8, 15);
    auto logProcCount = getBits(regs[EBX], 16, 23);
    auto localApicId = getBits(regs[EBX], 24, 31);

    printData("brandId", brandId);
    printData("CLFlush", CLFlush);
    printData("logProcCount", logProcCount);
    printData("localApicId", localApicId);

    // Fn0000_0001_ECX Feature Identifiers
    std::bitset<32> f1_ecx(regs[ECX]);
  
    // Fn0000_0001_EDX Feature Identifiers
    std::bitset<32> f1_edx(regs[EDX]);

    printData("f1_ecx", f1_ecx);
    printData("f1_edx", f1_edx);
    printData("Count Feature Identifiers", f1_ecx.count() + f1_edx.count());

    newLine();

    // Fn0000_000[2,3,4] - Reserved


    
    






















    // Fn0000_0005_E[A,B,C]X Monitor/MWait
    printData("Fn", "0000_0005");
    cpuid(regs, 5);

    auto monLineSizeMin = getBits(regs[EAX], 0, 15);
    // EAX[16,31] - Reserved
    auto monLineSizeMax = getBits(regs[EBX], 0, 15);
    // EBX[16,31] - Reserved
    
    auto emx = getBits(regs[ECX], 0, 0);
    auto ibe = getBits(regs[ECX], 1, 1);
    // ECX[2,31] - Reserved

    // Fn0000_0005_EDX - Reserved

    printData("monLineSizeMin", monLineSizeMin);
    printData("monLineSizeMin", monLineSizeMin);
    printData("emx", emx);
    printData("ibe", ibe);

    newLine();

    // Fn0000_0006_EAX Local APIC Timer Invariance
    printData("Fn", "0000_0006");
    cpuid(regs, 6);

    // EAX[0,1] - Reserved
    auto arat = getBits(regs[EAX], 2, 2);
    // EAX[3,31] - Reserved

    // Fn0000_0006_EBX - Reserved

    // Fn0000_0006_ECX Effective Processor Frequency Interface
    auto effFreq = getBits(regs[ECX], 0, 0);
    // ECX[1,31] - Reserved

    // Fn0000_0006_EDX - Reserved

    printData("arat", arat);
    printData("effFreq", effFreq);

    newLine();

    // Fn0000_0007_EAX_x0 Structured Extended Feature Identifiers (ECX=0)
    printData("Fn", "0000_0007");
    cpuid(regs, 7);

    auto maxSubFn = regs[EAX];
    std::bitset<32> f7_ebx(regs[EBX]);
    printData("CLFLUSHOPT", f7_ebx[23]);
    std::bitset<32> f7_ecx(regs[ECX]);

    printData("maxSubFn", maxSubFn);
    printData("f7_ebx", f7_ebx);
    printData("f7_ecx", f7_ecx);

    // Fn0000_0007_EDX_x0 - Reserved

    // Functions 8h–Ah—Reserved

    newLine();

    // Function Bh — Extended Topology Enumeration
    // Subfunction 0 of Fn0000_000B - Thread Level
    printData("Fn", "0000_000B");
    cpuid(regs, 0x0000'000B);

    auto threadMaskWidth = getBits(regs[EAX], 0, 4);
    // EAX[5,31] - Reserved
    auto numLogProc = getBits(regs[EBX], 0, 15);
    // EBX[16,31] - Reserved
    auto inputEcx = getBits(regs[ECX], 0, 7);
    auto hierarchyLevel = getBits(regs[ECX], 8, 15);
    // ECX[16,31] - Reserved
    auto x2APIC_ID = regs[EDX];

    printData("threadMaskWidth", threadMaskWidth);
    printData("numLogProc", numLogProc);
    printData("inputEcx", inputEcx);
    printData("hierarchyLevel", hierarchyLevel);
    printData("x2APIC_ID", x2APIC_ID);

    newLine();

    // Subfunction 1 of Fn0000_000B - Core Level
    printData("Fn", "0000_000B.1");
    cpuid(regs, 0x0000'000B, 1);

    auto coreMaskWidth = getBits(regs[EAX], 0, 4);
    // EAX[5,31] - Reserved
    auto numLogCores = getBits(regs[EBX], 0, 15);
    // EBX[16, 31] - Reserved
    inputEcx = getBits(regs[ECX], 0, 7);
    hierarchyLevel = getBits(regs[ECX], 8, 15);
    // ECX[16,31] - Reserved
    x2APIC_ID = getBits(regs[EDX], 0, 31);

    printData("coreMaskWidth", coreMaskWidth);
    printData("numLogCores", numLogCores);
    printData("inputEcx", inputEcx);
    printData("hierarchyLevel", hierarchyLevel);
    printData("x2APIC_ID", x2APIC_ID);

    newLine();

    // Function Ch—Reserved
    // Function Dh—Processor Extended State Enumeration
    printData("Fn", "0000_000D");
    cpuid(regs, 0x0000'000D);

    std::bitset<32> xFeatureSupportedMask(regs[EAX]);
    auto xFeatureEnabledSizeMax = regs[EBX];
    auto xFeatureSupportedSizeMax = regs[ECX];
    std::bitset<32> XFeatureSupportedMask(regs[EDX]);

    printData("xFeatureSupportedMask", xFeatureSupportedMask);
    printData("xFeatureEnabledSizeMax", xFeatureEnabledSizeMax);
    printData("xFeatureSupportedSizeMax", xFeatureSupportedSizeMax);
    printData("XFeatureSupportedMask", XFeatureSupportedMask);

    newLine();

    // Fn0000_000D_x1 Processor Extended State Enumeration
    printData("Fn", "0000_000D.1");
    cpuid(regs, 0x0000'000D, 1);

    auto xsaveopt = getBits(regs[EAX], 0, 0);
    auto xsavec = getBits(regs[EAX], 1, 1);
    auto xgetbv = getBits(regs[EAX], 2, 2);
    auto xsaves = getBits(regs[EAX], 3, 3);
    // EAX[4,31] - Reserved

    // TODO: parse EBX

    auto cet_u = getBits(regs[ECX], 11, 11);
    auto cet_s = getBits(regs[ECX], 12, 12);

    // EDX - Reserved

    printData("xsaveopt", xsaveopt);
    printData("xsavec", xsavec);
    printData("xgetbv", xgetbv);
    printData("xsaves", xsaves);
    printData("cet_u", cet_u);
    printData("cet_s", cet_s);

    newLine();

    // Subfunction 2 of Fn0000_000D
    printData("Fn", "0000_000D.2");
    cpuid(regs, 0x0000'000D, 2);

    auto ymmSaveStateSize = regs[EAX];
    auto ymmSaveStateOffset = regs[EBX];

    // E[C,D]X - Reserved

    printData("ymmSaveStateSize", ymmSaveStateSize);
    printData("ymmSaveStateOffset", ymmSaveStateOffset);

    newLine();

    // Subfunction 11 of Fn0000_000D
    printData("Fn", "0000_000D.11");
    cpuid(regs, 0x0000'000D, 11);

    auto cetUserSize = regs[EAX];
    auto cetUserOffset = regs[EBX];
    auto supervisorState = getBits(regs[ECX], 0, 0);
    // EDX - unused

    printData("cetUserSize", cetUserSize);
    printData("cetUserOffset", cetUserOffset);
    printData("supervisorState", supervisorState);

    newLine();

    // Subfunction 12 of Fn0000_000D
    printData("Fn", "0000_000D.12");
    cpuid(regs, 0x0000'000D, 12);

    auto cetSupervisorSize = regs[EAX];
    auto cetSupervisorOffset = regs[EBX];
    supervisorState = getBits(regs[ECX], 0, 0);
    // EDX - unused

    printData("cetSupervisorSize", cetSupervisorSize);
    printData("cetSupervisorOffset", cetSupervisorOffset);
    printData("supervisorState", supervisorState);

    newLine();

    // Subfunction 3Eh of Fn0000_000D
    printData("Fn", "0000_000D.3E");
    cpuid(regs, 0x0000'000D, 0x3E);

    auto lwpSaveStateSize = regs[EAX];
    auto lwpSaveStateOffset = regs[EBX];
    // E[C,D]X - Reserved

    printData("lwpSaveStateSize", lwpSaveStateSize);
    printData("lwpSaveStateOffset", lwpSaveStateOffset);

    newLine();

    // Function Eh—Reserved

    // Function Fh—PQOS Monitoring (PQM)
    // if f7_ebx[12] == 1 - supported
    // else fn - reserved

    if (f7_ebx[12]) {
      printData("Fn", "0000_000F");
      cpuid(regs, 0x0000'000F);

      // EAX - Reserved
      auto Max_RMID = regs[EBX];
      // ECX - Reserved
      auto L3CacheMon = getBits(regs[EDX], 1, 1);
      // EDX[2,31] - Reserved

      printData("Max_RMID", Max_RMID);
      printData("L3CacheMon", L3CacheMon);

      newLine();

      // Fn0000_000F_x1 L3 Cache Monitoring Capabilities
      printData("Fn", "0000_000F.1");
      cpuid(regs, 0x0000'000F, 1);
      
      auto counterSize = getBits(regs[EAX], 0, 7);
      auto overflowBit = getBits(regs[EAX], 8, 8);
      // EAX[9,31] - Reserved
      auto scaleFactor = regs[EBX];
      auto max_RMID = regs[ECX];
      auto L3CacheOccMon = getBits(regs[EDX], 0, 0);
      auto L3CacheBWMonEvt0 = getBits(regs[EDX], 1, 1);
      auto L3CacheBWMonEvt1 = getBits(regs[EDX], 2, 2);
      // EDX[3,31] - Reserved

      printData("counterSize", counterSize);
      printData("overflowBit", overflowBit);
      printData("scaleFactor", scaleFactor);
      printData("max_RMID", max_RMID);
      printData("L3CacheOccMon", L3CacheOccMon);
      printData("L3CacheBWMonEvt0", L3CacheBWMonEvt0);
      printData("L3CacheBWMonEvt1", L3CacheBWMonEvt1);

      newLine();
    }

    if (f7_ebx[15]) {
      // Fn0000_0010_x0 PQE Capabilities
      printData("Fn", "0000_0010");

      // E[A,B,C]X - Reserved
      auto L3Alloc = getBits(regs[EDX], 1, 1);
      printData("L3Alloc", L3Alloc);

      newLine();

      // Fn0000_0010_x1 L3 Cache Allocation Enforcement Capabilities
      printData("Fn", "0000_0010.1");
      cpuid(regs, 0x0000'0010, 1);

      auto CBM_LEN = getBits(regs[EAX], 0, 4);
      std::bitset<32> L3ShareAllocMask(regs[EBX]);
      auto CDP = getBits(regs[ECX], 2, 2);  
      auto COS_MAX = getBits(regs[EDX], 0, 15);

      printData("CBM_LEN", CBM_LEN);
      printData("L3ShareAllocMask", L3ShareAllocMask);
      printData("CDP", CDP);
      printData("COS_MAX", COS_MAX);

      newLine();      
    }

    // Functions 4000_0000h-4000_00FFh—Reserved for Hypervisor Use

    return 0;
}

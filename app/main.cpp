#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#ifdef WIN32
#include <intrin.h>
#else
#include <cmath>
#include <cpuid.h>
#endif

#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/util/ref.hpp"

namespace {
enum : uint32_t { EAX, EBX, ECX, EDX };

using cpuid_t = std::array<uint32_t, 4>;

void cpuid(cpuid_t &data, uint32_t leaf, uint32_t subleaf = 0u) {
#ifdef WIN32
  __cpuidex(data.data(), leaf, subleaf);
#else
  __get_cpuid_count(leaf, subleaf, &data[EAX], &data[EBX], &data[ECX],
                    &data[EDX]);
#endif
}

uint32_t getBits(uint32_t src, uint32_t begin, uint32_t end) noexcept {
  uint32_t mask = (1u << (end - begin + 1u)) - 1u;
  return (src >> begin) & mask;
}

std::string getProcessorBrandString() {
  cpuid_t regs;
  std::string brandString;

  for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; ++leaf) {
    cpuid(regs, leaf);
    brandString.append(reinterpret_cast<char *>(&regs[EAX]), 4);
    brandString.append(reinterpret_cast<char *>(&regs[EBX]), 4);
    brandString.append(reinterpret_cast<char *>(&regs[ECX]), 4);
    brandString.append(reinterpret_cast<char *>(&regs[EDX]), 4);
  }

  // Remove trailing spaces
  brandString.erase(brandString.find_last_not_of(' ') + 1);

  return brandString;
}
} // namespace

struct Context {
  void parse() {
    cpuid_t regs;

    // Fn0000_0000_EAX (num std fn)
    cpuid(regs, 0);
    numStdFunc = regs[EAX];

    // Fn0000_0000_E[D,C,B]X (vendor name 12 bytes)
    char ven[13]{};
    std::memcpy(ven, &regs[EBX], 4);
    std::memcpy(ven + 4, &regs[EDX], 4);
    std::memcpy(ven + 8, &regs[ECX], 4);
    vendor = ven;

    // Fn0000_0001_EAX (Family, Model, Stepping Identifiers)
    cpuid(regs, 1);
    stepping = getBits(regs[EAX], 0, 3);
    baseModel = getBits(regs[EAX], 4, 7);
    baseFamily = getBits(regs[EAX], 8, 11);
    extModel = getBits(regs[EAX], 16, 19);
    extFamily = getBits(regs[EAX], 20, 27);
    cpuFamily = baseFamily + extFamily;
    cpuModel = baseModel | (extModel << 4);

    // Fn0000_0001_EBX (LocalApicId, LogicalProcessorCount, CLFlush)
    brandId = getBits(regs[EBX], 0, 7);
    CLFlush = getBits(regs[EBX], 8, 15);
    logProcCount = getBits(regs[EBX], 16, 23);
    localApicId = getBits(regs[EBX], 24, 31);

    // Fn0000_0001_ECX Feature Identifiers
    f1_ecx = std::bitset<32>(regs[ECX]);

    // Fn0000_0001_EDX Feature Identifiers
    f1_edx = std::bitset<32>(regs[EDX]);

    // Fn0000_0005_E[A,B,C]X Monitor/MWait
    cpuid(regs, 5);
    monLineSizeMin = getBits(regs[EAX], 0, 15);
    monLineSizeMax = getBits(regs[EBX], 0, 15);
    emx = getBits(regs[ECX], 0, 0);
    ibe = getBits(regs[ECX], 1, 1);

    // Fn0000_0006_EAX Local APIC Timer Invariance
    cpuid(regs, 6);
    arat = getBits(regs[EAX], 2, 2);
    effFreq = getBits(regs[ECX], 0, 0);

    // Fn0000_0007_EAX_x0 Structured Extended Feature Identifiers (ECX=0)
    cpuid(regs, 7);
    maxSubFn = regs[EAX];
    f7_ebx = std::bitset<32>(regs[EBX]);
    f7_ecx = std::bitset<32>(regs[ECX]);

    // Function Bh — Extended Topology Enumeration
    // Subfunction 0 of Fn0000_000B - Thread Level
    cpuid(regs, 0x0000'000B);
    threadMaskWidth = getBits(regs[EAX], 0, 4);
    numLogProc = getBits(regs[EBX], 0, 15);
    inputEcx = getBits(regs[ECX], 0, 7);
    hierarchyLevel = getBits(regs[ECX], 8, 15);
    x2APIC_ID = regs[EDX];

    // Subfunction 1 of Fn0000_000B - Core Level
    cpuid(regs, 0x0000'000B, 1);
    coreMaskWidth = getBits(regs[EAX], 0, 4);
    numLogCores = getBits(regs[EBX], 0, 15);
    inputEcx = getBits(regs[ECX], 0, 7);
    hierarchyLevel = getBits(regs[ECX], 8, 15);
    x2APIC_ID = getBits(regs[EDX], 0, 31);

    // Function Dh—Processor Extended State Enumeration
    cpuid(regs, 0x0000'000D);
    xFeatureSupportedMask = std::bitset<32>(regs[EAX]);
    xFeatureEnabledSizeMax = regs[EBX];
    xFeatureSupportedSizeMax = regs[ECX];
    XFeatureSupportedMask = std::bitset<32>(regs[EDX]);

    // Fn0000_000D_x1 Processor Extended State Enumeration
    cpuid(regs, 0x0000'000D, 1);
    xsaveopt = getBits(regs[EAX], 0, 0);
    xsavec = getBits(regs[EAX], 1, 1);
    xgetbv = getBits(regs[EAX], 2, 2);
    xsaves = getBits(regs[EAX], 3, 3);
    cet_u = getBits(regs[ECX], 11, 11);
    cet_s = getBits(regs[ECX], 12, 12);

    // Subfunction 2 of Fn0000_000D
    cpuid(regs, 0x0000'000D, 2);
    ymmSaveStateSize = regs[EAX];
    ymmSaveStateOffset = regs[EBX];

    // Subfunction 11 of Fn0000_000D
    cpuid(regs, 0x0000'000D, 11);
    cetUserSize = regs[EAX];
    cetUserOffset = regs[EBX];
    supervisorState = getBits(regs[ECX], 0, 0);

    // Subfunction 12 of Fn0000_000D
    cpuid(regs, 0x0000'000D, 12);
    cetSupervisorSize = regs[EAX];
    cetSupervisorOffset = regs[EBX];
    supervisorState = getBits(regs[ECX], 0, 0);

    // Subfunction 3Eh of Fn0000_000D
    cpuid(regs, 0x0000'000D, 0x3E);
    lwpSaveStateSize = regs[EAX];
    lwpSaveStateOffset = regs[EBX];

    // Function Fh—PQOS Monitoring (PQM)
    if (f7_ebx[12]) {
      cpuid(regs, 0x0000'000F);
      Max_RMID = regs[EBX];
      L3CacheMon = getBits(regs[EDX], 1, 1);

      // Fn0000_000F_x1 L3 Cache Monitoring Capabilities
      cpuid(regs, 0x0000'000F, 1);
      counterSize = getBits(regs[EAX], 0, 7);
      overflowBit = getBits(regs[EAX], 8, 8);
      scaleFactor = regs[EBX];
      max_RMID = regs[ECX];
      L3CacheOccMon = getBits(regs[EDX], 0, 0);
      L3CacheBWMonEvt0 = getBits(regs[EDX], 1, 1);
      L3CacheBWMonEvt1 = getBits(regs[EDX], 2, 2);
    }

    if (f7_ebx[15]) {
      // Fn0000_0010_x0 PQE Capabilities
      cpuid(regs, 0x0000'0010);
      L3Alloc = getBits(regs[EDX], 1, 1);

      // Fn0000_0010_x1 L3 Cache Allocation Enforcement Capabilities
      cpuid(regs, 0x0000'0010, 1);
      CBM_LEN = getBits(regs[EAX], 0, 4);
      L3ShareAllocMask = std::bitset<32>(regs[EBX]);
      CDP = getBits(regs[ECX], 2, 2);
      COS_MAX = getBits(regs[EDX], 0, 15);
    }

    // Get and store the processor brand string
    brandString = getProcessorBrandString();
  }

  void dump() const {
    auto printData = [&out = std::cout](const char *name, auto &&data) {
      out << name << ": " << data << '\n';
    };

    auto newLine = [&out = std::cout]() { out << '\n'; };

    printData("numStdFunc", numStdFunc);
    printData("vendor", vendor);
    printData("stepping", stepping);
    printData("cpuFamily", cpuFamily);
    printData("cpuModel", cpuModel);
    printData("brandId", brandId);
    printData("CLFlush", CLFlush);
    printData("logProcCount", logProcCount);
    printData("localApicId", localApicId);
    printData("f1_ecx", f1_ecx);
    printData("f1_edx", f1_edx);
    printData("Count Feature Identifiers", f1_ecx.count() + f1_edx.count());

    newLine();

    printData("monLineSizeMin", monLineSizeMin);
    printData("monLineSizeMax", monLineSizeMax);
    printData("emx", emx);
    printData("ibe", ibe);

    newLine();

    printData("arat", arat);
    printData("effFreq", effFreq);

    newLine();

    printData("maxSubFn", maxSubFn);
    printData("f7_ebx", f7_ebx);
    printData("f7_ecx", f7_ecx);

    newLine();

    printData("threadMaskWidth", threadMaskWidth);
    printData("numLogProc", numLogProc);
    printData("inputEcx", inputEcx);
    printData("hierarchyLevel", hierarchyLevel);
    printData("x2APIC_ID", x2APIC_ID);

    newLine();

    printData("coreMaskWidth", coreMaskWidth);
    printData("numLogCores", numLogCores);
    printData("inputEcx", inputEcx);
    printData("hierarchyLevel", hierarchyLevel);
    printData("x2APIC_ID", x2APIC_ID);

    newLine();

    printData("xFeatureSupportedMask", xFeatureSupportedMask);
    printData("xFeatureEnabledSizeMax", xFeatureEnabledSizeMax);
    printData("xFeatureSupportedSizeMax", xFeatureSupportedSizeMax);
    printData("XFeatureSupportedMask", XFeatureSupportedMask);

    newLine();

    printData("xsaveopt", xsaveopt);
    printData("xsavec", xsavec);
    printData("xgetbv", xgetbv);
    printData("xsaves", xsaves);
    printData("cet_u", cet_u);
    printData("cet_s", cet_s);

    newLine();

    printData("ymmSaveStateSize", ymmSaveStateSize);
    printData("ymmSaveStateOffset", ymmSaveStateOffset);

    newLine();

    printData("cetUserSize", cetUserSize);
    printData("cetUserOffset", cetUserOffset);
    printData("supervisorState", supervisorState);

    newLine();

    printData("cetSupervisorSize", cetSupervisorSize);
    printData("cetSupervisorOffset", cetSupervisorOffset);
    printData("supervisorState", supervisorState);

    newLine();

    printData("lwpSaveStateSize", lwpSaveStateSize);
    printData("lwpSaveStateOffset", lwpSaveStateOffset);

    newLine();

    if (f7_ebx[12]) {
      printData("Max_RMID", Max_RMID);
      printData("L3CacheMon", L3CacheMon);

      newLine();

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
      printData("L3Alloc", L3Alloc);

      newLine();

      printData("CBM_LEN", CBM_LEN);
      printData("L3ShareAllocMask", L3ShareAllocMask);
      printData("CDP", CDP);
      printData("COS_MAX", COS_MAX);

      newLine();
    }

    printData("Processor Brand String", brandString);
  }

  int numStdFunc;
  std::string vendor;
  std::string brandString;
  uint32_t stepping;
  uint32_t baseModel;
  uint32_t baseFamily;
  uint32_t extModel;
  uint32_t extFamily;
  uint32_t cpuFamily;
  uint32_t cpuModel;
  uint32_t brandId;
  uint32_t CLFlush;
  uint32_t logProcCount;
  uint32_t localApicId;
  std::bitset<32> f1_ecx;
  std::bitset<32> f1_edx;
  uint32_t monLineSizeMin;
  uint32_t monLineSizeMax;
  uint32_t emx;
  uint32_t ibe;
  uint32_t arat;
  uint32_t effFreq;
  uint32_t maxSubFn;
  std::bitset<32> f7_ebx;
  std::bitset<32> f7_ecx;
  uint32_t threadMaskWidth;
  uint32_t numLogProc;
  uint32_t inputEcx;
  uint32_t hierarchyLevel;
  uint32_t x2APIC_ID;
  uint32_t coreMaskWidth;
  uint32_t numLogCores;
  std::bitset<32> xFeatureSupportedMask;
  uint32_t xFeatureEnabledSizeMax;
  uint32_t xFeatureSupportedSizeMax;
  std::bitset<32> XFeatureSupportedMask;
  uint32_t xsaveopt;
  uint32_t xsavec;
  uint32_t xgetbv;
  uint32_t xsaves;
  uint32_t cet_u;
  uint32_t cet_s;
  uint32_t ymmSaveStateSize;
  uint32_t ymmSaveStateOffset;
  uint32_t cetUserSize;
  uint32_t cetUserOffset;
  uint32_t supervisorState;
  uint32_t cetSupervisorSize;
  uint32_t cetSupervisorOffset;
  uint32_t lwpSaveStateSize;
  uint32_t lwpSaveStateOffset;
  uint32_t Max_RMID;
  uint32_t L3CacheMon;
  uint32_t counterSize;
  uint32_t overflowBit;
  uint32_t scaleFactor;
  uint32_t max_RMID;
  uint32_t L3CacheOccMon;
  uint32_t L3CacheBWMonEvt0;
  uint32_t L3CacheBWMonEvt1;
  uint32_t L3Alloc;
  uint32_t CBM_LEN;
  std::bitset<32> L3ShareAllocMask;
  uint32_t CDP;
  uint32_t COS_MAX;
};

int main(int argc, char *argv[]) {
  using namespace ftxui;
  auto screen = ScreenInteractive::Fullscreen();

  Context context;
  context.parse();
  // context.dump();

  int shift = 0;

  auto demoCpu = [&shift](int width, int height) {
    std::vector<int> output(width);
    for (int i = 0; i < width; ++i) {
      float v = 0.5f;
      v += 0.1f * std::sin((i + shift) * 0.1f);
      v += 0.2f * std::sin((i + shift + 10) * 0.15f);
      v += 0.1f * std::sin((i + shift) * 0.03f);
      v *= height;
      output[i] = (int)v;
    }
    return output;
  };

  auto monitor = Renderer([&] {
    auto frequency = vbox({
        text("Frequency [Mhz]") | hcenter,
        hbox({
            vbox({
                text("2400 "),
                filler(),
                text("1200 "),
                filler(),
                text("0 "),
            }),
            graph(std::ref(demoCpu)) | flex,
        }) | flex,
    });
    return frequency;
  });

  auto proc = Renderer([&] {
    auto info =
        vbox({text("Vendor: " + context.vendor),
              text("Model: " + context.brandString),
              text("Stepping: " + std::to_string(context.stepping)),
              text("FamilyID: " + std::to_string(context.cpuFamily)),
              text("ModelID: " + std::to_string(context.cpuModel)),
              text("num log cores: " + std::to_string(context.numLogCores)),
              text("num phy cores: " +
                   std::to_string(context.numLogCores / context.numLogProc)),
              text("-- More --")}) |
        border;
    return vbox({info});
  });

  int tabIndex = 0;
  std::vector<std::string> tabEntries = {
      "info",
      "monitor",
      "benchmark",
  };
  auto tabSelection =
      Menu(&tabEntries, &tabIndex, MenuOption::HorizontalAnimated());
  auto tabContent = Container::Tab(
      {
          proc,
          proc,
          monitor,
      },
      &tabIndex);

  auto exitButton =
      Button("Exit", [&] { screen.Exit(); }, ButtonOption::Animated());

  auto mainContainer = Container::Vertical({
      Container::Horizontal({
          tabSelection,
          exitButton,
      }),
      tabContent,
  });

  auto mainRenderer = Renderer(mainContainer, [&] {
    return vbox({
        text("CPUID-TUI") | bold | hcenter,
        hbox({
            tabSelection->Render() | flex,
            exitButton->Render(),
        }),
        tabContent->Render() | flex,
    });
  });

  std::atomic<bool> refreshUiContinue = true;
  std::thread refresh_ui([&] {
    while (refreshUiContinue) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(0.05s);
      screen.Post([&] { shift++; });
      screen.Post(Event::Custom);
    }
  });

  screen.Loop(mainRenderer);
  refreshUiContinue = false;
  refresh_ui.join();

  return 0;
}

#include "Opl3HardwareC86ctl.h"

#include <cstdio>

#include <windows.h>

#include "../../third_party/c86ctl/c86ctl.h"

// c86ctl.dll は LoadLibrary で読み込む (インポートライブラリにはリンクしない)。
// 関数の型はヘッダの宣言から取る。
struct Opl3HardwareC86ctl::Api
{
    HMODULE module = nullptr;
    decltype(&c86ctl::c86ctl_initialize) initialize = nullptr;
    decltype(&c86ctl::c86ctl_deinitialize) deinitialize = nullptr;
    decltype(&c86ctl::c86ctl_reset) reset = nullptr;
    decltype(&c86ctl::c86ctl_get_num_chips) get_num_chips = nullptr;
    decltype(&c86ctl::c86ctl_get_chip_type) get_chip_type = nullptr;
    decltype(&c86ctl::c86ctl_outex) outex = nullptr;
    decltype(&c86ctl::c86ctl_flushex) flushex = nullptr;    // 古い DLL には無い (無ければ c86ctl の自動送信に任せる)
};

namespace
{
    template <class F>
    bool load_proc(HMODULE module, const char* name, F& fn)
    {
        fn = reinterpret_cast<F>(GetProcAddress(module, name));
        return fn != nullptr;
    }
}

Opl3HardwareC86ctl::Opl3HardwareC86ctl() : api_(std::make_unique<Api>())
{
}

Opl3HardwareC86ctl::~Opl3HardwareC86ctl()
{
    close();
}


int Opl3HardwareC86ctl::findChip(int type)
{
	int numChips = api_->get_num_chips();
	for (int i = 0; i < numChips; ++i) {
		if ((api_->get_chip_type(i) & 0xffff) == type) {
			return i;
		}
	}
	return -1;
}

bool Opl3HardwareC86ctl::open()
{
    std::lock_guard lock(mutex_);
    if (open_) return true;
    lastError_.clear();

    api_->module = LoadLibraryW(L"c86ctl.dll");
    if (!api_->module) {
        wchar_t msg[128];
        swprintf_s(msg, L"Failed to load c86ctl.dll (error %lu).", GetLastError());
        lastError_ = msg;
        OutputDebugStringW(L"Opl3HardwareC86ctl: LoadLibrary(c86ctl.dll) failed\n");
        return false;
    }

    const char* missing = nullptr;
    if (!load_proc(api_->module, "c86ctl_initialize", api_->initialize)) missing = "c86ctl_initialize";
    else if (!load_proc(api_->module, "c86ctl_deinitialize", api_->deinitialize)) missing = "c86ctl_deinitialize";
    else if (!load_proc(api_->module, "c86ctl_reset", api_->reset)) missing = "c86ctl_reset";
    else if (!load_proc(api_->module, "c86ctl_get_num_chips", api_->get_num_chips)) missing = "c86ctl_get_num_chips";
    else if (!load_proc(api_->module, "c86ctl_get_chip_type", api_->get_chip_type)) missing = "c86ctl_get_chip_type";
    else if (!load_proc(api_->module, "c86ctl_outex", api_->outex)) missing = "c86ctl_outex";
    if (missing) {
        wchar_t msg[128];
        swprintf_s(msg, L"Function %hs was not found in c86ctl.dll.", missing);
        lastError_ = msg;
        OutputDebugStringW(L"Opl3HardwareC86ctl: GetProcAddress failed\n");
        unloadLibrary();
        return false;
    }
    if (!load_proc(api_->module, "c86ctl_flushex", api_->flushex)) {
        OutputDebugStringW(L"Opl3HardwareC86ctl: c86ctl_flushex not found (writes are sent by c86ctl's auto flush)\n");
    }

    int ret = api_->initialize();
	if (ret != C86CTL_ERR_NONE) {
		wchar_t msg[128];
		swprintf_s(msg, L"Opl3HardwareC86ctl: c86ctl_initialize() failed: %d\n", ret);
		OutputDebugStringW(msg);
		swprintf_s(msg, L"c86ctl_initialize() failed (error %d).", ret);
		lastError_ = msg;
		unloadLibrary();
		return false;
	}

	int numChips = api_->get_num_chips();
    if (numChips <= 0) {
        api_->deinitialize();
        unloadLibrary();
        OutputDebugStringW(L"Opl3HardwareC86ctl: no device found\n");
        lastError_ = L"No OPL3 hardware was found.";
        return false;
    }

    targetChipIdx = findChip(c86ctl::CHIP_OPL3);
	if (targetChipIdx == -1) 
		targetChipIdx = findChip(c86ctl::CHIP_OPLL);
    if (targetChipIdx == -1) 
        targetChipIdx = findChip(c86ctl::CHIP_YMF297);
    if (targetChipIdx == -1) 
        targetChipIdx = findChip(c86ctl::CHIP_YMF278B);
    if (targetChipIdx == -1) {
        // OPL系のchipが無ければ1個目のchipを無理やり利用する
        int chipType = api_->get_chip_type(0);
        wchar_t msg[128];
        swprintf_s(msg, L"Opl3HardwareC86ctl: chip %d is not OPL3 (type=%d)\n", 0, chipType);
        OutputDebugStringW(msg);
        targetChipIdx = 0;
    }

    open_ = true;
    writeCount_ = 0;
    flushCount_ = 0;
    return true;
}

void Opl3HardwareC86ctl::close()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;

    // TODO: 発音を止めてから切断する
    wchar_t msg[128];
    swprintf_s(msg, L"Opl3HardwareC86ctl: writes=%llu flushes=%llu\n",
               static_cast<unsigned long long>(writeCount_), static_cast<unsigned long long>(flushCount_));
    OutputDebugStringW(msg);

    // デバイスを解放してから DLL を解放する
    api_->deinitialize();
    open_ = false;
    unloadLibrary();
}

void Opl3HardwareC86ctl::unloadLibrary()
{
    if (api_->module) FreeLibrary(api_->module);
    *api_ = Api{};
}

void Opl3HardwareC86ctl::reset()
{
    std::lock_guard lock(mutex_);

    if (open_) api_->reset();
    // TODO: チップのリセット (IC 端子) またはレジスタの全クリアを送る
    shadow_.fill(0);
}

void Opl3HardwareC86ctl::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    if (!open_) return;

    api_->outex(targetChipIdx, reg, value);

    shadow_[reg & 0x1ff] = value;
    ++writeCount_;
}

void Opl3HardwareC86ctl::flush()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;
    // この tick で溜めた書き込みをまとめてデバイスへ送り出す
    if (api_->flushex) api_->flushex(targetChipIdx);
    ++flushCount_;
}

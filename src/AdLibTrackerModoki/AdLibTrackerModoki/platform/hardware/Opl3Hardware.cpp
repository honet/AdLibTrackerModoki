#include "Opl3Hardware.h"

#include <cstdio>

#include <windows.h>

#include "../../third_party/c86ctl/c86ctl.h"

// c86ctl.dll は LoadLibrary で読み込む (インポートライブラリにはリンクしない)。
// 関数の型はヘッダの宣言から取る。
struct Opl3Hardware::Api
{
    HMODULE module = nullptr;
    decltype(&c86ctl::c86ctl_initialize) initialize = nullptr;
    decltype(&c86ctl::c86ctl_deinitialize) deinitialize = nullptr;
    decltype(&c86ctl::c86ctl_reset) reset = nullptr;
    decltype(&c86ctl::c86ctl_get_num_chips) get_num_chips = nullptr;
    decltype(&c86ctl::c86ctl_outex) outex = nullptr;
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

Opl3Hardware::Opl3Hardware() : api_(std::make_unique<Api>())
{
}

Opl3Hardware::~Opl3Hardware()
{
    close();
}

bool Opl3Hardware::open()
{
    std::lock_guard lock(mutex_);
    if (open_) return true;
    lastError_.clear();

    api_->module = LoadLibraryW(L"c86ctl.dll");
    if (!api_->module) {
        wchar_t msg[128];
        swprintf_s(msg, L"Failed to load c86ctl.dll (error %lu).", GetLastError());
        lastError_ = msg;
        OutputDebugStringW(L"Opl3Hardware: LoadLibrary(c86ctl.dll) failed\n");
        return false;
    }

    const char* missing = nullptr;
    if (!load_proc(api_->module, "c86ctl_initialize", api_->initialize)) missing = "c86ctl_initialize";
    else if (!load_proc(api_->module, "c86ctl_deinitialize", api_->deinitialize)) missing = "c86ctl_deinitialize";
    else if (!load_proc(api_->module, "c86ctl_reset", api_->reset)) missing = "c86ctl_reset";
    else if (!load_proc(api_->module, "c86ctl_get_num_chips", api_->get_num_chips)) missing = "c86ctl_get_num_chips";
    else if (!load_proc(api_->module, "c86ctl_outex", api_->outex)) missing = "c86ctl_outex";
    if (missing) {
        wchar_t msg[128];
        swprintf_s(msg, L"Function %hs was not found in c86ctl.dll.", missing);
        lastError_ = msg;
        OutputDebugStringW(L"Opl3Hardware: GetProcAddress failed\n");
        unloadLibrary();
        return false;
    }

    int ret = api_->initialize();
	if (ret != C86CTL_ERR_NONE) {
		wchar_t msg[128];
		swprintf_s(msg, L"Opl3Hardware: c86ctl_initialize() failed: %d\n", ret);
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
        OutputDebugStringW(L"Opl3Hardware: no device found\n");
		lastError_ = L"No OPL3 hardware was found.";
		return false;
	}

    open_ = true;
    writeCount_ = 0;
    flushCount_ = 0;
    return true;
}

void Opl3Hardware::close()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;

    // TODO: 発音を止めてから切断する
    wchar_t msg[128];
    swprintf_s(msg, L"Opl3Hardware: writes=%llu flushes=%llu\n",
               static_cast<unsigned long long>(writeCount_), static_cast<unsigned long long>(flushCount_));
    OutputDebugStringW(msg);

    // デバイスを解放してから DLL を解放する
    api_->deinitialize();
    open_ = false;
    unloadLibrary();
}

void Opl3Hardware::unloadLibrary()
{
    if (api_->module) FreeLibrary(api_->module);
    *api_ = Api{};
}

void Opl3Hardware::reset()
{
    std::lock_guard lock(mutex_);

    if (open_) api_->reset();
    // TODO: チップのリセット (IC 端子) またはレジスタの全クリアを送る
    shadow_.fill(0);
}

void Opl3Hardware::writeReg(uint16_t reg, uint8_t value)
{
    std::lock_guard lock(mutex_);
    if (!open_) return;

    api_->outex(0, reg, value);

    shadow_[reg & 0x1ff] = value;
    ++writeCount_;
}

void Opl3Hardware::flush()
{
    std::lock_guard lock(mutex_);
    if (!open_) return;
    // TODO: 送信バッファの内容をデバイスへ送り出す
    ++flushCount_;
}

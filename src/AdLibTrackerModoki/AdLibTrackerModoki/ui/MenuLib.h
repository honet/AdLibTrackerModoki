#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "TxtScrIO.h"

// 2 つ並べて使うメニュー (menulib1.pas / menulib2.pas の移植)
//
// オリジナルは dialogio.pas の Menu の古い版を写した 2 つのユニットで、状態をそれぞれのユニットに持つ。
// 楽器の置き換え (REMAP) で左右の一覧に使う。本クローン版では同じクラスの 2 つのインスタンスにした。
namespace at2
{

class MenuLib
{
public:
    struct Setting
    {
        std::string frame_type;
        bool shadow_enabled;
        bool posbar_enabled;
        uint8_t title_attr;
        uint8_t menu_attr;
        uint8_t text_attr;
        uint8_t text2_attr;
        uint8_t default_attr;
        uint8_t short_attr;
        uint8_t short2_attr;
        uint8_t disbld_attr;
        uint8_t contxt_attr;
        uint8_t contxt2_attr;
        bool center_box;
        bool cycle_moves;
        bool edit_contents;
        bool reverse_use;
        uint8_t fixed_len;
        uint16_t terminate_keys[50];   // [1..50]
    };

    struct Environment
    {
        tSCREEN_MEM_PTR v_dest;
        uint16_t keystroke;
        std::string context;
        bool unpolite;
        bool winshade;
        uint8_t edit_pos;
        uint16_t curr_pos;
        void (*ext_proc)();
        bool do_refresh;
        bool preview;
    };

    Setting mn_setting;
    Environment mn_environment;

    MenuLib();

    // MenuLib1_Init / MenuLib2_Init
    void Init();

    // MenuLib1_Menu / MenuLib2_Menu。data[0..count-1] の項目 (各 len 文字まで) を len2 行の窓で選ばせる。
    // unpolite のときは前回の位置 (idx2 / page) のまま続ける
    uint16_t Menu(std::vector<std::string>& data, uint8_t x, uint8_t y, uint16_t spos, uint8_t len, uint8_t len2,
                  uint16_t count, const std::string& title);

private:
    struct MenuKey   // tMBUFFR の要素
    {
        char key;
        bool use;
    };

    std::string pstr(uint16_t item) const;
    std::string pdes(uint16_t item) const;
    void refresh();
    void SubPos(uint16_t& p);
    void AddPos(uint16_t& p);

    // オリジナルはユニットのグローバル変数 (呼び出しの間で値が残る)
    MenuKey mbuf[16385];   // [1..16384]
    uint16_t max_ = 0, len2b_ = 0, page_ = 0, first_ = 0, last_ = 0, temp2_ = 0, opage_ = 0, opos_ = 0, idx2_ = 0;
    uint16_t len2_ = 0;   // Menu の引数 len2 (SubPos / AddPos が使う)
    bool solid_ = false;
    uint8_t mnu_x_ = 0, mnu_y_ = 0, mnu_len_ = 0, mnu_len2_ = 0;
    std::vector<std::string>* mnu_data_ = nullptr;
    uint16_t mnu_count_ = 0;
    uint16_t vscrollbar_pos_ = 0;
};

extern MenuLib MenuLib1;
extern MenuLib MenuLib2;

} // namespace at2

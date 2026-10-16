// ヘルプ (adt2text.pas の HELP の移植)
//
// MainScreen.cpp から #include する (MainScreen.cpp の無名名前空間の中で使う)。ロックを取らない状態で呼ぶ。

constexpr uint8_t help_background = 0x40;
constexpr uint8_t help_title = 0x0b;
constexpr uint8_t help_border = 0x0f;
constexpr uint8_t help_topic = 0x0c;
constexpr uint8_t help_text = 0x07;
constexpr uint8_t help_hi_text = 0x0c;
constexpr uint8_t help_keys = 0x0f;
constexpr uint8_t help_indicators = 0x0e;

// help_data[1..HELP_LINES] (1 始まり)
std::string help_line(int index)
{
    if (index < 1 || index > HELP_LINES) return std::string();
    return help_data[index - 1];
}

void HELP(const std::string& topic)
{
    static const char* const key_comment_B = "  C   D   E   F   G   A   B   C   D   E   F   G   A   B   C   D   E";
    static const char* const shift_f5_1 = "~[Shift] F5~               @F5 with Trace";
    static const char* const shift_f8_1 = "~[Shift] F8~               @F8 with Trace";
    static const char* const shift_f9_1 = "~[Shift] F9~               @F9 with Trace";
    static const char* const alt_f8_1 = "~[Alt] F8~                 @F8 \xC3 without synchronization";
    static const char* const shift_f5_2 = "~[Shift] F5~               @F5 with no Trace";
    static const char* const shift_f8_2 = "~[Shift] F8~               @F8 with no Trace";
    static const char* const shift_f9_2 = "~[Shift] F9~               @F9 with no Trace";
    static const char* const alt_f8_2 = "~[Alt] F8~                 @F8 \xC3 with synchronization";

    uint8_t spec_attr_table[256] = {};   // [1..255]

    // 1 行を表示する。'~@@x' / '`@@x' はその位置に x を atr2 / atr3 で、'@@attr:..,..,..@@' は色を変え、
    // '@@spec_attr:..@@' は spec_attr_table の色を atr3 に使う
    const auto ListCStr = [&](tSCREEN_MEM_PTR dest, uint8_t x, uint8_t y, std::string str, uint8_t atr1, uint8_t atr2,
                              uint8_t atr3) {
        const auto pos = [&](const char* sub) { return static_cast<int>(str.find(sub) + 1); };   // Pascal の Pos (なければ 0)
        const auto show = [&](uint8_t a1, uint8_t a2, uint8_t a3) {
            ShowC3Str(dest, x, y, ExpStrR(str, B(74 + static_cast<int>(str.size()) - C3StrLen(str)), ' '), a1, a2, a3);
        };
        const int temp_pos_atr1 = pos("@@attr:");
        const int temp_pos_atr2 = pos("@@spec_attr:");
        if (pos("~@@") != 0 || pos("`@@") != 0) {
            const bool tilde = pos("~@@") != 0;
            const int mpos = tilde ? pos("~@@") : pos("`@@");
            const char mchr = mpos + 3 <= static_cast<int>(str.size()) ? str[mpos + 2] : ' ';
            str.erase(static_cast<size_t>(mpos - 1), 4);
            str.insert(static_cast<size_t>(mpos - 1), 1, ' ');
            show(atr1, atr2, atr3);
            ShowStr(dest, B(x + mpos - 1), y, std::string(1, mchr), tilde ? atr2 : atr3);
        } else if (temp_pos_atr1 != 0 && SameName("@@attr:??,??,??@@", Copy(str, temp_pos_atr1, 17))) {
            uint8_t new_atr1 = B(Str2num(Copy(str, temp_pos_atr1 + 7, 2), 16));
            if (new_atr1 == 0) new_atr1 = atr1;
            uint8_t new_atr2 = B(Str2num(Copy(str, temp_pos_atr1 + 10, 2), 16));
            if (new_atr2 == 0) new_atr2 = atr2;
            uint8_t new_atr3 = B(Str2num(Copy(str, temp_pos_atr1 + 13, 2), 16));
            if (new_atr3 == 0) new_atr3 = atr3;
            str.erase(static_cast<size_t>(temp_pos_atr1 - 1), 17);
            show(new_atr1, new_atr2, new_atr3);
        } else if (temp_pos_atr2 != 0 && SameName("@@spec_attr:??@@", Copy(str, temp_pos_atr2, 16))) {
            const uint8_t temp2 = B(Str2num(Copy(str, temp_pos_atr2 + 12, 2), 16));
            const uint8_t new_atr3 = (temp2 == 0 || spec_attr_table[temp2] == 0) ? atr3 : spec_attr_table[temp2];
            str.erase(static_cast<size_t>(temp_pos_atr2 - 1), 16);
            show(atr1, atr2, new_atr3);
        } else {
            show(atr1, atr2, atr3);
        }
    };

    ScreenMemCopy(screen_ptr, screen_backup);
    centered_frame_vdest = screen_ptr;

    spec_attr_table[1] = B(main_behavior << 4 & 0xf0);
    spec_attr_table[2] = B(main_hi_stat_line << 4 & 0xf0);

    HideCursor();
    const uint8_t page_len = B(work_maxln - 5);
    uint8_t xstart, ystart;
    centered_frame(xstart, ystart, 77, B(page_len + 2), " HELP ", help_background + help_border, help_background + help_title,
                   frame_double);

    const int last_page = HELP_LINES - page_len;
    const auto is_topic = [](int index) { return Copy(help_line(index), 1, 6) == "@topic"; };
    const auto name_of = [](int index) {
        const std::string s = help_line(index);
        return Copy(s, 8, static_cast<int>(s.size()) - 7);
    };
    int page = 1;
    while (page <= last_page && !(is_topic(page) && name_of(page) == topic)) ++page;
    if (page < 1) page = 1;
    if (page > last_page) page = last_page;

    uint16_t fkey;
    do {
        std::string temps = page > 1 ? "\x18" : "-";
        temps += page < last_page ? "\x19" : "-";
        ShowCStr(screen_ptr, B(xstart + 1 + 74 - static_cast<int>(temps.size())), B(ystart + page_len + 2), "[~" + temps + "~]",
                 help_background + help_border, help_background + help_indicators);

        uint8_t ypos = B(ystart + 1);
        for (int temp = page; ypos <= ystart + page_len + 1 && temp <= HELP_LINES; ++temp) {
            const std::string line = help_line(temp);
            const std::string head = Copy(line, 1, 6);
            if (head != "@topic" && head != "@input") {
                ListCStr(screen_ptr, B(xstart + 2), ypos, line, help_background + help_text, help_background + help_keys,
                         Copy(line, 1, 3) != " \xBA " ? help_background + help_hi_text : help_background + help_topic);
                ++ypos;
                continue;
            }
            // 設定で変わる行 (use_H_for_B は使わないため key_comment は常に B)
            const std::string name = name_of(temp);
            const char* input = nullptr;
            if (name == "key_comment") input = key_comment_B;
            else if (name == "shift_f5") input = !trace_by_default ? shift_f5_1 : shift_f5_2;
            else if (name == "shift_f8") input = !trace_by_default ? shift_f8_1 : shift_f8_2;
            else if (name == "shift_f9") input = !trace_by_default ? shift_f9_1 : shift_f9_2;
            else if (name == "alt_f8") input = !nosync_by_default ? alt_f8_1 : alt_f8_2;
            if (input != nullptr) {
                ListCStr(screen_ptr, B(xstart + 2), ypos, input, help_background + help_text, help_background + help_keys,
                         help_background + help_topic);
                ++ypos;
            }
        }

        fkey = getkey();
        switch (fkey) {
        case kUP:
            if (page > 1) --page;
            if (page > 1 && is_topic(page - 1)) --page;
            break;
        case kDOWN:
            if (is_topic(page) && page < last_page) ++page;
            if (page < last_page) ++page;
            break;
        case kPgUP:
            if (page > page_len) page -= page_len;
            else page = 1;
            if (page > 1 && is_topic(page - 1)) --page;
            break;
        case kPgDOWN:
            if (is_topic(page) && page < last_page) ++page;
            if (page + page_len <= last_page) page += page_len;
            else page = last_page;
            break;
        case kHOME: page = 1; break;
        case kEND: page = last_page; break;
        }
    } while (!(fkey == kENTER || fkey == kESC || _force_program_quit));

    // オリジナルどおり、戻すのは枠の下の線の 1 行上まで
    move_to_screen_data = screen_backup;
    move_to_screen_area[1] = xstart;
    move_to_screen_area[2] = ystart;
    move_to_screen_area[3] = B(xstart + 77 + 2);
    move_to_screen_area[4] = B(ystart + page_len + 1);
    move2screen();
}

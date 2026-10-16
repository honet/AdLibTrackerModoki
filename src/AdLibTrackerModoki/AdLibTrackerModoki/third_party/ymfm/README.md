# ymfm

- 作者: Aaron Giles
- 入手元: https://github.com/aaronsgiles/ymfm (commit 81aec25ccbb98f4873a255f7551ac4dadac59b4a)
- ライセンス: BSD 3-Clause (LICENSE 参照)

変更点: YMF262 (OPL3) に必要な `src/` 以下のファイルだけを取り込み、改行を CRLF にしたのみ。
`ymfm_pcm.*` は `ymfm_opl.h` が参照する (OPL4 用) ため、`ymfm_adpcm.*` は Y8950 用で `ymfm_opl.cpp` が参照するため同梱している。

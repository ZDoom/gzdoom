NP2kai において fmgen 008 へ行った改変． by AZO

１．ファイル名の変更． （fmgen_*.*）

２．fmgen.cpp Operator::MakeTable() のインクリメント修正．

[修正前]
*p++ = p[-512] / 2;

[修正後]
//*p++ = p[-512] / 2;
*p = p[-512] / 2;
p++;

３．各クラスのステートセーブ用構造体と、ステートセーブ／ロード実装

struct xxxData : ステートセーブ用構造体

xxx_DataSave() : ステートセーブ
xxx_DataLoad() : ステートロード

４．C言語用ラッパー fmgen_fmgwrap.cpp/.h

以上．

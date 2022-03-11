# pt2wdm 暫定対応β版

## 構築と動作確認

  ソリューションBonDriver_PTx+PTw.slnを構築する前に以下のファイルを配置する必要があります。

  - PTwCtrl/inc フォルダに以下のファイルを配置
    - EarthPtIf.h  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)
    - PtDrvIfLib.h  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)

  - PTwCtrl/lib フォルダに以下のファイルを配置
    - EarthIfLib.lib  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)
    - EarthIfLib64.lib  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)
    - PtDrvIfLib.lib  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)
    - PtDrvIfLib64.lib  ([pt2wdmドライバ](https://www.vector.co.jp/soft/winnt/hardware/se507005.html)に同封されているもの)

  ソリューションBonDriver_PTx+PTw.slnをコンパイルして出来上がったPTwCtrl.exeと以下のファイル群をアプリ側のBonDriverフォルダに配置して動作確認…
  ```
  BonDriver_PTw-S.dll ← pt2wdm BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T.dll ← pt2wdm 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-ST.ini ← pt2wdm用設定ファイル (BonDriver_PTx-ST.iniをリネームしたもの)
  PTwCtrl.exe ← pt2wdm 制御実行ファイル (PTwCtrl.vcxprojをコンパイルして出来上がったファイル)
  ```

  自動判別ではなく、IDで指定する場合は、次のように連番で記述してください。
  BonDriver_PTwで連番を利用する場合は以下の通りです。
  ```
  BonDriver_PTw-S0.dll ← BS/CS110 複合チューナーの最初のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T0.dll ← 地デジ 複合チューナーの最初のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-S1.dll ← BS/CS110 複合チューナーの最初のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T1.dll ← 地デジ 複合チューナーの最初のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-S2.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T2.dll ← 地デジ 複合チューナーの二枚目のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-S3.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T3.dll ← 地デジ 複合チューナーの二枚目のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-S4.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T4.dll ← 地デジ 複合チューナーの三枚目のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-S5.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T5.dll ← 地デジ 複合チューナーの三枚目のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  ```

  ***※ mod5.2 から、下記のような工程を施すことにより PTx の代替として PTw チューナーを容易にオープンすることも可能です。***

  以下のファイル群をアプリ側のBonDriverフォルダに配置…
  ```
  BonDriver_PTx-S.dll ← BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T.dll ← 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-ST.ini
  PTxCtrl.exe ← PT1/PT2/PT3 自動判別PT制御実行ファイル(※pt2wdmだけ使用の場合は無くても可)
  PTwCtrl.exe ← pt2wdm 制御実行ファイル (PTwCtrl.vcxprojをコンパイルして出来上がったファイル)
  ```
  そして、BonDriver_PTx-ST.ini ファイルの以下の部分を下記のように修正…
  ```
  [SET]
  ; xSparePTw=0 を xSparePTw=1 に変更
  xSparePTw=1
  ```
  すると、現在使用しているデバイスドライバが EARTHSOFT 純正であるか pt2wdm であるかに関わらず、適切なデバイスを自動判別して任意のチューナーを開くことが出来るようになります。
  EARTHSOFT純正ドライバからpt2wdmのドライバに対応したBonDriverに挿げ替える作業は、実質上記iniファイルの一か所を修正する作業だけで済むため、BonDriverの名前変更などの煩わしい一連の作業を省略できます。


  **※β版です。無保証( NO WARRANTY )です。テスト期間が短い為、潜在的なバグについては未知数です。**

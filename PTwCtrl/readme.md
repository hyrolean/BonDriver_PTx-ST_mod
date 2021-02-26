# pt2wdm 暫定対応α版

  PTwCtrl.vcxprojをコンパイルして出来上がったPTwCtrl.exeと以下のファイル群をアプリ側のBonDriverフォルダに配置して動作確認…
  ```
  BonDriver_PTw-S.dll ← BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-T.dll ← 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTw-ST.ini ← pt2wdm用設定ファイル (BonDriver_PTx-ST.iniをリネームしたもの)
  PTwCtrl.exe ← PTwCtrl.vcxprojをコンパイルして出来上がったファイル
  ```

  **※自分の環境ではうまく動かなかった(PT::Device::Open()でなぜか必ずコケる@pt2wdm1.1.0.0)ので、これからテコ入れするか放置するかなかったことにするかどうするか迷走中…**


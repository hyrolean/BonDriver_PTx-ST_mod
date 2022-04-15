

## ●オリジナルのBonDriver_PT3(人柱版4)およびBonDriver_PT-ST(人柱版3)との使用する上での相違点

### ■ ハイレゾリューションタイマー導入によるストリーム転送とチャンネルスキャンの高速化。

  Windows10 1803 以降でサポートされた高精度割込みタイマーを導入することにより、チャンネルスキャンのもたつきや、ストリーム転送のラグを極限まで抑えることを可能としました。

  高精度割込みタイマーを有効にするには、BonDriver_PTx-ST.ini の以下の項目を"1"に変更してください。
  ```
  [SET]
  ; UseHRTimer=0 を UseHRTimer=1 に変更
  UseHRTimer=1
  ```
- [mod7.3 BSAlmighty+DeltaHz+DefSpace+PTx+PTxScanS+SparePTw+UseHRTimer fixes](https://github.com/hyrolean/BonDriver_PTx-ST_mod/releases/tag/mod7.3)
(Described by 2022/04/15 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ PTxの代替としてのPTwチューナー差し替えの簡略化。

  以下の工程を追うことにより、PTwのBonDriverをPTxのBonDriverで簡単に差し替えすることが出来るようになりました。


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

- [mod6.1 BSAlmighty+DeltaHz+DefSpace+PTx+PTxScanS+SparePTw fixes](https://github.com/hyrolean/BonDriver_PTx-ST_mod/releases/tag/mod6.1)(***※[PTwCtrl.exe](https://github.com/hyrolean/BonDriver_PTx-ST_mod/tree/master/PTwCtrl)の構築が別途必要***)
    (Described by 2022/03/11 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ サテライト側の.ChSet.txtファイルを自動生成するプログラム PTxScanS.exe を追加。

  各々のBonDriverが存在するディレクトリにPTxScanS.exeを配置して実行し、指示に従うだけで容易にサテライト側の.ChSet.txtファイルを自動生成することを可能とする機能を追加しました。
  ```
  [PTxScanS.exe配置例]
  BonDriver_PTx-S0.dll ← BS/CS110 複合チューナーの最初のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S1.dll ← BS/CS110 複合チューナーの最初のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-ST.ini
  PTxCtrl.exe ← PT1/PT2/PT3 自動判別PT制御実行ファイル
  PTxScanS.exe ← BonDriver_PTx-S?.dll と同一ディレクトリに配置して実行すると、.ChSet.txtファイルを自動生成する
  ```
  ※ちなみに、スキャン結果を出力したファイルの名前をBonDriver_PTx-S.ChSet.txtに名前変更すると、すべてのサテライト用BonDriverで利用できるチャンネル設定ファイルとして扱うことが可能です。

  - [mod5.1 BSAlmighty+DeltaHz+DefSpace+PTx+PTxScanS fixes](https://github.com/hyrolean/BonDriver_PTx-ST_mod/releases/tag/mod5.1)
    (Described by 2021/05/10 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ PTCtrl.exe と PT3Ctrl.exe を PTxCtrl.exe に統合しました。

  PT制御実行ファイルを PTxCtrl.exe 一本に統合しました。
  PT1/PT2とPT3との同時運用には、いままでPTCtrl.exeとPT3Ctrl.exe両方の実行ファイルが必要でしたが、PTxCtrl.exeに両方の機能をまとめることにより、実質PTxCtrl.exe一本のみで運用することを可能としました。
  EDCBを利用する場合、最低以下の4ファイルをEDCBのBonDriverディレクトリに配置するだけでセットアップ完了となります。(PT3、PT1/PT2、両方のSDKのインストールは必要なく、最低どちらか一方のSDKをインストールしておくだけでもPTxCtrl.exeは問題なく動作します。)
  ```
  [EPGDataCap_BonのBonDriverフォルダ内の一例]
  BonDriver_PTx-S.dll ← BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T.dll ← 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-ST.ini
  PTxCtrl.exe ← PT1/PT2/PT3 自動判別PT制御実行ファイル
  ```

  - [mod4 BSAlmighty+DeltaHz+DefSpace+PTx fixes](https://github.com/hyrolean/BonDriver_PTx-ST_mod/releases/tag/mod4)
    (Described by 2021/04/05 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ BonDriver_PT-STとBonDriver_PT3-STを統合しました。

  BonDriverを統合し、IBonDriver3(Bonドライバインタフェース3)にも対応しました。
  今回、BonDriverを統合したことにより、たとえば、PT2とPT3をミックスして使っているような違うチューナーの種類をBonDriverで分別することによって発生していた煩わしいコストを削減できます。

  IBonDriver3に対応したことにより、たとえば、EDCBを利用する場合、最低以下の5ファイルをEDCBのBonDriverディレクトリに配置し、チャンネルスキャンした後に、EpgTimerの設定画面|基本設定|チューナー|の各々の**チューナー数**の項目を運用しているPT1/2/3の種類に関わらず、運用しているPTx全個体数(PT1/2/3を嵌めているPCIカード数)の2倍に調整すればインストール完了です。
  ```
  [EPGDataCap_BonのBonDriverフォルダ内の一例]
  BonDriver_PTx-S.dll ← BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T.dll ← 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-ST.ini
  PTCtrl.exe ← PT1/PT2 を最低でも一枚は運用している場合に配置。運用してない場合は配置不要。
  PT3Ctrl.exe ← PT3 を最低でも一枚は運用している場合に配置。運用してない場合は配置不要。
  ```
  あとは、順次使用するチューナーを自動判別してくれます。
  PT1/PT2/PT3などのチューナーの種類を意識する必要はなくなります。

  また、いままで通りの連番や固有名でも以前と変わりなくそのまま運用可能です。
  ```
  BonDriver_PT-S0.dll ← BS/CS110 PT1/PT2の最初のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT-T1.dll ← 地デジ PT1/PT2の最初のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT3-S2.dll ← BS/CS110 PT3の二枚目のチューナーのS側1番目のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT3-T3.dll ← 地デジ PT3の二枚目のチューナーのT側2番目のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT3-S.dll ← BS/CS110 PT3のS側ID自動判別チューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT-T.dll ← 地デジ PT1/PT2のT側ID自動判別チューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PT-ST.ini ← PT1/PT2用設定ファイル (BonDriver_PTx-ST.iniをリネームしたもの)
  BonDriver_PT3-ST.ini ← PT3用設定ファイル (BonDriver_PTx-ST.iniをリネームしたもの)
  ```

  PTxで連番を利用する場合は以下の通りです。
  ```
  BonDriver_PTx-S0.dll ← BS/CS110 複合チューナーの最初のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T0.dll ← 地デジ 複合チューナーの最初のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S1.dll ← BS/CS110 複合チューナーの最初のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T1.dll ← 地デジ 複合チューナーの最初のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S2.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T2.dll ← 地デジ 複合チューナーの二枚目のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S3.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T3.dll ← 地デジ 複合チューナーの二枚目のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S4.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T4.dll ← 地デジ 複合チューナーの三枚目のチューナーのT1端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-S5.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  BonDriver_PTx-T5.dll ← 地デジ 複合チューナーの三枚目のチューナーのT2端子のチューナー (BonDriver_PTx.dllをリネームしたもの)
  ```

  - [mod3.4 BSAlmighty+DeltaHz+DefSpace+PTx fixes](https://github.com/hyrolean/BonDriver_PTx-ST_mod/releases/tag/mod3.4)
    (Described by 2021/03/13 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

    **※PTx複合チューナーが PT3、PT1/PT2、どちらを優先的にチューナーを開くかは、BonDriver_PTx-ST.ini の xFirstPT3 項目の値を変えることによって挙動を変更出来ます。**

### ■ チャンネルファイル.ChSet.txtファイルに -/+ を値欄に記述することにより、直近に登録したチャンネルの値を参照できるようになりました。

  ```
  - … 直近に登録したチャンネルの値と同値
  + … 直近に登録したチャンネルの値に+1加算したもの
  ```
  この機能を上手く使いこなすことにより、チャンネルファイル.ChSet.txtを編集する際の通し番号を記述することにより発生する煩わしいコストを削減出来ます。
  - [mod2 BSAlmighty+DeltaHz+DefSpace fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod2)
    (Described by 2021/02/12 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ 既定のチャンネル情報を編集することにより、チャンネル情報構築の簡略化に対応。

  チャンネルファイル.ChSet.txtが読み込まれなかった場合は、BonDriver_PT3-ST.iniのDefSpaceセクションの記述内容に応じてチャンネル情報を自動構築します。
  特殊な受信環境でなければ、.ChSet.txtファイルの複雑怪奇な編集作業を実質まったく行う必要がなくなり、チャンネル情報書き換えによる煩わしいコストを省けます。
  - [mod2α2 BSAlmighty+DeltaHz+DefSpace fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod2_alpha2)
    (Described by 2021/02/04 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

    **※既定のチャンネル情報を有効にするには、いままで利用していた.ChSet.txtファイルを抹消し、代替に添付のBonDriver_PT3-ST.iniファイルをBonDriverと同じフォルダに置いてDefSpaceセクションをチューナー環境に応じて適切に編集してみて下さい。**

### ■ ヘタってきたチューナー用に周波数を微調整する機能を追加しました。

  チャンネルファイルの"PTxとしてのチャンネル"部分にch番号±δで調整できます。
  (単位は、1/7MHz。実際の周波数より2MHz上方修正する場合は、ch番号+14と書く。)
  たとえば、CATVのC37チャンネルの周波数を1/7MHz下方修正する場合は、添付のBonDriver_PT3-T.ChSet.txtの81行目を下記の様に記述することにより、
  ```
  C37Ch	1	24	36-1	0
  ```
  実際のC37チャンネルの周波数より1/7MHz下方(-143kHz)修正されます。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ BonDriverのdllファイル名と同じ名前の拡張子.ChSet.txtファイルが存在する場合、そちらを先に優先してチャンネルファイルとして読み込む仕様に変更しました。

  たとえば、
  ```
  BonDriver_PT3-T.ChSet.txt
  BonDriver_PT3-T1.ChSet.txt
  BonDriver_PT3-T1.dll
  ```
  の３ファイルが同一フォルダ上に存在する状態でBonDriver_PT3-T1.dllがアプリケーション側から読み込まれた場合は、BonDriver_PT3-T.ChSet.txtは読み込まれずに代
  替にBonDriver_PT3-T1.ChSet.txtがチャンネルファイルとして読み込まれます。
  ヘタったチューナーの周波数を微調整したい場合や、地上波とCATVに分けたい場合などに個別のチャンネルファイルとして取扱う目的で利用できるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

### ■ BonDriver_PT3-S.ChSet.txtのTSID項目に0～7の値を指定するとチューナーからTSIDを抜き取って下位3ビットがその値と一致するTSIDを持つストリームを自動選択します。

  トランスポンダの再編などによりチャンネル情報が変更された場合にチャンネルファイルのTSIDを実質一々書き換える必要がなくなりアプリケーション側のチャンネルスキャンだけで事が足りるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/07 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

    **※機能を有効にするには、添付のBonDriver_PT3-S.ChSet.txtを差し替えてください。また、添付のBonDriver_PT3-S.ChSet.txtを差し替えた結果、EDCBでのチャンネルのスキャン時に抜けが発生する場合は、下記のようにEDCBのディレクトリ上に存在する設定ファイルBonCtrl.iniのServiceChkTimeOutの値を15くらいに上げて再スキャンしてみてください。**
    ```
    ;チャンネルスキャン時の動作設定
    [CHSCAN]
    ServiceChkTimeOut=15
    ```

# BonDriver_PT3-ST MOD

## オリジナルのBonDriver_PT3(人柱版4)との使用する上での相違点

- 既定のチャンネル情報を編集することにより、チャンネル情報構築の簡略化に対応。
  チャンネルファイル.ChSet.txtが読み込まれなかった場合は、BonDriver_PT3-ST.iniのDefSpaceセクションの記述内容に応じてチャンネル情報を自動構築します。
  特殊な受信環境でなければ、.ChSet.txtファイルの複雑怪奇な編集作業を実質まったく行う必要がなくなり、チャンネル情報書き換えによる煩わしいコストを省けます。
  - [mod2α2 BSAlmighty+DeltaHz+DefSpace fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod2_alpha2)
    (Described by 2021/02/04 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
  #### ※既定のチャンネル情報を有効にするには、いままで利用していた.ChSet.txtファイルを抹消し、代替に添付のBonDriver_PT3-ST.iniファイルをBonDriverと同じフォルダに置いてDefSpaceセクションをチューナー環境に応じて適切に編集してみて下さい。
  
- ヘタってきたチューナー用に周波数を微調整する機能を追加しました。
  チャンネルファイルの"PTxとしてのチャンネル"部分にch番号±δで調整できます。
  (単位は、1/7MHz。実際の周波数より2MHz上方修正する場合は、ch番号+14と書く。)
  たとえば、CATVのC37チャンネルの周波数を1/7MHz下方修正する場合は、添付のBonDriver_PT3-T.ChSet.txtの81行目を下記の様に記述することにより、

  C37Ch	1	24	36-1	0

  実際のC37チャンネルの周波数より1/7MHz下方(-143kHz)修正されます。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

- BonDriverのdllファイル名と同じ名前の拡張子.ChSet.txtファイルが存在する場合、そちらを先に優先してチャンネルファイルとして読み込む仕様に変更しました。
  たとえば、

  BonDriver_PT3-T.ChSet.txt
  BonDriver_PT3-T1.ChSet.txt
  BonDriver_PT3-T1.dll

  の３ファイルが同一フォルダ上に存在する状態でBonDriver_PT3-T1.dllがアプリケーション側から読み込まれた場合は、BonDriver_PT3-T.ChSet.txtは読み込まれずに代
  替にBonDriver_PT3-T1.ChSet.txtがチャンネルファイルとして読み込まれます。
  ヘタったチューナーの周波数を微調整したい場合や、地上波とCATVに分けたい場合などに個別のチャンネルファイルとして取扱う目的で利用できるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
    
- BonDriver_PT3-S.ChSet.txtのTSID項目に0～7の値を指定するとチューナーからTSIDを抜き取って下位3ビットがその値と一致するTSIDを持つストリームを自動選択します。
  トランスポンダの再編などによりチャンネル情報が変更された場合にチャンネルファイルのTSIDを実質一々書き換える必要がなくなりアプリケーション側のチャンネルスキャンだけで事が足りるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT3-ST_mod/releases/tag/mod)
    (Described by 2020/01/07 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
  #### ※機能を有効にするには、添付のBonDriver_PT3-S.ChSet.txtを差し替えてください。

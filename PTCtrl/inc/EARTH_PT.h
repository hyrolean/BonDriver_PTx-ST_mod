// ========================================================================================
//	EARTH_PT.h
//	version 2.0 (2009.09.30)
// ========================================================================================

#ifndef _EARTH_PT_H
#define _EARTH_PT_H

#include "Prefix.h"

namespace EARTH {
namespace PT {
	class Device;
	class Device2;

	// +------------+
	// | バスクラス |
	// +------------+
	// バス上のデバイスを列挙します。またデバイスインスタンスを生成します。
	class Bus {
	public:
		// [機能] Bus インスタンスを生成
		// [説明] ドライバ名 は "windrvr6_EARTHSOFT_PT2", "windrvr6_EARTHSOFT_PT1" の順に試行します。
		// [返値] STATUS_INVALID_PARAM_ERROR → 引数 bus が NULL
		//        STATUS_WDAPI_LOAD_ERROR    → LoadLibrary(TEXT("wdapi1002.dll")) の返値が NULL
		//        STATUS_WD_DriverName_ERROR → WD_DriverName() の返値が NULL
		//        STATUS_WD_Open_ERROR       → WD_Open() でエラーが発生
		//        STATUS_WD_Version_ERROR    → WD_Version() でエラーが発生。またはバージョンが 10.0.2 でない
		//        STATUS_WD_License_ERROR    → WD_License() でエラーが発生
		typedef status (*NewBusFunction)(Bus **bus);

		// [機能] インスタンスを解放
		// [説明] delete は使えません。この関数を呼び出してください。
		// [返値] STATUS_ALL_DEVICES_MUST_BE_DELETED_ERROR → NewDevice() で生成されたデバイスが全て Delete() されていない
		virtual status Delete() = 0;

		// [機能] ソフトウェアバージョンを取得
		// [説明] バージョンが 2.0 の場合、値は 0x200 になります。
		//        上位 24 ビットが同じであればバイナリ互換になるように努めますので、
		//        ((version >> 8) == 2) であるかをチェックしてください。 
		// [返値] STATUS_INVALID_PARAM_ERROR → 引数 version が NULL
		virtual status GetVersion(uint *version) const = 0;

		// デバイス情報
		struct DeviceInfo {
			uint Bus;			// PCI バス番号
			uint Slot;			// PCI デバイス番号
			uint Function;		// PCI ファンクション番号 (正常動作時は必ず 0 になります)
			uint PTn;			// 品番 (PT1:1 PT2:2)
			uint BadBitCount;	// PCI データバスのビット化け数
		};

		// [機能] 認識されているデバイスのリストを取得
		// [説明] PCI バスをスキャンして以下の条件を全て満たすデバイスをリストアップします。
		//        (PT1) ベンダID: 0x10ee / デバイスID: 0x211a / サブシステムベンダID: ~0x10ee / サブシステムID: ~0x211a
		//        (PT2) ベンダID: 0x10ee / デバイスID: 0x222a / サブシステムベンダID: ~0x10ee / サブシステムID: ~0x222a
		//        
		//        スロットとボード端子の接触が悪い場合、これらの ID にビット化けが生じることがあります。
		//        このような状況でもデバイスを検出できるように、maxBadBitCount でビット化けの許容上限を指定することができます。
		//        64ビット(16ビット×4) の各ビットを比較し、相違ビット数が maxBadBitCount 以下のデバイスをリストアップします。
		//        
		//        deviceInfoCount は呼び出し前にデバイスの上限数を指定します。呼出し後は見つかったデバイス数を返します。
		//        maxBadBitCount は 3 以下の値を指定します。
		//        DeviceInfo::BadBitCount が 0 でないデバイスを Device::Open() することはできません。
		// [返値] STATUS_INVALID_PARAM_ERROR   → 引数 deviceInfoPtr, deviceInfoCount のいずれかが NULL
		//                                        または引数 maxBadBitCount が 3 より大きい
		//        STATUS_WD_PciScanCards_ERROR → WD_PciScanCards でエラーが発生
		virtual status Scan(DeviceInfo *deviceInfoPtr, uint *deviceInfoCount, uint maxBadBitCount = 0) = 0;

		// [機能] デバイスインスタンスを生成する
		// [説明] デバイスリソースの排他チェックはこの関数では行われません。Device::Open() で行われます。
		//        Device2 は非公開インターフェースです。device2 は NULL にしてください。
		// [返値] STATUS_INVALID_PARAM_ERROR → 引数 deviceInfoPtr, device のいずれかが NULL
		//                                      または引数 device2 が NULL でない
		virtual status NewDevice(const DeviceInfo *deviceInfoPtr, Device **device, Device2 **device2 = NULL) = 0;

	protected:
		virtual ~Bus() {}
	};

	// +----------------+
	// | デバイスクラス |
	// +----------------+
	// このインスタンス 1 つがボード 1 枚に対応しています。
	class Device {
	public:
		// ----
		// 解放
		// ----

		// [機能] インスタンスを解放
		// [説明] delete は使えません。この関数を呼び出してください。
		// [返値] STATUS_DEVICE_MUST_BE_CLOSED_ERROR → デバイスがオープン状態なのでインスタンスを解放できない
		virtual status Delete() = 0;
		
		// ------------------
		// オープン・クローズ
		// ------------------

		// [機能] デバイスのオープン
		// [説明] 以下の手順に沿って行われます。
		//        1. 既にデバイスがオープンされていないかを確認する。
		//        2. リビジョンID (コンフィギュレーション空間 アドレス 0x08) が 0x01 であるかを調べる。
		//        3. コンフィギュレーション空間のデバイス固有レジスタ領域を使って PCI バスでのビット化けがないかを確認する。
		//        4. この SDK で制御が可能な FPGA 回路のバージョンであるかを確認する。
		// [返値] STATUS_DEVICE_IS_ALREADY_OPEN_ERROR   → デバイスは既にオープンされている
		//        STATUS_WD_PciGetCardInfo_ERROR        → WD_PciGetCardInfo() でエラーが発生
		//        STATUS_WD_PciGetCardInfo_Bus_ERROR    → バス情報数が 1 以外
		//        STATUS_WD_PciGetCardInfo_Memory_ERROR → メモリ情報数が 1 以外
		//        STATUS_WD_CardRegister_ERROR          → WD_CardRegister() でエラーが発生
		//        STATUS_WD_PciConfigDump_ERROR         → WD_PciConfigDump() でエラーが発生
		//        STATUS_CONFIG_REVISION_ERROR          → リビジョンID が 0x01 でない
		//        STATUS_PCI_BUS_ERROR                  → PCI バスでのビット化けが発生
		//        STATUS_PCI_BASE_ADDRESS_ERROR         → コンフィギュレーション空間の BaseAddress0 が 0
		//        STATUS_FPGA_VERSION_ERROR             → 対応していない FPGA 回路バージョン
		//        STATUS_WD_CardCleanupSetup_ERROR      → WD_CardCleanupSetup() でエラーが発生
		//        STATUS_DCM_LOCK_TIMEOUT_ERROR         → DCM が一定時間経過後もロック状態にならない
		//        STATUS_DCM_SHIFT_TIMEOUT_ERROR        → DCM のフェーズシフトが一定時間経過後も完了しない
		virtual status Open() = 0;

		// [機能] デバイスのクローズ
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		virtual status Close() = 0;

		// --------------------------------------
		// PCI クロックカウンタ・レイテンシタイマ
		// --------------------------------------

		// [機能] PCI クロックカウンタを取得
		// [説明] カウンタ長は 32 ビットです。0xffffffff の次は 0 になります。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 counter が NULL
		virtual status GetPciClockCounter(uint *counter) = 0;

		// [機能] PCI レイテンシタイマ値の設定・取得
		// [説明] 下位 3 ビットは実装されていないため、取得した値は 8 の倍数になります。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 latencyTimer が NULL (GetPciLatencyTimer のみ)
		//        STATUS_WD_PciConfigDump_ERROR   → WD_PciConfigDump() でエラーが発生
		virtual status SetPciLatencyTimer(byte  latencyTimer)       = 0;
		virtual status GetPciLatencyTimer(byte *latencyTimer) const = 0;

		// ------------
		// 電源・初期化
		// ------------

		enum LnbPower {
			LNB_POWER_OFF,	// オフ
			LNB_POWER_15V,	// 15V 出力
			LNB_POWER_11V	// 11V 出力 (正確には PCI スロットの +12V から 0.6V 程度を引いた値)
		};

		// [機能] LNB 電源制御
		// [説明] チューナーの電源とは独立に制御可能です。デフォルト値は LNB_POWER_OFF です。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 lnbPower が NULL (GetLnbPower のみ)
		virtual status SetLnbPower(LnbPower  lnbPower)       = 0;
		virtual status GetLnbPower(LnbPower *lnbPower) const = 0;

		// [機能] デバイスをクローズ（異常終了にともなうクローズを含む）時の LNB 電源制御
		// [説明] デフォルト値は LNB_POWER_OFF です。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR  → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR       → 引数 lnbPower が NULL (GetLnbPowerWhenClose のみ)
		//        STATUS_WD_CardCleanupSetup_ERROR → WD_CardCleanupSetup() でエラーが発生 (SetLnbPowerWhenClose のみ)
		virtual status SetLnbPowerWhenClose(LnbPower  lnbPower)       = 0;
		virtual status GetLnbPowerWhenClose(LnbPower *lnbPower) const = 0;

		// [機能] チューナー電源・ハードウェアリセット制御
		// [説明] TUNER_POWER_ON_RESET_ENABLE から TUNER_POWER_ON_RESET_DISABLE の遷移には最低 15ms の待ち時間が必要です。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tunerPowerReset が NULL (GetTunerPowerReset のみ)
		enum TunerPowerReset {				// 電源／ハードウェアリセット
			TUNER_POWER_OFF,				// オフ／イネーブル
			TUNER_POWER_ON_RESET_ENABLE,	// オン／イネーブル
			TUNER_POWER_ON_RESET_DISABLE	// オン／ディセーブル
		};
		virtual status SetTunerPowerReset(TunerPowerReset  tunerPowerReset)       = 0;
		virtual status GetTunerPowerReset(TunerPowerReset *tunerPowerReset) const = 0;

		// [機能] チューナー初期化
		// [説明] SetTunerPowerReset(TUNER_POWER_ON_RESET_DISABLE) から最低 1μs 経過後に 1 回だけ呼び出します。 
		//        引数 tuner は 0 基底のチューナー番号です。
		//        PT1に限り PLL を初期化するために、内部的に SetFrequency(tuner, ISDB_S, 0) と SetFrequency(tuner, ISDB_T, 63) が
		//        実行されます。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きい
		//        STATUS_POWER_RESET_ERROR        → SetTunerPowerReset() で TUNER_POWER_ON_RESET_DISABLE 以外が指定されている
		//        STATUS_I2C_ERROR                → 復調IC からリードしたレジスタ値が異常
		virtual status InitTuner(uint tuner) = 0;

		// 受信方式
		enum ISDB {
			ISDB_S,
			ISDB_T,

			ISDB_COUNT
		};

		// [機能] チューナー省電力制御
		// [説明] チューナー初期後は省電力オンになっていますので、受信前に省電力をオフにする必要があります。
		//        (PT1) 復調IC のみが対象です。チューナーユニット内の他の回路は SetTunerPowerReset(TUNER_POWER_OFF) としない限り、
		//              電力を消費し続けます。復調IC の消費電力はチューナーモジュールの 15% です。
		//        (PT2) RFフロントエンド回路全体と復調IC が対象です。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		//                                           引数 sleep が NULL (GetTunerSleep のみ)
		virtual status SetTunerSleep(uint tuner, ISDB isdb, bool  sleep)       = 0;
		virtual status GetTunerSleep(uint tuner, ISDB isdb, bool *sleep) const = 0;

		// ----------
		// 局発周波数
		// ----------

		// [機能] 局発周波数の制御
		// [説明] offset で周波数の調整が可能です。単位は ISDB-S の場合は 1MHz、ISDB-T の場合は 1/7MHz です。
		//        例えば、C24 を標準より 2MHz 高い周波数に設定するには SetFrequency(tuner, ISDB_T, 23, 7*2) とします。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		//                                           引数 channel が NULL (GetFrequency のみ)
		//        STATUS_TUNER_IS_SLEEP_ERROR     → チューナーが省電力状態のため設定不可 (SetFrequency のみ)
		virtual status SetFrequency(uint tuner, ISDB isdb, uint  channel, int  offset = 0)       = 0;
		virtual status GetFrequency(uint tuner, ISDB isdb, uint *channel, int *offset = 0) const = 0;

		// (ISDB-S)
		// PLL 周波数ステップが 1MHz のため、実際に設定される周波数は f' になります。
		// +----+------+---------+---------+ +----+------+---------+---------+ +----+------+---------+---------+
		// | ch | TP # | f (MHz) | f'(MHz) | | ch | TP # | f (MHz) | f'(MHz) | | ch | TP # | f (MHz) | f'(MHz) |
		// +----+------+---------+---------+ +----+------+---------+---------+ +----+------+---------+---------+
		// |  0 | BS 1 | 1049.48 | 1049.00 | | 12 | ND 2 | 1613.00 | (同左)  | | 24 | ND 1 | 1593.00 | (同左)  |
		// |  1 | BS 3 | 1087.84 | 1088.00 | | 13 | ND 4 | 1653.00 | (同左)  | | 25 | ND 3 | 1633.00 | (同左)  |
		// |  2 | BS 5 | 1126.20 | 1126.00 | | 14 | ND 6 | 1693.00 | (同左)  | | 26 | ND 5 | 1673.00 | (同左)  |
		// |  3 | BS 7 | 1164.56 | 1165.00 | | 15 | ND 8 | 1733.00 | (同左)  | | 27 | ND 7 | 1713.00 | (同左)  |
		// |  4 | BS 9 | 1202.92 | 1203.00 | | 16 | ND10 | 1773.00 | (同左)  | | 28 | ND 9 | 1753.00 | (同左)  |
		// |  5 | BS11 | 1241.28 | 1241.00 | | 17 | ND12 | 1813.00 | (同左)  | | 29 | ND11 | 1793.00 | (同左)  |
		// |  6 | BS13 | 1279.64 | 1280.00 | | 18 | ND14 | 1853.00 | (同左)  | | 30 | ND13 | 1833.00 | (同左)  |
		// |  7 | BS15 | 1318.00 | (同左)  | | 19 | ND16 | 1893.00 | (同左)  | | 31 | ND15 | 1873.00 | (同左)  |
		// |  8 | BS17 | 1356.36 | 1356.00 | | 20 | ND18 | 1933.00 | (同左)  | | 32 | ND17 | 1913.00 | (同左)  |
		// |  9 | BS19 | 1394.72 | 1395.00 | | 21 | ND20 | 1973.00 | (同左)  | | 33 | ND19 | 1953.00 | (同左)  |
		// | 10 | BS21 | 1433.08 | 1433.00 | | 22 | ND22 | 2013.00 | (同左)  | | 34 | ND21 | 1993.00 | (同左)  |
		// | 11 | BS23 | 1471.44 | 1471.00 | | 23 | ND24 | 2053.00 | (同左)  | | 35 | ND23 | 2033.00 | (同左)  |
		// +----+------+---------+---------+ +----+------+---------+---------+ +----+------+---------+---------+
		// 
		// (ISDB-T)
		// +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+
		// | ch. | Ch. | f (MHz) | | ch. | Ch. | f (MHz) | | ch. | Ch. | f (MHz) | | ch. | Ch. | f (MHz) | | ch. | Ch. | f (MHz) |
		// +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+
		// |   0 |   1 |  93+1/7 | |  23 | C24 | 231+1/7 | |  46 | C47 | 369+1/7 | |  69 |  19 | 509+1/7 | |  92 |  42 | 647+1/7 |
		// |   1 |   2 |  99+1/7 | |  24 | C25 | 237+1/7 | |  47 | C48 | 375+1/7 | |  70 |  20 | 515+1/7 | |  93 |  43 | 653+1/7 |
		// |   2 |   3 | 105+1/7 | |  25 | C26 | 243+1/7 | |  48 | C49 | 381+1/7 | |  71 |  21 | 521+1/7 | |  94 |  44 | 659+1/7 |
		// |   3 | C13 | 111+1/7 | |  26 | C27 | 249+1/7 | |  49 | C50 | 387+1/7 | |  72 |  22 | 527+1/7 | |  95 |  45 | 665+1/7 |
		// |   4 | C14 | 117+1/7 | |  27 | C28 | 255+1/7 | |  50 | C51 | 393+1/7 | |  73 |  23 | 533+1/7 | |  96 |  46 | 671+1/7 |
		// |   5 | C15 | 123+1/7 | |  28 | C29 | 261+1/7 | |  51 | C52 | 399+1/7 | |  74 |  24 | 539+1/7 | |  97 |  47 | 677+1/7 |
		// |   6 | C16 | 129+1/7 | |  29 | C30 | 267+1/7 | |  52 | C53 | 405+1/7 | |  75 |  25 | 545+1/7 | |  98 |  48 | 683+1/7 |
		// |   7 | C17 | 135+1/7 | |  30 | C31 | 273+1/7 | |  53 | C54 | 411+1/7 | |  76 |  26 | 551+1/7 | |  99 |  49 | 689+1/7 |
		// |   8 | C18 | 141+1/7 | |  31 | C32 | 279+1/7 | |  54 | C55 | 417+1/7 | |  77 |  27 | 557+1/7 | | 100 |  50 | 695+1/7 |
		// |   9 | C19 | 147+1/7 | |  32 | C33 | 285+1/7 | |  55 | C56 | 423+1/7 | |  78 |  28 | 563+1/7 | | 101 |  51 | 701+1/7 |
		// |  10 | C20 | 153+1/7 | |  33 | C34 | 291+1/7 | |  56 | C57 | 429+1/7 | |  79 |  29 | 569+1/7 | | 102 |  52 | 707+1/7 |
		// |  11 | C21 | 159+1/7 | |  34 | C35 | 297+1/7 | |  57 | C58 | 435+1/7 | |  80 |  30 | 575+1/7 | | 103 |  53 | 713+1/7 |
		// |  12 | C22 | 167+1/7 | |  35 | C36 | 303+1/7 | |  58 | C59 | 441+1/7 | |  81 |  31 | 581+1/7 | | 104 |  54 | 719+1/7 |
		// |  13 |   4 | 173+1/7 | |  36 | C37 | 309+1/7 | |  59 | C60 | 447+1/7 | |  82 |  32 | 587+1/7 | | 105 |  55 | 725+1/7 |
		// |  14 |   5 | 179+1/7 | |  37 | C38 | 315+1/7 | |  60 | C61 | 453+1/7 | |  83 |  33 | 593+1/7 | | 106 |  56 | 731+1/7 |
		// |  15 |   6 | 185+1/7 | |  38 | C39 | 321+1/7 | |  61 | C62 | 459+1/7 | |  84 |  34 | 599+1/7 | | 107 |  57 | 737+1/7 |
		// |  16 |   7 | 191+1/7 | |  39 | C40 | 327+1/7 | |  62 | C63 | 465+1/7 | |  85 |  35 | 605+1/7 | | 108 |  58 | 743+1/7 |
		// |  17 |   8 | 195+1/7 | |  40 | C41 | 333+1/7 | |  63 |  13 | 473+1/7 | |  86 |  36 | 611+1/7 | | 109 |  59 | 749+1/7 |
		// |  18 |   9 | 201+1/7 | |  41 | C42 | 339+1/7 | |  64 |  14 | 479+1/7 | |  87 |  37 | 617+1/7 | | 110 |  60 | 755+1/7 |
		// |  19 |  10 | 207+1/7 | |  42 | C43 | 345+1/7 | |  65 |  15 | 485+1/7 | |  88 |  38 | 623+1/7 | | 111 |  61 | 761+1/7 |
		// |  20 |  11 | 213+1/7 | |  43 | C44 | 351+1/7 | |  66 |  16 | 491+1/7 | |  89 |  39 | 629+1/7 | | 112 |  62 | 767+1/7 |
		// |  21 |  12 | 219+1/7 | |  44 | C45 | 357+1/7 | |  67 |  17 | 497+1/7 | |  90 |  40 | 635+1/7 | +-----+-----+---------+
		// |  22 | C23 | 225+1/7 | |  45 | C46 | 363+1/7 | |  68 |  18 | 503+1/7 | |  91 |  41 | 641+1/7 |
		// +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+ +-----+-----+---------+
		// 
		// C24～C27 は、ケーブルテレビ局により下記の周波数で送信されている場合があります。
		// +-----+---------+
		// | Ch. | f (MHz) |
		// +-----+---------+
		// | C24 | 233+1/7 |
		// | C25 | 239+1/7 |
		// | C26 | 245+1/7 |
		// | C27 | 251+1/7 |
		// +-----+---------+

		// ----------
		// 周波数誤差
		// ----------

		// [機能] 周波数誤差を取得
		// [説明] 値の意味は次の通りです。
		//        クロック周波数誤差: clock/100 (ppm)
		//        キャリア周波数誤差: carrier (Hz)
		//        放送波の周波数精度は十分に高い仮定すると、誤差が発生する要素として以下のようなものが考えられます。
		//        (ISDB-S) LNB での周波数変換精度 / 衛星側 PLL-IC に接続されている振動子の精度 / 復調 IC に接続されている振動子の精度
		//        (ISDB-T) 地上側 PLL-IC に接続されている振動子の精度 / 復調 IC に接続されている振動子の精度
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		//                                           引数 clock, carrier のいずれかが NULL
		//        STATUS_TUNER_IS_SLEEP_ERROR     → チューナーが省電力状態
		virtual status GetFrequencyOffset(uint tuner, ISDB isdb, int *clock, int *carrier) = 0;

		// --------
		// C/N・AGC
		// --------

		// [機能] C/N と AGC を取得
		// [説明] C/N は低レイテンシで測定できるため、アンテナの向きを調整するのに便利です。
		//        値の意味は次の通りです。
		//        C/N                : cn100/100 (dB)
		//        現在の AGC 値      : currentAgc
		//        利得最大時の AGC 値: maxAgc
		//        currentAgc の範囲は 0 から maxAgc までです。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		//                                           引数 cn100, currentAgc, maxAgc のいずれかが NULL
		//        STATUS_TUNER_IS_SLEEP_ERROR     → チューナーが省電力状態
		virtual status GetCnAgc(uint tuner, ISDB isdb, uint *cn100, uint *currentAgc, uint *maxAgc) = 0;

		// -------------------
		// TS-ID (ISDB-S のみ)
		// -------------------

		// [機能] TS-ID を設定
		// [説明] 設定値が復調IC の動作に反映されるまで時間が掛かります。
		//        GetLayerS() を呼び出す前に、GetIdS() を使って切り替えが完了したことを確認してください。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きい
		virtual status SetIdS(uint tuner, uint id) = 0;

		// [機能] 現在処理中の TS-ID を取得
		// [説明] GetLayerS() で取得できるレイヤ情報は、この関数で示される TS-ID のものになります。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きい。または引数 id が NULL
		virtual status GetIdS(uint tuner, uint *id) = 0;

		// ------------
		// エラーレート
		// ------------

		// 階層インデックス
		enum LayerIndex {
			// ISDB-S
			LAYER_INDEX_L = 0,	// 低階層
			LAYER_INDEX_H,		// 高階層

			// ISDB-T
			LAYER_INDEX_A = 0,	// A 階層
			LAYER_INDEX_B,		// B 階層
			LAYER_INDEX_C		// C 階層
		};

		// 階層数
		enum LayerCount {
			// ISDB-S
			LAYER_COUNT_S = LAYER_INDEX_H + 1,

			// ISDB-T
			LAYER_COUNT_T = LAYER_INDEX_C + 1
		};

		// エラーレート
		struct ErrorRate {
			uint Numerator, Denominator;
		};

		// [機能] リードソロモン復号で訂正されたエラーレートを取得
		// [説明] 測定に時間が掛かりますが、受信品質を正確に把握するには C/N ではなくこのエラーレートを参考にしてください。
		//        ひとつの目安として 2×10^-4 以下であれば、リードソロモン復号後にほぼエラーフリーになるといわれています。
		//        エラーレートの集計単位は次の通りです。
		//        ISDB-S: 1024 フレーム
		//        ISDB-T: 32 フレーム (モード 1,2) / 8 フレーム (モード 3)
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner, isdb, layerIndex が範囲外。または errorRate が NULL
		virtual status GetCorrectedErrorRate(uint tuner, ISDB isdb, LayerIndex layerIndex, ErrorRate *errorRate) = 0;

		// [機能] リードソロモン復号で訂正されたエラーレートを計算するためのエラーカウンタを初期化
		// [説明] 全階層のカウンタを初期化します。特定の階層のカウンタをリセットすることはできません。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		virtual status ResetCorrectedErrorCount(uint tuner, ISDB isdb) = 0;

		// [機能] リードソロモン復号で訂正しきれなかった TS パケット数を取得
		// [説明] 下位24ビットのみ有効です（回路規模を小さくするため回路番号01 にてビット数を縮小）。
		//        0x??ffffff の次は 0x??000000 になります。
		//        TS パケットの 2nd Byte MSB を数えても同じ数値になります。
		//        このカウンタは DMA 転送開始時に初期化されます。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外。または引数 count が NULL
		virtual status GetErrorCount(uint tuner, ISDB isdb, uint *count) = 0;

		// --------------------------
		// TMCC・レイヤー・ロック判定
		// --------------------------

		// ISDB-S TMCC 情報
		// (参考) STD-B20 2.9 TMCC情報の構成 ～ 2.11 TMCC情報の更新
		struct TmccS {
			uint Indicator;		// 変更指示 (5ビット)
			uint Mode[4];		// 伝送モードn (4ビット)
			uint Slot[4];		// 伝送モードnへの割当スロット数 (6ビット)
								// [相対TS／スロット情報は取得できません]
			uint Id[8];			// 相対TS番号nに対するTS ID (16ビット)
			uint Emergency;		// 起動制御信号 (1ビット)
			uint UpLink;		// アップリンク制御情報 (4ビット)
			uint ExtFlag;		// 拡張フラグ (1ビット)
			uint ExtData[2];	// 拡張領域 (61ビット)
		};

		// [機能] ISDB-S の TMCC 情報を取得
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 tmcc が NULL
		virtual status GetTmccS(uint tuner, TmccS *tmcc) = 0;

		// ISDB-S 階層情報
		struct LayerS {
			uint Mode [LAYER_COUNT_S];	// 伝送モード (3ビット) 
			uint Count[LAYER_COUNT_S];	// ダミースロットを含めた割当スロット数 (6ビット)
		};

		// [機能] ISDB-S のレイヤ情報を取得
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 layerS が NULL
		virtual status GetLayerS(uint tuner, LayerS *layerS) = 0;

		// ISDB-T TMCC 情報
		// (参考) STD-B31 3.15.6 TMCC情報 ～ 3.15.6.8 セグメント数
		struct TmccT {
			uint System;					// システム識別 (2ビット)
			uint Indicator;					// 伝送パラメータ切り替え指標 (4ビット)
			uint Emergency;					// 緊急警報放送用起動フラグ (1ビット)
											// カレント情報
			uint Partial;					// 部分受信フラグ (1ビット)
											// 階層情報
			uint Mode      [LAYER_COUNT_T];	// キャリア変調方式 (3ビット)
			uint Rate      [LAYER_COUNT_T];	// 畳込み符号化率 (3ビット)
			uint Interleave[LAYER_COUNT_T];	// インターリーブ長 (3ビット)
			uint Segment   [LAYER_COUNT_T];	// セグメント数 (4ビット)
											// [ネクスト情報は取得できません]
			uint Phase;						// 連結送信位相補正量 (3ビット)
			uint Reserved;					// リザーブ (12ビット)
		};

		// [機能] ISDB-T の TMCC 情報を取得
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 tmcc が NULL
		virtual status GetTmccT(uint tuner, TmccT *tmcc) = 0;

		// [機能] ISDB-T ロック判定を取得
		// [説明] レイヤが存在し、なおかつそのレイヤがエラーフリーであるときに true になります。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 locked が NULL
		virtual status GetLockedT(uint tuner, bool locked[LAYER_COUNT_T]) = 0;

		// 受信階層
		enum LayerMask {
			LAYER_MASK_NONE,

			// ISDB-S
			LAYER_MASK_L = 1 << LAYER_INDEX_L,
			LAYER_MASK_H = 1 << LAYER_INDEX_H,

			// ISDB-T
			LAYER_MASK_A = 1 << LAYER_INDEX_A,
			LAYER_MASK_B = 1 << LAYER_INDEX_B,
			LAYER_MASK_C = 1 << LAYER_INDEX_C
		};

		// [機能] 受信階層の設定
		// [説明] ISDB-S の低階層を受信しないように設定することはできません。
		// [返値] STATUS_DEVICE_IS_NOT_OPEN_ERROR → デバイスがオープンされていない
		//        STATUS_INVALID_PARAM_ERROR      → 引数 tuner が 1 より大きいか引数 isdb が範囲外
		//                                           引数 layerMask が範囲外 (SetLayerEnable のみ)
		//                                           引数 layerMask が NULL (GetLayerEnable のみ)
		virtual status SetLayerEnable(uint tuner, ISDB isdb, LayerMask  layerMask)       = 0;
		virtual status GetLayerEnable(uint tuner, ISDB isdb, LayerMask *layerMask) const = 0;

		// --------
		// DMA 転送
		// --------

		// バッファサイズ
		enum {
			BUFFER_PAGE_COUNT = 511
		};

		// バッファ情報
		struct BufferInfo {
			uint VirtualSize;
			uint VirtualCount;
			uint LockSize;
		};
		// バッファはドライバ内部で VirtualAlloc(4096*BUFFER_PAGE_COUNT*VirtualSize) を VirtualCount 回呼び出すことにより確保されます。
		// VirtualCount が 2 以上の場合はバッファが分割されるため、アドレスが不連続になることにご注意ください。
		// LockSize はドライバ内部でメモリをロックする単位です。
		// 
		// VirtualSize の範囲は 0 以外の任意の数値です。
		// (VirtualSize * VirtualCount) は転送カウンタのビット長による制限を受けるため、範囲は 1～4095 です。
		// (VirtualSize % LockSize) は 0 でなければなりません。
		// 
		// DMA バッファは CPU 側から見てキャッシュ禁止になっています。このため、バッファの内容をバイト単位で複数回
		// 読み出す場合などに速度低下が発生します。これを避けるにはデータをキャッシュ可能なメモリにコピーして、
		// コピーされたデータにアクセスします。

		// [機能] DMA バッファの確保・解放
		// [説明] DMA バッファを開放するには SetBufferInfo(NULL) とします。
		//        バッファが確保されていないときに GetBufferInfo() を呼び出すと、bufferInfo が指す全てのメンバは 0 になります。
		//        バッファの構成を変更する場合は、現在のバッファを解放してから改めて確保します。
		virtual status SetBufferInfo(const BufferInfo *bufferInfo)       = 0;
		virtual status GetBufferInfo(      BufferInfo *bufferInfo) const = 0;

		// [機能] DMA バッファのポインタを取得
		// [説明] index で指定した DMA バッファのポインタを取得します。index の範囲は 0 から BufferInfo::VirtualCount-1 です。
		virtual status GetBufferPtr(uint index, void **ptr) const = 0;

		// [機能] 転送カウンタをリセット・インクリメント
		// [説明] FPGA 回路は次のように動作します。
		//			while (true) {
		//				/* 転送カウンタをチェック */
		//				if (転送カウンタ == 0) {
		//					TransferInfo::TransferCounter0 = true;
		//					break;
		//				}
		//				if (転送カウンタ <= 1) {
		//					TransferInfo::TransferCounter1 = true;
		//					/* ここでは break しない */
		//				}
		//
		//				/* 転送カウンタをデクリメント */
		//				転送カウンタ--;
		//
		//				/* データ転送 */
		//				for (uint i=0; i<BUFFER_PAGE_COUNT; i++) {
		//					/* 4096+64 バイトのデータが溜まるまで待つ */
		//					while (true) {
		//						if (4096+64 <= バッファ上のデータバイト数) {
		//							break;
		//						}
		//					}
		//					/* 4096 バイトのデータを転送 */
		//					Transfer();
		//				}
		//			}
		//        ホスト側からは、4096*BUFFER_PAGE_COUNT バイト単位で転送カウンタをインクリメントすることになります。
		//        転送カウンタ長は 12 ビットです。
		virtual status ResetTransferCounter() = 0;
		virtual status IncrementTransferCounter() = 0;

		// [機能] ストリームごとの転送制御
		// [説明] 各ストリームを転送するかどうかを設定することができます。
		//        必要のないストリームをオフにすることで PCI バスの帯域を無駄に使うことがなくなります。
		//        DMA 転送動作中にも変更可能です。
		virtual status SetStreamEnable(uint tuner, ISDB isdb, bool  enable)       = 0;
		virtual status GetStreamEnable(uint tuner, ISDB isdb, bool *enable) const = 0;

		// [機能] ストリームごとの 3 ビット補助データの設定
		// [説明] 1 TS パケット(188バイト) は 63 マイクロパケットを使って転送されますが、
		//        3バイト×63マイクロパケット=189バイトとなり、末尾のマイクロパケットには未使用部分が 1 バイトあります。
		//        このバイトの下位 3 ビットをユーザーが自由に設定することができます。
		//        復調IC からの信号を FPGA 内にデータを取り込んでからできるだけ早い時刻に 3 ビットのデータが
		//        付加されますので、タイムスタンプ代わりに利用することができます。
		//        FPGA 内では値の書き込みは PCI クロックに同期し、値の読み出しは TS クロックに同期しています。
		//        このため、設定する数列はグレイコードなどのハミング距離が 1 のものを使ってください。
		virtual status SetStreamGray(uint tuner, ISDB isdb, uint  gray)       = 0;
		virtual status GetStreamGray(uint tuner, ISDB isdb, uint *gray) const = 0;

		// [機能] DMA 開始・停止の制御
		// [説明] DMA 転送は全く CPU を介在することなく動作します。
		//        GetTransferEnable() で true  が得られるときに SetTransferEnable(true ) としたり、
		//        GetTransferEnable() で false が得られるときに SetTransferEnable(false) とするとエラーになります。
		//        
		//        GetTransferEnable() で取得できる値は、単に SetTransferEnable() で最後に設定された値と同じです。
		//        転送カウンタが 0 になるなど、ハードウェア側で DMA 転送が自動的に停止する要因がいくつかありますが、
		//        その場合でも GetTransferEnable() で得られる値は変わりません。
		virtual status SetTransferEnable(bool  enable)       = 0;
		virtual status GetTransferEnable(bool *enable) const = 0;

		struct TransferInfo {
			bool TransferCounter0;	// 転送カウンタが 0 であるのを検出した
			bool TransferCounter1;	// 転送カウンタが 1 以下であるのを検出した
			bool BufferOverflow;	// PCI バスを長期に渡り確保できなかったため、ボード上の FIFO(サイズ=8MB) が溢れた
		};							// (これらのフラグは、一度でも条件成立を検出すると DMA 転送を再開するまでクリアされません)

		// [機能] DMA 状態の取得
		virtual status GetTransferInfo(TransferInfo *) = 0;

		// マイクロパケットの構造
		// +------------+----+----+----+----+----+----+----+----+----+----+----+
		// | ビット位置 | 31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | .. |  0 |
		// +------------+----+----+----+----+----+----+----+----+----+----+----+
		// |    内容    |      id      |    counter   | st | er |     data     |
		// +------------+--------------+--------------+----+----+--------------+
		// id     : ストリームID
		// counter: ストリームごとのカウンタ
		// st     : TS パケット開始位置フラグ
		// er     : エラーフラグ (TransferCounter0 と TransferCounter1 と BufferOverflow の論理和)
		// data   : データ

		// ストリームID
		// +----+------------------------+
		// | id | 説明                   |
		// +----+------------------------+
		// |  0 | 禁止                   |
		// |  1 | チューナー番号0 ISDB-S |
		// |  2 | チューナー番号0 ISDB-T |
		// |  3 | チューナー番号1 ISDB-S |
		// |  4 | チューナー番号1 ISDB-T |
		// |  5 | 予約                   |
		// |  6 | 予約                   |
		// |  7 | 予約                   |
		// +----+------------------------+
		// ストリームID が 0 になることは絶対にありません。
		// DMA 転送がどこまで進んでいるのかを調べるには、転送前に ストリームID を 0 に設定して、
		// その箇所が 0 以外になったかどうかを調べます。
		// 実用上は転送前に 4 バイトのマイクロパケット領域に 0 を書き込み、0 以外になったかどうかを調べることになります。

		// マイクロパケットから TS パケットを再構成する方法についてはサンプルコードをご参照ください。
		// 次の関数を呼び出した直後に 188 バイトに満たないパケットが発生することがあり、切捨て処理が必要です。
		// ・SetTunerSleep()
		// ・SetFrequency()
		// ・SetIdS()
		// ・SetLayerEnable()
		// ・SetStreamEnable()
		// ・SetTransferEnable(true)

	protected:
		virtual ~Device() {}
	};

	enum Status {
		// エラーなし
		STATUS_OK,

		// 一般的なエラー
		STATUS_GENERAL_ERROR = (1)*0x100,
		STATUS_NOT_IMPLIMENTED,
		STATUS_INVALID_PARAM_ERROR,
		STATUS_OUT_OF_MEMORY_ERROR,
		STATUS_INTERNAL_ERROR,

		// バスクラスのエラー
		STATUS_WDAPI_LOAD_ERROR = (2)*256,	// wdapi1002.dll がロードできない
		STATUS_ALL_DEVICES_MUST_BE_DELETED_ERROR,

		// デバイスクラスのエラー
		STATUS_PCI_BUS_ERROR = (3)*0x100,
		STATUS_CONFIG_REVISION_ERROR,
		STATUS_FPGA_VERSION_ERROR,
		STATUS_PCI_BASE_ADDRESS_ERROR,
		STATUS_FLASH_MEMORY_ERROR,

		STATUS_DCM_LOCK_TIMEOUT_ERROR,
		STATUS_DCM_SHIFT_TIMEOUT_ERROR,

		STATUS_POWER_RESET_ERROR,
		STATUS_I2C_ERROR,
		STATUS_TUNER_IS_SLEEP_ERROR,

		STATUS_PLL_OUT_OF_RANGE_ERROR,
		STATUS_PLL_LOCK_TIMEOUT_ERROR,

		STATUS_VIRTUAL_ALLOC_ERROR,
		STATUS_DMA_ADDRESS_ERROR,
		STATUS_BUFFER_ALREADY_ALLOCATED_ERROR,

		STATUS_DEVICE_IS_ALREADY_OPEN_ERROR,
		STATUS_DEVICE_IS_NOT_OPEN_ERROR,

		STATUS_BUFFER_IS_IN_USE_ERROR,
		STATUS_BUFFER_IS_NOT_ALLOCATED_ERROR,

		STATUS_DEVICE_MUST_BE_CLOSED_ERROR,

		// WinDriver 関連のエラー
		STATUS_WD_DriverName_ERROR = (4)*0x100,

		STATUS_WD_Open_ERROR,
		STATUS_WD_Close_ERROR,

		STATUS_WD_Version_ERROR,
		STATUS_WD_License_ERROR,

		STATUS_WD_PciScanCards_ERROR,

		STATUS_WD_PciConfigDump_ERROR,

		STATUS_WD_PciGetCardInfo_ERROR,
		STATUS_WD_PciGetCardInfo_Bus_ERROR,
		STATUS_WD_PciGetCardInfo_Memory_ERROR,

		STATUS_WD_CardRegister_ERROR,
		STATUS_WD_CardUnregister_ERROR,

		STATUS_WD_CardCleanupSetup_ERROR,

		STATUS_WD_DMALock_ERROR,
		STATUS_WD_DMAUnlock_ERROR,

		STATUS_WD_DMASyncCpu_ERROR,
		STATUS_WD_DMASyncIo_ERROR
	};

	// ------------------------------
	// 特殊イベントにおける動作の詳細
	// ------------------------------

	// 1. ボードの電源が投入されたときは、ハードウェアは以下の状態になります。
	//    ・SetLnbPower(LNB_POWER_OFF)
	//    ・SetTunerPowerReset(TUNER_POWER_OFF)
	//    ・DMA 動作は停止
	// 2. PCI リセットがアサートされたときも、電源投入時と同じ状態になります。
	// 3. アプリケーションが異常終了した場合を含めデバイスをクローズするときは、以下の処理が順番に実行されます。
	//    ・DMA 動作を停止
	//    ・SetLnbPower([SetLnbPowerWhenClose() で指定された値])
	//    ・SetTunerPowerReset(TUNER_POWER_OFF)
	//    ・DMA バッファを解放
}
}

#endif

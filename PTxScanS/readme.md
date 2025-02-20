# PTxScanS
BonDriverからチャンネルファイルを自動生成するツールです。

## 使い方

```
  PTxScnanS [-param|+param] [--verbose-param] [BonDriver.dll]

    param, verbose-param:
      +c, --chset
        Enable writing the scanning results to the .ChSet.txt file.
      -c, --no-chset
        Disable writing the scanning results to the .ChSet.txt file.
      +f, --force-CSV
        Force writing the scanning results to the .CSV.txt file.
      +t, --transponder
        Enable writing the transponder information to the scanning results.
      -t, --no-transponder
        Disable writing the transponder information to the scanning results.
      +s, --tsid-stream
        Write TSID columns as stream number columns to the scanning results.
      +d, --deep-tuning
        Test the integrity of the TSID referred from the TMCC-Info one by one.
      -?|+?, --help
        Show this help message and exit.

    BonDriver.dll:
      Load indicated .dll automatically if specified at the last argument.
```


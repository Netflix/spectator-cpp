# Benchmarks


## Benchmarking different table implementations to keep meters in the registry

```
bazel test -c opt tables_bench --test_output=all
```

On my system it shows a significant performance advantage by using `tsl::hopscotch_map` 
enabling storing the hash on the table (helps during resizing). 
Testing with different number of meters shows:

```
2020-02-28 00:25:31
Running ./bazel-out/k8-opt/bin/tables_bench
Run on (4 X 3098.96 MHz CPU s)
CPU Caches:
  L1 Data 32K (x2)
  L1 Instruction 32K (x2)
  L2 Unified 1024K (x2)
  L3 Unified 33792K (x1)
Load Average: 0.00, 0.16, 0.21
----------------------------------------------------------------------
Benchmark                            Time             CPU   Iterations
----------------------------------------------------------------------
BM_FlatHashMap/8                   970 ns          970 ns       716923
BM_FlatHashMap/256                1006 ns         1006 ns       693917
BM_FlatHashMap/65536              1772 ns         1772 ns       395919
BM_FlatHashMap/8388608            2408 ns         2408 ns       373805
BM_UnorderedMap/8                  988 ns          988 ns       709259
BM_UnorderedMap/256               1037 ns         1037 ns       683717
BM_UnorderedMap/65536             1708 ns         1708 ns       453828
BM_UnorderedMap/8388608           1957 ns         1957 ns       468259
BM_HopscotchMap/8                  972 ns          972 ns       721277
BM_HopscotchMap/256                987 ns          987 ns       709611
BM_HopscotchMap/65536             1578 ns         1578 ns       444142
BM_HopscotchMap/8388608           1815 ns         1815 ns       475262
BM_HopscotchMapHash/8              969 ns          969 ns       718416
BM_HopscotchMapHash/256            978 ns          978 ns       715589
BM_HopscotchMapHash/65536         1448 ns         1448 ns       524299
BM_HopscotchMapHash/8388608       1491 ns         1491 ns       606882
```

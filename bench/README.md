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

## Benchmarking different implementations for tags

```
bazel test -c opt tags_bench --test_output=all
```

This tests different hash table implementations to maintain the key/values for tags.
The first parameterized test uses 4 key/values times the input so a `/4` is a test
maintaining a 16 entry tag set. 3/4 of those key/values are small strings (which could have
different behavior thanks to the small string optimization usual in `std::string`
implementations.)

In general our current `ska::flat_hash_map` has roughly
the same performance as the `hopscotch` based tables but is significantly faster when
starting from a given set of tags which is the behavior we experience when we start with
a base-id and add tags to it.

```
g++ 7 on bionic

Run on (4 X 2936.1 MHz CPU s)
CPU Caches:
  L1 Data 32K (x2)
  L1 Instruction 32K (x2)
  L2 Unified 1024K (x2)
  L3 Unified 33792K (x1)
Load Average: 1.95, 2.14, 1.29
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
BM_UnorderedMap/1                 405 ns          405 ns      1726676
BM_UnorderedMap/2                 830 ns          830 ns       842982
BM_UnorderedMap/4                1776 ns         1776 ns       393477
BM_UnorderedMap/8                3580 ns         3580 ns       195222
BM_FlatHashMap/1                  350 ns          350 ns      1998110
BM_FlatHashMap/2                  757 ns          757 ns       911140
BM_FlatHashMap/4                 1512 ns         1512 ns       458092
BM_FlatHashMap/8                 2976 ns         2976 ns       235218
BM_HopscotchMap/1                 358 ns          358 ns      1955560
BM_HopscotchMap/2                 753 ns          753 ns       931632
BM_HopscotchMap/4                1556 ns         1556 ns       441878
BM_HopscotchMap/8                3054 ns         3054 ns       226568
BM_HopscotchMapHash/1             353 ns          353 ns      1983117
BM_HopscotchMapHash/2             729 ns          729 ns       961091
BM_HopscotchMapHash/4            1530 ns         1530 ns       460196
BM_HopscotchMapHash/8            2916 ns         2916 ns       239604
BM_UnorderedMapWithTag            861 ns          862 ns       810127
BM_FlatHashMapWithTag             860 ns          860 ns       815085
BM_HopscotchMapWithTag           2147 ns         2147 ns       326313
BM_HopscotchMapHashWithTag       1048 ns         1048 ns       668778
================================================================================

* Apple clang 11 on OSX Catalina

CPU Caches:
  L1 Data 32K (x8)
  L1 Instruction 32K (x8)
  L2 Unified 262K (x8)
  L3 Unified 16777K (x1)
Load Average: 1.94, 2.00, 2.11
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
BM_UnorderedMap/1                 530 ns          529 ns      1183972
BM_UnorderedMap/2                1090 ns         1090 ns       622698
BM_UnorderedMap/4                2094 ns         2094 ns       328355
BM_UnorderedMap/8                4382 ns         4374 ns       148945
BM_FlatHashMap/1                  493 ns          492 ns      1423722
BM_FlatHashMap/2                  985 ns          985 ns       716985
BM_FlatHashMap/4                 1995 ns         1994 ns       355362
BM_FlatHashMap/8                 3918 ns         3913 ns       176124
BM_HopscotchMap/1                 491 ns          490 ns      1408720
BM_HopscotchMap/2                1027 ns         1027 ns       658247
BM_HopscotchMap/4                1988 ns         1987 ns       348618
BM_HopscotchMap/8                4039 ns         4039 ns       177745
BM_HopscotchMapHash/1             496 ns          496 ns      1331406
BM_HopscotchMapHash/2            1067 ns         1066 ns       683120
BM_HopscotchMapHash/4            1989 ns         1989 ns       354013
BM_HopscotchMapHash/8            3986 ns         3985 ns       174339
BM_UnorderedMapWithTag           2062 ns         2062 ns       344887
BM_FlatHashMapWithTag             799 ns          799 ns       812829
BM_HopscotchMapWithTag           1971 ns         1970 ns       359683
BM_HopscotchMapHashWithTag       1313 ns         1312 ns       540011
================================================================================
```

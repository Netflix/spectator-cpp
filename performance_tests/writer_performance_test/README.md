# Writer performance test

Measures what it costs to record meters through the writers, and how much of what was recorded arrives. It does so
for a list of configurations: the socket writer, the buffer size, the number of recording threads, the load, and the
way the meters are used. It needs nothing but this repository, because the test starts its own receiver in place of
spectatord.

## Run it

Performance numbers only mean something from an optimized build. Build one in a directory of its own, next to the
normal build:

```shell
source venv/bin/activate
BUILD_DIR=cmake-build-release BUILD_TYPE=Release ./build.sh skiptest
```

The first time, this also builds the dependencies in release mode, which takes a few minutes.

Then, to see the full performance:

```shell
./cmake-build-release/performance_tests/writer_performance_test/writer_performance_test --full
```

There are three sizes of run:

| Option    | What it measures                                                                         | Time             |
|-----------|------------------------------------------------------------------------------------------|------------------|
| (none)    | Both writers with 1, 4 and 8 threads, three buffer sizes, one paced rate, the meter uses  | about 2 minutes  |
| `--full`  | The same with 1 to 16 threads, every buffer size at every thread count, two paced rates   | about 12 minutes |
| `--smoke` | Two configurations at a low rate; fails if a recorded line does not arrive                | a few seconds    |

Add `--list` to see the configurations of a run and how long they take, without running them.

The test also runs from the normal build, for instance to check that it works
(`./cmake-build/performance_tests/writer_performance_test/writer_performance_test --smoke`), but it then warns that
its numbers say little.

## How it works

For every configuration, the test

1. opens a socket of its own on this machine, a UDP port or a Unix socket in `/tmp`, and reads everything that
   arrives there, counting datagrams and lines;
2. starts a second process, which creates a `Registry` for that socket through the public API and lets every thread
   record with a `Counter`;
3. lets the threads record for a short warm-up, and then measures them for a fixed time;
4. lets that process exit normally, so that the library sends the lines it still holds, and compares what was
   recorded with what arrived.

Every configuration gets a process of its own, because the library sets its writer up once per process. Every
configuration is measured three times, in rotation, and the report shows the median.

The threads record in one of two ways:

- **Flat out:** as fast as they can. This shows the highest rate, how it changes with the number of threads, and how
  long a call can take when the threads record faster than the lines are sent.
- **Paced:** a fixed number of lines per second and thread, 20,000 unless `--rate` says otherwise. This shows how
  long a call takes under an ordinary load, and how long a line takes to reach the receiver.

The counter is used in one of four ways: created once per thread, or created for every line as in
`registry.CreateCounter(name, tags).Increment()`, each with two tags or with none. Everything but the comparison of
these four uses a counter that was created once and has two tags.

## Reading the report

The report has one table for each kind of load, and one that compares the ways of using a counter.

| Column                                   | Meaning                                                                    |
|------------------------------------------|----------------------------------------------------------------------------|
| `lines/s`, `per thread`                  | Lines recorded per second during the measurement, by all threads together and by each of them. The median of the measurements. |
| `speed-up`                               | `lines/s` as a multiple of the same configuration with one thread.         |
| `spread`                                 | How far the slowest and the fastest measurement are apart, as a share of the median. A difference between two configurations that is smaller than this is not a difference. |
| `call p50`, `p99`, `p99.9`, `longest`    | How long a call that records one line took. Half of the calls took no longer than `p50`, and 99% no longer than `p99`. |
| `cpu/line`                               | Processor time of the recording process per line, on all its threads, which includes the thread that sends. |
| `arrived`                                | The share of the recorded lines that the receiver got. `100%` is shown only if every line arrived. |
| `lines/datagram`                         | How many lines a datagram held on average.                                 |
| `delay p50`, `delay p99`, `longest delay` | Paced load: the time from recording a line to its arrival at the receiver. |
| `bytes/line`                             | The length of a line, which the tags change.                               |
| `vs baseline`                            | With `--baseline`: `lines/s` as a multiple of the earlier run's.            |

## Options

| Option            | Meaning                                                                                      |
|-------------------|----------------------------------------------------------------------------------------------|
| `--full`          | The long run, see above                                                                      |
| `--smoke`         | The check that the test works, see above                                                     |
| `--list`          | Print the configurations and the time they take, and stop                                    |
| `--writers LIST`  | `udp`, `unix` or both: `--writers unix`                                                      |
| `--threads LIST`  | Numbers of recording threads: `--threads 1,2,4,8,16,32`                                      |
| `--buffers LIST`  | Buffer sizes in bytes, `0` for no buffering: `--buffers 0,1024,8192`                         |
| `--rate N`        | Lines per second and thread under the paced load                                             |
| `--seconds S`     | How long each configuration is measured                                                      |
| `--warmup S`      | How long the threads record before the measurement begins                                    |
| `--repeats N`     | How often each configuration is measured                                                     |
| `--csv FILE`      | Also write every measurement to `FILE`                                                       |
| `--baseline FILE` | A file that an earlier run wrote with `--csv`: the report shows this run against it          |
| `--label TEXT`    | A name for the run, for the report and the CSV file                                          |

With `--threads` or `--buffers`, every buffer size is measured with every number of threads.

## Comparing two versions of the library

Run the test on the first version and keep its measurements. Then build the second version the same way and run the
test against them:

```shell
# on the first version, for example main
./cmake-build-release/performance_tests/writer_performance_test/writer_performance_test --full --label main --csv main.csv

# on the second version
./cmake-build-release/performance_tests/writer_performance_test/writer_performance_test --full --label my-change --baseline main.csv
```

The second report has a column `vs baseline`, in which `2.00x` means twice the lines per second of the first
version. Compare runs from the same machine only.

## Numbers you can rely on

- Use an optimized build. The top of the report says whether it is one.
- Run the test on a machine that is doing nothing else, and on Linux if the numbers are to say something about
  production.
- Look at `spread` before comparing two rows.
- The receiver runs on the same machine as the recording threads and takes processor time too.

## Configurations that are left out

A system limits how large a datagram can be, and the test leaves out the buffer sizes that do not fit; it lists them
under the report. macOS, for example, sends at most 2048 bytes on a Unix datagram socket by default, so there the
Unix writer is measured with a buffer of 512 bytes only. Linux carries every size that the test uses.

## Measuring a new setting of the writer

1. Add the setting to `Scenario` in `scenario.h`: the field, and its part in `Key()`, `ToArguments()` and
   `FromArguments()`.
2. Pass it to the `WriterConfig` in `recording_run.cpp`.
3. Add the values to measure to `Selection` in `plan.h`, and the scenarios to `MakePlan()` in `plan.cpp`.
4. Show it in the tables and in the CSV file, both in `report.cpp`.

## Files

| File                                   | Contents                                                              |
|----------------------------------------|-----------------------------------------------------------------------|
| `writer_performance_test.cpp`          | The command line, starting the recording process, the run             |
| `scenario.h`                           | One configuration, and how it is handed to the recording process      |
| `plan.h`, `plan.cpp`                   | Which configurations a run consists of                                |
| `recording_run.h`, `recording_run.cpp` | The recording process: the threads that record, and what they measure |
| `receiver.h`, `receiver.cpp`           | The receiving socket that stands in for spectatord                    |
| `report.h`, `report.cpp`               | The tables, the CSV file, the comparison with a baseline              |
| `histogram.h`                          | Counting durations, for the percentiles                               |

export declare class Counter {
  increment(): void;
  add(delta: number | bigint): void;
}

export declare class Gauge {
  set(value: number): void;
}

export declare class MaxGauge {
  set(value: number): void;
}

export declare class AgeGauge {
  set(seconds: number): void;
  now(): void;
}

export declare class MonotonicCounter {
  set(amount: number): void;
}

export declare class MonotonicCounterUint {
  set(amount: number | bigint): void;
}

export declare class Timer {
  record(seconds: number): void;
}

export declare class PercentileTimer {
  record(seconds: number): void;
}

export declare class DistributionSummary {
  record(amount: number | bigint): void;
}

export declare class PercentileDistributionSummary {
  record(amount: number | bigint): void;
}

export declare class Registry {
  /**
   * @param location - "udp" | "udp://host:port" | "unix" | "unix:///path" |
   *                   "memory" | "noop". Defaults to "udp".
   * @param bufferSize - bytes to buffer before flushing to the socket (0 = unbuffered)
   */
  constructor(location?: string, bufferSize?: number);

  counter(name: string, tags?: Record<string, string>): Counter;
  gauge(name: string, tags?: Record<string, string>, ttlSeconds?: number): Gauge;
  maxGauge(name: string, tags?: Record<string, string>): MaxGauge;
  ageGauge(name: string, tags?: Record<string, string>): AgeGauge;
  monotonicCounter(name: string, tags?: Record<string, string>): MonotonicCounter;
  monotonicCounterUint(name: string, tags?: Record<string, string>): MonotonicCounterUint;
  timer(name: string, tags?: Record<string, string>): Timer;
  pctTimer(name: string, tags?: Record<string, string>): PercentileTimer;
  distSummary(name: string, tags?: Record<string, string>): DistributionSummary;
  pctDistSummary(name: string, tags?: Record<string, string>): PercentileDistributionSummary;

  /** Returns all lines written to the memory writer as a string array. */
  dumpLines(): string[];
}

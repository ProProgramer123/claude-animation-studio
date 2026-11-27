/** What a headless browser saw when it opened the animation. */
export interface CheckResult {
  /** Uncaught exceptions and console errors, plus resources that failed to load. */
  errors: string[];
  /** True when two screenshots a second apart were identical, i.e. nothing moved. */
  static: boolean;
  /** A PNG screenshot of the animation, if one was taken. */
  screenshot?: Buffer;
}

export interface AnimationChecker {
  readonly name: string;
  /** Returns null when the check can't run (for example, no browser installed). */
  check(htmlPath: string): Promise<CheckResult | null>;
}

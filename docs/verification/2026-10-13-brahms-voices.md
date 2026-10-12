# Brahms voice continuity and filler rests

The marked imported song has a one-quarter pickup at tick 0, then 3/4 from tick 256 (PPQ 256). In the marked bars, a half-note melody is followed by two eighth notes while a lower inner note lasts one quarter. The previous longest-duration-first greedy voice allocation put that lower note into the melody lane and inserted artificial rests in a second lane.

Display lanes now prioritize the higher simultaneous onset and choose the closest previous pitch among available lanes. This keeps the continuous melody in one voice. Supplemental filler rests are hidden only when other voices on the same staff cover their full interval; uncovered silence remains visible. MIDI events, durations, scoring and playback are unchanged.

A regression reproduces both marked bar patterns, checks continuous melody, separate quarter-note inner voice and suppressed filler rests, and checks that an actual uncovered pause remains visible. The exact imported passage was visually verified in the existing local browser; screenshot: brahms-voices-corrected.jpg. Bass rests remain because the imported bass stops for that beat. The 3/4 meter does not prohibit rests.

Verification: production build, all 34 frontend tests and all ten native/browser integration suites passed. No physical MIDI hardware was connected.

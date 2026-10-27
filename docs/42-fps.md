# The 42 fps ceiling

Everything runs at 42.2-42.4 fps on my Retroid Pocket 4 Pro. I spent a while looking for why and didn't
find it. This is what I measured, in case it helps somebody who knows the engine side better than me.

The numbers come from temporary instrumentation I added to DXVK and removed afterwards. It isn't in
this repo.

## What it isn't

| Suspect | Why not |
|---|---|
| The GPU | Same fps with the GPU at 17-19% (menu, 16 draw calls) and 65% (Zombies, about 2000 draw calls) |
| The display | The panel has one mode, 60 Hz, vsync period 16.67 ms. 42.3 isn't a fraction of that |
| Vsync | Present mode is MAILBOX. `dxgi.syncInterval = 0` made no difference |
| DXVK's frame limiter | Off by default (`dxgi.maxFrameRate` 0, no `DXVK_FRAME_RATE`) |
| Presenting or submitting | `vkQueuePresentKHR` 0.32 ms, `vkQueueSubmit` 0.03 ms on average (about 3 submits per frame), DXVK's thread starts the present 0.02 ms after the engine emits it and it's done in about 0.4 ms |
| `com_maxfps` | The engine writes `"60"` in its own config. I put `seta com_maxfps "0"` in `players/config_mp.cfg`, the engine ran the file and set the dvar to 0 (it wrote that back in the config it regenerated), fps stayed at 42.2 |

## What it looks like

- The cycle of the thread that calls Present is a constant 23.56 ms (42.3 fps).
- Before the swap chain fix (see README) that was 18.3 ms in the engine plus 5.3 ms inside Present,
  waiting on the swap chain recreation. After the fix it's 0.07 ms inside Present and 23.56 ms in the
  engine. The total didn't move, so the wait was already hiding behind something else.
- 23.56 ms is almost exactly two times 11.76 ms, which is an 85 fps interval. That made me think of a
  frame cap in the engine. The `com_maxfps` test above rules that one out. Might be a coincidence.

## Where the thread spends its time

I sampled the PC of the thread that calls Present, about 1000 samples per second for 8 seconds, in the
Multiplayer menu (7997 samples). The game binary has symbols, so I could name the guest functions.

- About 10% of the samples are game code. Nearly all of that is `nx_wait_objects`,
  `WaitForMultipleObjects`, `nx_handle_get`, `try_acquire`, `event_set` and `Sys_WaitBackendEvent`.
- Of the rest, `sched_yield` is about a third of all samples and `nanosleep`/other libc waits another
  chunk. About 18% is in `libt6host.so` (the port's host layer: `host_guest_range_ok`,
  `nxhost_mutex_lock`).
- `top -H` shows one thread of the game at about 78% CPU. I assume it's the same one, and that the
  CPU time is the yield loops. The next busiest threads are at 22% and 7%. No thread is saturated.

So the thread that presents is waiting for another engine thread, probably the front end, and that wait
is a constant 23.5 ms, in a menu and in a match. My guess is that it's in the port's emulation of
Windows events and mutexes (`WaitForMultipleObjects` done by polling with `try_acquire` and
`sched_yield`) or something it waits on. I'm not sure.

## What I couldn't do

- Sample the other threads. Only 2 of the 22 `SDLThread` threads answered the `SIGPROF` I used. The
  rest block signals.
- Use `simpleperf`. It's denied on the device (`perf_event_paranoid`), for hardware counters and for
  `cpu-clock`.

Next thing I'd try, with a rooted device or a build that allows perf: record all threads and look for
who wakes up the thread that presents.

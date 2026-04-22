/// <reference path="../distribution/openrct2.d.ts" />

// profile-run.js
// Drop into your OpenRCT2 plugin folder.
// Waits for the game to finish loading (mode == "normal"), delays STARTUP_DELAY_MS,
// then profiles for DURATION_MS and stores results in sharedStorage.
//
// Usage: adjust DURATION_MS / STARTUP_DELAY_MS below, then load the plugin.

const DURATION_MS = 60000;      // how long to profile (milliseconds)
const STARTUP_DELAY_MS = 5000;  // delay after game load before profiling starts

registerPlugin({
    name: "profile-run",
    version: "1.0",
    authors: ["local"],
    type: "intransient",
    licence: "MIT",
    targetApiVersion: 111,
    main: function () {
        if (typeof profiler === "undefined") {
            console.log("[profile-run] profiler API not available in this build.");
            return;
        }

        // Wait for the title screen to settle, then start profiling
        console.log(`[profile-run] Starting profiler in ${STARTUP_DELAY_MS / 1000}s...`);
        context.setTimeout(function () {
            profiler.reset();
            profiler.start();
            console.log(`[profile-run] Profiling started. Will stop in ${DURATION_MS / 1000}s.`);

            context.setTimeout(function () {
                runStop();
            }, DURATION_MS);
        }, STARTUP_DELAY_MS);

        function runStop() {
            profiler.stop();

            const data = profiler.getData();
            const active = data.filter(function (f) { return f.callCount > 0; });

            // Sort by (maxTime - minTime) descending — worst variability first
            active.sort(function (a, b) {
                return (b.maxTime - b.minTime) - (a.maxTime - a.minTime);
            });

            // Build CSV
            const lines = [
                "function_name,calls,min_microseconds,max_microseconds,average_microseconds,total_microseconds,min-max-delta_microseconds"
            ];

            for (const f of active) {
                const avg = f.callCount > 0 ? f.totalTime / f.callCount : 0;
                const delta = f.maxTime - f.minTime;
                const name = f.name.replace(/"/g, '""');
                lines.push(
                    `"${name}",${f.callCount},${f.minTime.toFixed(3)},${f.maxTime.toFixed(3)},${avg.toFixed(3)},${f.totalTime.toFixed(3)},${delta.toFixed(3)}`
                );
            }

            const csv = lines.join("\n") + "\n";

            try {
                context.sharedStorage.set("profile-run.last-csv", csv);
                console.log(`[profile-run] Done. ${active.length} functions profiled over ${DURATION_MS / 1000}s.`);
                console.log(`[profile-run] Top 5 by variability (max-min delta):`);
                for (let i = 0; i < Math.min(5, active.length); i++) {
                    const f = active[i];
                    const delta = (f.maxTime - f.minTime).toFixed(1);
                    const avg = (f.callCount > 0 ? f.totalTime / f.callCount : 0).toFixed(1);
                    console.log(`  ${i + 1}. delta=${delta}µs  avg=${avg}µs  calls=${f.callCount}  ${f.name}`);
                }
                console.log(`[profile-run] Full CSV stored in sharedStorage key 'profile-run.last-csv'.`);
                console.log(`[profile-run] To export: console.log(context.sharedStorage.get('profile-run.last-csv'))`);
            } catch (e) {
                console.log(`[profile-run] Error storing results: ${e}`);
            }
        }
    }
});

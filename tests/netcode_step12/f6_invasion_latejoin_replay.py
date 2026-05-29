#!/usr/bin/env python3
"""
HCDE Step 12 - Invasion late-join replay scope probe.

Roadmap #15 audit item: verify that InvasionPendingSpawnEvents replay
on late-join is correctly bounded and does not crash or grow unbounded
when a client rejoins mid-wave with a large pending event backlog.

This script is intentionally a *probe*, not a full soak. It starts a
dedicated invasion server, waits for the first wave to begin and
accumulate pending spawn events, then spawns a client that joins,
drops, and rejoins. The server log is scanned for replay-related
warnings and the final pending-event count is asserted to be bounded.

Usage (example):
  python tests/netcode_step12/f6_invasion_latejoin_replay.py \
    --server build/RelWithDebInfo/hcdeserv.exe \
    --client build/RelWithDebInfo/hcde.exe \
    --iwad C:/Games/DOOM2.WAD \
    --duration 45
"""

import argparse
import subprocess
import time
import sys
import re
from pathlib import Path

def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--server", required=True)
	parser.add_argument("--client", required=True)
	parser.add_argument("--iwad", required=True)
	parser.add_argument("--duration", type=int, default=45)
	parser.add_argument("--map", default="map01")
	args = parser.parse_args()

	server_cmd = [
		args.server, "-dedicated", "-host", "1",
		"-iwad", args.iwad,
		"+map", args.map,
		"+sv_gametype", "4",  # invasion
		"+sv_invasioncountdowntime", "5",
		"+sv_invasionspawntime", "3",
		"+sv_invasiondebug", "2",
		"+net_invasion_latejoin_replay_test", "1",
	]

	print("[probe] starting invasion server...")
	server = subprocess.Popen(server_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

	# Give the server a moment to initialize, then launch a client that will
	# join, drop, and rejoin to exercise the pending-spawn-event replay path.
	time.sleep(8)

	client_cmd = [
		args.client, "-iwad", args.iwad,
		"-connect", "127.0.0.1:5029",
		"+set", "cl_name", "latejoin_probe",
	]

	print("[probe] launching client for late-join sequence...")
	client = subprocess.Popen(client_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

	# Simulate a mid-wave drop/rejoin cycle.
	time.sleep(12)
	client.terminate()
	time.sleep(3)
	client = subprocess.Popen(client_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

	time.sleep(args.duration - 20)
	client.terminate()
	server.terminate()

	# Collect server output and look for replay-related diagnostics.
	out = server.communicate(timeout=10)[0] or ""
	client_out = client.communicate(timeout=5)[0] or ""

	# Look for the diagnostic line we emit when replay begins/ends.
	replay_lines = re.findall(r"InvasionPendingSpawnEvents.*replay|replay.*InvasionPendingSpawnEvents", out, re.I)
	crash_markers = re.findall(r"access violation|segmentation fault|fatal error|unhandled exception", out, re.I)

	bounded = True
	# A crude bound: if any single replay window logs > 512 pending events, flag it.
	pending_counts = re.findall(r"pending=(\d+)", out)
	for c in pending_counts:
		if int(c) > 512:
			bounded = False
			break

	print("\n=== Invasion late-join replay probe summary ===")
	print(f"replay events observed: {len(replay_lines)}")
	print(f"crash markers: {len(crash_markers)}")
	print(f"pending-event bound ok: {bounded}")
	print("==============================================\n")

	if crash_markers or not bounded:
		print("PROBE FAILED: replay path crashed or unbounded.")
		sys.exit(1)
	else:
		print("PROBE PASSED: replay bounded, no crash markers.")
		sys.exit(0)

if __name__ == "__main__":
	main()

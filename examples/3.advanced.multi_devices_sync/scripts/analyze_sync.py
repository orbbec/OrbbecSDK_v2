import argparse
import csv
import glob
import os
import re
import statistics
import sys
from collections import Counter

DEFAULT_FRAME_RATE = None              # None = auto-detect; grouping tolerance = 1e6 / fps / 2 us
DEFAULT_TSP_RANGE_THRESHOLD = 5000.0  # us; in-group range >= this -> abnormal
DEFAULT_TIMESTAMP_SOURCE = "auto"     # global | device | auto
DEFAULT_FRAME_NUM_SOURCE = "auto"     # hw | sw | auto (drop detection)

CSV_NAME_RE = re.compile(r"^sync_(depth|color)_dev(\d+)_(.+)\.csv$", re.IGNORECASE)


# --------------------------------------------------------------------------- #
# Data structures
# --------------------------------------------------------------------------- #
class FrameRecord:
    """One parsed CSV row; lives only while its match round is being processed."""
    __slots__ = ("row_id", "sw_frame_num", "hw_frame_num",
                 "system_ts", "device_ts", "global_ts")

    def __init__(self, row_id, sw_frame_num, hw_frame_num,
                 system_ts, device_ts, global_ts):
        self.row_id = row_id
        self.sw_frame_num = sw_frame_num
        self.hw_frame_num = hw_frame_num
        self.system_ts = system_ts
        self.device_ts = device_ts
        self.global_ts = global_ts

    def ts(self, source):
        return self.global_ts if source == "global" else self.device_ts


class DropStats:
    """Sequential drop scan over one frame-number column, in file order
    (frame numbers are expected to be increasing)."""
    __slots__ = ("missing", "count", "first_num", "_last")

    def __init__(self):
        self.missing = []
        self.count = 0
        self.first_num = None
        self._last = None

    def add(self, num):
        if num == -1:
            return
        self.count += 1
        if self.first_num is None:
            self.first_num = num
        if self._last is not None:
            if num > self._last + 1:
                self.missing.extend(range(self._last + 1, num))
            # num <= self._last (counter reset / duplicate): restart the
            # baseline without counting the overlap as drops
        self._last = num


class StreamStats:
    """Pass-1 metadata and statistics for one CSV. Holds no frame data."""
    __slots__ = ("index", "sn", "sensor", "path", "frame_count",
                 "global_min", "global_max", "global_valid",
                 "device_min", "device_max", "device_valid",
                 "hw_valid", "sw_valid",
                 "gap_hist", "hw_drops", "sw_drops")

    def __init__(self, index, sn, sensor, path):
        self.index = index
        self.sn = sn
        self.sensor = sensor
        self.path = path
        self.frame_count = 0
        self.global_min = None
        self.global_max = None
        self.global_valid = False
        self.device_min = None
        self.device_max = None
        self.device_valid = False
        self.hw_valid = False
        self.sw_valid = False
        self.gap_hist = Counter()   # system_ts gap value -> count
        self.hw_drops = DropStats()
        self.sw_drops = DropStats()

    def is_ts_valid(self, source):
        return self.global_valid if source == "global" else self.device_valid

    def ts_range(self, source):
        if source == "global":
            return self.global_min, self.global_max
        return self.device_min, self.device_max


class MinMax:
    """Running min/max accumulator."""
    __slots__ = ("lo", "hi")

    def __init__(self):
        self.lo = None
        self.hi = None

    def add(self, value):
        if self.lo is None or value < self.lo:
            self.lo = value
        if self.hi is None or value > self.hi:
            self.hi = value


def fmt_pct(part, total, min_digits=1):
    """Percentage text; keeps adding decimals when a non-zero value would
    otherwise round to zero (e.g. 383 / 10511865 -> 0.0036%)."""
    value = 100.0 * part / total if total else 0.0
    text = "%.*f" % (min_digits, value)
    if part:
        for digits in range(min_digits, min_digits + 6):
            text = "%.*f" % (digits, value)
            if float(text) != 0.0:
                break
    return text


# --------------------------------------------------------------------------- #
# Pass 1: streaming scan (metadata + statistics, no frame storage)
# --------------------------------------------------------------------------- #
def _row_field(row, col_index, name):
    i = col_index.get(name)
    if i is None or i >= len(row):
        return None
    return row[i]


def _int_field(row, col_index, name, default):
    v = _row_field(row, col_index, name)
    return int(v) if v not in (None, "") else default


def scan_stream(path, index, sn, sensor):
    """Single pass over one CSV collecting frame count, timestamp validity and
    ranges, the system_ts gap histogram, and frame-number drop stats."""
    st = StreamStats(index, sn, sensor, path)
    frame_count = 0
    global_min = global_max = None
    device_min = device_max = None
    global_valid = device_valid = False
    hw_valid = sw_valid = False
    last_sys = None
    gap_hist = Counter()
    hw_drops = DropStats()
    sw_drops = DropStats()

    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header is None:
            return st
        col_index = {}
        for i, name in enumerate(header):
            col_index[name] = i

        for row in reader:
            if not row:
                continue
            if _row_field(row, col_index, "global_ts_us") in (None, "") and \
               _row_field(row, col_index, "device_ts_us") in (None, ""):
                continue
            sw = _int_field(row, col_index, "sw_frame_num", -1)
            hw = _int_field(row, col_index, "hw_frame_num", -1)
            system_ts = _int_field(row, col_index, "system_ts_us", 0)
            device_ts = _int_field(row, col_index, "device_ts_us", 0)
            global_ts = _int_field(row, col_index, "global_ts_us", 0)

            frame_count += 1
            if global_min is None or global_ts < global_min:
                global_min = global_ts
            if global_max is None or global_ts > global_max:
                global_max = global_ts
            if device_min is None or device_ts < device_min:
                device_min = device_ts
            if device_max is None or device_ts > device_max:
                device_max = device_ts
            if global_ts != 0:
                global_valid = True
            if device_ts != 0:
                device_valid = True
            if last_sys is not None and 0 < system_ts - last_sys < 1000000:
                gap_hist[system_ts - last_sys] += 1
            last_sys = system_ts

            if sw != -1:
                sw_valid = True
            if hw != -1:
                hw_valid = True
            sw_drops.add(sw)
            hw_drops.add(hw)

    st.frame_count = frame_count
    st.global_min = global_min
    st.global_max = global_max
    st.global_valid = global_valid
    st.device_min = device_min
    st.device_max = device_max
    st.device_valid = device_valid
    st.hw_valid = hw_valid
    st.sw_valid = sw_valid
    st.gap_hist = gap_hist
    st.hw_drops = hw_drops
    st.sw_drops = sw_drops
    return st


def discover_csv(data_dir):
    """Match sync CSV filenames and run the Pass-1 scan on each file.
    Returns ({sensor: {index: StreamStats}}, found_count, conflicts); conflicts
    lists devN collisions (same index, different SN) that make the inputs
    ambiguous and must be resolved before analysis."""
    result = {"depth": {}, "color": {}}
    found = 0
    conflicts = []
    for name in os.listdir(data_dir):
        m = CSV_NAME_RE.match(name)
        if not m:
            continue
        sensor = m.group(1).lower()
        index = int(m.group(2))
        sn = m.group(3)
        prev = result[sensor].get(index)
        if prev is not None and prev.sn != sn:
            conflicts.append("dev%d %s: SN %s (%s) and SN %s (%s)" %
                             (index, sensor, prev.sn, os.path.basename(prev.path), sn, name))
            continue
        path = os.path.join(data_dir, name)
        try:
            st = scan_stream(path, index, sn, sensor)
        except (OSError, ValueError, KeyError) as e:
            print("WARN: failed to read %s: %s" % (name, e))
            continue
        result[sensor][index] = st
        found += 1
    for idx in sorted(set(result["depth"]) & set(result["color"])):
        d_sn = result["depth"][idx].sn
        c_sn = result["color"][idx].sn
        if d_sn != c_sn:
            conflicts.append("dev%d: depth SN %s but color SN %s" % (idx, d_sn, c_sn))
    return result, found, conflicts


def histogram_median(hist):
    """Exact median of the multiset represented by a value -> count mapping of
    integers (equals statistics.median of the fully expanded list)."""
    n = sum(hist.values())
    if n == 0:
        return None
    lo_rank = (n - 1) // 2
    hi_rank = n // 2
    lo = hi = None
    cum = 0
    for value, cnt in sorted(hist.items()):
        if lo is None and cum + cnt > lo_rank:
            lo = value
        if hi is None and cum + cnt > hi_rank:
            hi = value
        if lo is not None and hi is not None:
            break
        cum += cnt
    return (lo + hi) / 2


def detect_fps(sensors_data):
    """Detect frame rate from the median of per-stream median frame intervals.
    Returns fps or None if there is not enough data."""
    medians = []
    for sensor in sensors_data:
        for st in sensors_data[sensor].values():
            if sum(st.gap_hist.values()) >= 5:
                medians.append(histogram_median(st.gap_hist))
    if not medians:
        return None
    return round(1e6 / statistics.median(medians), 2)


# --------------------------------------------------------------------------- #
# Pass 2: streaming reader and matcher
# --------------------------------------------------------------------------- #
class FrameReader:
    """Streams one CSV and exposes only the current head frame.
    Matching relies on rows being sorted by the chosen timestamp."""
    __slots__ = ("stats", "ts_source", "_file", "_reader", "_col_index",
                 "_head", "_eof")

    def __init__(self, stats, ts_source):
        self.stats = stats
        self.ts_source = ts_source
        self._file = None
        self._reader = None
        self._col_index = {}
        self._head = None
        self._eof = False

    def open(self):
        self._file = open(self.stats.path, "r", encoding="utf-8-sig", newline="")
        self._reader = csv.reader(self._file)
        header = next(self._reader, None)
        if header is not None:
            for i, name in enumerate(header):
                self._col_index[name] = i
        return self

    def close(self):
        if self._file is not None:
            self._file.close()
            self._file = None

    def _next_record(self):
        for row in self._reader:
            if not row:
                continue
            if _row_field(row, self._col_index, "global_ts_us") in (None, "") and \
               _row_field(row, self._col_index, "device_ts_us") in (None, ""):
                continue
            return FrameRecord(
                _int_field(row, self._col_index, "row_id", 0),
                _int_field(row, self._col_index, "sw_frame_num", -1),
                _int_field(row, self._col_index, "hw_frame_num", -1),
                _int_field(row, self._col_index, "system_ts_us", 0),
                _int_field(row, self._col_index, "device_ts_us", 0),
                _int_field(row, self._col_index, "global_ts_us", 0))
        return None

    def head(self):
        """Current frame, or None at end of stream."""
        if self._head is None and not self._eof:
            self._head = self._next_record()
            if self._head is None:
                self._eof = True
        return self._head

    def head_ts(self):
        rec = self.head()
        return rec.ts(self.ts_source) if rec is not None else None

    def advance(self):
        self._head = None


class CsvSink:
    """Output CSV created lazily on the first row (empty results write no file)."""
    __slots__ = ("path", "header", "rows", "_file", "_writer")

    def __init__(self, path, header):
        self.path = path
        self.header = header
        self.rows = 0
        self._file = None
        self._writer = None

    def write(self, values):
        if self._file is None:
            base = os.path.dirname(self.path)
            if base:
                os.makedirs(base, exist_ok=True)
            self._file = open(self.path, "w", encoding="utf-8-sig", newline="")
            self._writer = csv.writer(self._file)
            self._writer.writerow(self.header)
        self._writer.writerow(values)
        self.rows += 1

    def close(self):
        if self._file is not None:
            self._file.close()
            self._file = None


def match_streaming(readers, half_gap_us, on_group, on_fail):
    """Greedy grouping over per-device stream heads: each round picks the
    earliest head as the anchor; if every device head falls within the
    half-frame interval they form one group, otherwise the anchor frame is
    unmatched and dropped.

    This is equivalent to the previous in-memory pointer walk: the anchor is
    always the minimum head timestamp, so each other device can only ever
    match its own head (never a later frame), and both sides advance by at
    most one frame per round.

    Returns (group_count, fail_count)."""
    group_count = 0
    fail_count = 0
    n = len(readers)
    while True:
        anchor = -1
        anchor_ts = None
        for i in range(n):
            t = readers[i].head_ts()
            if t is not None and (anchor_ts is None or t < anchor_ts):
                anchor_ts = t
                anchor = i
        if anchor == -1:
            break

        ok = True
        for i in range(n):
            t = readers[i].head_ts()
            if t is None or t - anchor_ts > half_gap_us:
                ok = False
                break

        if ok:
            on_group([r.head() for r in readers])
            group_count += 1
            for r in readers:
                r.advance()
        else:
            on_fail(readers[anchor], readers[anchor].head())
            fail_count += 1
            readers[anchor].advance()
    return group_count, fail_count


# --------------------------------------------------------------------------- #
# Analysis
# --------------------------------------------------------------------------- #
def resolve_ts_source(requested, devices, sensor, lines):
    """Resolve the time base from the request and data validity across all devices."""
    global_valid = all(d.is_ts_valid("global") for d in devices)
    device_valid = all(d.is_ts_valid("device") for d in devices)
    chosen = requested
    notes = []
    if requested == "auto":
        if global_valid:
            chosen = "global"
        elif device_valid:
            chosen = "device"
            notes.append("Auto-degraded to device timestamps (global_ts_us all 0, device may not support global timestamp)")
        else:
            chosen = "device"
            notes.append("WARN: both global and device timestamps are abnormal; still matching on device")
    elif requested == "global" and not global_valid:
        invalid = [d.index for d in devices if not d.is_ts_valid("global")]
        notes.append("WARN: global selected but dev%s has all-zero global_ts_us" % invalid)
    elif requested == "device" and not device_valid:
        notes.append("WARN: device timestamps are abnormal")
    for n in notes:
        lines.append("  " + n)
    if chosen == "device":
        lines.append("  [%s] RISK: matching on DEVICE timestamps - device clocks are per-device;"
                     " without clock sync during capture, cross-device diffs include clock"
                     " offset/drift, indicative only." % sensor.upper())
    return chosen


def resolve_per_device_ts(requested, streams):
    """Resolve the pairing time base for the depth/color streams of ONE device."""
    global_valid = all(s.is_ts_valid("global") for s in streams)
    if requested == "auto":
        return ("global", "") if global_valid else ("device", " - global_ts_us all 0")
    if requested == "global" and not global_valid:
        return "global", " - WARN global_ts_us all 0"
    return "device", ""


def analyze_sensor(sensor, devices_map, ts_source, half_gap_us, threshold, lines, out_dir):
    """Multi-device matching for one sensor; streams the CSVs and writes
    matched/failed rows as they are produced."""
    lines.append("\n========== Sensor: %s ==========" % sensor.upper())
    devices = sorted(devices_map.values(), key=lambda x: x.index)
    if len(devices) < 2:
        lines.append("    Fewer than 2 devices, cannot evaluate sync, skip")
        return 0, 0

    for d in devices:
        lines.append("    dev%d(%s): %d frames" % (d.index, d.sn, d.frame_count))

    if any(d.frame_count for d in devices):
        t_min = min(d.ts_range(ts_source)[0] for d in devices if d.frame_count)
        t_max = max(d.ts_range(ts_source)[1] for d in devices if d.frame_count)
        duration_s = (t_max - t_min) / 1e6
        if duration_s > 0:
            min_frames = min(d.frame_count for d in devices)
            lines.append("    Duration: %.3f s  (~%.2f fps)" % (duration_s, min_frames / duration_s))

    global_ok = all(d.is_ts_valid("global") for d in devices)
    device_ok = all(d.is_ts_valid("device") for d in devices)
    matched_header = ["groupId", "diffGlobalUs", "diffDeviceUs"]
    for d in devices:
        prefix = "dev%d" % d.index
        matched_header += [prefix + "SwNum",
                           prefix + "HwNum",
                           prefix + "SystemUs",
                           prefix + "GlobalUs",
                           prefix + "DeviceUs"]
    matched_sink = CsvSink(os.path.join(out_dir, "sync_matched_%s.csv" % sensor), matched_header)
    failed_sink = CsvSink(os.path.join(out_dir, "sync_failed_%s.csv" % sensor),
                          ["sn", "rowId", "swNum", "hwNum", "systemUs", "deviceUs", "globalUs"])
    abnormal_count = [0]

    def on_group(group):
        gid = matched_sink.rows
        g_list = [f.global_ts for f in group]
        d_list = [f.device_ts for f in group]
        row = [gid,
               (max(g_list) - min(g_list)) if global_ok else "",
               (max(d_list) - min(d_list)) if device_ok else ""]
        for f in group:
            row += [f.sw_frame_num, f.hw_frame_num, f.system_ts, f.global_ts, f.device_ts]
        matched_sink.write(row)
        ts_list = [f.ts(ts_source) for f in group]
        if max(ts_list) - min(ts_list) >= threshold:
            abnormal_count[0] += 1

    def on_fail(reader, frame):
        failed_sink.write([reader.stats.sn, frame.row_id, frame.sw_frame_num,
                           frame.hw_frame_num, frame.system_ts, frame.device_ts,
                           frame.global_ts])

    readers = [FrameReader(d, ts_source).open() for d in devices]
    try:
        groups, failed = match_streaming(readers, half_gap_us, on_group, on_fail)
    finally:
        for r in readers:
            r.close()
        matched_sink.close()
        failed_sink.close()

    total_anchors = groups + failed
    lines.append("    Completeness      : %s%% (%d groups, %d unmatched)" % (fmt_pct(groups, total_anchors), groups, failed))

    if not groups:
        lines.append("    (no complete groups, cannot evaluate)")
        return groups, failed

    lines.append("    Abnormal (>= %.0fus): %s%% (%d / %d)" %
                 (threshold, fmt_pct(abnormal_count[0], groups), abnormal_count[0], groups))
    if failed:
        lines.append("    Failed CSV: %s (%d unmatched)" % (failed_sink.path, failed))
    if groups:
        lines.append("    Matched CSV: %s (%d groups)" % (matched_sink.path, groups))
    return groups, failed


def analyze_per_device(sensors_data, ts_source, frame_num_source, half_gap_us, per_dir, lines):
    """Single-device checks per device: depth-color pairing diff + per-stream drop stats."""
    lines.append("\n========== Intra-device checks (depth-color, frame drops) ==========")
    indices = sorted(set(sensors_data["depth"]) | set(sensors_data["color"]))
    drop_rows = []
    files_written = 0

    for idx in indices:
        depth = sensors_data["depth"].get(idx)
        color = sensors_data["color"].get(idx)
        sn = (depth or color).sn
        streams = [s for s in (depth, color) if s and s.frame_count]

        fn_src = frame_num_source
        fn_note = ""
        if frame_num_source == "auto":
            fn_src = "hw" if all(s.hw_valid for s in streams) else "sw"
            if fn_src == "sw":
                fn_note = " (hw_frame_num all -1)"
        elif frame_num_source == "hw" and not all(s.hw_valid for s in streams):
            lines.append("    dev%d(%s) WARN: --frame-num-source hw but hw_frame_num all -1, drop detection unreliable" % (idx, sn))

        lines.append("")
        if depth and color and depth.frame_count and color.frame_count:
            src, ts_note = resolve_per_device_ts(ts_source, [depth, color])
            lines.append("    dev%d(%s)" % (idx, sn))
            lines.append("        ts source       : %s%s" % (src, ts_note.replace(" - ", " (") + (")" if ts_note else "")))
            lines.append("        frame num       : %s%s" % (fn_src.upper(), fn_note))
            lines.append("        frames          : depth %d, color %d" % (depth.frame_count, color.frame_count))

            global_ok = depth.is_ts_valid("global") and color.is_ts_valid("global")
            device_ok = depth.is_ts_valid("device") and color.is_ts_valid("device")
            path = os.path.join(per_dir, "per_device_dev%d_%s.csv" % (idx, sn))
            sink = CsvSink(path, ["groupId", "diffGlobalUs", "diffDeviceUs",
                                  "depthSwNum", "depthHwNum", "depthSystemUs", "depthGlobalUs", "depthDeviceUs",
                                  "colorSwNum", "colorHwNum", "colorSystemUs", "colorGlobalUs", "colorDeviceUs"])
            g_diffs = MinMax()
            d_diffs = MinMax()

            def on_group(pair):
                d, c = pair[0], pair[1]
                g_diffs.add(d.global_ts - c.global_ts)
                d_diffs.add(d.device_ts - c.device_ts)
                sink.write([sink.rows,
                            (d.global_ts - c.global_ts) if global_ok else "",
                            (d.device_ts - c.device_ts) if device_ok else "",
                            d.sw_frame_num, d.hw_frame_num, d.system_ts, d.global_ts, d.device_ts,
                            c.sw_frame_num, c.hw_frame_num, c.system_ts, c.global_ts, c.device_ts])

            def on_fail(reader, frame):
                pass  # per-device pairing counts unmatched frames but writes no failed CSV

            readers = [FrameReader(depth, src).open(), FrameReader(color, src).open()]
            try:
                groups, failed = match_streaming(readers, half_gap_us, on_group, on_fail)
            finally:
                for r in readers:
                    r.close()
                sink.close()

            lines.append("        pairs           : %d (unmatched %d)" % (groups, failed))
            if groups:
                if global_ok:
                    lines.append("        global diff     : %d ~ %d us (depth - color)" % (g_diffs.lo, g_diffs.hi))
                else:
                    lines.append("        global diff     : n/a (global_ts_us invalid)")
                if src == "device" and device_ok:
                    lines.append("        device diff     : %d ~ %d us (depth - color)" % (d_diffs.lo, d_diffs.hi))
                lines.append("        CSV             : %s" % path)
                files_written += 1
        else:
            lines.append("    dev%d(%s)" % (idx, sn))
            lines.append("        frame num       : %s%s" % (fn_src.upper(), fn_note))
            lines.append("        pairing         : skip (only one stream has data)")

        starts = []
        parts = []
        for label, s in (("depth", depth), ("color", color)):
            if not s or not s.frame_count:
                continue
            drops = s.hw_drops if fn_src == "hw" else s.sw_drops
            if drops.missing:
                parts.append("%s %d (%s%%)" % (label, len(drops.missing), fmt_pct(len(drops.missing), drops.count + len(drops.missing), 2)))
            else:
                parts.append("%s 0" % label)
            if drops.first_num is not None and drops.first_num > 1:
                starts.append("%s at %sNum %d" % (label, "hw" if fn_src == "hw" else "sw", drops.first_num))
            for m in drops.missing:
                drop_rows.append((sn, label, fn_src, m))
        if parts:
            lines.append("        dropped frames  : " + ", ".join(parts) + "  (positions in Drops CSV)")
        if starts:
            lines.append("        first csv row    : " + ", ".join(starts) + "  (earlier frames not captured, not counted as drops)")

    if drop_rows:
        sink = CsvSink(os.path.join(per_dir, "per_device_drops.csv"),
                       ["sn", "sensor", "frameNumSource", "missingFrameNum"])
        try:
            for r in drop_rows:
                sink.write(list(r))
        finally:
            sink.close()
        lines.append("    Drops CSV: %s (%d frames)" % (sink.path, len(drop_rows)))
        files_written += 1

    return files_written


# --------------------------------------------------------------------------- #
def clean_output_dirs(base_out):
    """Delete analysis CSVs left by a previous run (zero-row sinks write no
    file, so without this a stale result would survive into the current one)."""
    for sub in ("analysis_multi_device", "analysis_per_device"):
        for pattern in ("sync_matched_*.csv", "sync_failed_*.csv", "per_device_*.csv"):
            for path in glob.glob(os.path.join(base_out, sub, pattern)):
                try:
                    os.remove(path)
                except OSError as e:
                    print("WARN: could not remove stale output %s: %s" % (path, e))


def main():
    parser = argparse.ArgumentParser(
        description="Multi-device sync timestamp CSV analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("data_dir", nargs="?", default=None,
                        help="Directory with sync_*_dev*_<SN>.csv (default: current directory)")
    parser.add_argument("--fps", type=float, default=DEFAULT_FRAME_RATE,
                        help="Frame rate; grouping tolerance = half frame (default: auto-detect from data)")
    parser.add_argument("--threshold", type=float, default=DEFAULT_TSP_RANGE_THRESHOLD,
                        help="Abnormal in-group range threshold in us (default %.0f)" % DEFAULT_TSP_RANGE_THRESHOLD)
    parser.add_argument("--ts-source", choices=["global", "device", "auto"],
                        default=DEFAULT_TIMESTAMP_SOURCE,
                        help="Time base: global/device/auto (default %s)" % DEFAULT_TIMESTAMP_SOURCE)
    parser.add_argument("--frame-num-source", choices=["hw", "sw", "auto"],
                        default=DEFAULT_FRAME_NUM_SOURCE,
                        help="Frame number source for drop detection: hw/sw/auto (default %s)" % DEFAULT_FRAME_NUM_SOURCE)
    parser.add_argument("--output", default=None,
                        help="Output directory base for CSVs (default: <data_dir>, writes analysis_multi_device/ and analysis_per_device/)")
    args = parser.parse_args()

    data_dir = os.path.abspath(args.data_dir) if args.data_dir else os.getcwd()
    if not os.path.isdir(data_dir):
        print("ERROR: directory does not exist: %s" % data_dir)
        return 1

    sensors_data, found, conflicts = discover_csv(data_dir)
    if found == 0:
        print("ERROR: no sync_*_dev*_<SN>.csv found under %s" % data_dir)
        return 1
    if conflicts:
        print("ERROR: one devN maps to multiple serial numbers; clean the data dir or analyze one batch per directory:")
        for c in conflicts:
            print("  " + c)
        return 1

    threshold = args.threshold
    ts_source = args.ts_source

    fps = args.fps
    fps_note = ""
    if fps is None:
        fps = detect_fps(sensors_data)
        if fps is None:
            fps = 30.0
            fps_note = "  (auto-detect failed, too few frames, fallback to 30)"
        else:
            fps_note = "  (auto-detected from median frame interval)"
    if fps <= 0 or fps >= 1000:
        print("ERROR: invalid frameRate=%.1f" % fps)
        return 1

    half_gap_us = 1000000.0 / fps / 2.0
    base_out = args.output or data_dir
    out_dir = os.path.join(base_out, "analysis_multi_device")
    os.makedirs(out_dir, exist_ok=True)
    clean_output_dirs(base_out)

    print("=" * 60)
    print("Multi-device sync CSV analysis")
    print("  Data dir   : %s" % data_dir)
    print("  Frame rate : %.2f fps%s" % (fps, fps_note))
    print("    - frames from all devices are grouped when their timestamps fall within")
    print("      half a frame interval (%.0f us); a wrong fps gives a wrong tolerance" % half_gap_us)
    print("  Threshold  : %.0f us" % threshold)
    print("    - a matched group whose max-min timestamp range reaches this value")
    print("      is counted as Abnormal")
    print("  Time base  : %s" % ts_source)
    print("    - timestamps used for matching; global = global_ts_us, device = device_ts_us,")
    print("      auto = global when available, degrades to device otherwise")
    print("  Frame num  : %s" % args.frame_num_source)
    print("    - frame number used for drop detection; hw = hw_frame_num, sw = sw_frame_num,")
    print("      auto = hw when available, degrades to sw otherwise (see 'frame num' per device)")
    print("=" * 60)

    lines = []
    for sensor in ("depth", "color"):
        devices = sensors_data[sensor]
        if not devices:
            lines.append("\n========== Sensor: %s ==========" % sensor.upper())
            lines.append("    No data, skip")
            continue
        chosen_ts = resolve_ts_source(ts_source, list(devices.values()), sensor, lines)
        analyze_sensor(sensor, devices, chosen_ts, half_gap_us, threshold, lines, out_dir)

    per_dir = os.path.join(base_out, "analysis_per_device")
    analyze_per_device(sensors_data, ts_source, args.frame_num_source, half_gap_us, per_dir, lines)

    print("\n" + "\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())

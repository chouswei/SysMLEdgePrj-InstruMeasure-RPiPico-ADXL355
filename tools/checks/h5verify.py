#!/usr/bin/env python3
"""Verify a pico355-hmi HDF5 log with h5py.

Usage: python3 -I h5verify.py <path> <expected_n> <range_code> <odr_hz> <hpf_code> <overflow>
(hpf_code -1 = high-pass not applied). Exit 0 = all checks pass, 1 = a check failed.
Scale values come from the cited no-OS lines (adxl355.h:62-65, adxl355.c:18): 3.9e-6 g/LSB at
range code 1, times 1, 2, 4 for codes 1, 2, 3. LSB per g is the derived reciprocal.
"""
import sys

import h5py
import numpy as np

MUL = {1: 1, 2: 2, 3: 4}
G_PER_LSB_2G = 3.9e-6
CHUNK = 4096
errors = []


def check(cond, msg):
    if not cond:
        errors.append(msg)


def text(v):
    return v.decode() if isinstance(v, bytes) else str(v)


def main(argv):
    if len(argv) != 7:
        print(__doc__)
        return 2
    path, n, rng, odr, hpf, ovf = argv[1], int(argv[2]), int(argv[3]), int(argv[4]), int(argv[5]), int(argv[6])
    scale = G_PER_LSB_2G * MUL[rng]
    with h5py.File(path, "r") as f:
        check("accel_g" in f and isinstance(f["accel_g"], h5py.Group), "group accel_g missing")
        check("raw_counts" in f and isinstance(f["raw_counts"], h5py.Group), "group raw_counts missing")
        want = {
            "accel_g/x_g": "float64", "accel_g/y_g": "float64", "accel_g/z_g": "float64",
            "raw_counts/x": "int32", "raw_counts/y": "int32", "raw_counts/z": "int32",
            "sample_index": "uint64",
        }
        data = {}
        for name, dt in want.items():
            if name not in f:
                errors.append(f"dataset {name} missing")
                continue
            d = f[name]
            check(d.shape == (n,), f"{name} shape {d.shape} != ({n},)")
            check(str(d.dtype) == dt, f"{name} dtype {d.dtype} != {dt}")
            check(d.chunks == (CHUNK,), f"{name} chunks {d.chunks} != ({CHUNK},)")
            check(d.maxshape == (None,), f"{name} maxshape {d.maxshape} != (None,)")
            data[name] = d[()]
        a = f.attrs
        for k in ("odr_hz", "range_g", "scale_g_per_lsb", "scale_lsb_per_g", "hpf_code",
                  "fifo_overflow_count", "units", "sensor", "started_utc"):
            check(k in a, f"root attribute {k} missing")
        if not errors:
            check(int(a["odr_hz"]) == odr, f"odr_hz {a['odr_hz']} != {odr}")
            check(int(a["range_g"]) == 2 * MUL[rng], f"range_g {a['range_g']}")
            check(abs(float(a["scale_g_per_lsb"]) - scale) < 1e-12, f"scale_g_per_lsb {a['scale_g_per_lsb']}")
            check(abs(float(a["scale_lsb_per_g"]) * scale - 1.0) < 1e-9, "scale_lsb_per_g is not the reciprocal")
            check(int(a["hpf_code"]) == hpf, f"hpf_code {a['hpf_code']} != {hpf}")
            check(int(a["fifo_overflow_count"]) == ovf, f"fifo_overflow_count {a['fifo_overflow_count']} != {ovf}")
            check(text(a["units"]) == "g", f"units {a['units']}")
            check(text(a["sensor"]) == "ADXL355", f"sensor {a['sensor']}")
            check(len(text(a["started_utc"])) == 20 and text(a["started_utc"]).endswith("Z"),
                  f"started_utc {a['started_utc']}")
            for ax, g in (("x", "x_g"), ("y", "y_g"), ("z", "z_g")):
                raw = data[f"raw_counts/{ax}"].astype(np.float64)
                check(np.allclose(data[f"accel_g/{g}"], raw * scale, rtol=0, atol=1e-12), f"{g} != raw {ax} x scale")
            idx = data["sample_index"]
            check(n == 0 or bool(np.all(np.diff(idx.astype(np.int64)) == 1)), "sample_index not contiguous")
    if errors:
        for e in errors:
            print("FAIL:", e)
        return 1
    print(f"h5verify: OK ({n} samples)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

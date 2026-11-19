"""Train the on-device activity model and export it as an int8 C header.

    python tools/train_activity.py            # writes include/activity_weights.h

The model is a tiny MLP (Multi-Layer Perceptron), 5 inputs -> 8 hidden -> 3
classes (still, walk, run). Training data is synthetic accelerometer windows
made with the same signal shapes the firmware simulator uses, and the
features are computed exactly the way src/imu.c computes them.

After training, weights are quantized to int8 and the script re-runs the
test set through an integer-only copy of the C inference code, so the
accuracy it prints is the accuracy of what ships, not of the float model.
Needs only numpy.
"""
from __future__ import annotations

import pathlib

import numpy as np

HZ = 50
WIN = 2 * HZ
CLASSES = ["still", "walk", "run"]
ROOT = pathlib.Path(__file__).resolve().parent.parent
rng = np.random.default_rng(7)


def window(label: int) -> np.ndarray:
    """One 2 s window of x, y, z acceleration in g."""
    t = np.arange(WIN) / HZ
    if label == 0:
        lying = rng.random() < 0.3
        g = np.array([1.0, 0.0, 0.0]) if lying else np.array([0.0, 0.0, 1.0])
        xyz = np.tile(g, (WIN, 1)) + rng.normal(0, 0.02, (WIN, 3))
    else:
        f = rng.uniform(1.5, 2.2) if label == 1 else rng.uniform(2.5, 3.3)
        amp = rng.uniform(0.2, 0.45) if label == 1 else rng.uniform(0.7, 1.2)
        noise = 0.05 if label == 1 else 0.1
        phase = rng.uniform(0, 2 * np.pi)
        z = 1.0 + amp * np.sin(2 * np.pi * f * t + phase)
        x = 0.15 * amp * np.sin(np.pi * f * t + phase)
        xyz = np.stack([x, np.zeros(WIN), z], axis=1) + rng.normal(0, noise, (WIN, 3))
    return xyz


def features(xyz: np.ndarray) -> np.ndarray:
    """Same five features as act_window_features() in src/imu.c."""
    m = np.sqrt((xyz ** 2).sum(axis=1))
    mean = m.mean()
    d = m - mean
    s = np.where(d > 0.05, 1, np.where(d < -0.05, -1, 0))
    s = s[s != 0]
    crossings = int((s[1:] != s[:-1]).sum()) if len(s) > 1 else 0
    return np.array([mean, m.std(), m.max() - m.min(), crossings / 2.0, xyz[:, 2].mean()])


def dataset(n_per_class: int):
    X, y = [], []
    for label in range(3):
        for _ in range(n_per_class):
            X.append(features(window(label)))
            y.append(label)
    return np.array(X, dtype=np.float32), np.array(y)


def train(X, y, hidden=8, steps=3000, lr=0.1):
    n, d = X.shape
    W1 = rng.normal(0, 0.5, (d, hidden)).astype(np.float32)
    b1 = np.zeros(hidden, np.float32)
    W2 = rng.normal(0, 0.5, (hidden, 3)).astype(np.float32)
    b2 = np.zeros(3, np.float32)
    Y = np.eye(3)[y]
    for _ in range(steps):
        h = np.maximum(0, X @ W1 + b1)
        z = h @ W2 + b2
        p = np.exp(z - z.max(1, keepdims=True))
        p /= p.sum(1, keepdims=True)
        g = (p - Y) / n
        gW2, gb2 = h.T @ g, g.sum(0)
        gh = (g @ W2.T) * (h > 0)
        gW1, gb1 = X.T @ gh, gh.sum(0)
        W1 -= lr * gW1; b1 -= lr * gb1; W2 -= lr * gW2; b2 -= lr * gb2
    return W1, b1, W2, b2


def quantize(W1, b1, W2, b2, Xcal):
    s_in = 4.0 / 127.0
    s_w1 = np.abs(W1).max() / 127.0
    s_w2 = np.abs(W2).max() / 127.0
    h = np.maximum(0, Xcal @ W1 + b1)
    s_h = max(h.max(), 1e-6) / 127.0
    q = {
        "W1": np.clip(np.round(W1 / s_w1), -127, 127).astype(int),
        "b1": np.round(b1 / (s_in * s_w1)).astype(int),
        "W2": np.clip(np.round(W2 / s_w2), -127, 127).astype(int),
        "b2": np.round(b2 / (s_h * s_w2)).astype(int),
        "s_in": s_in, "s_w1": s_w1, "s_w2": s_w2, "s_h": s_h,
    }
    return q


def int8_predict(q, Xn):
    """Integer-only inference, mirroring activity_model_predict() in C."""
    out = []
    for x in Xn:
        qi = np.clip(np.round(x / q["s_in"]), -127, 127).astype(int)
        acc1 = qi @ q["W1"] + q["b1"]
        h = np.maximum(acc1, 0) * (q["s_in"] * q["s_w1"])
        qh = np.clip(np.round(h / q["s_h"]), 0, 127).astype(int)
        acc2 = qh @ q["W2"] + q["b2"]
        out.append(int(np.argmax(acc2)))
    return np.array(out)


def c_array(name, arr, ctype):
    flat = ", ".join(str(int(v)) for v in np.asarray(arr).flatten())
    return f"static const {ctype} {name}[{np.asarray(arr).size}] = {{{flat}}};\n"


def f_array(name, arr):
    flat = ", ".join(f"{float(v):.6f}f" for v in arr)
    return f"static const float {name}[{len(arr)}] = {{{flat}}};\n"


def main():
    Xtr, ytr = dataset(400)
    Xte, yte = dataset(150)
    mu, sigma = Xtr.mean(0), Xtr.std(0) + 1e-6
    Ntr, Nte = (Xtr - mu) / sigma, (Xte - mu) / sigma
    W1, b1, W2, b2 = train(Ntr, ytr)

    float_acc = (np.argmax(np.maximum(0, Nte @ W1 + b1) @ W2 + b2, 1) == yte).mean()
    q = quantize(W1, b1, W2, b2, Ntr)
    pred = int8_predict(q, Nte)
    int8_acc = (pred == yte).mean()
    print(f"test accuracy: float {float_acc:.3f}, int8 {int8_acc:.3f} (n={len(yte)})")
    for c in range(3):
        row = [int(((yte == c) & (pred == k)).sum()) for k in range(3)]
        print(f"  {CLASSES[c]:>5}: {row}")

    hdr = ROOT / "include" / "activity_weights.h"
    with open(hdr, "w") as f:
        f.write("/* Generated by tools/train_activity.py. Do not edit by hand.\n")
        f.write(f" * Synthetic test set accuracy: float {float_acc:.3f}, int8 {int8_acc:.3f}\n */\n")
        f.write("#ifndef ACTIVITY_WEIGHTS_H\n#define ACTIVITY_WEIGHTS_H\n\n#include <stdint.h>\n\n")
        f.write(f"#define ACT_HIDDEN {W1.shape[1]}\n\n")
        f.write(f_array("ACT_MU", mu))
        f.write(f_array("ACT_SIGMA", sigma))
        f.write(c_array("ACT_W1", q["W1"], "int8_t"))
        f.write(c_array("ACT_B1", q["b1"], "int32_t"))
        f.write(c_array("ACT_W2", q["W2"], "int8_t"))
        f.write(c_array("ACT_B2", q["b2"], "int32_t"))
        f.write(f"\n#define ACT_S_IN {q['s_in']:.8f}f\n#define ACT_S_W1 {q['s_w1']:.8f}f\n")
        f.write(f"#define ACT_S_W2 {q['s_w2']:.8f}f\n#define ACT_S_H {q['s_h']:.8f}f\n\n#endif\n")
    print(f"wrote {hdr.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
